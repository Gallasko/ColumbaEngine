# Audit: Terrain

**Files**: `/Core/terrain.h` (103 lines, header only)
**Total**: ~103 lines

## What's Good

- **`enum class TerrainType : uint8_t`** — strongly typed, compact storage.
- **Free functions** (`isOre`, `isBlockingTerrain`, `isMinableTerrain`, `oreToItem`, `terrainToItem`, `terrainTier`, `terrainHitsRequired`) provide a clean, centralized API for terrain queries. No scattered `if (type == 2)` comparisons elsewhere.
- **All functions are `inline`** — no ODR issues.
- **Comments reference the item registry**, providing traceability.

## What's Bad / Code Smells

### 1. Magic item IDs — project-wide problem at its worst
```cpp
case TerrainType::OreIron:   return 1;   // Iron Ore
```
Should be `return Items::IRON_ORE;`. If item registration order changes, these functions silently produce wrong results.

### 2. Duplicated logic between `oreToItem` and `terrainToItem`
The four ore cases are duplicated verbatim. `terrainToItem` should delegate to `oreToItem`.

### 3. Unsafe defaults for unknown terrain types
- `terrainTier` returns 0 (mineable bare-handed) for unknown types
- `terrainHitsRequired` returns 0 (mined in zero hits) for unknown types

Both should return sentinel values (e.g., 255 for tier, a high number for hits) to prevent accidental free resources from future terrain types.

### 4. Heavy include for a lightweight need
`#include "itemregistry.h"` pulls in the entire item registry just for the `ItemId` typedef and `ITEM_NONE`. A separate `itemtypes.h` would reduce coupling.

## Could Live in the Engine

The pattern of "enum class + free query functions" is good but game-specific. The engine could provide a `TerrainRegistry` where terrain types and their properties are data-driven rather than hard-coded switch statements.

## Refactoring Suggestions

1. **Define named item constants** and use them here.
2. **Have `terrainToItem` delegate** to `oreToItem` for ore cases.
3. **Change default returns** to safe sentinel values.
4. **Extract `ItemId` typedef** into a standalone `itemtypes.h`.
5. **Consider a data-driven approach** — `constexpr` lookup table indexed by `TerrainType`.

## Summary Rating

**Clean enum design undermined by hard-coded magic item IDs.** The most impactful fix is introducing named constants.
