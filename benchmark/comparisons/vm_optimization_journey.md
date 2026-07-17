# Optimizing the PgScript VM — a measurement-driven journey

We spent a single session iterating on the PgScript interpreter, hunting for performance against CPython and Lua 5.4. The work didn't follow a heroic narrative — it was a sequence of measurements, wrong hypotheses, corrections, and finally some real wins. This is what we changed, why, and what each step bought us.

---

## The setup

PgScript is a bytecode-interpreted scripting language embedded in PgEngine, a 2D ECS-first C++17 game engine. The VM is a stack machine with NaN-boxed values and a bytecode optimizer that runs a chain of peephole passes before execution. We wanted to know where it stood against CPython and Lua 5.4 on canonical workloads, and what we could close the gap with.

We built a small benchmark harness with 6 scenarios:

- **hot_loop** — tight integer loop, summing 1..N
- **compute_pi** — Leibniz series, mostly numeric
- **fib_recursive** — recursive function calls
- **binary_tree** — object allocation + traversal
- **matmul** — 2D array indexing + nested loops
- **table_lookup** — hash-table dispatch

Each measured both **wall-clock time** (full subprocess, including interpreter startup) and **script-time** (in-script `now()` self-timing — pure VM cost).

Initial state, PgScript O3 vs CPython 3.10 at large workloads:

| scenario | gap |
|---|---|
| hot_loop 1M | 1.39× slower |
| compute_pi 1M | 1.50× slower |
| matmul 64 | 1.75× slower |
| table_lookup 1M | 1.75× slower |
| fib_recursive 28 | 2.28× slower |
| binary_tree 16 | 3.04× slower |

Mostly losing, sometimes by a lot. Time to dig in.

---

## Methodology first: a benchmarking pitfall

Before any optimization, we hit a methodology bug that's worth flagging.

Our initial `hot_loop.pg` looked like this:

```
var sum = 0;
var i = 1;
while (i <= count) {
    sum = sum + i;
    i = i + 1;
}
```

`var` at script top-level in PgScript creates **globals**, not locals. So the hot loop was doing **~10 hash-table lookups by string name** per iteration just to read `sum`, `i`, and `count`. The optimizer passes only fired on locals — they had nothing to optimize.

Wrapping the workload in a function (`fun hotLoop(n) { ... }`) made the variables proper local slots. Result: **~5× speedup before changing any VM code**. The Python and Lua versions either had no such trap (Lua uses `local` explicitly) or were already function-scoped.

**Lesson:** *Benchmarks are only as honest as the scripts being benchmarked.* When comparing language implementations, ensure each script exercises the language's idiomatic fast path.

---

## False start: the dispatch hypothesis

With clean scripts in hand, we used `perf stat` to compare PgScript and CPython on `hot_loop 1M`:

| metric | PgScript | CPython |
|---|---|---|
| IPC | 3.94 | 3.26 |
| branch-miss rate | 0.02% | 0.42% |
| L1 icache misses | 362K | 2,526K |
| host instructions | 1,106M | 1,002M |
| wall (this perf run) | 67 ms | 77 ms |

We had hypothesized that PgScript's function-pointer dispatch (a table lookup + indirect call per opcode) was costing us. **Perf said otherwise**: the CPU was running near-peak IPC (3.94) and branch prediction was essentially perfect (0.02% miss). The dispatch loop was fine. PgScript just ran *more host instructions* per script opcode — ~110 vs CPython's ~83.

This was important. **The bottleneck wasn't dispatch overhead; it was the work each opcode did.** It shaped the rest of the session: every subsequent change targeted opcode body cost, not dispatch architecture.

---

## Optimization 1: Decoded variants for hot opcodes (+small)

PgScript has two dispatch paths per opcode: a normal one that reads operands via `*currentFrame->ip++`, and a "decoded" variant that takes the pre-extracted operand from the `DecodedInstruction` struct. Several hot opcodes (`OP_Short_Int`, `OP_AddLL`, `OP_LessEqual`, etc.) were missing their decoded twin. Each opcode without one paid an extra `currentFrame->ip = ...` store from the dispatcher.

We added decoded variants for the comparison and arithmetic opcodes. The macro template made it a one-line per opcode change.

**Gain:** −2% host instructions. Real but small. The CPU branch predictor was already hiding most of the missing-decoded cost.

---

## Optimization 2: The `isPure()` fast path (+5%)

