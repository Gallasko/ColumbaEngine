#pragma once

#include "Systems/basicsystems.h"
#include "2D/simple2dobject.h"

#include "grid.h"

using namespace pg;

// Viewport index for the game camera (FollowCamera2D registers as cameraList[0] = viewport 1)
static constexpr size_t GAME_VIEWPORT = 1;

class GridSystem : public System<InitSys, Listener<TickEvent>>
{
public:
    GridSystem() {}

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
    }

    void execute() override
    {
        if (deltaTime <= 0.0f)
            return;

        // Grid logic updates will go here (conveyor movement, etc.)

        deltaTime = 0.0f;
    }

    // Place a tile on the grid
    void setCell(size_t layer, int x, int y, uint16_t tileId)
    {
        if (not grid.isInBounds(x, y))
            return;

        auto& cell = grid.getCell(layer, x, y);

        // Remove existing entity if any
        if (cell.entityId != 0)
        {
            ecsRef->removeEntity(cell.entityId);
            cell.entityId = 0;
        }

        cell.tileId = tileId;

        if (tileId == 0)
            return;

        // Create visual entity for this cell
        auto [worldX, worldY] = grid.gridToWorld(x, y);
        float z = grid.getLayer(layer).zIndex;

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

private:
    void createGridBackground()
    {
        constexpr float BG_Z = 0.0f; // Behind all layers (z/100 in shader, must be >= 0)

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

    constant::Vector4D getTileColor(uint16_t tileId) const
    {
        switch (tileId)
        {
            case 1: return {80.0f, 140.0f, 80.0f, 255.0f};   // Grass / terrain
            case 2: return {140.0f, 140.0f, 160.0f, 255.0f};  // Stone / building
            case 3: return {200.0f, 160.0f, 60.0f, 255.0f};   // Item / resource
            case 4: return {100.0f, 160.0f, 220.0f, 255.0f};  // Conveyor
            default: return {200.0f, 200.0f, 200.0f, 255.0f}; // Generic
        }
    }

    Grid grid;
    float deltaTime = 0.0f;

    size_t terrainLayer = 0;
    size_t buildingLayer = 0;
    size_t itemLayer = 0;

    std::vector<uint64_t> bgEntities;
};
