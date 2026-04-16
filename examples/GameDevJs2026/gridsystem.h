#pragma once

#include <algorithm>

#include "Systems/basicsystems.h"
#include "2D/simple2dobject.h"
#include "2D/texture.h"

#include "grid.h"
#include "buildingregistry.h"
#include "canvasgenerator.h"
#include "terrain.h"

using namespace pg;

// Viewport index for the game camera (FollowCamera2D registers as cameraList[0] = viewport 1)
static constexpr size_t GAME_VIEWPORT = 1;

// Direction offsets: RIGHT=0, DOWN=1, LEFT=2, UP=3
static constexpr int DIR_DX[4] = { 1, 0, -1, 0 };
static constexpr int DIR_DY[4] = { 0, 1, 0, -1 };

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
    uint16_t tileId;
};

struct BuildingRemovedEvent
{
    int x, y;
    uint16_t tileId;
};

// Pick the correct LINE_*_N variant based on direction and neighbor connectivity.
// For RIGHT/DOWN: _0 = ender at back, _2 = ender at front
// For UP/LEFT:    _0 = ender at front, _2 = ender at back (inverted)
// _1 = middle (connected both)
static size_t resolveLineTileVariant(uint8_t exitDir, bool connectedBack, bool connectedFront)
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

class GridSystem : public System<InitSys, Listener<TickEvent>>
{
public:
    GridSystem(BuildingRegistry* registry) : registry(registry) {}

    virtual std::string getSystemName() const override { return "Grid System"; }

    void init() override
    {
        // Create default layers
        terrainLayer = grid.addLayer("terrain", 1.0f);
        buildingLayer = grid.addLayer("buildings", 2.0f);
        itemLayer = grid.addLayer("items", 3.0f);

        // Procedurally generate the starting canvas and render its terrain
        generateAndRenderTerrain();
    }

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
    void regenerateTerrain(uint32_t seed)
    {
        printf("GridSystem: regenerating terrain with seed 0x%08X\n", seed);

        for (uint64_t id : bgEntities)
            ecsRef->removeEntity(id);
        bgEntities.clear();

        generateAndRenderTerrain(seed);
    }

    virtual void onEvent(const TickEvent& event) override
    {
        deltaTime += event.tick / 1000.0f;
        animElapsed += event.tick;
    }

    void execute() override
    {
        if (deltaTime <= 0.0f)
            return;

        // Advance conveyor animation globally — all conveyors stay in sync
        if (conveyors.empty())
        {
            // Reset accumulator so first placement doesn't fast-forward
            animElapsed = 0;
        }
        else if (animElapsed >= FRAME_DURATION_MS)
        {
            animElapsed -= FRAME_DURATION_MS;
            currentFrame = (currentFrame + 1) % NUM_ANIM_FRAMES;

            for (const auto& conv : conveyors)
            {
                auto ent = ecsRef->getEntity(conv.entityId);
                if (not ent)
                    continue;

                auto tex = ent->get<Texture2DComponent>();
                size_t frameIndex = conv.tileIndex * NUM_ANIM_FRAMES + currentFrame;
                tex->setTexture("Conveyor_Belt." + std::to_string(frameIndex));
            }
        }

        deltaTime = 0.0f;
    }