Reading the dispatch loop carefully revealed something striking — after every opcode dispatch, the dispatcher ran **four chains of opcode-equality checks** to detect jumps, conditional jumps, calls, and returns:

```cpp
OpCode opcode = static_cast<OpCode>(instr.originalOpcode);
if (opcode == OP_Jump || opcode == OP_Loop || ...) { /* 4 cmps */ }
if (opcode == OP_Jump_If_False_Popping || ...)     { /* 4 cmps */ }
if (opcode == OP_Call || opcode == OP_Invoke || ...){ /* 6 cmps */ }
if (opcode == OP_Return)                            { /* 1 cmp  */ }
```

For an arithmetic opcode like `OP_AddLL`, **all 14 comparisons run, all return false, every single iteration.** Yet the `DecodedInstruction` already had a `flags` byte with an `isPure()` predicate (set by `register_builtin_operations` for arithmetic ops). The flag was being computed but never used.

We added one fast path:

```cpp
if (instr.isPure()) {
    instructionIndex++;
    continue;
}
// existing control-flow chain for non-pure ops
```

**Gain on `hot_loop 1M`: 1,083M → 1,028M instructions (−5%), 273M → 263M cycles (−4%).** A single conditional saved ~14 instructions per pure-opcode dispatch. Since 80%+ of dispatches in a tight loop are pure, this compounded.

**Lesson:** *Data structures often carry pre-baked information the runtime then re-computes anyway.* The flag was there. We just had to use it.

---

## Optimization 3: Pre-baked conditional jump targets (+8% cycles)

A deeper version of the same insight. The dispatcher's conditional jump branch did this:

```cpp
size_t currentIpOffset = currentFrame->ip - startingIp;
instructionIndex = decoded->findInstructionIndex(currentIpOffset);
// findInstructionIndex: unordered_map<size_t, size_t>::find
```

An **unordered_map lookup per conditional jump**. At 1M iterations of `hot_loop`, that's 1M hash lookups.

But the decode pass `resolveJumpTargets()` was already pre-computing `instr.nextInstuctionIndex` for all jumps — including conditional ones. We were just ignoring it for conditionals. A conditional jump can only land in two places: the next sequential instruction, or the pre-baked target. We rewrote:

```cpp
const size_t fallThroughOffset = instr.bytecodeOffset + 1 + instr.operandBytes;
if ((currentFrame->ip - startingIp) == fallThroughOffset)
    instructionIndex++;
else
    instructionIndex = instr.nextInstuctionIndex;
```

**Gain on hot_loop 1M: 263M → 241M cycles (−8.4%), IPC 3.91 → 4.24.** The cycle drop was bigger than the instruction drop because the hash lookup had been stalling the pipeline.

---

## Optimization 4: Peephole fusion passes (+30% on hot_loop)

PgScript already had passes for `Get_Local + Get_Local + Add → AddLL` and `Get_Local + Constant + Add + Set_Local + Pop → Short_Int + Post_Incr_Local`. We added two more:

**`SetLocalPopFusion`** — every assignment statement compiles to `Set_Local + Pop` (Set_Local leaves the value on the stack for expression statements; the trailing Pop drops it). One fused `Set_Local_Pop` op replaces the pair.

**`ComparisonLocalIndexing`** — same shape as the existing arithmetic pass, but for comparisons: `Get_Local + Get_Local + Less → LessLL`, same for `LessEqual`.

Both required also adding the corresponding opcodes (`OP_Set_Local_Pop`, `OP_LessLL`, `OP_LessEqualLL`) with both eager and decoded handlers.

**Ordering matters more than the passes themselves.** `IncrementOptimizationPass` needs to see `Set_Local + Pop` (5-op pattern), but `SetLocalPopFusion` collapses that to one op. So `IncrementOptimization` had to run first. And both had to run before `FuseOpPop` — which merges adjacent `Pop`s into `PopN` and would have erased the trailing `Pop` needed by the increment pattern.

We also hit a subtle bug in the bytecode rewriter while adding these: the pattern matcher rejected any pattern containing a jump target, including the *first* instruction. But a jump landing at the *start* of a fused pattern is safe — execution just runs the new fused op, which is semantically equivalent. Loosening the check (only reject jump targets *inside* the pattern, not at its start) let the loop-condition `LessEqual` fuse, which is what made it actually pay off.

**Gain on hot_loop 1M: 60 ms → 38.6 ms (−36%).** Inner loop went from 10 opcodes to 7.

