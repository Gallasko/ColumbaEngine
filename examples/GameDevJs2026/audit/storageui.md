# Audit: StorageUI

**Files**: `/UI/storageui.h` (113 lines), `/UI/storageui.cpp` (291 lines)
**Total**: ~404 lines

## What's Good

- Nearly identical structure to DepotUI — consistent patterns.
- 8-slot grid (2x4) is well-proportioned.
- All the same good patterns: cached slot positions, `ensurePanelCreated`, external click check.

## What's Bad / Code Smells

### 1. `using namespace pg;` in header
### 2. `setEntityVisibility` duplication
### 3. `refreshItemSlotDisplay` duplication — verbatim copy from DepotUI
### 4. `getPanelX()` duplication

### 5. Stale cached positions — potential bug
`cachedSlotX`/`cachedSlotY` are set in `createPanel()` and never updated. If panel is created, closed, screen resized, then reopened: backdrop is at old position but hit tests use new positions.

### 6. Inconsistent null-checking on `inventoryUI` in `close()`

## Could Live in the Engine

This is essentially DepotUI with different slot count and data source. A parameterized `GridSlotPanel` would replace both.

## Refactoring Suggestions

1. Extract a `GridSlotPanelUI` base class shared with DepotUI.
2. Update cached slot positions on resize (or recompute in `open()`).

## Summary Rating

**Functional copy-paste of DepotUI — correct but begging for generalization.**
