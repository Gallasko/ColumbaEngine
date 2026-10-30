# PgScript SSA effect analysis: implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the core of the whole-program effect analysis with one built on SSA values and
access paths. It must finish on every Chronicle rules program, be fast, find idempotent writes,
and run only for ahead-of-time compiles.

**Architecture:**
- **Locations.** An object is named by an *access path* from a root, never by expanding a
  container. Roots are a global, an input, a parameter, or a literal's site; paths are field keys
  or `*`. Paths are interned as integer ids.
- **Bodies.** Each function body and each top-level statement is built once into an SSA IR:
  locals are SSA values with phis, `lookup` is a phi, and every path of an `if` is taken. The IR
  is evaluated symbolically against callee summaries, which reverse-topological order and a
  fixpoint inside each recursive group make cheap.
- **Aliases.** Stores between objects (`t.x = otherTable`, `push`, a global assigned a literal)
  are edges. Aliases and "what a global reaches" are resolved from those edges once, at the end.
- **The pass.** `RunOncePass` keeps its interface (`ProgramEffects`). The analysis runs only when
  a switch on `EntitySystem` is on.

**Tech Stack:** C++17 in the engine (`/src/Engine/Compiler/ast/`), GoogleTest (`test_compiler`),
CMake/make in `/release`.

**Spec:** `/docs/superpowers/specs/2026-10-30-pgscript-ssa-effects-design.md`

## Global Constraints

- The analysis runs only when `EntitySystem::setWholeProgramAnalysis(true)` was called. The
  default is off. When off, `RunOncePass` does exactly what it did at commit `fbcde219`, and its
  module-cache facts are `""`.
- `ChronicleRules` turns it on. `ChronicleSolver` turns it on only when
  `CHRONICLE_SOLVER_ANALYSIS=1`. The game, the dev loop and the tests leave it off; a test that
  wants it turns it on itself.
- Soundness over precision. When unsure, the answer is "not invariant", "writes anything" or
  "not idempotent". A wrong const or a wrong join is the one unacceptable outcome.
- No work budget. A guard of `MaxLocsPerValue = 256` collapses a value to the unknown root and
  counts the collapse. On the Chronicle rules the count must be 0.
- Paths are at most `MaxDepth = 6` keys deep. A deeper child is its depth-6 ancestor, marked
  deep: it stands for everything below.
- Idempotence is computed and exposed only. `RunOncePass` does not use it.
- PgScript has no `switch` statement: `if`, `and`/`or`, and `lookup`/`pick` are the branches.
  Loops are `while`, `for (;;)` and `for (var v : x)`.
- Builds: `cd /home/corse/Documents/PgEngine/release && make -j4 <targets>`, never a bare `-j`.
- Other sessions share the tree. No `git stash`, no whole-file rewrites of files this plan does
  not own, and `git add` only the files a task lists. Commit only what is listed, ending the
  message with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- `S=/tmp/claude-1000/-home-corse-Documents-PgEngine/32f8922d-6d1a-43a7-8c80-ede73f84e5a2/scratchpad/ssa`
  holds verification outputs. Create it with `mkdir -p $S`.
- The solver always gets `report=<file>`. Without it, it overwrites the user's untracked
  `/chronicle-solver.txt`.

## Review Focus

1. **A global reassigned inside a function** (`fun f() { lastAge = 4 }`) is a rebind: never
   inferred const. Test in Task 6.
2. **A write through an alias stored in another table** (`index["x"] = rows[0]` then
   `index["x"].a = 5`) is a write of `rows`. Test in Task 6.
3. **A closure, a nested `fun`, a class, `this`, or a value called** are unknown: the callee may
   write everything reachable from what it is handed. Test in Task 5.
4. **`break` and `continue`** feed the loop's exit and header phis. A value set before a `break`
   reaches the code after the loop. Test in Task 4.
5. **A function defined twice, in two files** is not followed: calling it is an unknown call.
   Test in Task 6.

---

### Task 1: Commit the scope-aware "written anywhere" check on its own

The scope-aware check and `readProgramFiles` are already written, uncommitted, in
`program_view.{h,cpp}`, with the test `ALocalOfTheSameNameWritesNoFunction`. They don't depend on
the analysis.

**Files:**
- Commit as they are: `/src/Engine/Compiler/ast/pass/program_view.h`,
  `/src/Engine/Compiler/ast/pass/program_view.cpp`
- Commit as it is: `/test/pgcompiler/unit/test_run_once.cc` (its only change is the new test)

**Interfaces:**
- Produces: `ast::ProgramFiles ast::readProgramFiles(VM* vm, const std::queue<StatementPtr>& statements)`.
  It returns `{std::vector<std::vector<StatementPtr>> files; bool complete;}`, the script's file
  first, then each imported file once.

- [ ] **Step 1: Check that the two files hold nothing of the old analysis**

Run: `git diff -- src/Engine/Compiler/ast/pass/program_view.h src/Engine/Compiler/ast/pass/program_view.cpp | grep -n "PROFILE\|effects"`
Expected: no output.

