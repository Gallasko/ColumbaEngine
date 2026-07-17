# Audit: Inventory

**Files**: `/Registries/inventory.h` (33 lines), `/Registries/inventory.cpp` (92 lines)
**Total**: ~125 lines

## What's Good

- **Cleanest file in the project so far.** Pure data structure with no ECS/engine dependencies. Could be unit-tested trivially.
- **`insert()` is well-implemented** — two-phase approach (stack into existing matching slots first, then fill empties) is correct and handles partial inserts gracefully. Returns overflow count so the caller can decide what to do with leftovers.
- **`remove()` returns actual count removed** — good API. Caller can detect "tried to remove 5 but only had 3" without a separate check.
- **`canAccept()` checks both empty slots and partial stacks** — correct. Won't falsely reject items that can stack onto existing slots.
- **`hasAtLeast()` short-circuits** — `if (total >= count) return true;` exits early. Good for hot paths.
- **No allocation in core operations** — insert/remove/hasAtLeast/countItem all operate on the existing `slots` vector with no allocations.
- **Simple constructor** — `explicit Inventory(size_t numSlots = 0)` prevents implicit conversions and is clear.

## What's Bad / Code Smells

### 1. `countItem` can overflow `uint16_t`
```cpp
uint16_t Inventory::countItem(ItemId id) const
{
    uint16_t total = 0;
    for (const auto& slot : slots)
        if (slot.id == id) total += slot.count;
    return total;
}
```
If an inventory has many slots of the same item (e.g. 700 slots × 100 count = 70,000), `uint16_t` maxes at 65,535 and wraps. Unlikely with current game sizes (player has ~20 slots), but the type choice is silently dangerous. Using `uint32_t` for the accumulator (or `size_t`) would be safe.

### 2. `getSlot()` has no bounds check
```cpp
const ItemStack& getSlot(size_t index) const { return slots[index]; }
```
UB if `index >= slots.size()`. A debug assert would help.

### 3. Missing convenience methods
- No `isFull()` — callers must do `!hasEmptySlot() && !canAccept(id, reg)`.
- No `clear()` to empty all slots (useful for save/load or death penalty).
- No `totalItemCount()` for debug/display.

These are minor — the API covers the essential operations well.

### 4. `insert` doesn't consolidate partial stacks
If you have `[{wood, 3}, {stone, 5}, {wood, 10}]` and insert wood, it'll fill the first wood slot, then the second. It won't merge the two wood stacks. This is probably intentional (Factorio-style fixed slots), but it means inventories can get fragmented over time.

## Could Live in the Engine

### This entire file is engine-ready
`Inventory` has zero dependencies on game-specific code. It depends only on `ItemStack` and `ItemRegistry`, which are themselves generic. This is the strongest candidate for engine promotion in the Registries directory.

A generic `SlottedContainer<IdType>` with stack limits, insert/remove, and count queries would serve any inventory-based game (RPG bags, factory storage, chest contents, etc.).

## Refactoring Suggestions

1. **Widen `countItem` accumulator to `uint32_t`** to prevent overflow.
2. **Add debug asserts** to `getSlot()`.
3. **Consider adding `isFull()` and `clear()`** for common use cases.
4. **Promote to engine** — this is pure game logic with no game-specific dependencies.

## Summary Rating

**Excellent.** This is the best-written file audited so far. Clean, focused, correct, zero dependencies on engine internals. The only real issue is the potential `uint16_t` overflow in `countItem`, which is low-risk in practice. This file is ready to be lifted into the engine as-is.
