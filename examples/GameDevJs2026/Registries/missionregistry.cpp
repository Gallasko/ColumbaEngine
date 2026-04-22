#include "missionregistry.h"

MissionRegistry createDefaultMissionRegistry()
{
    MissionRegistry reg;

    // Tier 1: Iron Delivery (delivery mission — no core cost, no timer)
    reg.addMission({
        "Iron Delivery",
        "Deliver iron plates to a depot.",
        0, 0,
        {{35, 3}}, // reward: 3 Tickets
        "", "completed_scout",
        {{5, 10}} // deliver: 10 Iron Plates
    });

    // Tier 2: Mineral Expedition
    reg.addMission({
        "Mineral Expedition",
        "Explore mineral-rich caves for resources.",
        1, 60000, // 1 core, 60s
        {{35, 5}, {2, 10}, {3, 5}}, // 5 Tickets, 10 Copper Ore, 5 Coal
        "completed_scout", "completed_mineral"
    });

    // Tier 3: Deep Mining
    reg.addMission({
        "Deep Mining",
        "Venture deep underground for valuable materials.",
        2, 90000, // 2 cores, 90s
        {{35, 8}, {5, 5}, {4, 5}}, // 8 Tickets, 5 Iron Plate, 5 Stone
        "completed_mineral", "completed_deep"
    });

    // Tier 4: Factory Salvage
    reg.addMission({
        "Factory Salvage",
        "Salvage parts from an abandoned factory.",
        2, 120000, // 2 cores, 120s
        {{35, 12}, {9, 2}, {7, 3}}, // 12 Tickets, 2 Circuit, 3 Iron Gear
        "completed_deep", "completed_salvage"
    });

    // Tier 5: Frontier Exploration
    reg.addMission({
        "Frontier Exploration",
        "Push into unknown territory. High reward.",
        3, 180000, // 3 cores, 180s
        {{35, 20}}, // 20 Tickets
        "completed_salvage", "completed_frontier"
    });

    return reg;
}
