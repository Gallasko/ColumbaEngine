# Audit: DepotSystem

**Files**: `/Systems/depotsystem.h` (54 lines), `/Systems/depotsystem.cpp` (66 lines)
**Total**: ~120 lines

## What's Good

- `DEPOT_TILE_ID = 10` is a named constant — good!
- Clean register/unregister pattern matching other machine systems.
- `depot_placed` fact emission on first depot — nice trigger for progression.
- Items returned to player on unregister.

## What's Bad / Code Smells

### 1. `using namespace pg;` in header
### 2. Magic number `4` for inventory slot count
Should be `constexpr size_t DEPOT_SLOT_COUNT = 4`.

### 3. Identical pattern to StorageSystem
Register/unregister + save/load + map-by-machineKey is duplicated verbatim across `StorageSystem`, `DepotSystem`, `CraftingSystem`, `MinerSystem`, and `InserterSystem`.

## Could Live in the Engine

A `MachineSystemBase<DataType>` template would eliminate huge amounts of boilerplate across 5 systems.

## Refactoring Suggestions

1. Name the slot count.
2. Consider a shared base for the machine register/unregister pattern.
3. Replace `printf` with engine logging.

## Summary Rating

**Compact and correct, minimal surface area for bugs.**
