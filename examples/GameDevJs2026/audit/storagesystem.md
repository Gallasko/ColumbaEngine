# Audit: StorageSystem

**Files**: `/Systems/storagesystem.h` (50 lines), `/Systems/storagesystem.cpp` (58 lines)
**Total**: ~108 lines

## What's Good

- `STORAGE_TILE_ID = 9` is a named constant.
- Clean register/unregister following the same pattern as depot.
- Items returned to player on removal.
- Minimal code surface area.

## What's Bad / Code Smells

### 1. `using namespace pg;` in header
### 2. Magic number `8` for slot count
### 3. Nearly identical to DepotSystem
Same register/unregister/save/load/getXxx pattern — screams for a shared base class.

## Could Live in the Engine

Same as DepotSystem — a `MachineSystemBase<DataType>` template.

## Refactoring Suggestions

1. Name the slot count.
2. Unify with DepotSystem into a `ContainerSystem<DataType>` template.

## Summary Rating

**Correct and clean but duplicates the DepotSystem pattern almost verbatim.**
