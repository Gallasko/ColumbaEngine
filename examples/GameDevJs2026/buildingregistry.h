#pragma once

#include <string>
#include <vector>
#include <cstdint>

#include "pgconstant.h"

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

struct BuildingRegistry
{
    std::vector<BuildingDef> buildings;

    void addBuilding(const BuildingDef& def)
    {
        buildings.push_back(def);
    }

    const BuildingDef& get(size_t index) const
    {
        return buildings[index];
    }

    size_t count() const { return buildings.size(); }

    // Find a building def by tileId, returns nullptr if not found
    const BuildingDef* findByTileId(uint16_t tileId) const
    {
        for (const auto& def : buildings)
        {
            if (def.tileId == tileId)
                return &def;
        }
        return nullptr;
    }
};

inline BuildingRegistry createDefaultRegistry()
{
    BuildingRegistry reg;

    // Slot 0 (key 1): Conveyor belt
    reg.addBuilding({
        4,                                             // tileId
        "Conveyor",                                    // name
        "Conveyor_Belt",                               // textureName
        {100.0f, 160.0f, 220.0f, 255.0f},             // color
        1, 1,                                          // 1x1
        PlacementMode::LineDrag,                       // drag to draw path
        true,                                          // hasDirection
        true                                           // isAnimated
    });

    // Slot 1 (key 2): Furnace placeholder
    reg.addBuilding({
        5,
        "Furnace",
        "",
        {200.0f, 100.0f, 60.0f, 255.0f},
        1, 1,
        PlacementMode::ClickToPlace,
        false,
        false
    });

    // Slot 2 (key 3): Assembler placeholder (2x2)
    reg.addBuilding({
        6,
        "Assembler",
        "",
        {120.0f, 80.0f, 180.0f, 255.0f},
        2, 2,
        PlacementMode::ClickToPlace,
        false,
        false
    });

    // Slot 3 (key 4): Miner (2x3)
    reg.addBuilding({
        7,                                             // tileId
        "Miner",                                       // name
        "Miner_Machine_1",                             // textureName (idle atlas)
        {180.0f, 140.0f, 60.0f, 255.0f},              // color (fallback)
        2, 3,                                          // 2x3
        PlacementMode::ClickToPlace,
        false,                                         // hasDirection
        false                                          // isAnimated (managed by MinerSystem)
    });

    return reg;
}
