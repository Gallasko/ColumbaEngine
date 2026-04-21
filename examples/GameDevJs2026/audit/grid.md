# Audit: Grid

**Files**: `/Core/grid.h` (85 lines, header only)
**Total**: ~85 lines

## What's Good

- **Clear, focused data structures.** `CellData`, `GridLayer`, and `Grid` have well-defined responsibilities.
- **`isInBounds` helper** centralizes bounds checking.
- **`worldToGrid` / `gridToWorld` conversions** are defined once, preventing ad-hoc pixel math scattered across the codebase.
- **Constants** (`TILE_SIZE`, `WIDTH`, `HEIGHT`) are `static constexpr` with clear names.
- **No `using namespace` directives.** This header is clean.

## What's Bad / Code Smells

### 1. `getCell()` and `getLayer()` have NO bounds checks — CRITICAL
```cpp
CellData& getCell(size_t layer, int x, int y) { return layers[layer].cells[y][x]; }
```
Out-of-range `layer`, `x`, or `y` causes undefined behavior. This is called from 9+ files across the codebase. **Highest-priority fix across all files reviewed.**

### 2. `WIDTH`/`HEIGHT` are duplicated
Both `GridLayer` and `Grid` define `WIDTH = 32, HEIGHT = 32`. If one is changed without the other, silent data corruption occurs. `Grid::WIDTH` should reference `GridLayer::WIDTH`.

### 3. `worldToGrid` has negative-coordinate truncation bug
`static_cast<int>(worldX) / TILE_SIZE` truncates toward zero, so `worldToGrid(-1.0f, 0)` returns `(0, 0)` instead of `(-1, 0)`. If the camera can scroll to negative world coordinates, this produces wrong grid cells. Should use `std::floor`.

### 4. `CellData::direction` and `enterDirection` use raw `uint8_t`
No enum — the semantic meaning (0=up, 1=right, etc.) is undocumented in this file.

### 5. `CellData::ownerX`/`ownerY` are `int8_t`
Range -128..127. With a 32x32 grid this is fine, but nothing documents or enforces this constraint.

## Could Live in the Engine

- **The entire `Grid`/`GridLayer`/`CellData` system.** This is a generic tile-grid with layers — a fundamental building block for any 2D tile game. A templated `TileGrid<CellT, W, H>` with bounds-checked accessors would be a strong engine contribution.
- **`worldToGrid` / `gridToWorld`** conversions are universal for tile-based games.

## Refactoring Suggestions

1. **Add debug-mode bounds assertions** to `getLayer`, `getCell`:
   ```cpp
   CellData& getCell(size_t layer, int x, int y) {
       assert(layer < layers.size());
       assert(isInBounds(x, y));
       return layers[layer].cells[y][x];
   }
   ```
2. **Unify `WIDTH`/`HEIGHT`** — define once in `GridLayer` and alias in `Grid`.
3. **Fix `worldToGrid`** using `std::floor()`.
4. **Replace `uint8_t direction`** with `enum class Direction : uint8_t`.

## Summary Rating

**Solid data model, but the missing bounds checks on `getCell`/`getLayer` are the highest-priority fix in the entire project.**
