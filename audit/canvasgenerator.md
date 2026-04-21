# Audit: CanvasGenerator

**Files**: `/Core/canvasgenerator.h` (73 lines), `/Core/canvasgenerator.cpp` (216 lines)
**Total**: ~289 lines

## What's Good

- **Self-contained procedural generation** with zero engine coupling — only depends on `grid.h` and `terrain.h`. Easy to unit test.
- **Custom `XorShift32` RNG** is intentionally isolated from the global RNG, with a comment explaining why. Good design for deterministic seeding.
- **`GenerationParams` struct** with sensible defaults makes the generator configurable without needing a builder.
- **Seed derivation** in `canvasgen_detail::deriveSeed` uses good hash constants and avoids zero output.
- **Tree placement validates the full footprint** before stamping, preventing clipping at canvas edges.
- **Tile counting pass** after generation pre-computes data that would otherwise be recomputed at runtime.
- **Named constants** `TREE_W` and `TREE_H` for the tree footprint.

## What's Bad / Code Smells

### 1. Stateless class with a single static method
`CanvasGenerator` should be either a free function in a namespace (`namespace canvasgen { CanvasGenResult generate(...); }`) or the class should hold state.

### 2. `deriveSeed` is exposed in the header
It's in a named namespace `canvasgen_detail` but still visible to all includers. Should be in an anonymous namespace in the .cpp.

### 3. Modulo bias in `rangeInt`
`next() % range` has slight bias when `range` does not divide `2^32`. Acceptable for game generation but worth a comment.

### 4. `paintBlob` uses `sqrt` per cell
Could use squared distance comparison, though at 32x32 scale this is negligible.

## Could Live in the Engine

- **`XorShift32`** — a lightweight seedable local RNG. The engine's `RandomNumberGenerator` is a global singleton, but many systems need local seedable RNGs. This should be `pg::LocalRng` or similar.
- **`CanvasGenerator` / procedural terrain generation** — the blob-painting, seed-derivation, and scatter-placement logic is not game-specific.

## Refactoring Suggestions

1. Replace the static-method class with a namespace-scoped free function.
2. Move `XorShift32` to the engine as a reusable math utility.
3. Hide `deriveSeed` as file-local.
4. Add a comment about modulo bias.

## Summary Rating

**Well-designed, deterministic, and self-contained — one of the cleanest files in the project.** Minor structural quibbles only.
