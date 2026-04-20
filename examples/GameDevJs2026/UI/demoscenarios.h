#pragma once

#include <cstdint>
#include <vector>
#include <string>

// A tile placed in the demo mini-grid
struct DemoTileDef
{
    int x, y;
    uint16_t tileId;
    uint8_t direction;       // 0=R, 1=D, 2=L, 3=U
};

// An item that participates in the demo simulation
struct DemoItemSpawn
{
    uint16_t itemId;         // Item type to spawn
    int spawnX, spawnY;      // Where items originate (belt start, miner output, etc.)
    int spawnIntervalTicks;  // How often to spawn a new item (0 = only at reset)
};

struct DemoScenario
{
    uint16_t tileId;         // Which building this demo is for
    const char* title;
    const char* description;
    int gridW, gridH;        // Dimensions of the mini-grid
    std::vector<DemoTileDef> tiles;
    std::vector<DemoItemSpawn> itemSpawns;
};

// ─────────────────────────────────────────────────────────────────────────────
// Scenario definitions
// ─────────────────────────────────────────────────────────────────────────────

inline DemoScenario createInserterDemo()
{
    DemoScenario s;
    s.tileId = 8;
    s.title = "Inserter";
    s.description = "Picks from behind, drops in front. R to rotate.";
    s.gridW = 7;
    s.gridH = 4;

    // Layout (row 1): belt belt belt INSERTER [furnace occupies 2x3]
    // Belt going right at y=1
    s.tiles.push_back({1, 1, 4, 0}); // belt right
    s.tiles.push_back({2, 1, 4, 0}); // belt right
    s.tiles.push_back({3, 1, 4, 0}); // belt right
    // Inserter at (4,1) facing right -> picks from belt at (3,1), drops at (5,1)
    s.tiles.push_back({4, 1, 8, 0}); // inserter facing right
    // Furnace at (5, 0) occupying (5,0)-(6,2)
    s.tiles.push_back({5, 0, 5, 0}); // furnace

    // Items spawn on first belt cell
    s.itemSpawns.push_back({1, 1, 1, 20}); // iron ore, spawn every 20 ticks

    return s;
}

inline DemoScenario createConveyorDemo()
{
    DemoScenario s;
    s.tileId = 4;
    s.title = "Conveyor Belt";
    s.description = "Items travel in arrow direction. Drag to place.";
    s.gridW = 7;
    s.gridH = 3;

    // A straight belt line going right at y=1
    for (int x = 0; x < 7; ++x)
        s.tiles.push_back({x, 1, 4, 0}); // belt right

    // Items spawn at start
    s.itemSpawns.push_back({1, 0, 1, 15}); // iron ore at belt start

    return s;
}

inline DemoScenario createMinerDemo()
{
    DemoScenario s;
    s.tileId = 7;
    s.title = "Miner";
    s.description = "Place on ore. Use inserter to move output to belt.";
    s.gridW = 8;
    s.gridH = 4;

    // Miner at (1,0) occupying (1,0)-(2,2)
    s.tiles.push_back({1, 0, 7, 0}); // miner

    // Inserter at (3,1) facing right -> picks from miner area (2,1), drops on belt (4,1)
    s.tiles.push_back({3, 1, 8, 0}); // inserter facing right

    // Belt going right starting at (4,1)
    s.tiles.push_back({4, 1, 4, 0});
    s.tiles.push_back({5, 1, 4, 0});
    s.tiles.push_back({6, 1, 4, 0});
    s.tiles.push_back({7, 1, 4, 0});

    // No item spawns — miner produces output internally
    return s;
}

inline DemoScenario createFurnaceDemo()
{
    DemoScenario s;
    s.tileId = 5;
    s.title = "Furnace";
    s.description = "Smelts ore into bars. Use inserters to automate.";
    s.gridW = 9;
    s.gridH = 4;

    // Input belt going right at y=1
    s.tiles.push_back({0, 1, 4, 0});
    s.tiles.push_back({1, 1, 4, 0});
    s.tiles.push_back({2, 1, 4, 0});

    // Input inserter at (3,1) facing right -> picks from belt(2,1), drops into furnace
    s.tiles.push_back({3, 1, 8, 0});

    // Furnace at (4,0) occupying (4,0)-(5,2)
    s.tiles.push_back({4, 0, 5, 0});

    // Output inserter at (6,1) facing right -> picks from furnace, drops onto belt
    s.tiles.push_back({6, 1, 8, 0});

    // Output belt going right
    s.tiles.push_back({7, 1, 4, 0});
    s.tiles.push_back({8, 1, 4, 0});

    // Items spawn on input belt
    s.itemSpawns.push_back({1, 0, 1, 20}); // iron ore

    return s;
}

inline DemoScenario createAssemblerDemo()
{
    DemoScenario s;
    s.tileId = 6;
    s.title = "Assembler";
    s.description = "Crafts intermediate goods. Use inserters to automate.";
    s.gridW = 9;
    s.gridH = 4;

    // Input belt going right at y=1
    s.tiles.push_back({0, 1, 4, 0});
    s.tiles.push_back({1, 1, 4, 0});
    s.tiles.push_back({2, 1, 4, 0});

    // Input inserter at (3,1) facing right -> picks from belt(2,1), drops into assembler
    s.tiles.push_back({3, 1, 8, 0});

    // Assembler at (4,0) occupying (4,0)-(5,2)
    s.tiles.push_back({4, 0, 6, 0});

    // Output inserter at (6,1) facing right -> picks from assembler, drops onto belt
    s.tiles.push_back({6, 1, 8, 0});

    // Output belt going right
    s.tiles.push_back({7, 1, 4, 0});
    s.tiles.push_back({8, 1, 4, 0});

    // Items spawn on input belt: iron plates
    s.itemSpawns.push_back({5, 0, 1, 20}); // iron plate

    return s;
}
