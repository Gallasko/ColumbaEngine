# Audit: InserterSystem

**Files**: `/Systems/insertersystem.h` (113 lines), `/Systems/insertersystem.cpp` (474 lines)
**Total**: ~587 lines

## What's Good

- Clean three-state FSM: `Idle → Swinging → Returning → Idle`.
- `INSERTER_TILE_ID = 8` is a named constant.
- Deferred initialization handles the case where the entity isn't ready when the inserter is placed.
- Held item visual follows an arc using `atan2` and `sin/cos` — visually polished.
- Properly returns held item to player on building removal.
- Comprehensive support for pickup/drop from/to: belts, miners, machines, storage, depots.

## What's Bad / Code Smells

### 1. `tryPickup` and `tryDrop` are long type-switch chains
Lines 206-318 and 321-389: each is a chain of `if (cell.tileId == X)` blocks. Every new building type requires editing these methods. An **item source/sink interface** with `canPickupFrom()` / `canDropInto()` would be extensible.

### 2. Direct slot manipulation instead of Inventory API
`slot.count -= 1; if (slot.count == 0) slot.clear();` is repeated 4 times. Should be an `Inventory::takeOne()` method.

### 3. `using namespace pg;` in header
### 4. Magic tile IDs `4`, `5`, `6` in tryPickup/tryDrop
### 5. `M_PI` usage
Not guaranteed by the C++ standard. Should use `<numbers>` (C++20) or a portable constant.

### 6. Unwieldy 7-parameter constructor

## Could Live in the Engine

The inserter arm animation pattern (state machine + arc interpolation) could be an engine utility for animated entity transfers.

## Refactoring Suggestions

1. Define `IItemSource` / `IItemSink` interfaces — inserters call polymorphically instead of type-switching.
2. Add `Inventory::takeOne(ItemId)` method.
3. Define `constexpr double PI` or use `<numbers>`.
4. Bundle constructor parameters into a struct.

## Summary Rating

**Impressive visual polish and correct FSM, but the type-switch dispatching in tryPickup/tryDrop is the biggest extensibility concern in the entire codebase.**
