#include "buildingregistry.h"

BuildingRegistry createDefaultBuildingRegistry()
{
    BuildingRegistry reg;

    // Slot 0: Conveyor belt
    reg.addBuilding({
        "Conveyor",                                    // name
        "Conveyor_Belt",                               // textureName
        {100.0f, 160.0f, 220.0f, 255.0f},             // color
        1, 1,                                          // gridW, gridH (visual)
        0, 0,                                          // footprintW, footprintH (0 = same as visual)
        PlacementMode::LineDrag,                       // drag to draw path
        true,                                          // hasDirection
        true                                           // isAnimated
    });

    // Slot 1: Furnace (2x4 visual, 2x2 footprint at bottom)
    reg.addBuilding({
        "Furnace",
        "Stone_Furnace",                               // idle atlas
        {200.0f, 100.0f, 60.0f, 255.0f},
        2, 4,                                          // 2x4 visual (32x64 sprite, matches active animation)
        0, 2,                                          // footprint: 2x2 (bottom 2 rows)
        PlacementMode::ClickToPlace,
        false,
        false                                          // animation managed by CraftingSystem
    });

    // Slot 2: Assembler (2x3)
    reg.addBuilding({
        "Assembler",
        "Assembler_Machine_1",                         // idle atlas
        {120.0f, 80.0f, 180.0f, 255.0f},
        2, 3,                                          // 2x3 visual (32x48 sprite)
        0, 0,                                          // footprint: same as visual
        PlacementMode::ClickToPlace,
        false,
        false                                          // animation managed by CraftingSystem
    });

    // Slot 3: Miner (2x3)
    reg.addBuilding({
        "Miner",                                       // name
        "Miner_Machine_1",                             // textureName (idle atlas)
        {180.0f, 140.0f, 60.0f, 255.0f},              // color (fallback)
        2, 3,                                          // 2x3
        0, 0,                                          // footprint: same as visual
        PlacementMode::ClickToPlace,
        false,                                         // hasDirection
        false                                          // isAnimated (managed by MinerSystem)
    });

    // Slot 4: Inserter arm (1x1)
    reg.addBuilding({
        "Inserter",                                    // name
        "Robotic_Arms_1",                              // textureName
        {220.0f, 160.0f, 60.0f, 255.0f},              // color (orange fallback)
        1, 1,                                          // 1x1
        0, 0,                                          // footprint: same as visual
        PlacementMode::ClickToPlace,
        true,                                          // hasDirection (R to rotate)
        false                                          // isAnimated (managed by InserterSystem)
    });

    // Slot 5: Storage (1x1)
    reg.addBuilding({
        "Storage",                                     // name
        "Crate",                                       // textureName
        {160.0f, 120.0f, 80.0f, 255.0f},              // color (brown fallback)
        1, 1,                                          // 1x1
        0, 0,                                          // footprint: same as visual
        PlacementMode::ClickToPlace,
        false,                                         // hasDirection
        false                                          // isAnimated
    });

    // Slot 6: Depot (2x2)
    reg.addBuilding({
        "Depot",                                       // name
        "Assembler_Machine_4",                         // textureName (assembler v4 sprite)
        {60.0f, 120.0f, 200.0f, 255.0f},              // color (blue fallback)
        2, 2,                                          // 2x2
        0, 0,                                          // footprint: same as visual
        PlacementMode::ClickToPlace,
        false,                                         // hasDirection
        false                                          // isAnimated
    });

    // Slot 7: Tree (2x3 visual, 2x2 footprint at bottom)
    reg.addBuilding({
        "Tree",                                        // name
        "Tree",                                        // textureName (Tree atlas)
        {60.0f, 140.0f, 60.0f, 255.0f},               // color (green fallback)
        2, 3,                                          // 2x3 visual (32x48 sprite)
        0, 2,                                          // footprint: 2x2 (bottom 2 rows)
        PlacementMode::ClickToPlace,
        false,                                         // hasDirection
        false                                          // isAnimated
    });

    return reg;
}