- [ ] **Step 2: Build and run the RunOnce tests**

Run: `cd release && make -j4 test_compiler && ./test_compiler --gtest_filter='RunOncePass.*'`
Expected: `KeepsAFunctionDefinedOnceAndNeverWritten` and `ALocalOfTheSameNameWritesNoFunction` pass.
`JoinsTheConstantFillsOfAConstToIt` may still fail here; Task 2 fixes it.

- [ ] **Step 3: Commit**

First check with `git diff -- test/pgcompiler/unit/test_run_once.cc` that the new TEST is its
only change, then:
```bash
git add src/Engine/Compiler/ast/pass/program_view.h src/Engine/Compiler/ast/pass/program_view.cpp test/pgcompiler/unit/test_run_once.cc
git commit -m "PgScript: a local of a function's name does not keep it from being kept" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 2: The switch, with the analysis off by default

**Files:**
- Modify: `/src/Engine/ECS/entitysystem.h` (setter beside `setVMOptimizationLevel`, member beside `vmOptimizationLevel` at :1085)
- Modify: `/src/Engine/ECS/entitysystem.cpp:1151` (`std::make_unique<RunOncePass>(wholeProgramAnalysis)`)
- Modify: `/src/Engine/Compiler/ast/pass/run_once.{h,cpp}`
- Modify: `/examples/Chronicle/precompile.cpp`
- Modify: `/examples/Chronicle/solver/main.cpp` (near :616, where `ecs` is made)
- Test: `/test/pgcompiler/unit/test_run_once.cc`

**Interfaces:**
- Produces:
  - `void EntitySystem::setWholeProgramAnalysis(bool on)`
  - `bool EntitySystem::wholeProgramAnalysis = false`
  - `explicit RunOncePass::RunOncePass(bool wholeProgram = false)`
  - Test helper `runOnce(const std::string& source, bool wholeProgram = false)` in `test_run_once.cc`

- [ ] **Step 1: Write the failing tests**

```cpp
TEST(RunOncePass, InfersNoConstWhenTheAnalysisIsOff)
{
    auto statements = runOnce("var x = 1\nfun f() \n{\n    return x\n}\n");
    EXPECT_FALSE(variable(statements, "x")->isConst);
}

TEST(RunOncePass, InfersAConstWhenTheAnalysisIsOn)
{
    auto statements = runOnce("var x = 1\nfun f() \n{\n    return x\n}\n", true);
    EXPECT_TRUE(variable(statements, "x")->isConst);
}
```

- [ ] **Step 2: Run them to see the first fail**

Run: `make -j4 test_compiler && ./test_compiler --gtest_filter='RunOncePass.*'`
Expected:
- the build fails, because `runOnce` takes one argument;
- once the helper takes the flag, `InfersNoConstWhenTheAnalysisIsOff` fails, because the
  analysis currently always runs.

- [ ] **Step 3: Implement the switch**

`RunOncePass::runPass` runs `readProgramFiles` and `effects.analyze` only when `wholeProgram` is
true. Otherwise `analyzed = false`. `factsAboutIdentifiers` returns `""` when `wholeProgram` is
false.

`precompile.cpp`:
- calls `ecs.setWholeProgramAnalysis(true)` before `precompileRules`;
- accepts an optional third argument `--no-analysis`, which leaves it off (for timing);
- usage string: `usage: ChronicleRules <rules dir> <out dir> [--no-analysis]`.

`solver/main.cpp` calls `ecs.setWholeProgramAnalysis(true)` when
`std::getenv("CHRONICLE_SOLVER_ANALYSIS")` equals `"1"`.

- [ ] **Step 4: Run the whole compiler suite**

Run: `./test_compiler`
Expected: all pass, `JoinsTheConstantFillsOfAConstToIt` included.

- [ ] **Step 5: Record the baselines (analysis off)**

```bash
mkdir -p $S && cd /home/corse/Documents/PgEngine && make -C release -j4 ChronicleSolver ChronicleRules
rm -rf $S/rules-base && cp -r examples/Chronicle/rules $S/rules-base
CHRONICLE_SOLVER_CACHE=0 release/ChronicleSolver $S/rules-base examples/Chronicle/solver/rulecheck.pg $S/rulecheck-base.txt
CHRONICLE_SOLVER_CACHE=0 release/ChronicleSolver $S/rules-base examples/Chronicle/solver/solver.pg beam width=2 lives=1 threads=0 report=$S/beam-base.txt
cd examples/Chronicle && time ../../release/ChronicleRules rules $S/pgc-off --no-analysis
```
Expected: both solver runs exit 0, and the time is about 0.2 s.

- [ ] **Step 6: Commit**

```bash
git add src/Engine/ECS/entitysystem.h src/Engine/ECS/entitysystem.cpp src/Engine/Compiler/ast/pass/run_once.h src/Engine/Compiler/ast/pass/run_once.cpp src/Engine/Compiler/ast/pass/effects.h src/Engine/Compiler/ast/pass/effects.cpp src/Engine/Compiler/ast/ast_pass.h src/Engine/Compiler/ast/ast_pass.cpp src/Engine/Compiler/module_cache.cpp CMakeLists.txt examples/Chronicle/precompile.cpp examples/Chronicle/solver/main.cpp test/pgcompiler/unit/test_run_once.cc
git commit -m "PgScript: the whole-program analysis runs only when asked (ChronicleRules)" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```
This commits the old analysis core behind the switch. Tasks 3–6 replace it.

---

### Task 3: Locations: interned access paths

**Files:**
- Create: `/src/Engine/Compiler/ast/analysis/locations.h`, `/src/Engine/Compiler/ast/analysis/locations.cpp`
- Modify: `/CMakeLists.txt`. Add the `.cpp` after `src/Engine/Compiler/ast/pass/effects.cpp` in
  **both** source lists (:220 and :315). Add `test/pgcompiler/unit/test_effects.cc` after
  `test_run_once.cc` in the `test_compiler` list (:1866).
- Test: `/test/pgcompiler/unit/test_effects.cc` (new)

**Interfaces (namespace `pg::ast::analysis`):**
- Produces:
```cpp
using LocId = uint32_t;
using KeyId = uint32_t;
enum class RootKind : uint8_t { Global, Input, Param, Site, Unknown };
constexpr uint8_t MaxDepth = 6;

