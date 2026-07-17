# Audit: GridAtlas

**Files**: `/Core/gridatlas.h` (11 lines), `/Core/gridatlas.cpp` (27 lines)
**Total**: ~38 lines

## What's Good

- **Minimal and single-purpose.** Takes grid dimensions, produces atlas frames. No unnecessary state.
- **Correct UV math.** Handles the bottom-origin UV convention properly.
- **Frame naming by index** works well for grid-based atlases.

## What's Bad / Code Smells

### 1. No validation of inputs
If `cols == 0`, the modulo `i % cols` causes a division-by-zero crash. If `count > cols * rows`, UV coordinates silently go out-of-bounds.

### 2. Directly manipulates base-class fields
`this->imagePath = ...`, `this->atlasWidth = ...`, etc. are set in the constructor body instead of an initializer list. Couples `GridAtlas` tightly to `LoadedAtlas` internals.

## Could Live in the Engine

**`GridAtlas` itself.** This is a completely generic "chop a sprite sheet into a uniform grid of frames" loader. Zero game-specific logic. Should be `pg::GridAtlasLoader` in the engine.

## Refactoring Suggestions

1. Add validation: assert `cols > 0`, assert `count <= cols * (atlasH / frameH)`.
2. Use base-class initializer or a dedicated `init()` method.
3. Move to the engine.

## Summary Rating

**Clean utility that belongs in the engine — just needs input validation.**
