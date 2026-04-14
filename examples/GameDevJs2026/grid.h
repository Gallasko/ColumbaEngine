#pragma once

#include <array>
#include <vector>
#include <string>
#include <cstdint>
#include <utility>

struct CellData
{
    uint16_t tileId = 0;    // 0 = empty
    uint64_t entityId = 0;  // ECS entity id, 0 = no entity
    uint8_t  direction = 0; // 0-3 rotation for directional buildings
    int8_t   ownerX = 0;    // For multi-cell: grid X of the top-left owner cell
    int8_t   ownerY = 0;    // For multi-cell: grid Y of the top-left owner cell
    bool     isOwner = true; // True if this is the primary cell (holds the entity)
};

struct GridLayer
{
    static constexpr int WIDTH = 32;
    static constexpr int HEIGHT = 32;

    std::string name;
    float zIndex = 0.0f;
    std::array<std::array<CellData, WIDTH>, HEIGHT> cells = {};

    GridLayer(const std::string& name, float zIndex)
        : name(name), zIndex(zIndex) {}
};

struct Grid
{
    static constexpr int TILE_SIZE = 16;
    static constexpr int WIDTH = 32;
    static constexpr int HEIGHT = 32;

    std::vector<GridLayer> layers;

    size_t addLayer(const std::string& name, float zIndex)
    {
        layers.emplace_back(name, zIndex);
        return layers.size() - 1;
    }

    GridLayer& getLayer(size_t index) { return layers[index]; }
    const GridLayer& getLayer(size_t index) const { return layers[index]; }

    CellData& getCell(size_t layer, int x, int y)
    {
        return layers[layer].cells[y][x];
    }

    const CellData& getCell(size_t layer, int x, int y) const
    {
        return layers[layer].cells[y][x];
    }

    bool isInBounds(int x, int y) const
    {
        return x >= 0 and x < WIDTH and y >= 0 and y < HEIGHT;
    }

    // Convert world pixel coordinates to grid cell coordinates
    std::pair<int, int> worldToGrid(float worldX, float worldY) const
    {
        return {
            static_cast<int>(worldX) / TILE_SIZE,
            static_cast<int>(worldY) / TILE_SIZE
        };
    }

    // Convert grid cell coordinates to world pixel coordinates (top-left corner)
    std::pair<float, float> gridToWorld(int gridX, int gridY) const
    {
        return {
            static_cast<float>(gridX * TILE_SIZE),
            static_cast<float>(gridY * TILE_SIZE)
        };
    }

    float totalWidth() const { return WIDTH * TILE_SIZE; }
    float totalHeight() const { return HEIGHT * TILE_SIZE; }
};
