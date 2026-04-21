#include "buildingregistry.h"

BuildingRegistry createDefaultBuildingRegistry()
{
    BuildingRegistry reg;

    // Slot 0: Conveyor belt
    reg.addBuilding({
        "Conveyor",                                    // name
        "Conveyor_Belt",                               // textureName
        {100.0f, 160.0f, 220.0f, 255.0f},             // color
        1, 1,                                          // 1x1
        PlacementMode::LineDrag,                       // drag to draw path
        true,                                          // hasDirection
        true                                           // isAnimated
    });

    // Slot 1: Furnace (2x3)
    reg.addBuilding({
        "Furnace",
        "Stone_Furnace",                               // idle atlas
        {200.0f, 100.0f, 60.0f, 255.0f},
        2, 3,                                          // 2x3 matches idle sprite (32x48)
        PlacementMode::ClickToPlace,
        false,
        false                                          // animation managed by CraftingSystem
    });

    // Slot 2: Assembler (2x3)
    reg.addBuilding({
        "Assembler",
        "Assembler_Machine_1",                         // idle atlas
        {120.0f, 80.0f, 180.0f, 255.0f},
        2, 3,                                          // 2x3 matches sprite (32x48)
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
        PlacementMode::ClickToPlace,
        false,                                         // hasDirection
        false                                          // isAnimated
    });

    // Slot 6: Depot (2x2)
    reg.addBuilding({
        "Depot",                                       // name
        "",                                            // textureName (placeholder — uses color fallback)
        {60.0f, 120.0f, 200.0f, 255.0f},              // color (blue fallback)
        2, 2,                                          // 2x2
        PlacementMode::ClickToPlace,
        false,                                         // hasDirection
        false                                          // isAnimated
    });

    return reg;
}