---

## Optimization 5: Cleaning up `op_get_local` (+10-25%)

Once the dispatch loop was tight, the next bottleneck was a single handler. `op_get_local_decoded` looked like this:

```cpp
uint8_t slot = instr.operands.byte;
if (slot >= 255) { runtimeError(...); return; }   // dead — uint8 max IS 255
Value value = currentFrame->slots[slot];
if (requiresRefCount(value)) {                    // explicit branch
    push(retainValue(value));                     // retainValue does same check internally
} else {
    push(value);
}
```

Two problems on every `OP_Get_Local`:

1. **`slot >= 255` is dead code** — `slot` is `uint8_t`, can only equal 255 as an out-of-bound marker that well-formed bytecode never emits. The branch always falls through.
2. **`requiresRefCount(value)` is called twice on the heap path** — once at the call site, then again inside the inline `retainValue` (which early-returns for primitives).

Cleanup:

```cpp
uint8_t slot = instr.operands.byte;
assert(slot < 255);                                // compiled out in release
push(retainValue(currentFrame->slots[slot]));      // single check via inline early-return
```

Just `op_get_local` runs 2–4 times per loop iteration in tight scenarios — so even saving a branch each is amplified.

**Gain across the bench:** mean +16%, all 18 scenarios improved, zero regressions. `fib_recursive`, which had previously shown a small regression, flipped to +3-4% gain. Recursive code calls Get_Local heavily on every frame entry — the dead-branch tax was hurting it.

---

## Optimization 6: The big one — reorganizing the NaN-boxed tag layout (+25%)

The `requiresRefCount(v)` check was running on every value passed into `retainValue` / `releaseValue` — that's every assignment, every push, every parameter pass. The check did this:

```cpp
if (not IS_TAGGED(v))      return false;  // doubles
if (IS_INT(v) || IS_BOOL(v)) return false;
if (IS_SMALL_STRING(v))    return false;
if (IS_INTERNED_STRING(v)) return false;
if (IS_CUSTOM_PTR(v))      return false;
return true;
```

Five sequential type checks — each a mask + compare + branch — for what should ideally be a single bit test.

PgScript uses NaN-boxing: 64-bit `Value`s where doubles are stored directly in IEEE 754 form, and tagged values (ints, strings, pool indices, etc.) use the NaN signaling bits as a marker plus a 3-bit `tag` field. Two sign-bit halves, 8 tag slots each.

The original layout mixed refcounted and non-refcounted types across both sign-bit sides. If we could move them so **all refcounted types** lived on one side and **all non-refcounted** on the other, the check would become:

```cpp
inline bool requiresRefCount(Value v) {
    return (v & (QNAN_MASK | SIGN_BIT)) == QNAN_MASK;  // 1 mask, 1 compare
}
```

The hitch: 9 refcounted types (STRING, CLOSURE, FUNCTION, UPVALUE, CLASS, NATIVE, INSTANCE, BOUND_METHOD, VECTOR) into 8 slots.

The way out: *do natives really need to be refcounted, given they're defined by the user/engine and live until the end of the script?* Investigation confirmed: `TAG_NATIVE` values are registered only at VM setup, never created during script execution, and torn down in bulk by `pools.nativeFuncPool.destroyAll()` in `VM::~VM()`. There was no code path that decremented a native's refcount at runtime. Moving NATIVE to the non-refcounted side freed the 9th slot.

We swapped:
- **Sign bit = 0 (refcounted):** STRING, CLOSURE, FUNCTION, UPVALUE, CLASS, INSTANCE, BOUND_METHOD, VECTOR
- **Sign bit = 1 (non-refcounted):** INT, BOOL, NATIVE, SMALL_STRING, INTERNED_STRING, CUSTOM_PTR

The bytecode serializer was layout-independent (writes logical types, not raw bit patterns), so no `.pgc` format change was needed.

**Gain: mean +23%, median +26%, 18/18 wins.** This was the single biggest change of the session.

| scenario | before | after | delta |
|---|---|---|---|
| hot_loop 100k | 5.84 ms | 3.36 ms | **+42%** |
| hot_loop 1M | 59.86 ms | 35.72 ms | **+40%** |
| table_lookup 1M | 90.97 ms | 61.26 ms | **+33%** |
| matmul 32 | 5.23 ms | 3.85 ms | **+27%** |
| compute_pi 1M | 205.87 ms | 155.46 ms | **+25%** |

