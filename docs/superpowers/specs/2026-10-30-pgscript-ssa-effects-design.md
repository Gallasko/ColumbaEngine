# PgScript whole-program effect analysis on SSA values: design

Date: 2026-10-30
Branch: `Chronicle` (ColumbaEngine repo, local tree `/home/corse/Documents/PgEngine`)

## Intent

PgScript rule scripts run whole at every call: their top-level tables are rebuilt and refilled
each time. `RunOncePass` (`/src/Engine/Compiler/ast/pass/run_once.cpp`) already keeps top-level
functions and the constant fills of a `const` from run to run. To go further (infer that a `var`
is as good as a `const`, run once the statements that finish building a const table), the
compiler must know, for the whole program, what each statement reads and writes, through calls.

A first analysis was written for this (`/src/Engine/Compiler/ast/pass/effects.{h,cpp}`,
uncommitted). Its decisions are correct (identical rulecheck answers, same beam tasks), but it
is slow and imprecise, and it gives up on half the rules programs. This design replaces its core
with a model built on SSA values, as the user described it. The result must work on every
program and be fast; it is the base later optimization passes will stand on.

## Why the current analysis fails (measured)

Profile of a full rules compile (counters added for the measurement, since removed), work counted
in set elements:

- `windows`, `forecast` and `epitaph` give up at the 3 M budget **in the first round**. 3.02 M of
  3.07 M is one statement, the `activitytable` loop over the activities (from
  `/examples/Chronicle/rules/activitytable.pg:1913`), and 2.29 M of that is its first 4 calls of
  `amountsOf` (`/examples/Chronicle/rules/lib.pg:211`). The 67 function summaries cost 5.8 k.
- Its model: an object is an allocation site (a string such as `@0x55…`); a parameter's insides
  are the parameter itself; a summary's store into a parameter keeps no field name (`*`). So
  `amountsOf`'s summary reads "into everything reachable from argument 0, field `*`, put
  everything reachable from argument 0".
- At `amountsOf(a, "gains")`, `a = activities[i]` is all 191 row literals. The store expands
  them to 1,593 objects (nearly the whole heap) and adds the 191 rows to each one's `*` field:
  ≈ 306 k per call, ≈ 612 k from the second call on (the expansion now walks those names too).
  The heap ends at 418,857 names held. Every row then reaches every row, so even with no budget
  nothing usable would come out of these programs.
- `town` (1 M work, 66 ms) pays the same helper in `towntable`'s loop (158 k) plus 732 k of field
  reads returning large sets. `expedition` (1.95 M, 310 ms, 9 rounds) spends 590 k in
  `planRow -> planGives` and about 263 k per round on rounds that find nothing new.
- Net effect today: the rules compile goes from about 0.2 s to 1.5 s, and the beam life from
  329.3 G to 340.7 G instructions.

## Goals

1. The analysis finishes on every Chronicle rules program, with no work budget and no "give up"
   path, and still finishes on any program a user can write (a hard ceiling that degrades to
   "unknown" may stay as a guard, but no Chronicle program may reach it).
2. It is precise on the patterns the rules use: `amountsOf(a, "gains")` is found to write, idempotently, the
   `amount` field of the entries of a row's `gains` list, and nothing else. No value is ever
   widened to "every row".
3. It is fast. Compile time is not capped (see Constraints), but the user wants it fast; the
   target is in "Success" below.
4. It feeds `RunOncePass` what the current analysis feeds it:
   - inferred consts: top-level `var`s defined once, never rebound, never written elsewhere,
     made of what is the same on every run;
   - joining a statement to the last open const when it only builds the open consts;
   - the "written anywhere" check, scope-aware (a local of the same name is not the global).
