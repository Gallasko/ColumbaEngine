#include "buildingregistry.h"

void BuildingRegistry::addBuilding(const BuildingDef& def)
{
    buildings.push_back(def);
}

const BuildingDef* BuildingRegistry::findByTileId(uint16_t tileId) const
{
    for (const auto& def : buildings)
    {
        if (def.tileId == tileId)
            return &def;
    }
    return nullptr;
}

BuildingRegistry createDefaultRegistry()
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

    // Slot 1 (key 2): Furnace (2x3)
    reg.addBuilding({
        5,
        "Furnace",
        "Stone_Furnace",                               // idle atlas
        {200.0f, 100.0f, 60.0f, 255.0f},
        2, 3,                                          // 2x3 matches idle sprite (32x48)
        PlacementMode::ClickToPlace,
        false,
        false                                          // animation managed by CraftingSystem
    });

    // Slot 2 (key 3): Assembler (2x3)
    reg.addBuilding({
        6,
        "Assembler",
        "Assembler_Machine_1",                         // idle atlas
        {120.0f, 80.0f, 180.0f, 255.0f},
        2, 3,                                          // 2x3 matches sprite (32x48)
        PlacementMode::ClickToPlace,
        false,
        false                                          // animation managed by CraftingSystem
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

    // Slot 4 (key 5): Inserter arm (1x1)
    reg.addBuilding({
        8,                                             // tileId
        "Inserter",                                    // name
        "Robotic_Arms_1",                              // textureName
        {220.0f, 160.0f, 60.0f, 255.0f},              // color (orange fallback)
        1, 1,                                          // 1x1
        PlacementMode::ClickToPlace,
        true,                                          // hasDirection (R to rotate)
        false                                          // isAnimated (managed by InserterSystem)
    });

    // Slot 5 (key 6): Storage (1x1)
    reg.addBuilding({
        9,                                             // tileId
        "Storage",                                     // name
        "Crate",                                       // textureName
        {160.0f, 120.0f, 80.0f, 255.0f},              // color (brown fallback)
        1, 1,                                          // 1x1
        PlacementMode::ClickToPlace,
        false,                                         // hasDirection
        false                                          // isAnimated
    });

    return reg;
}