class Locations
{
public:
    static constexpr KeyId AnyKey = 0;                    // "*": a key not known when compiled, a list's entries
    KeyId key(const std::string& name);                   // interned; "*" gives AnyKey
    KeyId paramKey(uint32_t param);                       // "[#k]": the field named by parameter k (summaries only)
    bool isParamKey(KeyId k) const; uint32_t paramOfKey(KeyId k) const;
    LocId global(const std::string& name);                // "g:name"
    LocId input(const std::string& name);                 // "in:name"
    LocId param(uint32_t i);                              // "#i"
    LocId site(const void* node);                         // "@n", n = order of first sight
    LocId unknown();                                      // "?"
    LocId child(LocId parent, KeyId key);                 // interned; past MaxDepth: the ancestor at depth MaxDepth, deep
    LocId parent(LocId l) const; KeyId keyOf(LocId l) const; uint8_t depth(LocId l) const;
    RootKind rootKind(LocId l) const; LocId rootOf(LocId l) const; bool deep(LocId l) const;
    bool overlaps(LocId a, LocId b) const;                // may name a common object or one inside the other
    bool within(LocId inner, LocId outer) const;          // every object `inner` names is `outer` or inside it
    std::string text(LocId l) const;                      // "g:activities.*.gains", "#0.[#1].*.amount"
};
```
- `overlaps`: same root, and the keys of the shorter path agree pairwise, where `AnyKey` agrees
  with any key. So a path overlaps everything below it. A deep location overlaps everything
  under its own path. The unknown root overlaps everything.
- `within(inner, outer)`: `outer`'s path is a prefix of `inner`'s, where an `AnyKey` in `outer`
  matches any key of `inner` but not the reverse. If `outer` is deep, so is everything under it.

- [ ] **Step 1: Write the failing tests** (`TEST(EffectLocations, ...)`)

```cpp
Locations l;
const LocId a = l.global("a");
const LocId ax = l.child(a, l.key("x")), axs = l.child(ax, Locations::AnyKey);
EXPECT_EQ(l.text(axs), "g:a.x.*");
EXPECT_EQ(l.child(a, l.key("x")), ax);                                       // interned
const LocId amountAny = l.child(l.child(l.child(a, Locations::AnyKey), l.key("gains")), l.key("amount"));
const LocId amountGains = l.child(l.child(l.child(a, l.key("gains")), l.key("gains")), l.key("amount"));
EXPECT_TRUE(l.overlaps(amountAny, amountGains));
EXPECT_FALSE(l.overlaps(ax, l.child(a, l.key("y"))));
EXPECT_TRUE(l.overlaps(a, axs));                                             // a prefix overlaps below it
EXPECT_TRUE(l.within(l.child(ax, l.key("y")), l.child(a, Locations::AnyKey)));
EXPECT_FALSE(l.within(l.child(a, Locations::AnyKey), ax));
EXPECT_FALSE(l.overlaps(ax, l.child(l.global("b"), l.key("x"))));
LocId deepest = a;
for (int i = 0; i < 8; ++i) deepest = l.child(deepest, l.key("k"));
EXPECT_EQ(l.depth(deepest), MaxDepth);
EXPECT_TRUE(l.deep(deepest));
EXPECT_EQ(l.child(deepest, l.key("z")), deepest);
EXPECT_EQ(l.text(l.child(l.param(0), l.paramKey(1))), "#0.[#1]");
EXPECT_TRUE(l.overlaps(l.unknown(), ax));
```

- [ ] **Step 2: Run them to see them fail**

Run: `make -j4 test_compiler`
Expected: the build fails because `locations.h` doesn't exist.

- [ ] **Step 3: Implement `Locations`**

Store the nodes in a `std::vector` of `{parent, key, depth, root kind, deep}`. Intern children
with an `std::unordered_map<uint64_t, LocId>` keyed `(parent << 32) | key`, roots with maps by
name, index, or node pointer, and keys with an `unordered_map<std::string, KeyId>`. Param keys
are a separate range of ids.

- [ ] **Step 4: Run the tests**

Run: `make -j4 test_compiler && ./test_compiler --gtest_filter='EffectLocations.*'`
Expected: PASS.

- [ ] **Step 5: Commit**

Files: `locations.{h,cpp}`, `test_effects.cc`, `CMakeLists.txt`.
Message: `PgScript: access paths for the effect analysis`.

---

### Task 4: The SSA IR of a body, built from the AST

**Files:**
- Create: `/src/Engine/Compiler/ast/analysis/ssa.h`, `/src/Engine/Compiler/ast/analysis/ssa.cpp` (CMake: both lists, after `locations.cpp`)
- Test: `/test/pgcompiler/unit/test_effects.cc`

**Interfaces:**
- Consumes: `Locations` (Task 3).
- Produces:
```cpp
using ValueId = uint32_t;
enum class Op : uint8_t { Const, Param, Global, Literal, Field, Phi, Call, Native, Pure, Unknown };
enum class NativeKind : uint8_t { None, Fresh, Lookup, Entry, IndexBy, Copy, Push, Insert, Take, Merge };
NativeKind nativeKind(const std::string& name);           // the table now in effects.cpp natives()

