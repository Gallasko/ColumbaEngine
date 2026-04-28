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
    std::string name;             // Display name
    std::string textureName;      // Atlas texture prefix ("" = use color)
    pg::constant::Vector4D color; // Fallback color when no texture
    int gridW = 1;                // Visual width in cells
    int gridH = 1;                // Visual height in cells
    int footprintW = 0;           // Grid occupation width (0 = use gridW)
    int footprintH = 0;           // Grid occupation height (0 = use gridH)
    PlacementMode mode = PlacementMode::ClickToPlace;
    bool hasDirection = false;    // R-key rotation applies
    bool isAnimated = false;      // GridSystem should animate this

    // Footprint helpers — footprint is anchored at the bottom of the visual.
    int getFootprintW() const { return footprintW > 0 ? footprintW : gridW; }
    int getFootprintH() const { return footprintH > 0 ? footprintH : gridH; }
    bool hasOverflow() const { return getFootprintH() < gridH; }
    int overflowH() const { return gridH - getFootprintH(); }
};

struct BuildingRegistry : public pg::Registry<BuildingDef>
{
    void addBuilding(const BuildingDef& def)
    {
        add(def);
    }
};

BuildingRegistry createDefaultBuildingRegistry();
