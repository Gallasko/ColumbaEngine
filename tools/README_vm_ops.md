# VM op-kernel code generation (self-hosted)

The VM's binary-operator semantics are declared ONCE in `/tools/vm_ops_def.pg`
and rendered by `/tools/gen_vm_ops.pg` (a PgScript program, run by
`PgCompilerBootstrap`) into four committed files under
`/src/Engine/Compiler/generated/`:

| file | consumed by | contains |
|---|---|---|
| `ops_functors.inc` | `decoded_fusion.h` | `FAdd`… functor structs: inline typed kernels + `applyCond` for comparisons |
| `ops_binop_tables.inc` | `decoded_fusion.h` | `BinOp` enum, `classifyBinary`, `isComparison`, `isCommutative`, `binOpName` |
| `ops_select.inc` | `decoded_fusion.h` | `selectFusedBinary` / `selectFusedLocalCompound` switches |
| `ops_base_handlers.inc` | `vm_binary_op.cpp` | base stack/stack `op_*_decoded` handlers |
| `ops_values_helpers.inc` | `vm_binary_op.cpp` | `VM::*Values` bodies (kernel ladder + handwritten `*ValuesTail` fallback) |

Because every consumer renders from the same kernel table, adding an op or a
type specialization to the spec updates the fused fast path, the base handler,
the slow-path helper and the fusion selection tables together — they cannot
drift.

## Workflow

1. Edit `/tools/vm_ops_def.pg` (schema documented in its header).
2. Build. Regeneration is AUTOMATIC: the build persists a stage-0 copy of
   `PgCompilerBootstrap` (`<build-dir>/stage0/`), and a build-time rule
   reruns the generator with it whenever the spec or generator changed —
   before the engine libraries compile.
3. Verify:
   - `/benchmark/comparisons/script/check_semantics.sh` — optimized vs
     `--no-opt` differential (byte-identical stdout on the semantics scripts)
   - `/benchmark/comparisons/script/check_vm_ops_fresh.sh` — committed `.inc`
     matches a fresh regeneration (run this in CI / pre-commit)
   - `/benchmark/comparisons/script/bench_pg.sh` — perf regression check
4. Commit the spec change TOGETHER with the regenerated `.inc` files.

Manual force-regen (uses the freshly built bootstrap, not stage-0):
`cmake --build <build-dir> --target GenerateVmOps`, or from `/tools`:
`<build-dir>/PgCompilerBootstrap gen_vm_ops.pg ../src/Engine/Compiler/generated`

## How the bootstrap cycle is broken (stage-0)

The generated files are inputs to `ColumbaEngineMinimal`, which
`PgCompilerBootstrap` (the generator's runner) links against — so the regen
rule cannot depend on the `PgCompilerBootstrap` TARGET (circular). Instead,
every successful build copies the bootstrap binary to
`<build-dir>/stage0/PgCompilerBootstrap` (POST_BUILD), and the regen rule
(`tools/regen_vm_ops_stage0.cmake`) runs THAT copy — a file-level dependency
only, no cycle. On a clean build directory no stage-0 exists yet; the
committed `.inc` files are the bootstrap seed and regen is skipped for that
first build (the freshness check guards the seed). Because generator output
depends only on the spec, regenerating with the previous build's compiler
reaches a fixed point in one pass.

Handwritten pieces that intentionally stay out of the generator: the generic
fused-handler templates and pick ladder (`decoded_fusion.h`), the fusion pass
(`decoded_chunk.cpp`), and the `*ValuesTail` fallbacks in `vm_binary_op.cpp`
(string concat, `ElementType` conversions, error throws).
