#pragma once

#include <algorithm>

#include "Systems/basicsystems.h"
#include "2D/simple2dobject.h"
#include "2D/texture.h"

#include "grid.h"
#include "buildingregistry.h"

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

// Pick the correct LINE_*_N variant based on direction and neighbor connectivity.
// For RIGHT/DOWN: _0 = ender at back, _2 = ender at front
// For UP/LEFT:    _0 = ender at front, _2 = ender at back (inverted)
// _1 = middle (connected both), _3 = standalone (LEFT/RIGHT only)
static size_t resolveLineTileVariant(uint8_t exitDir, bool connectedBack, bool connectedFront)
{
    switch (exitDir)
    {
        case 0: // RIGHT: _0=ender back, _1=middle, _2=ender front, _3=standalone
            if (connectedBack and connectedFront) return LINE_RIGHT_1;
            if (connectedBack)                    return LINE_RIGHT_2;
            if (connectedFront)                   return LINE_RIGHT_0;
            return LINE_RIGHT_3;

        case 1: // DOWN: _0=ender back, _1=middle, _2=ender front
            if (connectedBack and connectedFront) return LINE_DOWN_1;
            if (connectedBack)                    return LINE_DOWN_2;
            if (connectedFront)                   return LINE_DOWN_0;
            return LINE_DOWN_0;

        case 2: // LEFT (inverted): _0=ender front, _1=middle, _2=ender back, _3=standalone
            if (connectedBack and connectedFront) return LINE_LEFT_1;
            if (connectedBack)                    return LINE_LEFT_0;
            if (connectedFront)                   return LINE_LEFT_2;
            return LINE_LEFT_3;

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

        // Draw grid background (checkerboard)
        createGridBackground();
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
        if (animElapsed >= FRAME_DURATION_MS and not conveyors.empty())
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
        // Check all cells are free and in bounds
        for (int dy = 0; dy < def.gridH; ++dy)
            for (int dx = 0; dx < def.gridW; ++dx)
                if (not grid.isInBounds(x + dx, y + dy) or
                    grid.getCell(layer, x + dx, y + dy).tileId != 0)
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
            // Static texture
            auto tex = make2DTexture(ecsRef,
                static_cast<float>(Grid::TILE_SIZE * def.gridW),
                static_cast<float>(Grid::TILE_SIZE * def.gridH),
                def.textureName);

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

    void createGridBackground()
    {
        constexpr float BG_Z = 0.0f;

        for (int y = 0; y < Grid::HEIGHT; ++y)
        {
            for (int x = 0; x < Grid::WIDTH; ++x)
            {
                bool dark = (x + y) % 2 == 0;
                constant::Vector4D color = dark
                    ? constant::Vector4D{60.0f, 65.0f, 75.0f, 255.0f}
                    : constant::Vector4D{75.0f, 80.0f, 90.0f, 255.0f};

                auto [worldX, worldY] = grid.gridToWorld(x, y);

                auto shape = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f, color);
                auto pos = shape.get<PositionComponent>();
                pos->setX(worldX);
                pos->setY(worldY);
                pos->setZ(BG_Z);
                pos->setWidth(static_cast<float>(Grid::TILE_SIZE));
                pos->setHeight(static_cast<float>(Grid::TILE_SIZE));

                shape.get<Simple2DObject>()->setViewport(GAME_VIEWPORT);

                bgEntities.push_back(shape.entity->id);
            }
        }
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
};