`hot_loop` flipped from 1.39× slower than CPython to **1.15× faster**.

---

## The final picture

After all the changes, here's where PgScript O3 stands at the largest workload per scenario:

| scenario | PgScript O3 vs CPython | vs Lua 5.4 |
|---|---:|---:|
| **hot_loop 1M** | **0.87× — PgScript faster** ★ | 5.2× slower |
| compute_pi 1M | 1.16× slower | 6.0× slower |
| table_lookup 1M | 1.13× slower | 9.6× slower |
| matmul 64 | 1.43× slower | 6.7× slower |
| fib_recursive 28 | 2.05× slower | 7.2× slower |
| binary_tree 16 | 3.05× slower | 2.8× slower |

PgScript is now within ~1.2× of CPython on the four numerically-tight scenarios, and faster on the tightest of them all. Where it still loses:

- **fib_recursive (2× off CPython):** function-call frame setup is heavier than CPython's. No specialized fast call paths.
- **binary_tree (3× off CPython, 2.8× off Lua):** dominated by `OP_Get_Property` / `OP_Set_Property`, which still go through string-keyed lookup. Python's `__slots__` and Lua's table-slot access are both index-based.

### The Lua gap is structural

Lua's inner loop for the same `hot_loop` is **5-7 dispatches per iteration** vs PgScript's 9. Lua is a register VM — `ADD R2 R2 R3` reads two registers, computes, and writes back in one dispatch. PgScript needs `OP_AddLL` + `OP_Set_Local_Pop` to do the same. That's two dispatches vs one. Compounded across every assignment.

Closing this would require a register-VM rewrite. That's a different project.

---

## Cumulative gain across the session

Script time (pure VM cost), at the largest workload per scenario, before vs after the whole arc:

| scenario | start | end | improvement |
|---|---:|---:|---:|
| **hot_loop 1M** | 59.86 ms | **35.72 ms** | **−40%** |
| **table_lookup 1M** | 90.97 ms | **60.14 ms** | **−34%** |
| **matmul 64** | 42.15 ms | **33.94 ms** | **−19%** |
| **compute_pi 1M** | 205.87 ms | **155.46 ms** | **−24%** |
| fib_recursive 28 | 125.23 ms | 115.25 ms | −8% |
| binary_tree 16 | 109.69 ms | 106.03 ms | −3% |

---

## Lessons

**1. Hypothesis-before-measurement is expensive.** The first idea — that function-pointer dispatch was the bottleneck — was wrong. `perf stat` proved it in 30 seconds. Tools that show what the CPU is actually doing (IPC, branch-miss rate, icache misses) settle architectural debates much faster than reading code.

**2. Look for pre-baked information the runtime then re-computes.** Twice in this session, the biggest wins came from data structures that already had the right info — flags, jump indices — that the dispatcher was ignoring. The work was done at decode time; the runtime just had to use it.

**3. The data layout is the algorithm.** The single largest gain (the tag reorganization) didn't change a single line of dispatch code. It just rearranged which bit patterns mean what. `requiresRefCount` went from 5 sequential branches to 1 mask-compare for free — every call site benefited automatically.

**4. Pass ordering is fragile and important.** Three peephole passes had to run in a specific order: IncrementOpt → SetLocalPopFusion → FuseOpPop. Each consumed a pattern the next needed. Getting the order wrong silently destroys the optimization without breaking correctness.

**5. Methodology matters more than micro-optimizations.** The first 5× speedup didn't come from the VM — it came from refactoring our benchmark scripts to use function-scoped locals. Always check that the workload is exercising the language's idiomatic fast path before tuning the engine.

**6. Don't optimize what perf says is fine.** PgScript's function-pointer dispatch *could* be slower than threaded code in principle. In practice, perf showed IPC of 3.94 — the CPU was already doing nearly 4 instructions per cycle. Rewriting the dispatch loop wouldn't have moved the needle. The bottleneck was per-opcode work, and that's where we focused.

---

The session's wins were small individually and substantial together. Each came from a measurement, not a guess. The next frontier — closing the Lua gap structurally, or making `binary_tree`-class workloads competitive — is in a different category: architectural changes (register VM, inline-cached field access) rather than peepholes. But the dispatcher itself is now genuinely competitive: at 1.4 ns per opcode dispatch, with IPC near 4.2, it's running about as well as a function-pointer-dispatched stack VM realistically can.
