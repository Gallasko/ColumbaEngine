# Audit: DepotUI

**Files**: `/UI/depotui.h` (113 lines), `/UI/depotui.cpp` (291 lines)
**Total**: ~404 lines

## What's Good

- Well-structured `constexpr` layout constants — no hardcoded values in the .cpp outside of these.
- Clean open/close lifecycle: opens inventory alongside, registers external click check.
- `ensurePanelCreated()` / `setPanelVisibility()` pattern avoids unnecessary entity recreation.
- `refreshItemSlotDisplay()` helper is reusable across slots.
- Cached slot positions for hit testing is efficient.

## What's Bad / Code Smells

### 1. `using namespace pg;` in header
### 2. `setEntityVisibility` duplication
Identical 5-line function copy-pasted across 9+ UI files.

### 3. `getPanelX()` duplicates inventory width calculation
Same formula appears in 4 files (minerui, machineui, storageui, depotui). Should be `InventoryUISystem::getPanelWidth()`.

### 4. No bounds check on `slotIndex` in `refreshSlot()`
If `slotIndex >= NUM_SLOTS`, `slotBgEntityId[slotIndex]` is out of bounds.

### 5. Inconsistent null-checking
`close()` calls `inventoryUI->setExternalClickCheck(nullptr)` without null-checking (but `open()` does check).

## Could Live in the Engine

- The "side-panel-with-N-slots-beside-inventory" pattern is generic enough for a `SlotGridPanelUI` engine component.
- `setEntityVisibility` is trivially engine-level.

## Refactoring Suggestions

1. Extract `setEntityVisibility` into shared utility.
2. Add `InventoryUISystem::getPanelWidth()` static method.
3. Add bounds check in `refreshSlot()`.
4. Null-check `inventoryUI` consistently.

## Summary Rating

**Clean and functional, but heavily duplicated code pattern begging for a base class.**
