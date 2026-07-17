# Audit: MinerUI

**Files**: `/UI/minerui.h` (109 lines), `/UI/minerui.cpp` (334 lines)
**Total**: ~443 lines

## What's Good

- Simple and focused — single output slot + progress bar.
- `lastDisplayedStack` optimization avoids redundant refreshes when stack hasn't changed.
- Clean integration with `InventoryUISystem` external click check.
- Progress bar math is straightforward and correct.

## What's Bad / Code Smells

### 1. `using namespace pg;` in header
### 2. `setEntityVisibility` duplication
### 3. Layout constants re-declared as local variables in 4 methods
`titleH = 20.0f`, `gapAfterTitle = 6.0f`, `gapAfterSlot = 8.0f` are local variables redeclared in `isClickOnPanel`, `getPanelY`, `createPanel`, and implicitly in layout. If any copy drifts, hit-testing breaks. Should be `static constexpr` class members.

### 4. `getPanelX()` duplicates inventory width calculation
### 5. Inconsistent null-checking on `inventoryUI` in `close()`

## Could Live in the Engine

Same "side-panel-with-slots" pattern as depot/storage/machine — all could share a base.

## Refactoring Suggestions

1. Promote layout locals to `static constexpr` class members.
2. Share inventory width calculation.
3. Extract `setEntityVisibility` to shared utility.

## Summary Rating

**Works correctly but is the most fragile file due to repeated magic locals that must stay in sync.**
