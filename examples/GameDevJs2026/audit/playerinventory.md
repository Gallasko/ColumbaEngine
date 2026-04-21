# Audit: PlayerInventory

**Files**: `/Systems/playerinventory.h` (59 lines), `/Systems/playerinventory.cpp` (55 lines)
**Total**: ~114 lines

## What's Good

- Clean event-driven gain/lose interface.
- `discovered_` fact emission on every item pickup — smart cross-cutting feature.
- Save/load gracefully handles old saves with fewer slots.
- `NUM_SLOTS`, `MAIN_SLOTS`, `HOTBAR_START`, `HOTBAR_COUNT` are named constants.

## What's Bad / Code Smells

### 1. `using namespace pg;` in header
### 2. Duplicated snake_case conversion
Lines 39-47 — third occurrence of this exact logic.

### 3. Mutable `getInventory()` reference
Returns mutable reference — any code can modify the inventory directly, bypassing events and fact tracking. Defeats the purpose of the event system.

### 4. No overflow handling
If inventory is full, `insert` fails silently. No notification to player.

### 5. Inconsistent operator style
Uses `&&` here but `and` elsewhere — should be consistent project-wide.

## Could Live in the Engine

A player inventory with gain/lose events and item discovery tracking is extremely common.

## Refactoring Suggestions

1. Remove the mutable `getInventory()` overload (or make private).
2. Handle inventory-full (emit `PlayerInventoryFullEvent` or return excess as world drops).
3. Extract the snake_case utility.
4. Standardize on `and`/`or` vs `&&`/`||` project-wide.

## Summary Rating

**Simple and effective, but the mutable accessor is a design hole.**