struct Node
{
    Op op;
    std::vector<ValueId> in;      // Field: {base}; Phi/Call/Native/Pure: operands; Const/Param/Global/Literal: none
    KeyId key = Locations::AnyKey; // Field: the key (AnyKey, a name, or a param key)
    LocId site = 0;               // Literal: its site
    uint32_t param = 0;           // Param: its index
    std::string name;             // Global: the name; Call/Native: the callee; Const: its text ("1", "\"a\"", "none")
};
struct Store { ValueId base; KeyId key; ValueId value; };   // base[key] = value (AnyKey for push and unknown keys)
struct BodyIR
{
    std::vector<Node> nodes;
    std::vector<Store> stores;                             // in program order
    std::vector<std::pair<std::string, ValueId>> rebinds;  // a global assigned (name, value)
    std::vector<ValueId> returns;
    bool unknown = false;                                  // a closure, nested fun, class, `this` or dprint inside
    std::unordered_map<const Expression*, ValueId> valueOf; // the IR mapped back to the AST
    std::vector<ValueId> leaves(ValueId v) const;          // through phis, each non-phi once, in id order
    std::string dump() const;
};
// `isNative(name)`: true when the program defines nothing of that name (the builder still checks locals)
BodyIR buildFunction(const FunctionStatement& f, Locations& locs, const std::function<bool(const std::string&)>& isNative);
BodyIR buildTopLevel(const StatementPtr& s, Locations& locs, const std::function<bool(const std::string&)>& isNative);
```

**Rules the builder follows:**
- **Values.** Params come first (`%0 = param 0`, …). Locals are SSA values. A name that isn't a
  local is a `Global` node, read each time it appears.
- **Branches.** `if`, `and`/`or`: both sides are built, and each local assigned on either side
  gets a `Phi` at the join.
- **Loops.** At the loop header, every local assigned in the body or the increment gets a `Phi`
  whose inputs are the value before the loop, the end of the body, and each `continue`. After
  the loop, a `Phi` takes the header value and each `break`.
- **`for (var v : x)`.** `v` is `Field(x, AnyKey)`.
- **Field keys.** `t.name` and `t["name"]` use `key("name")`. A key that is a parameter
  (directly) uses `paramKey(i)`. Any other key uses `AnyKey`.
- **Lookup natives.** `lookup(t, k, d)` and `pick(t, k, d)` are `Phi(Field(t, key), d)`, or just
  the `Field` without a default. Other natives are `Native` nodes; calls of non-locals are `Call`.
- **Writes.** `x.f = v` and `x[k] = v` are `Store`s. `push(l, v)` and `insert(l, i, v)` are
  `Store{l, AnyKey, v}` plus their `Native` node. `x = v` on a global is a rebind.
- **Literals.** A literal gets a `Literal` node with `site(node)`. Each entry whose value is not
  `Const` or `Pure` adds `Store{literal, key, value}`, where the key is the explicit key, the
  constant index, or `AnyKey`.
- **Flow-sensitive fields of local tables.** Take a table made by a literal in this body,
  outside any loop, and accessed only with constant keys. After `t.k = v`, a later read of
  `t.k` is `v` itself (a strong update). At joins the tracked value becomes a `Phi`. Any other
  object's field read is a `Field` node.
- **Dump format.** One line per node: `%i = const 1`, `%i = param 0`, `%i = global name`,
  `%i = literal @n`, `%i = field %b key` (`*` or `[#k]`), `%i = phi %a %b`,
  `%i = call name %a`, `%i = native name %a`, `%i = pure %a %b`, `%i = unknown`. Then
  `store %b key %v`, `rebind name %v` and `return %v` lines, in program order.

- [ ] **Step 1: Write the failing tests** (`TEST(EffectSsa, ...)`, sources as strings; blocks on several lines, as PgScript needs)

```cpp
// LookupIsAPhiOfTheFieldAndTheDefault
// fun f(t, k) { var l = lookup(t, k, [])  return l }
EXPECT_EQ(ir.dump(), "%0 = param 0\n%1 = param 1\n%2 = field %0 [#1]\n%3 = literal @0\n%4 = phi %2 %3\nreturn %4\n");

// AnIfMergesItsBranches
// fun g(c) { var x = 1  if (c) { x = 2 }  return x }
// leaves of the return: two Const nodes, names "1" and "2"

// ABreakReachesAfterTheLoop
// fun h(n) { var x = 0  while (x < n) { if (x > 5) { x = 9  break }  x = x + 1 }  return x }
// leaves of the return: Const "0", Const "9", and a Pure node

// AContinueFeedsTheHeader
// fun c(n) { var x = 0  for (var i = 0; i < n; i++) { if (i == 2) { x = 7  continue }  x = 1 }  return x }
// leaves of the return: Const "0", Const "7", Const "1"

// ALocalTableFieldIsTheValueWritten
// fun t() { var t = {x: 1}  t.x = 2  return t.x }
// leaves of the return: one Const "2"; stores holds the write of x

// AClosureMakesTheBodyUnknown
// fun m(t) { var g = fun() { t.x = 1 }  return 0 }  ->  ir.unknown == true

// ALoopOverATableGivesItsEntries
// fun e(t) { for (var v : t) { push(v, 1) } }  ->  stores[0].base is a Field of %0 with AnyKey
```

- [ ] **Step 2: Run them to see them fail**

Run: `make -j4 test_compiler`
Expected: the build fails because `ssa.h` doesn't exist.

- [ ] **Step 3: Implement `buildFunction` and `buildTopLevel` in `ssa.cpp`**

Keep a stack of scopes (`name -> ValueId`), and for each open loop a list of `(break, continue)`
environments. A `ForStatement`'s initializer opens a scope. Find the locals a loop assigns with a
pre-scan of its body before building it, and patch the header phis' back-edge inputs after.
Remove the old `natives()` table from `effects.cpp` only in Task 6.

- [ ] **Step 4: Run the tests**

Run: `make -j4 test_compiler && ./test_compiler --gtest_filter='EffectSsa.*'`
Expected: PASS.

- [ ] **Step 5: Commit**

Files: `ssa.{h,cpp}`, `test_effects.cc`, `CMakeLists.txt`.
Message: `PgScript: an SSA form of a body for the effect analysis`.

---

### Task 5: Evaluation, function summaries and idempotence

**Files:**
- Create: `/src/Engine/Compiler/ast/analysis/summaries.h`, `/src/Engine/Compiler/ast/analysis/summaries.cpp` (CMake: both lists, after `ssa.cpp`)
- Test: `/test/pgcompiler/unit/test_effects.cc`

**Interfaces:**
- Consumes: `Locations`, `BodyIR`, `Node`, `Store`, `nativeKind`.
- Produces:
```cpp
constexpr size_t MaxLocsPerValue = 256;
struct Idempotence
{
    bool shape = false;                   // every stored value is the field's own old value or invariant-made
    std::set<uint32_t> needsParams;       // ... provided these arguments are invariant at the call
    std::set<std::string> needsGlobals;   // ... and these globals are invariant in the program
};
struct Write { LocId at; Idempotence idempotence; };
struct Edge { LocId at; LocId value; };   // an object stored at a location (param locations allowed in a summary)
struct Summary
{
    uint32_t arity = 0;
    std::vector<LocId> returns;
    std::vector<Write> writes;
    std::vector<Edge> edges;
    std::set<std::string> reads;          // globals read, transitively through calls
    std::set<std::string> rebinds;
    bool unknown = false;
    std::string digest(const Locations& locs) const;
};
class Evaluator
{
public:
    Evaluator(Locations& locs, const std::map<std::string, Summary>& summaries,
              const std::function<bool(const std::string&)>& followed);  // a program function defined once, never rebound
    Summary evaluate(const BodyIR& body);
    size_t collapsed() const;             // values that hit MaxLocsPerValue
};
// Every followed function summed up: reverse topological order of the call graph, a fixpoint inside each cycle
std::map<std::string, Summary> summarize(const std::map<std::string, BodyIR>& bodies, Locations& locs,
                                         const std::function<bool(const std::string&)>& followed, size_t& collapsed);
```

**Rules the evaluator follows.** Each value gets a set of `LocId`s, its points-to set.
- **Plain nodes.**
  - `Param i` → `{param(i)}`. `Global g` → `{global(g)}` (symbolic; aliases are resolved in
    Task 6). `Literal` → `{site}`.
  - `Field(b, k)` → `{child(l, k) for l in b}`.
  - `Phi` → the union of its inputs.
  - `Const`, `Pure`, and `Fresh` natives → `{}`.
- **Natives.**
  - `Entry` → field `AnyKey` of argument 0.
  - `Take` → field `AnyKey` of argument 0, plus a write of argument 0.
  - `Copy` → a new `site` made by the node, with an edge `site.* ← arg0.*`.
  - `IndexBy` → a new `site`, with an edge `site.* ← arg0.*`.
  - `Push`/`Insert` → a write of argument 0 (the store is in `stores`).
  - `Merge` → a write of argument 0, with an edge `arg0.* ← arg1.*`.
- **Stores.** Each `Store` writes `child(l, key)` for each `l` of its base, and adds an edge from
  there to each loc of the value.
- **Calls.** A followed callee's summary is applied by substitution:
  - `param(i)` followed by a path becomes each loc of argument `i` followed by that path,
    re-interned and so re-bounded;
  - `paramKey(k)` becomes `key(name)` when argument `k` is a `Const` string node, and
    `AnyKey` otherwise;
  - the callee's returns, writes, edges, reads, rebinds and `unknown` join the caller's.
- **Unknown calls.** Any other call is unknown: `unknown = true`, every loc of every argument is
  written deep (`child` to depth 6, deep), and the result is those locs.
- **Unknown bodies.** `body.unknown` gives `unknown = true` and a write of `unknown()`.
- **The guard.** A value over `MaxLocsPerValue` locs becomes `{unknown()}`, and `collapsed` is
  counted.

**Idempotence of a store** `base[key] = v`. Look at each of `body.leaves(v)` and decide:
- **The field's own old value:** a `Field` node whose base has the same points-to set as the
  store's base, and the same key → fine.
- **Invariant-made:** `Const`; `Param i` (add `i` to `needsParams`); `Global g` (add `g` to
  `needsGlobals`); a `Pure`/`Fresh` native whose operands are all invariant-made; a `Field` of an
  invariant-made base whose locs no write of this body overlaps → fine.
- **Anything else** (`Call` result, `Unknown`, a field this body writes) → `shape = false`.

A call's writes keep the callee's idempotence, substituted: each `needsParams` index becomes the
needs of the argument's leaves, checked by the same rule in the caller.

- [ ] **Step 1: Write the failing tests** (`TEST(EffectSummaries, ...)`)

Each one parses the functions, builds them, and runs `summarize`. Texts come from `locs.text`.
```cpp
// AmountsOfWritesTheAmountOfItsListEntriesIdempotently  (the exact source of lib.pg:211-221)
// writes: one, at "#0.[#1].*.amount", shape true, needsParams {}, needsGlobals {}
// returns: contains "#0.[#1]"; unknown false

// ACounterIsNotIdempotent       fun bump(t) { t.count = t.count + 1 }   -> write "#0.count", shape false
// AFlagIsIdempotent             fun mark(t) { t.done = true }          -> write "#0.done", shape true
// AParameterStoredNeedsItsArgument  fun put(t, v) { t.x = v }          -> shape true, needsParams {1}; edge "#0.x" <- "#1"
// AGlobalStoredNeedsTheGlobal   fun put(t) { t.x = level }             -> shape true, needsGlobals {"level"}
// APushIsNotIdempotent          fun add(l, v) { push(l, v) }           -> a write within "#0", shape false; edge "#0.*" <- "#1"
// ACallSubstitutesTheKey        fun g(a) { amountsOf(a, "gains") } + amountsOf -> write "#0.gains.*.amount", shape true
// RecursionConverges            fun walk(t) { if (t.next) { walk(t.next) }  t.seen = 1 }
//                               -> returns; writes include "#0.seen" and a loc within "#0.next"
// AValueCalledIsUnknown         fun cb(f, t) { f(t) }                  -> unknown true; a deep write of "#1"
// AClosureIsUnknown             fun mk(t) { var g = fun() { t.x = 1 }  return 0 } -> unknown true
// NothingCollapses              collapsed == 0 for all of the above
```

- [ ] **Step 2: Run them to see them fail**

Run: `make -j4 test_compiler`
Expected: the build fails because `summaries.h` doesn't exist.

- [ ] **Step 3: Implement the `Evaluator` and `summarize` in `summaries.cpp`**

The points-to sets are `std::vector<LocId>`, kept sorted and unique. A body's values are
evaluated in id order and then repeated until no set grows, which closes the loop phis. Find the
call graph's strongly connected components with Tarjan's algorithm, over followed callees. In
each cycle, repeat until every digest is unchanged. Summaries only grow, so this ends.

- [ ] **Step 4: Run the tests**

Run: `make -j4 test_compiler && ./test_compiler --gtest_filter='EffectSummaries.*'`
Expected: PASS.

- [ ] **Step 5: Commit**

Files: `summaries.{h,cpp}`, `test_effects.cc`, `CMakeLists.txt`.
Message: `PgScript: function summaries by access path, with idempotent writes`.

---

### Task 6: `ProgramEffects` on the new core

**Files:**
- Modify (rewrite the internals; the public questions stay): `/src/Engine/Compiler/ast/pass/effects.h`, `/src/Engine/Compiler/ast/pass/effects.cpp`
- Modify: `/src/Engine/Compiler/ast/pass/run_once.cpp` (only if a signature it uses changes)
- Test: `/test/pgcompiler/unit/test_effects.cc`, `/test/pgcompiler/unit/test_run_once.cc`

**Interfaces:**
- Consumes: `Locations`, `buildFunction`, `buildTopLevel`, `summarize`, `Evaluator`, `Summary`.
- Produces (kept from today, same meanings as in the current `effects.h` comments):
  - `bool analyze(const std::vector<std::vector<StatementPtr>>& files)`: now false only when
    `collapsed() > 0` would make it unsound to use.
  - `StatementEffects effectsOf(const StatementPtr&) const`
  - `bool invariant(const std::string&) const`
  - `bool onlyMakes(const StatementEffects&, const std::set<std::string>& targets) const`
  - `const std::set<std::string>& inferredConsts() const`
  - `std::string factsOf(const std::string&) const`
  - `int definitions(const std::string&) const`
- Produces (new):
  - `bool idempotent(const StatementPtr& s) const`: every write `shape`, its needed globals invariant.
  - `const analysis::Summary* summaryOf(const std::string& function) const`
  - `std::vector<std::string> writtenTexts(const StatementPtr& s) const`: sorted texts of the
    resolved locations it writes.
  - `std::vector<std::string> regionsOf(const std::string& global) const`: sorted texts.
  - `size_t collapsed() const`
- `StatementEffects` becomes:
  `{ std::set<std::string> reads; std::vector<analysis::LocId> writes; std::vector<analysis::Edge> edges; std::set<std::string> rebinds; bool unknown; }`.
  `writesDeep` and `writesAny` go: deep and unknown locations carry them.

**Algorithm (`analyze`):**
1. **Definitions.** Count definitions and collect `consts`, `vars`, `definitionOf` and the
   function bodies, as today. A function defined more than once is not followed.
2. **IR.** Build each followed body, then `summarize`. Build and evaluate each top-level statement
   (files in order, imports and functions skipped) with an `Evaluator` over the final summaries.
   Store its `StatementEffects`. A top-level `var`/`const` adds the edge
   `global(name) ← its value's locs`; a rebind adds the same edge.
3. **Aliases.**
   - `aliases(l)` = `l`, plus: for every prefix `p` of `l` (`l` included), for every edge
     `(at, v)` with `overlaps(at, p)`, the location `v` followed by the rest of `l`'s path after
     `p`. Take the closure, memoized by `LocId`.
   - Index the edges by root so each lookup scans only edges of the same root.
4. **Regions.**
   - `regions(g)`: start from `{global(g)}`. Add `v` for every edge `(at, v)` whose `at` is
     within a region already held, and repeat until nothing is added.
   - `reachedBy`: for each region root, the globals holding it.
5. **Writes.** `written(s)` = the aliases of each of its writes.
6. **Inferred consts.** Same rule as today, over these:
   - a write of `s` "touches" `g` when it `overlaps` one of `g`'s regions;
   - `onlyMakes` accepts a written location within a target's region, or one whose root is a
     `Site` that no global's region overlaps;
   - it refuses `Input`, `Param` and `Unknown` roots.
7. **`factsOf`.** Same shape as today: `c`/`i` flags, the summary digest, the region count, and
   the globals that share a region.

Remove the old walker, `Roots`, `FunctionEffects`, `Budget`, `elementsSeen`, `expandedAt` and the
old `natives()`.

- [ ] **Step 1: Write the failing tests**

`test_effects.cc`, `TEST(ProgramEffects, ...)`, each on one parsed file passed to `analyze({statements})`:
```cpp
// TheActivityLoopWritesOnlyTheAmounts
//   amountsOf (lib.pg source) +
//   var activities = [{id: "a", gains: [{stat: "s"}]}, {id: "b", gains: [{stat: "t", amount: 2}]}]
//   var other = {x: [1]}
//   for (var i = 0; i < len(activities); i++) { amountsOf(activities[i], "gains") }   // statement L
// writtenTexts(L) contains "g:activities.*.gains.*.amount"; no text starts with "g:other";
// regionsOf("activities") has no text in common with regionsOf("other");
// inferredConsts() contains "other" and not "activities"; idempotent(L) is true; collapsed() == 0

// AWriteThroughAnAliasWritesTheOriginal
//   var rows = [{a: 1}, {a: 2}]   var index = {}   index["x"] = rows[0]   index["x"].a = 5
// inferredConsts() contains neither "rows" nor "index"

// AGlobalAssignedInAFunctionIsNoConst
//   var lastAge = 3   fun f() { lastAge = 4 }   f()
// inferredConsts() does not contain "lastAge"

// AFunctionDefinedTwiceIsNotFollowed
//   fun g(t) { return 0 }   fun g(t) { t.x = 1 }   var rows = {x: 0}   g(rows)
// summaryOf("g") == nullptr; inferredConsts() does not contain "rows"

// AConstantVarIsInferred
//   var lastAge = 30   var noBreak = {a: 1}   fun f() { return lastAge }
// inferredConsts() == {"lastAge", "noBreak"}
```
`test_run_once.cc`:
```cpp
// TheLoopFillingAConstJoinsIt (analysis on)
//   const places = [{id: "a"}, {id: "b"}]
//   const byId = {}
//   for (var i = 0; i < len(places); i++) { byId[places[i].id] = places[i] }
// variable(statements, "byId")->keptFills.size() == 1; statements.size() == 2
// The same source with the analysis off: keptFills.size() == 0, statements.size() == 3
```

- [ ] **Step 2: Run them to see the new ones fail**

Run: `make -j4 test_compiler && ./test_compiler --gtest_filter='ProgramEffects.*:RunOncePass.*'`
Expected:
- `TheActivityLoopWritesOnlyTheAmounts` fails, or the build fails on the new accessors
  (`writtenTexts`, `regionsOf`, `idempotent`, `summaryOf`, `collapsed`);
- the RunOnce tests already there keep passing.

- [ ] **Step 3: Rewrite `ProgramEffects` on the new core**

Follow the algorithm above.

- [ ] **Step 4: Run the whole compiler suite**

Run: `./test_compiler`
Expected: all pass.

- [ ] **Step 5: Commit**

Files: `effects.{h,cpp}`, `run_once.cpp` if changed, `test_effects.cc`, `test_run_once.cc`.
Message: `PgScript: the effect analysis on SSA values and access paths`.

---

### Task 7: The rules: decisions, speed, rulecheck and beam

**Files:**
- Modify only if the profile in Step 3 asks for it: the files of Tasks 3–6
- Record the results in: `/docs/superpowers/specs/2026-10-30-pgscript-ssa-effects-design.md` (a closing "Results" section)

**Interfaces:**
- Consumes: the switch (Task 2) and `ProgramEffects` (Task 6).

- [ ] **Step 1: Every program finishes, and nothing collapses**

Temporarily log, in `RunOncePass::runPass` and only when `PG_EFFECTS_LOG` is set, the program's
first file, `analyze`'s result, `collapsed()`, the inferred consts and the joins. Then run:
```bash
cd examples/Chronicle && PG_EFFECTS_LOG=1 ../../release/ChronicleRules rules $S/pgc-on 2> $S/effects-log.txt
```
Expected: `analyze` is true and `collapsed` is 0 for all eight programs (`milestones`, `windows`,
`forecast`, `resources`, `achievements`, `epitaph`, `town`, `expedition`). The log shows:
- `towntable`'s loop joined to `townPlaces` in `town` and `resources`;
- `lastAge`, `anyAge`, `noBreak`, and `town`'s `foundLine`/`opened` inferred.

- [ ] **Step 2: Speed**

```bash
cd examples/Chronicle
for i in 1 2 3; do /usr/bin/time -f %e ../../release/ChronicleRules rules $S/pgc-on; done
for i in 1 2 3; do /usr/bin/time -f %e ../../release/ChronicleRules rules $S/pgc-off --no-analysis; done
```
Expected: the best "on" time is at most the best "off" time plus 0.2 s.

- [ ] **Step 3: If over 0.2 s, profile and fix**

Run: `perf record -g ../../release/ChronicleRules rules $S/pgc-on` and `perf report --no-children | head -40`.
The allowed fix is to cache each `FunctionStatement`'s `BodyIR`, and the summaries of functions
whose callees are the same, across the programs of one process. Key the cache by the AST node
pointer; the `ProgramView` summary cache keeps nodes alive per file content. Rerun Step 2.

- [ ] **Step 4: Rulecheck and beam with the analysis on**

```bash
rm -rf $S/rules-on && cp -r examples/Chronicle/rules $S/rules-on
CHRONICLE_SOLVER_ANALYSIS=1 CHRONICLE_SOLVER_CACHE=0 release/ChronicleSolver $S/rules-on examples/Chronicle/solver/rulecheck.pg $S/rulecheck-on.txt
cmp $S/rulecheck-base.txt $S/rulecheck-on.txt && echo identical
CHRONICLE_SOLVER_ANALYSIS=1 CHRONICLE_SOLVER_CACHE=0 perf stat -e instructions:u release/ChronicleSolver $S/rules-on examples/Chronicle/solver/solver.pg beam width=2 lives=1 threads=0 report=$S/beam-on.txt
diff <(sed 1d $S/beam-base.txt) <(sed 1d $S/beam-on.txt) && echo same
```
Expected: `identical` and `same`. Record the instruction count against the off run's (Task 2,
Step 5, rerun under `perf stat` if needed).

- [ ] **Step 5: The switch off changes nothing**

Run: `make -C release -j4 test_compiler test_chronicle && release/test_compiler && release/test_chronicle`
Expected: everything passes. The rules-test times are as before (about 0.33 s each).

- [ ] **Step 6: Remove the temporary log, write the results into the spec, and commit**

Files: the spec, plus any fix from Step 3.
Message: `PgScript: the SSA effect analysis on the Chronicle rules`.
