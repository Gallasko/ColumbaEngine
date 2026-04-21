# Audit: GridSystem

**Files**: `/Systems/gridsystem.h` (248 lines), `/Systems/gridsystem.cpp` (825 lines)
**Total**: ~1,073 lines

## What's Good

- Well-designed multi-layer grid (terrain, building, item) with proper z-indexing.
- `resolveLineTileVariant` and `isNeighborConnected` handle complex belt connectivity logic correctly, including corners.
- Autotile logic (`grassAutotileFrame`, `oreAutotileFrame`) is sophisticated with fixpoint pass for `renderAsDirt`.
- Save/load properly serializes terrain + buildings separately; `restoreBuilding` correctly skips `BuildingPlacedEvent`.
- `regenerateTerrain` preserves player buildings while refreshing terrain — nice dev tool.
- Conveyor animation is globally synchronized (correct for visual consistency).
- `ConveyorTileIndex` enum provides meaningful names for atlas indices.
- `removeBuilding` properly handles multi-cell buildings by finding the owner cell.

## What's Bad / Code Smells

### 1. `placeBuilding` and `restoreBuilding` are ~90% identical
Lines 168-253 vs 255-330. Only difference: `restoreBuilding` skips validation and `BuildingPlacedEvent`. Textbook DRY violation — a private helper with a `bool emitEvent` parameter would halve the code.

### 2. `using namespace pg;` in header
This header is included by nearly every other system, maximizing pollution.

### 3. Magic tile ID `4` for conveyor belt
Appears ~6 times throughout.

### 4. Linear scan for conveyor entries
In `save()`, `updateConveyorTileIndex`, and `removeConveyorEntry`. An `unordered_map<uint64_t, size_t>` (entityId → index) would fix this.

### 5. Per-tile entity creation
`spawnTextureTile` creates a new entity per tile — for a 64x64 grid with ground + grass + overlay + trees, this is thousands of entities. Consider batching if entity count becomes a performance concern.

## Could Live in the Engine

- The **autotile algorithm** (4-direction cardinal + 4-diagonal inner corner, with fixpoint pass) is a general-purpose 9-tile/47-tile autotile many 2D games need.
- The **conveyor animation system** (global frame sync across all animated tiles) is reusable.
- The **multi-layer grid with building placement validation** is game-generic.

## Refactoring Suggestions

1. Merge `placeBuilding`/`restoreBuilding` into a single private method with `bool emitEvent`.
2. Replace conveyor `vector` with `unordered_map<uint64_t, ConveyorEntry>` for O(1) lookup.
3. Define `CONVEYOR_TILE_ID = 4`.
4. Move autotile algorithm into the engine.

## Summary Rating

**The most important system — solid architecture but has the biggest DRY violation and performance concern (linear conveyor scans).**
