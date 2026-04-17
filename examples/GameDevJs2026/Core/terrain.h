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

// Terrain that blocks building placement. Rocks and trees are solid obstacles;
// grass and ore are walkable/buildable (miners need to sit on ore).
inline bool isBlockingTerrain(TerrainType t)
{
    return t == TerrainType::Rock or t == TerrainType::Tree;
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

// Returns true for terrain types the player can manually mine by clicking.
inline bool isMinableTerrain(TerrainType t)
{
    return isOre(t) or t == TerrainType::Tree or t == TerrainType::Rock;
}

// Map any minable terrain type to the ItemId it yields when manually mined.
// Extends oreToItem() to also cover trees (Wood=15) and rocks (Stone=4).
inline ItemId terrainToItem(TerrainType t)
{
    switch (t)
    {
        case TerrainType::OreIron:   return 1;   // Iron Ore
        case TerrainType::OreCopper: return 2;   // Copper Ore
        case TerrainType::OreCoal:   return 3;   // Coal
        case TerrainType::OreStone:  return 4;   // Stone
        case TerrainType::Tree:      return 15;  // Wood
        case TerrainType::Rock:      return 4;   // Stone
        default:                     return ITEM_NONE;
    }
}

// Minimum tool tier required to mine this terrain.
// 0 = bare hands, 1 = stone tools, 2 = iron tools.
inline uint8_t terrainTier(TerrainType t)
{
    switch (t)
    {
        case TerrainType::Tree:      return 0;
        case TerrainType::Rock:      return 0;
        case TerrainType::OreStone:  return 0;
        case TerrainType::OreCoal:   return 1;
        case TerrainType::OreCopper: return 1;
        case TerrainType::OreIron:   return 1;
        default:                     return 0;
    }
}

// Number of hits required to mine each terrain type by hand.
inline int terrainHitsRequired(TerrainType t)
{
    switch (t)
    {
        case TerrainType::Tree:      return 5;
        case TerrainType::Rock:      return 3;
        case TerrainType::OreIron:   return 6;
        case TerrainType::OreCopper: return 6;
        case TerrainType::OreCoal:   return 4;
        case TerrainType::OreStone:  return 4;
        default:                     return 0;
    }
}
