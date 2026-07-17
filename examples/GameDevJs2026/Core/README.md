# Core

Foundation data types and utilities shared across the game. Nothing in here depends on ECS systems — it's all plain data, helpers, and procedural generation.

- `grid.h` — `CellData`, `GridLayer`, `Grid` (fixed 32×32, 16px tiles, layered cells)
- `terrain.h` — `TerrainType` enum + `isOre`, `isBlockingTerrain`, `oreToItem` helpers
- `gridatlas.h` / `.cpp` — `GridAtlas` (builds an atlas from a grid-sliced image)
- `canvasgenerator.h` / `.cpp` — procedural canvas generation (ores, trees, rocks)
- `machinekey.h` — shared `machineKey(x, y)` inline helper (used by miner/crafting/inserter maps)
- `worldfacts.h` / `.cpp` — `WorldFacts` ECS system + `FactChecker` for recipe unlocks (templates stay in header)
