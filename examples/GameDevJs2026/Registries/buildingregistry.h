#pragma once

#include <cstdint>
#include <string>

#include "pgconstant.h"

#include "Helpers/registry.h"

enum class PlacementMode : uint8_t
{
    ClickToPlace, // Single click places building (furnaces, assemblers)
    LineDrag      // Drag draws a path with corners (conveyors, pipes)
};

struct BuildingDef
{
    uint16_t tileId;              // Stored in CellData (>0; 0=empty)
    std::string name;             // Display name
    std::string textureName;      // Atlas texture prefix ("" = use color)
    pg::constant::Vector4D color; // Fallback color when no texture
    int gridW = 1;                // Footprint width in cells
    int gridH = 1;                // Footprint height in cells
    PlacementMode mode = PlacementMode::ClickToPlace;
    bool hasDirection = false;    // R-key rotation applies
    bool isAnimated = false;      // GridSystem should animate this
};

struct BuildingRegistry : public pg::Registry<BuildingDef>
{
    void addBuilding(const BuildingDef& def)
    {
        add(def, static_cast<size_t>(def.tileId));
    }

    const BuildingDef* findByTileId(uint16_t tileId) const
    {
        return findByKey(static_cast<size_t>(tileId));
    }
};

BuildingRegistry createDefaultBuildingRegistry();
