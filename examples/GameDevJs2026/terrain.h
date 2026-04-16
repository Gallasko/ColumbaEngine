#pragma once

#include <cstdint>

#include "itemregistry.h"

// Terrain type semantics for the procedurally generated canvas.
// Stored in the terrain layer and queried by miners / renderers.
enum class TerrainType : uint8_t
{
    None      = 0,
    Grass     = 1,
    OreIron   = 2,
    OreCopper = 3,
    OreCoal   = 4,
    OreStone  = 5,
    Tree      = 6,
    Rock      = 7,
    Water     = 8
};

inline bool isOre(TerrainType t)
{
    return t == TerrainType::OreIron
        or t == TerrainType::OreCopper
        or t == TerrainType::OreCoal
        or t == TerrainType::OreStone;
}

// Map terrain type to the ItemId produced by a miner on that tile.
// Matches the IDs assigned in createDefaultItemRegistry() at itemregistry.h:
//   Iron Ore = 1, Copper Ore = 2, Coal = 3, Stone = 4
inline ItemId oreToItem(TerrainType t)
{
    switch (t)
    {
        case TerrainType::OreIron:   return 1;
        case TerrainType::OreCopper: return 2;
        case TerrainType::OreCoal:   return 3;
        case TerrainType::OreStone:  return 4;
        default:                     return ITEM_NONE;
    }
}