5. It recognizes **idempotent writes** by itself and exposes them (per write, per function, per
   statement), with tests. **This milestone computes them only: `RunOncePass` does not use them**
   (user's decision). Nothing in today's rules would newly become a const from them (the
   `activitytable` loop also writes `a.rank`, `a.steps` from `character` and `done`), and using
   them soundly needs more than the rule below:
   - "nothing reads the field before the write on a later run": what the last run left is
     already there when the next run reads it earlier (`has(row, "amount")`, `len`, a key
     loop would see it);
   - control dependence: `if (count >= step.uses) a.rank = step.rank`, where `count` comes from
     the input `done`, writes an invariant value but leaves run 1's `rank` for a run 2 that
     should not have it.

   Those checks belong to the later work that uses idempotence.

## Non-goals (this milestone)

- The passes that will use it later: constant table folding, hoisting, removing conditions that
  are always true or false. They come after, as separate work.
- Using conditions to drop paths. The first pass takes every path. Refining with conditions is a
  later step, done only once this first pass is complete.
- Running during the game, the dev loop, the tests or the solver (see Constraints).
- A `constexpr`-like keyword (the user's idea, for later): code that only builds a const table,
  such as `activitytable`'s, would say so, and the compiler would run it once without the
  analysis having to prove it. `activitytable` needs this, because its loop also does per-run work
  (it reads `character` and `done`) that no analysis can prove invariant. Not designed here.

## Anti-goals

- **A wrong const or a wrong join.** Either one changes what the rules compute. Every rule must be
  sound: when unsure, the answer is "not invariant" / "writes anything".
- **Any cost in compiles that happen while running or while developing.** With the switch off,
  compile time and the module cache's sharing must be exactly what they are without the
  analysis.
- **Expanding containers into their entries** as the general mechanism (the current analysis's
  failure).

## Constraints

- **Ahead of time only.** The analysis runs only for an ahead-of-time compile: `ChronicleRules`
  (`/examples/Chronicle/precompile.cpp` → `chronicle::precompileRules` in
  `/examples/Chronicle/Core/rulevm.cpp`), which ships the `.pgc` files of the web build. That
  compile may take as long as it needs.
- **Off everywhere else, by default.** Native game startup, a rule reloaded, the tests, the
  solver: no analysis. `RunOncePass` then does what it does today without it (functions kept
  once, constant fills joined). The analysis's own tests turn it on explicitly.
- When off, the analysis contributes nothing to the module cache key
  (`/src/Engine/Compiler/module_cache.cpp`, `AstPass::factsAboutIdentifiers`).
- Repo practice: builds with `make -j4`, never a bare `-j`, run from `/release`. Other sessions
  share the tree: no `git stash`, no whole-file rewrites of files you did not write. Commit only
  when the user asks.

## The model (the user's decisions)

These are the user's, as they stated them:

- **Values are SSA.** Each value is defined once, by the expression that makes it, and gets an
  id. The defining expression already says what kind of value it is (a table or list literal, a
  number, a string, a call's result), with nothing to compute. A local assigned twice is two
  values, merged with a phi where paths join.
- **A container is not expanded.** A list or table's entries are one value standing for "an
  entry". `a = activities[i]` is `%2 = %1[%0]`: one value, "a row of `activities`", not 191
  rows. A container is split only when the code targets a specific entry of a container whose
  contents are known (a constant key or index into a defined literal).
- **`lookup(t, k, d)` is a pure read with two outcomes,** like an `if`: `phi(t.k, d)`. In
  `amountsOf`, `list` is a new SSA value that is either `table.key` or the default, and
  `list[i]` is one entry that may come from either.
- **Every path is taken.** An `if` or a `switch` takes all branches and merges with phis. Only
  after a first full pass may conditions be used to simplify (a later step).
- **The model finds idempotent writes itself.** `amountsOf` must not be declared read-only by
  hand. The model sees that it writes and sees that the write changes nothing on a second run.

### How the user's example reads

```
// in the activitytable loop:     a = activities[i];  amountsOf(a, "gains")
%0 = i
%1 = activities                  // one value: the table; its entries are "a row"
%2 = %1[%0]                      // a: one row
// amountsOf(table = %2, key = "gains")
%3 = %2.gains                    // lookup(table, key, []): table.key ...
%4 = []                          // ... or the default
%5 = phi(%3, %4)                 // list
%6 = %5[*]                       // list[i]: one entry of either
%7 = phi(%6.amount, 1)           // lookup(list[i], "amount", 1)
%6.amount = %7                   // writes back what it held, or 1 where it held nothing
```

The write lands in the entries of a row's `gains` list (through `%3`) or in the fresh `[]`
(through `%4`, which nothing else sees). It is idempotent: after one run the field is present,
so a second run writes back the value it holds.

### How far the SSA goes (the user's decision)

- **Per function, flow-sensitive.** Inside a function body or a top-level statement, locals and
  the fields of the tables made there are SSA values, flow-sensitive: a field written then read
  in the same body is read as the value written, with phis where paths join.
- **Globals are flow-insensitive across the program.** A global is "everything it ever holds",
  across every statement and function of the program (the rules rebind globals freely:
  `activities` three times in `activitytable.pg`, `lastAge = primeEnd(character)` in several
  programs). The fields of objects reached through globals or parameters are likewise
  "everything ever stored there", by access path.
- **The IR is kept and mapped back to the AST nodes,** so later passes (folding, hoisting,
  removing conditions inside a function) can build on it without a second analysis.
- Not this milestone: memory SSA over globals and fields across the whole program.

## Decisions left to me (marked as my calls, open to correction)

- **The idempotence rule.** A store `o.f = v` is idempotent when every input of `v` (through
  phis) is either `o.f`'s own value before the store, or a value made only of what is the same
  on every run and that the statement does not write (constants, consts, inferred consts,
  invariant reads). So `x.f = phi(x.f, 1)` and `x.f = 3` are; `x.count = x.count + 1`,
  `push(x.list, v)`, and any write of a value read from an input (`character`, `done`) are not.
  A statement or a function is idempotent when all its writes are.
- **Function summaries by access path.** A function is summed up once, its parameters as roots:
  what it reads, what it writes, and what it returns, each as a path from a parameter or a
  global (`#0.[#1].*.amount`, where the field named by parameter 1 is filled in at the call;
  here with `"gains"`), and for each write whether it is idempotent. A call substitutes its
  arguments' SSA values into the paths. Recursion and mutual calls are solved by iterating the
  summaries to a fixpoint, which must converge because paths are bounded (a path longer than a
  small limit collapses to "anything reachable from this root").
- **Unknowns stay conservative**, as today: a closure, a method, `this`, a class, a native the
  analysis does not know: "may write all it is handed, may return any of it".
- **The switch.** A flag the precompile sets before compiling, off by default; my first choice is
  a field on `EntitySystem` next to `vmOptimizationLevel`
  (`/src/Engine/ECS/entitysystem.h:1085`), passed to `RunOncePass` when the passes are
  registered (`/src/Engine/ECS/entitysystem.cpp:1151`). An environment variable alone is not
  enough: tests must be able to set it per test.
- **The `RunOncePass` interface stays.** It keeps asking the same questions (`invariant`,
  `onlyMakes`, `inferredConsts`, `effectsOf`, `factsOf`), so the pass changes little.
  Idempotence is exposed beside them but not asked by the pass (Goal 5).
- **The scope-aware "written anywhere" check** already in `program_view.cpp` (uncommitted, with its
  test `ALocalOfTheSameNameWritesNoFunction`) is independent of this redesign and can be
  committed on its own first.

## Success

- Every rules program finishes the analysis (`windows`, `forecast`, `epitaph` included), with
  no budget hit.
- The `RunOncePass` decisions on the rules (kept functions, inferred consts, joins) are those of
  the current analysis or a superset found sound by the same rules; idempotence changes none of
  them.
- `amountsOf`'s summary is an idempotent write of `#0.[#1].*.amount`, and after the
  `activitytable` loop no row's fields hold other rows.
- The joins and inferred consts found today are still found: `towntable`'s loop joins
  `townPlaces` (in `town` and `resources`); `lastAge`, `anyAge`, `noBreak`, `town`'s
  `foundLine`/`opened` are inferred.
- Speed (my target): the analysis of the whole rules (all eight programs) under 0.2 s, about the
  time of the rest of the compile.
- Rulecheck answers byte-identical to the baseline, and the beam life reaches the same tasks the
  same ways, both run on rules compiled with the analysis on.
- With the switch off: rules compile time and `test_compiler` / `test_chronicle` results as
  without the analysis.

## Verification tools (exist today)

- Rulecheck: `ChronicleSolver <rules copy> examples/Chronicle/solver/rulecheck.pg <out>`, with
  `CHRONICLE_SOLVER_CACHE=0`; answers compared byte for byte with a baseline.
- Beam life: `CHRONICLE_SOLVER_CACHE=0 ChronicleSolver <rules copy> examples/Chronicle/solver/solver.pg
  beam width=2 lives=1 threads=0 report=<file>`; reports compared without their first line.
  Always pass `report=`: without it the solver writes over the user's `chronicle-solver.txt`.
- Both compile from source, so with the switch off by default they need a way to turn it on
  (the same flag), or must run on `.pgc` made by `ChronicleRules`.
- Unit tests: `/test/pgcompiler/unit/test_run_once.cc` (RunOnce), in `test_compiler`.
  `RunOncePass.JoinsTheConstantFillsOfAConstToIt` currently fails with the old analysis on;
  its expectations depend on whether the analysis is on in that test.

## Left to the builder

- The data structures: how SSA ids, phis, paths and summaries are stored (integer ids, not
  strings, was the main slowness besides the model).
- Whether the IR is built once from the AST for each file and cached across the programs that
  import it (`lib.pg` and `activitytable.pg` are analysed by several programs today).
- How loops close their phis (fixpoint per loop) and the bound on path length.
- Whether the new core replaces `effects.cpp` in place or lives beside it until it matches.
- The tests: at least `amountsOf`'s summary, the `activitytable`-shaped loop not polluting rows,
  an idempotent vs non-idempotent write, an `if` merging two values, recursion converging, and
  the switch off changing nothing.

## Results

Measured 2026-11-01 on `Chronicle`, with one build of `ChronicleRules` and `ChronicleSolver` for
both sides.

### The eight programs

| Program | Analyzed | Collapsed | Inferred | Joined |
|---|---|---|---|---|
| `milestones` | yes | 0 | 9 | 94 fills |
| `windows` | yes | 0 | 10 | 94 fills, `townPlaces` |
| `forecast` | yes | 0 | 10 | 94 fills, `townPlaces` |
| `resources` | yes | 0 | 12 | 94 fills, `townPlaces` |
| `achievements` | yes | 0 | 11 | 94 fills |
| `epitaph` | yes | 0 | 14 | 94 fills, `townPlaces` |
| `town` | yes | 0 | 15 | 94 fills, `townPlaces` |
| `expedition` | yes | 0 | 9 | 94 fills |

The 94 fills are `statNames` 51 and `statShort` 43. `lastAge`, `anyAge`, `noBreak` and `town`'s
`foundLine`/`opened` are inferred. The `.pgc` files come to 2 942 683 bytes on, 2 938 923 off.

### Speed

| Whole rules compile | Time | Instructions |
|---|---|---|
| Off | 0.21 s | 1.87 G |
| On | 0.39 s | 3.69 G |

That is +0.18 s. The analysis itself, all eight programs, takes about 0.16 s: 139 ms to analyze,
23 ms to read the programs, 1 ms of module cache facts. The rest is the modules compiled again
(22 compiles on, 15 off), where a program decides otherwise for them.

### Rulecheck and beam

| Run | Off | On |
|---|---|---|
| Rulecheck | 18.3 G instructions | 21.7 G, byte-identical answers |
| Beam, whole run | 316.9 G, 44 s | 475.0 G, 59 s |
| Beam, search | 43 s | 41 s |
| Beam, tasks | 102 of 266 | 102 of 266, the same report |

The beam costs more with the analysis on because the switch also analyzes the solver's own
`solver.pg`, about 15 s of that run (25% of its samples). The search itself is 2 s shorter.

`test_compiler` (431) and `test_chronicle` (762) pass with the switch off.

### Departures from this design during the build

- **Object solve:** the final aliases and regions are solved over objects, not rewritten as paths,
  because path closure did not finish on `windows`.
- **Pass order:** `ProgramConstsPass` runs first in the AST pipeline, so the analysis and what it
  decides see the same statements.
- **`RunOncePass`:** with the switch on, its fills are off, and `ProgramConstsPass` decides them.
- **A file that runs more than once:** it joins only constant fills, and infers no var holding an
  object. What such a file imports counts as running more than once too.
- **Module cache key:** it holds what was decided for the files a module is made of, not what the
  decisions rest on (summaries, regions).
- **BodyIR cache:** not added. The target was met without it.

### Where on differs from off, by decision

- **The `count` ruling:** `const count = {n: 0}` then `count.n = count.n + 1` keeps joining, so it
  gives 1, 1, 1 on and 1, 2, 3 off.
- **Fills behind a folded loop:** `StaticLoopEvaluation` removes an empty loop before the off
  `RunOncePass`, so the fill behind it joins off. The analysis reads the source, where the loop
  still stands, so that fill stays apart on and runs on every run. First runs agree.
