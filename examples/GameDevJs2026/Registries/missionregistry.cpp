#include "missionregistry.h"

MissionRegistry createDefaultMissionRegistry()
{
    MissionRegistry reg;

    // ===== Progression missions (delivery-based, unlock recipes) =====

    // Item IDs: 1=Iron Ore, 2=Copper Ore, 3=Coal, 4=Stone, 5=Iron Plate
    //           7=Iron Gear, 8=Copper Wire, 9=Circuit, 15=Wood, 35=Ticket

    // Mission 0: First Steps — unlocks Stone Pickaxe recipe
    reg.addMission({
        "First Steps",
        "Gather basic resources and deliver them.",
        "Stone Pickaxe",
        0, 0,
        {{35, 1}}, // reward: 1 Ticket
        "", "mission_tools",
        {{15, 3}, {4, 3}}, // deliver: 3 Wood + 3 Stone
        false, false // consumeItems=false (keep items), repeatable=false
    });

    // Mission 1: Stone Masonry — unlocks Furnace recipe
    reg.addMission({
        "Stone Masonry",
        "Collect stone for building a smelter.",
        "Furnace",
        0, 0,
        {{35, 1}}, // reward: 1 Ticket
        "mission_tools", "mission_furnace",
        {{4, 10}}, // deliver: 10 Stone
        false, false // consumeItems=false, repeatable=false
    });

    // Mission 2: Ore Discovery — unlocks furnace smelting recipes
    reg.addMission({
        "Ore Discovery",
        "Mine iron ore to unlock smelting.",
        "Smelting",
        0, 0,
        {{35, 2}}, // reward: 2 Tickets
        "mission_furnace", "mission_smelting",
        {{1, 5}}, // deliver: 5 Iron Ore
        false, false // consumeItems=false, repeatable=false
    });

    // Mission 3: Metal Working — unlocks Iron Gear, Copper Wire, Storage
    reg.addMission({
        "Metal Working",
        "Produce iron plates to advance crafting.",
        "Gears & Wire",
        0, 0,
        {{35, 2}}, // reward: 2 Tickets
        "mission_smelting", "mission_metals",
        {{5, 10}}, // deliver: 10 Iron Plate
        true, false // consumeItems=true, repeatable=false
    });

    // Mission 4: Mechanical Parts — unlocks Assembler, Conveyor Belt, Iron Pickaxe
    reg.addMission({
        "Mechanical Parts",
        "Craft intermediate components.",
        "Assembler",
        0, 0,
        {{35, 3}}, // reward: 3 Tickets
        "mission_metals", "mission_mechanical",
        {{7, 5}, {8, 5}}, // deliver: 5 Iron Gear + 5 Copper Wire
        true, false // consumeItems=true, repeatable=false
    });

    // Mission 5: Scaling Up — unlocks Miner, Inserter
    reg.addMission({
        "Scaling Up",
        "Scale up production for automation.",
        "Miner & Inserter",
        0, 0,
        {{35, 3}}, // reward: 3 Tickets
        "mission_mechanical", "mission_automation",
        {{5, 10}, {7, 5}}, // deliver: 10 Iron Plate + 5 Iron Gear
        true, false // consumeItems=true, repeatable=false
    });

    // Mission 6: Electronics — unlocks Circuit (hand), Depot
    reg.addMission({
        "Electronics",
        "Produce circuits to unlock advanced tech.",
        "Circuits & Depot",
        0, 0,
        {{35, 5}}, // reward: 5 Tickets
        "mission_automation", "mission_electronics",
        {{9, 3}}, // deliver: 3 Circuit
        true, false // consumeItems=true, repeatable=false
    });

    // ===== Endgame missions (existing, gated behind progression) =====

    // Iron Delivery (repeatable, endgame idle loop)
    reg.addMission({
        "Iron Delivery",
        "Deliver iron plates to a depot.",
        "",
        0, 0,
        {{35, 1}}, // reward: 1 Ticket
        "mission_electronics", "completed_scout",
        {{5, 10}}, // deliver: 10 Iron Plates
        true, true, MissionCategory::Endgame
    });

    // World Expedition
    reg.addMission({
        "World Expedition",
        "Unlock map extensions.",
        "",
        1, 60000, // 1 core, 60s
        {{35, 5}, {2, 10}, {3, 5}}, // 5 Tickets, 10 Copper Ore, 5 Coal
        "completed_scout", "completed_mineral",
        {}, true, false, MissionCategory::Endgame
    });

    // Deep Mining
    reg.addMission({
        "Deep Mining",
        "Venture deep underground for valuable materials.",
        "",
        2, 90000, // 2 cores, 90s
        {{35, 8}, {5, 5}, {4, 5}}, // 8 Tickets, 5 Iron Plate, 5 Stone
        "completed_mineral", "completed_deep",
        {}, true, false, MissionCategory::Endgame
    });

    // Factory Salvage
    reg.addMission({
        "Factory Salvage",
        "Salvage parts from an abandoned factory.",
        "",
        2, 120000, // 2 cores, 120s
        {{35, 12}, {9, 2}, {7, 3}}, // 12 Tickets, 2 Circuit, 3 Iron Gear
        "completed_deep", "completed_salvage",
        {}, true, false, MissionCategory::Endgame
    });

    // Frontier Exploration
    reg.addMission({
        "Frontier Exploration",
        "Push into unknown territory. High reward.",
        "",
        3, 180000, // 3 cores, 180s
        {{35, 20}}, // 20 Tickets
        "completed_salvage", "completed_frontier",
        {}, true, false, MissionCategory::Endgame
    });

    return reg;
}
