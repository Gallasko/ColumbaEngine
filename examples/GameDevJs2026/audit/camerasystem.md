# Audit: CameraSystem

**Files**: `/Systems/camerasystem.h` (90 lines), `/Systems/camerasystem.cpp` (183 lines)
**Total**: ~273 lines

## What's Good

- Well-structured separation of zoom, pan (keyboard + mouse drag), and resize handling.
- Zoom-toward-mouse math is correct and avoids the engine's built-in `screenToWorld` limitation (documented in comment).
- Clean use of `std::clamp` for zoom bounds.
- Keyboard pan speed scales inversely with zoom — UX-correct.
- `deltaTime` reset after `execute()` prevents double-processing.

## What's Bad / Code Smells

### 1. `using namespace pg;` in header
Pollutes every file that includes this header.

### 2. No camera bounds clamping
The user can pan infinitely beyond the grid.

### 3. Asymmetric zoom factors
`1.0f / 0.9f` and `1.0f / 1.1f` are non-obvious magic numbers. "Zoom in ~11%, zoom out ~9%" — the asymmetry is odd and undocumented.

### 4. `inventoryUI` is a raw pointer with no lifecycle guarantee
Set via `setInventoryUI()`. If `InventoryUISystem` is destroyed first, this dangles.

### 5. Misleading name `deltaTime`
Actually accumulates multiple tick events, not a single frame delta.

## Could Live in the Engine

A `FreeRoamCamera2D` with zoom-toward-cursor, keyboard pan, and right-mouse drag is reusable. The engine has `BaseCamera2D` but lacks these controls.

## Refactoring Suggestions

1. Move `using namespace pg;` out of the header.
2. Clamp camera position to configurable world bounds.
3. Name the zoom factors as `constexpr`.
4. Consider a weak reference for `inventoryUI`.

## Summary Rating

**Good camera code with solid math; header namespace pollution and missing bounds are the main concerns.**