    // Place a building on the grid using its BuildingDef
    void placeBuilding(size_t layer, int x, int y, const BuildingDef& def, size_t direction, size_t conveyorTileIndex = LINE_RIGHT_1, size_t enterDir = SIZE_MAX)
    {
        // Check all cells are free, in bounds, and not on blocking terrain.
        for (int dy = 0; dy < def.gridH; ++dy)
            for (int dx = 0; dx < def.gridW; ++dx)
                if (not grid.isInBounds(x + dx, y + dy) or
                    grid.getCell(layer, x + dx, y + dy).tileId != 0 or
                    isBlockingTerrain(getTerrainAt(x + dx, y + dy)))
                    return;

        auto [worldX, worldY] = grid.gridToWorld(x, y);
        float z = grid.getLayer(layer).zIndex;
        uint64_t entityId = 0;

        if (def.isAnimated and not def.textureName.empty())
        {
            // Animated sprite (conveyor belts)
            size_t frameIndex = conveyorTileIndex * NUM_ANIM_FRAMES + currentFrame;
            std::string texName = def.textureName + "." + std::to_string(frameIndex);

            auto tex = make2DTexture(ecsRef,
                static_cast<float>(Grid::TILE_SIZE * def.gridW),
                static_cast<float>(Grid::TILE_SIZE * def.gridH),
                texName);

            auto pos = tex.get<PositionComponent>();
            pos->setX(worldX);
            pos->setY(worldY);
            pos->setZ(z);

            tex.get<Texture2DComponent>()->setViewport(GAME_VIEWPORT);
            entityId = tex.entity->id;

            conveyors.push_back({entityId, conveyorTileIndex});
        }
        else if (not def.textureName.empty())
        {
            // Static texture (use first atlas frame)
            std::string texName = def.textureName + ".0";
            auto tex = make2DTexture(ecsRef,
                static_cast<float>(Grid::TILE_SIZE * def.gridW),
                static_cast<float>(Grid::TILE_SIZE * def.gridH),
                texName);

            auto pos = tex.get<PositionComponent>();
            pos->setX(worldX);
            pos->setY(worldY);
            pos->setZ(z);

            tex.get<Texture2DComponent>()->setViewport(GAME_VIEWPORT);
            entityId = tex.entity->id;
        }
        else
        {
            // Colored square placeholder
            auto shape = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f, def.color);
            auto pos = shape.get<PositionComponent>();
            pos->setX(worldX);
            pos->setY(worldY);
            pos->setZ(z);
            pos->setWidth(static_cast<float>(Grid::TILE_SIZE * def.gridW));
            pos->setHeight(static_cast<float>(Grid::TILE_SIZE * def.gridH));

            shape.get<Simple2DObject>()->setViewport(GAME_VIEWPORT);
            entityId = shape.entity->id;
        }

        // Mark all cells in the footprint
        uint8_t actualEnterDir = (enterDir == SIZE_MAX) ? static_cast<uint8_t>(direction) : static_cast<uint8_t>(enterDir);
        for (int dy = 0; dy < def.gridH; ++dy)
        {
            for (int dx = 0; dx < def.gridW; ++dx)
            {
                auto& cell = grid.getCell(layer, x + dx, y + dy);
                cell.tileId = def.tileId;
                cell.direction = static_cast<uint8_t>(direction);
                cell.enterDirection = actualEnterDir;
                cell.ownerX = static_cast<int8_t>(x);
                cell.ownerY = static_cast<int8_t>(y);
                cell.isOwner = (dx == 0 and dy == 0);
                cell.entityId = (dx == 0 and dy == 0) ? entityId : 0;
            }
        }

