#pragma once

#include "Systems/basicsystems.h"

#include "grid.h"
#include "buildingregistry.h"
#include "canvasgenerator.h"
#include "terrain.h"
#include "saveserialization.h"

using namespace pg;

// Viewport index for the game camera (FollowCamera2D registers as cameraList[0] = viewport 1)
inline constexpr size_t GAME_VIEWPORT = 1;

// Direction offsets: RIGHT=0, DOWN=1, LEFT=2, UP=3
inline constexpr int DIR_DX[4] = { 1, 0, -1, 0 };
inline constexpr int DIR_DY[4] = { 0, 1, 0, -1 };

// Tile index in the atlas JSON frame ordering (tileIndex * 8 + block = frame index)
enum ConveyorTileIndex : size_t
{
    CORNER_CW_0_0  = 0,
    CORNER_CW_1_0  = 1,
    CORNER_CW_0_1  = 2,
    CORNER_CW_1_1  = 3,
    CORNER_CCW_0_0 = 4,
    CORNER_CCW_1_0 = 5,
    CORNER_CCW_0_1 = 6,
    CORNER_CCW_1_1 = 7,
    LINE_UP_0      = 8,
    LINE_UP_1      = 9,
    LINE_UP_2      = 10,
    LINE_DOWN_0    = 11,
    LINE_DOWN_1    = 12,
    LINE_DOWN_2    = 13,
    LINE_LEFT_0    = 14,
    LINE_LEFT_1    = 15,
    LINE_LEFT_2    = 16,
    LINE_LEFT_3    = 17,
    LINE_RIGHT_0   = 18,
    LINE_RIGHT_1   = 19,
    LINE_RIGHT_2   = 20,
    LINE_RIGHT_3   = 21,
};

struct ConveyorEntry
{
    uint64_t entityId = 0;
    size_t tileIndex = 0;
};

struct BuildingPlacedEvent
{
    int x, y;
    std::string tileName;
};

struct BuildingRemovedEvent
{
    int x, y;
    std::string tileName;
};

// Pick the correct LINE_*_N variant based on direction and neighbor connectivity.
// For RIGHT/DOWN: _0 = ender at back, _2 = ender at front
// For UP/LEFT:    _0 = ender at front, _2 = ender at back (inverted)
// _1 = middle (connected both)
inline size_t resolveLineTileVariant(uint8_t exitDir, bool connectedBack, bool connectedFront)
{
    switch (exitDir)
    {
        case 0: // RIGHT: _0=ender back, _1=middle, _2=ender front
            if (connectedBack and connectedFront) return LINE_RIGHT_1;
            if (connectedBack)                    return LINE_RIGHT_2;
            if (connectedFront)                   return LINE_RIGHT_0;
            return LINE_RIGHT_0; // standalone: show back ender

        case 1: // DOWN: _0=ender back, _1=middle, _2=ender front
            if (connectedBack and connectedFront) return LINE_DOWN_1;
            if (connectedBack)                    return LINE_DOWN_2;
            if (connectedFront)                   return LINE_DOWN_0;
            return LINE_DOWN_0;

        case 2: // LEFT (inverted): _0=ender front, _1=middle, _2=ender back
            if (connectedBack and connectedFront) return LINE_LEFT_1;
            if (connectedBack)                    return LINE_LEFT_0;
            if (connectedFront)                   return LINE_LEFT_2;
            return LINE_LEFT_2; // standalone: show back ender

        case 3: // UP (inverted): _0=ender front, _1=middle, _2=ender back
            if (connectedBack and connectedFront) return LINE_UP_1;
            if (connectedBack)                    return LINE_UP_0;
            if (connectedFront)                   return LINE_UP_2;
            return LINE_UP_0;
    }
    return LINE_RIGHT_1;
}

class GridSystem : public System<InitSys, Listener<TickEvent>, SaveSys>
{
public:
    GridSystem(BuildingRegistry* registry) : registry(registry) {}

    virtual std::string getSystemName() const override { return "Grid System"; }

    void init() override;

    // SaveSys
    virtual void save(Archive& archive) override;
    virtual void load(const UnserializedObject& serializedString) override;

    // Query the terrain type at a given grid cell (returns None for out-of-bounds).
    TerrainType getTerrainAt(int x, int y) const
    {
        if (not grid.isInBounds(x, y))
            return TerrainType::None;
        return terrainGrid[y][x];
    }

    const std::vector<OrePatch>& getOrePatches() const { return orePatches; }

    // Destroy all procgen terrain entities and regenerate the canvas with a new seed.
    // Player-placed buildings are untouched (they live on the building layer with
    // their own entity ids, not in bgEntities). Useful as a dev tool to eyeball seeds.
    void regenerateTerrain(uint32_t seed);

    virtual void onEvent(const TickEvent& event) override
    {
        deltaTime += event.tick / 1000.0f;
        animElapsed += event.tick;
    }

    void execute() override;

