# Audit: MachineDemoSystem

**Files**: `/UI/machinedemosystem.h` (174 lines), `/UI/machinedemosystem.cpp` (850 lines)
**Total**: ~1,024 lines

## What's Good

- Impressive mini factory simulation: belts, inserters, machines with real animation and physics.
- Fixed-size arrays (`items[MAX_ITEMS]`, etc.) avoid heap allocations during simulation — good for hot loop.
- Belt processing iterates in reverse to prevent cascading in one tick — correct behavior.
- Deferred open/close via `pendingOpen`/`pendingClose` flags avoids state mutation during event processing.
- `destroyPanel()` thoroughly cleans up all entities including simulation-spawned ones.

## What's Bad / Code Smells

### 1. Highest magic number density in the project
Tile IDs `4`, `5`, `6`, `7`, `8`; item IDs `1`, `5`, `7`; process timers `15`, `10`; texture names all hardcoded.

### 2. `using namespace pg;` in header
### 3. `DIR_TO_TILE[4]` declared twice
Line 316 in `initSimulation` and line 429 in `simulationTick`.

### 4. O(n) scans for `findBeltAt()` and `findMachineAt()`
On every tick for every inserter. Fine for `MAX_BELTS=12` but doesn't scale.

### 5. `findMachineAt()` hardcodes machine dimensions
`w=2, h=3` for tile IDs 5/6/7 rather than querying `BuildingRegistry`.

### 6. Entity churn
`allocateItem()` creates a new entity every spawn, `freeItem()` removes it. Pooling would be cleaner.

### 7. No resize handling
`screenW`/`screenH` never updated.

### 8. Mixed `&&`/`and` style

## Could Live in the Engine

- Fixed-timestep simulation loop (`tickAccumulator` pattern) is generic.
- Entity pooling (allocate/free pattern) could be an engine utility.

## Refactoring Suggestions

1. Create named constants for all tile/item IDs.
2. Deduplicate `DIR_TO_TILE` into a single shared constant.
3. Query `BuildingRegistry` for machine dimensions.
4. Add `Listener<ResizeEvent>`.
5. Consider entity pooling for simulation items.
6. Pick one style: `&&` or `and`.

## Summary Rating

**Ambitious and largely correct simulation, but the highest magic-number density in the project and needs cleanup.**