        sendEvent(BuildingPlacedEvent{x, y, def.tileId});
    }

    // Remove a building from the grid (handles multi-cell)
    void removeBuilding(size_t layer, int x, int y)
    {
        if (not grid.isInBounds(x, y))
            return;

        auto& cell = grid.getCell(layer, x, y);
        if (cell.tileId == 0)
            return;

        // Find the owner cell
        int ox = cell.isOwner ? x : static_cast<int>(cell.ownerX);
        int oy = cell.isOwner ? y : static_cast<int>(cell.ownerY);
        auto& ownerCell = grid.getCell(layer, ox, oy);

        // Remove entity
        if (ownerCell.entityId != 0)
        {
            removeConveyorEntry(ownerCell.entityId);
            ecsRef->removeEntity(ownerCell.entityId);
        }

        // Look up building def to know the footprint
        const BuildingDef* def = registry->findByTileId(ownerCell.tileId);
        uint16_t savedTileId = ownerCell.tileId;
        int w = def ? def->gridW : 1;
        int h = def ? def->gridH : 1;

        // Clear all cells in the footprint
        for (int dy = 0; dy < h; ++dy)
        {
            for (int dx = 0; dx < w; ++dx)
            {
                if (grid.isInBounds(ox + dx, oy + dy))
                    grid.getCell(layer, ox + dx, oy + dy) = CellData{};
            }
        }

        sendEvent(BuildingRemovedEvent{ox, oy, savedTileId});

        // Update adjacent belts that may now be disconnected
        for (int dy = 0; dy < h; ++dy)
            for (int dx = 0; dx < w; ++dx)
                updateNeighborBelts(layer, ox + dx, oy + dy);
    }

    // Legacy: place a single-cell tile (kept for backward compat with setCell/clearCell calls)
    void setCell(size_t layer, int x, int y, uint16_t tileId, size_t conveyorTileIndex = LINE_RIGHT_1)
    {
        if (not grid.isInBounds(x, y))
            return;

        auto& cell = grid.getCell(layer, x, y);

        // Remove existing entity if any
        if (cell.entityId != 0)
        {
            removeConveyorEntry(cell.entityId);
            ecsRef->removeEntity(cell.entityId);
            cell.entityId = 0;
        }

        cell.tileId = tileId;

        if (tileId == 0)
            return;

        // Create visual entity for this cell
        auto [worldX, worldY] = grid.gridToWorld(x, y);
        float z = grid.getLayer(layer).zIndex;

        if (tileId == 4) // Conveyor belt - use sprite
        {
            createConveyorEntity(cell, worldX, worldY, z, conveyorTileIndex);
        }
        else // Other tiles - use colored squares
        {
            constant::Vector4D color = getTileColor(tileId);

            auto shape = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f, color);
            auto pos = shape.get<PositionComponent>();
            pos->setX(worldX);
            pos->setY(worldY);
            pos->setZ(z);
            pos->setWidth(static_cast<float>(Grid::TILE_SIZE));
            pos->setHeight(static_cast<float>(Grid::TILE_SIZE));

            shape.get<Simple2DObject>()->setViewport(GAME_VIEWPORT);

            cell.entityId = shape.entity->id;
        }
    }

    // Remove a tile from the grid
    void clearCell(size_t layer, int x, int y)
    {
        setCell(layer, x, y, 0);
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
    bool isNeighborConnected(size_t layer, int x, int y, uint8_t checkDir) const
    {
        int nx = x + DIR_DX[checkDir];
        int ny = y + DIR_DY[checkDir];

        if (not grid.isInBounds(nx, ny))
            return false;

        const auto& neighbor = grid.getCell(layer, nx, ny);
        if (neighbor.tileId == 0)
            return false;

        // Non-belt buildings (machines) are always connectable
        if (neighbor.tileId != 4)
            return true;

        // Belt neighbor: the neighbor connects on the side facing us if either
        // its enter or exit direction points towards/away from us on that side.
        // The side of the neighbor facing us is the opposite of checkDir.
        uint8_t sideOfNeighborFacingUs = (checkDir + 2) % 4;

        // Neighbor's exit side faces us
        if (neighbor.direction == sideOfNeighborFacingUs)
            return true;

        // Neighbor's enter side faces us (enter direction is where items come FROM,
        // so the enter side is the opposite of enterDirection)
        uint8_t neighborEnterSide = (neighbor.enterDirection + 2) % 4;
        if (neighborEnterSide == sideOfNeighborFacingUs)
            return true;

        return false;
    }

    // Update a conveyor's tile index and refresh its texture immediately
    void updateConveyorTileIndex(size_t layer, int x, int y, size_t newTileIndex)
    {
        if (not grid.isInBounds(x, y))
            return;

        auto& cell = grid.getCell(layer, x, y);
        if (cell.tileId != 4 or cell.entityId == 0)
            return;

        for (auto& conv : conveyors)
        {
            if (conv.entityId == cell.entityId)
            {
                conv.tileIndex = newTileIndex;

                auto ent = ecsRef->getEntity(conv.entityId);
                if (ent)
                {
                    size_t frameIndex = newTileIndex * NUM_ANIM_FRAMES + currentFrame;
                    ent->get<Texture2DComponent>()->setTexture(
                        "Conveyor_Belt." + std::to_string(frameIndex));
                }
                break;
            }
        }
    }

    // Re-evaluate a belt cell's variant based on its current neighbors. Skips corners.
    void resolveAndUpdateBelt(size_t layer, int x, int y)
    {
        if (not grid.isInBounds(x, y))
            return;

        const auto& cell = grid.getCell(layer, x, y);
        if (cell.tileId != 4 or cell.entityId == 0)
            return;

        // Check if this is a corner (tileIndex 0-7) — corners are always connected, skip
        for (const auto& conv : conveyors)
        {
            if (conv.entityId == cell.entityId)
            {
                if (conv.tileIndex <= CORNER_CCW_1_1)
                    return;
                break;
            }
        }

        uint8_t exitDir = cell.direction;
        uint8_t backDir = (exitDir + 2) % 4;

        bool connectedFront = isNeighborConnected(layer, x, y, exitDir);
        bool connectedBack  = isNeighborConnected(layer, x, y, backDir);

        size_t newTileIndex = resolveLineTileVariant(exitDir, connectedBack, connectedFront);
        updateConveyorTileIndex(layer, x, y, newTileIndex);
    }

    // Update all 4 neighbors of (x,y) — call after placing or removing a building
    void updateNeighborBelts(size_t layer, int x, int y)
    {
        for (int dir = 0; dir < 4; ++dir)
        {
            resolveAndUpdateBelt(layer, x + DIR_DX[dir], y + DIR_DY[dir]);
        }
    }

private:
    static constexpr size_t NUM_ANIM_FRAMES = 8;
    static constexpr size_t FRAME_DURATION_MS = 100;

    // Procedurally generate the canvas terrain (grass / ores / trees / rocks) and render it.
    void generateAndRenderTerrain(uint32_t seed = 0xC0FFEEu)
    {
        // All terrain stays inside integer z planes that sit below the building
        // layer (z=2) and item layer (z=3). Within the same z-plane, render
        // order controls stacking — we draw ground first, then grass, then
        // single-tile overlays, and finally trees in a separate pass.
        constexpr float GROUND_Z  = 0.0f;
        constexpr float GRASS_Z   = 1.0f;
        constexpr float OVERLAY_Z = 1.0f;
        constexpr float TREE_Z    = 1.0f;

        GenerationParams params;
        params.seed = seed;

        auto gen = CanvasGenerator::generate(params);
        terrainGrid = gen.terrain;
        orePatches = gen.orePatches;
        treeInstances = gen.trees;

        // Per-cell pass: ground base + grass + single-tile overlays (ores, rocks).
        // Trees are rendered afterwards from the instance list so each occupies
        // exactly one 2x3 sprite anchored to its footprint.
        for (int y = 0; y < Grid::HEIGHT; ++y)
        {
            for (int x = 0; x < Grid::WIDTH; ++x)
            {
                auto [worldX, worldY] = grid.gridToWorld(x, y);

                // Always draw a ground base so gaps between non-grass tiles look natural.
                spawnTextureTile("Ground.0", worldX, worldY, GROUND_Z,
                    Grid::TILE_SIZE, Grid::TILE_SIZE);

                TerrainType t = terrainGrid[y][x];

                // Grass is painted under grass, tree, and rock cells so that
                // transparent pixels in the tree/rock sprites show grass (not
                // dirt) behind them, and so the environment looks uniformly
                // green between decorations. Near ore patches the grass
                // auto-tiles to a dirt edge via grassAutotileFrame. When the
                // cell is encircled by ore (or otherwise unrepresentable by
                // the autotile), grassAutotileFrame returns the GRASS_AS_DIRT
                // sentinel and we skip the overlay so the Ground.0 base shows
                // through as plain dirt.
                if (t == TerrainType::Grass
                 or t == TerrainType::Tree
                 or t == TerrainType::Rock)
                {
                    uint16_t frame = grassAutotileFrame(x, y);
                    if (frame != GRASS_AS_DIRT)
                    {
                        spawnTextureTile("Environment_Tileset." + std::to_string(frame),
                            worldX, worldY, GRASS_Z, Grid::TILE_SIZE, Grid::TILE_SIZE);
                    }
                }

                if (isOre(t))
                {
                    uint16_t frame = oreAutotileFrame(x, y, t);
                    spawnTextureTile("Environment_Tileset." + std::to_string(frame),
                        worldX, worldY, OVERLAY_Z, Grid::TILE_SIZE, Grid::TILE_SIZE);
                }
                else if (t == TerrainType::Rock)
                {
                    spawnTextureTile("Rock_Tile.0", worldX, worldY, OVERLAY_Z,
                        Grid::TILE_SIZE, Grid::TILE_SIZE);
                }
                // Grass handled above; trees are rendered in the pass below.
            }
        }

        // Whole-struct tree pass: one 2x3 sprite per TreeInstance, anchored flush with
        // its top-left cell so trees never clip past the canvas edges.
        for (const auto& tree : treeInstances)
        {
            auto [tx, ty] = grid.gridToWorld(tree.anchorX, tree.anchorY);
            spawnTextureTile("Tree.0", tx, ty, TREE_Z,
                static_cast<float>(Grid::TILE_SIZE * TREE_W),
                static_cast<float>(Grid::TILE_SIZE * TREE_H));
        }
    }

    // Sentinel returned by grassAutotileFrame() when the cell is encircled by
    // ore (or in any configuration the 9-tile grass autotile can't represent).
    // The caller skips drawing a grass overlay for that cell so the Ground.0
    // base tile shows through as plain dirt.
    static constexpr uint16_t GRASS_AS_DIRT = 0xFFFF;

    // Pick the autotile frame for a grass cell from Environment_Tileset.
    // The first 3 rows of the tileset hold the grass-to-dirt autotile:
    //   - Cols 0-2 are the standard 3x3 block (TL/T/TR, L/C/R, BL/B/BR)
    //     where any non-grass-like cardinal neighbor produces a dirt edge.
    //   - Cols 3-4 of rows 0 and 2 are the 4 interior corners used when all
    //     four cardinal neighbors are grass but a diagonal neighbor is ore
    //     (producing a small dirt poke in that corner).
    // Grass / Tree / Rock cells all count as "grass" for autotiling because
    // they share the same grass background. Out-of-bounds is treated as grass
    // so the canvas edge doesn't spawn an unwanted dirt transition.
    //
    // Returns GRASS_AS_DIRT when the cell is encircled by ore or otherwise
    // can't be represented by the 9-tile + inner-corner set (3+ ore sides,
    // or opposite-pair ore sides forming a grass strip).
    uint16_t grassAutotileFrame(int x, int y) const
    {
        constexpr uint16_t ATLAS_COLS = 12;
        constexpr uint16_t GRASS_BASE = 13;
        static constexpr uint16_t GRASS_VARIATIONS[] = { 10, 11, 22, 23 };
        constexpr uint32_t VARIATION_EVERY = 12;

        auto isGrassLike = [&](int nx, int ny)
        {
            if (not grid.isInBounds(nx, ny))
                return true;
            return not isOre(terrainGrid[ny][nx]);
        };

        bool hasN = isGrassLike(x,     y - 1);
        bool hasS = isGrassLike(x,     y + 1);
        bool hasW = isGrassLike(x - 1, y);
        bool hasE = isGrassLike(x + 1, y);

        // The 9-tile autotile can only express 0/1/2 ore sides, and when
        // there are exactly 2 ore sides they must be adjacent (forming a
        // corner). Anything else has no valid tile, so render as dirt.
        int oreSides = 0;
        if (not hasN) ++oreSides;
        if (not hasS) ++oreSides;
        if (not hasW) ++oreSides;
        if (not hasE) ++oreSides;

        const bool oppositeOnly = (oreSides == 2) and
            (((not hasN) and (not hasS)) or ((not hasW) and (not hasE)));

        if (oreSides >= 3 or oppositeOnly)
            return GRASS_AS_DIRT;

        int localRow;
        if (not hasN and hasS)      localRow = 0;
        else if (hasN and not hasS) localRow = 2;
        else                        localRow = 1;

        int localCol;
        if (not hasW and hasE)      localCol = 0;
        else if (hasW and not hasE) localCol = 2;
        else                        localCol = 1;

        // If we landed on the center cell (all 4 cardinals are grass-like),
        // check diagonals for interior-corner transitions. Priority order
        // NW > NE > SW > SE is arbitrary but deterministic.
        if (localRow == 1 and localCol == 1 and hasN and hasS and hasE and hasW)
        {
            bool hasNW = isGrassLike(x - 1, y - 1);
            bool hasNE = isGrassLike(x + 1, y - 1);
            bool hasSW = isGrassLike(x - 1, y + 1);
            bool hasSE = isGrassLike(x + 1, y + 1);

            // Interior corner frames: cols 3-4, rows 0 and 2.
            // The tileset's top row of inner corners shows the SOUTH notches
            // (dirt bleeds in from the bottom), and the bottom row shows the
            // NORTH notches:
            //   frame  3 (col 3 row 0) -> SE inner corner
            //   frame  4 (col 4 row 0) -> SW inner corner
            //   frame 27 (col 3 row 2) -> NE inner corner
            //   frame 28 (col 4 row 2) -> NW inner corner
            if (not hasSE) return 3;
            if (not hasSW) return 4;
            if (not hasNE) return 27;
            if (not hasNW) return 28;

            // Fully surrounded by grass: use the base fill tile, sprinkling in
            // rare variations so the interior doesn't look flat.
            uint32_t h = static_cast<uint32_t>(x) * 73856093u
                       ^ static_cast<uint32_t>(y) * 19349663u;
            if (h % VARIATION_EVERY == 0)
                return GRASS_VARIATIONS[(h / VARIATION_EVERY)
                    % (sizeof(GRASS_VARIATIONS) / sizeof(GRASS_VARIATIONS[0]))];
            return GRASS_BASE;
        }

        return static_cast<uint16_t>(localRow * ATLAS_COLS + localCol);
    }

    // Pick the autotile frame for an ore cell from Environment_Tileset.
    // The last 3 rows of the tileset (rows 18-20) hold 4 ore-type 3x3 blocks in
    // the order rocks / coal / iron / copper. Each block uses the standard
    // 3x3 layout where the 8 border tiles represent neighbor presence and the
    // center (row 1, col 1) is the solid fill used when the tile is surrounded
    // by same-ore neighbors on all sides or is isolated.
    uint16_t oreAutotileFrame(int x, int y, TerrainType t) const
    {
        // Atlas is row-major with 12 cols (see application.cpp registration).
        constexpr uint16_t ATLAS_COLS = 12;

        // Top-left frame index of each ore's 3x3 block.
        uint16_t baseOffset;
        switch (t)
        {
            case TerrainType::OreStone:  baseOffset = 18 * ATLAS_COLS + 0; break; // 216
            case TerrainType::OreCoal:   baseOffset = 18 * ATLAS_COLS + 3; break; // 219
            case TerrainType::OreIron:   baseOffset = 18 * ATLAS_COLS + 6; break; // 222
            case TerrainType::OreCopper: baseOffset = 18 * ATLAS_COLS + 9; break; // 225
            default:                     baseOffset = 18 * ATLAS_COLS + 6; break;
        }

        auto sameOre = [&](int nx, int ny)
        {
            if (not grid.isInBounds(nx, ny))
                return false;
            return terrainGrid[ny][nx] == t;
        };

        bool hasN = sameOre(x,     y - 1);
        bool hasS = sameOre(x,     y + 1);
        bool hasW = sameOre(x - 1, y);
        bool hasE = sameOre(x + 1, y);

        // Pick the sub-row: top row if the cell has open sky above, bottom row
        // if it has open sky below, middle otherwise (includes the "fully
        // surrounded" and "fully isolated" cases, both of which render as the
        // solid center tile).
        int localRow;
        if (not hasN and hasS)      localRow = 0;
        else if (hasN and not hasS) localRow = 2;
        else                        localRow = 1;

        int localCol;
        if (not hasW and hasE)      localCol = 0;
        else if (hasW and not hasE) localCol = 2;
        else                        localCol = 1;

        return static_cast<uint16_t>(baseOffset + localRow * ATLAS_COLS + localCol);
    }

    // Helper: spawn a textured tile at a world position and record it for cleanup.
    void spawnTextureTile(const std::string& texName, float worldX, float worldY, float z,
                          float width, float height)
    {
        auto tex = make2DTexture(ecsRef, width, height, texName);
        auto pos = tex.get<PositionComponent>();
        pos->setX(worldX);
        pos->setY(worldY);
        pos->setZ(z);
        tex.get<Texture2DComponent>()->setViewport(GAME_VIEWPORT);
        bgEntities.push_back(tex.entity->id);
    }

    void createConveyorEntity(CellData& cell, float worldX, float worldY, float z, size_t tileIndex)
    {
        // Use the current global animation frame so new conveyors are immediately in sync
        size_t frameIndex = tileIndex * NUM_ANIM_FRAMES + currentFrame;
        std::string texName = "Conveyor_Belt." + std::to_string(frameIndex);

        auto tex = make2DTexture(ecsRef,
            static_cast<float>(Grid::TILE_SIZE),
            static_cast<float>(Grid::TILE_SIZE),
            texName);

        auto pos = tex.get<PositionComponent>();
        pos->setX(worldX);
        pos->setY(worldY);
        pos->setZ(z);

        tex.get<Texture2DComponent>()->setViewport(GAME_VIEWPORT);

        cell.entityId = tex.entity->id;

        conveyors.push_back({cell.entityId, tileIndex});
    }

    void removeConveyorEntry(uint64_t entityId)
    {
        conveyors.erase(
            std::remove_if(conveyors.begin(), conveyors.end(),
                [entityId](const ConveyorEntry& e) { return e.entityId == entityId; }),
            conveyors.end());
    }

    constant::Vector4D getTileColor(uint16_t tileId) const
    {
        switch (tileId)
        {
            case 1: return {80.0f, 140.0f, 80.0f, 255.0f};   // Grass / terrain
            case 2: return {140.0f, 140.0f, 160.0f, 255.0f};  // Stone / building
            case 3: return {200.0f, 160.0f, 60.0f, 255.0f};   // Item / resource
            case 4: return {100.0f, 160.0f, 220.0f, 255.0f};  // Conveyor
            case 5: return {200.0f, 100.0f, 60.0f, 255.0f};   // Furnace
            case 6: return {120.0f, 80.0f, 180.0f, 255.0f};   // Assembler
            case 7: return {180.0f, 140.0f, 60.0f, 255.0f};   // Miner
            default: return {200.0f, 200.0f, 200.0f, 255.0f}; // Generic
        }
    }

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
