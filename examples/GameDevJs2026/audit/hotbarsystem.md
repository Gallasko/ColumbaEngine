# Audit: HotbarSystem

**Files**: `/UI/hotbarsystem.h` (129 lines), `/UI/hotbarsystem.cpp` (326 lines)
**Total**: ~455 lines

## What's Good

- **Best anchor usage in the project.** Backdrop and slots auto-reposition on resize via `UiAnchor` system.
- `SlotVisual` struct groups entity IDs cleanly.
- `refreshSlot()` reads anchor-resolved positions rather than recalculating — resilient.
- Supports `iconWidthRatio` for non-square item icons.
- `slotAtPosition()` has proper bounds checking.

## What's Bad / Code Smells

### 1. `using namespace pg;` in header
### 2. `HOTBAR_SLOTS` is a global-scope `inline constexpr`
Should be a class static or in a namespace — occupies global namespace for every includer.

### 3. `setVis` lambda
Yet another copy of `setEntityVisibility`.

### 4. Hit-testing vs anchor drift
`slotAtPosition()` recalculates from constants rather than reading resolved positions. Could drift from rendered position if anchor system shifts anything.

## Could Live in the Engine

A reusable `ItemSlotGrid` widget (array of bg + icon + count entities, refresh logic, hit testing) could serve both hotbar and inventory.

## Refactoring Suggestions

1. Move `HOTBAR_SLOTS` into the class as `static constexpr`.
2. Extract `setVis` into the shared utility.
3. Consider anchoring item/text entities to their slot backgrounds.

## Summary Rating

**Best-in-class anchor usage, but namespace pollution and global constants are notable issues.**