    // Place a building on the grid using its BuildingDef
    void placeBuilding(size_t layer, int x, int y, const BuildingDef& def, size_t direction, size_t conveyorTileIndex = LINE_RIGHT_1, size_t enterDir = SIZE_MAX);

    // Remove a building from the grid (handles multi-cell)
    void removeBuilding(size_t layer, int x, int y);

    // Legacy: place a single-cell tile (kept for backward compat with setCell/clearCell calls)
    void setCell(size_t layer, int x, int y, const std::string& tileName, size_t conveyorTileIndex = LINE_RIGHT_1);

    // Remove a tile from the grid
    void clearCell(size_t layer, int x, int y)
    {
        setCell(layer, x, y, "");
    }

    // Get tile at position
    const CellData& getCell(size_t layer, int x, int y) const
    {
        return grid.getCell(layer, x, y);
    }

    Grid& getGrid() { return grid; }
    const Grid& getGrid() const { return grid; }

    size_t getTerrainLayer() const { return terrainLayer; }
    size_t getBuildingLayer() const { return buildingLayer; }
    size_t getItemLayer() const { return itemLayer; }

    size_t getCurrentAnimFrame() const { return currentFrame; }

    const BuildingRegistry* getRegistry() const { return registry; }

    // Check if the neighbor in direction checkDir from (x,y) is connectable.
    // For belt neighbors: checks if the neighbor connects on the side facing us
    // (using both enter and exit directions to handle corners correctly).
    // For non-belt buildings (machines): always connectable.
    bool isNeighborConnected(size_t layer, int x, int y, uint8_t checkDir) const;

    // Update a conveyor's tile index and refresh its texture immediately
    void updateConveyorTileIndex(size_t layer, int x, int y, size_t newTileIndex);

    // Re-evaluate a belt cell's variant based on its current neighbors. Skips corners.
    void resolveAndUpdateBelt(size_t layer, int x, int y);

    // Update all 4 neighbors of (x,y) — call after placing or removing a building
    void updateNeighborBelts(size_t layer, int x, int y);

private:
    static constexpr size_t NUM_ANIM_FRAMES = 8;
    static constexpr size_t FRAME_DURATION_MS = 100;

    // Sentinel returned by grassAutotileFrame() when the cell is encircled by
    // ore (or in any configuration the 9-tile grass autotile can't represent).
    // The caller skips drawing a grass overlay for that cell so the Ground.0
    // base tile shows through as plain dirt.
    static constexpr uint16_t GRASS_AS_DIRT = 0xFFFF;

    // Shape of the per-cell boolean map used by grassAutotileFrame's two-pass
    // logic. `true` means the grass cell at (x,y) should render as plain dirt
    // (its grass overlay is skipped). Built by the fixpoint pass in
    // generateAndRenderTerrain before the render loop runs.
    using RenderAsDirtGrid = std::array<std::array<bool, Grid::WIDTH>, Grid::HEIGHT>;

    // Procedurally generate the canvas terrain (grass / ores / trees / rocks) and render it.
    void generateAndRenderTerrain(uint32_t seed = 0xC0FFEEu);

    // Render terrain visuals from the current terrainGrid (shared by both
    // fresh generation and save-file restoration).
    void renderTerrain();

    // Restore a building from save data (like placeBuilding but skips
    // BuildingPlacedEvent since other systems haven't been created yet).
    void restoreBuilding(size_t layer, int x, int y, const BuildingDef& def,
                         size_t direction, size_t conveyorTileIndex, size_t enterDir);

    // Pick the autotile frame for a grass cell from Environment_Tileset.
    uint16_t grassAutotileFrame(int x, int y, const RenderAsDirtGrid& renderAsDirt) const;

    // Pick the autotile frame for an ore cell from Environment_Tileset.
    uint16_t oreAutotileFrame(int x, int y, TerrainType t) const;

    // Helper: spawn a textured tile at a world position and record it for cleanup.
    void spawnTextureTile(const std::string& texName, float worldX, float worldY, float z,
                          float width, float height);

    void createConveyorEntity(CellData& cell, float worldX, float worldY, float z, size_t tileIndex);

    void removeConveyorEntry(uint64_t entityId);

    constant::Vector4D getTileColor(const std::string& tileName) const;

    BuildingRegistry* registry = nullptr;

    Grid grid;
    float deltaTime = 0.0f;

    // Conveyor animation state (global — all conveyors animate in sync)
    size_t animElapsed = 0;
    size_t currentFrame = 0;
    std::vector<ConveyorEntry> conveyors;

    size_t terrainLayer = 0;
    size_t buildingLayer = 0;
    size_t itemLayer = 0;

    std::vector<uint64_t> bgEntities;

    // Procedurally generated terrain data (parallel to the terrain grid layer).
    TerrainGrid                terrainGrid{};
    std::vector<OrePatch>      orePatches;
    std::vector<TreeInstance>  treeInstances;

};
