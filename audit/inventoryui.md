# Audit: InventoryUI

**Files**: `/UI/inventoryui.h` (157 lines), `/UI/inventoryui.cpp` (532 lines)
**Total**: ~689 lines

## What's Good

- Comprehensive drag-and-drop system with held-item visuals that follow the cursor.
- Clean separation of `pickUpFromSlot`/`dropOnSlot` for internal and `pickUpFromExternal`/`dropOnExternal` for cross-panel transfers.
- `cancelHeldDataOnly()` correctly returns items to source or falls back to `insert()`.
- `externalClickCheck` callback allows external panels to intercept clicks without tight coupling.
- `InventoryOpenedEvent`/`InventoryClosedEvent` are clean ECS events.
- Slot backgrounds properly anchored to the backdrop using `UiAnchor`.

## What's Bad / Code Smells

### 1. `using namespace pg;` in header
### 2. Redundant position recalculation
`slotAtPosition()`, `refreshSlot()`, and `slotScreenPos()` all recompute `panelW`, `panelH`, `bdX`, `bdY` independently. Should cache or compute once.

### 3. `externalSourceSlot` stores raw pointer to external `ItemStack`
Dangerous if the owning system destroys/moves data while inventory is open with a held item.

### 4. Magic offset values
`8.0f` in `updateHeldPosition()` and `-4.0f` in count text positioning are undocumented.

### 5. `heldFromSlot` uses `int` with `-1` sentinel
`std::optional<size_t>` would be type-safer.

## Could Live in the Engine

The entire drag-and-drop slot-grid inventory system is highly reusable. A `DragDropSlotPanel` engine component would serve any game with item management.

## Refactoring Suggestions

1. Cache `panelW`, `panelH`, `bdX`, `bdY` as members (update on resize).
2. Use `std::optional<size_t>` for `heldFromSlot`.
3. Document or name offset constants.
4. Guard `externalSourceSlot` usage more carefully.

## Summary Rating

**The most complex and most well-engineered UI file. Needs minor cleanup for robustness and DRY.**
