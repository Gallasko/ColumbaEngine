#pragma once

#include <vector>
#include <string>
#include <variant>

#include "ECS/entityref.h"
#include "glm/glm.hpp"

namespace pg
{
    // --------------------------------------------------------------------------
    // Editor tools
    // --------------------------------------------------------------------------
    enum class EditorTool
    {
        Place,      // left-click places block, right-click removes
        ColorPick,  // left-click samples color from existing voxel
    };

    // --------------------------------------------------------------------------
    // Palette — default preset solid RGBA colors (0..255 per channel)
    // --------------------------------------------------------------------------
    inline std::vector<glm::vec4> defaultPalette()
    {
        return {
            { 255,  64,  64, 255 }, // 0  red
            { 255, 128,  64, 255 }, // 1  orange
            { 255, 224,  64, 255 }, // 2  yellow
            { 128, 224,  64, 255 }, // 3  lime
            {  64, 200,  64, 255 }, // 4  green
            {  64, 200, 128, 255 }, // 5  spring green
            {  64, 200, 200, 255 }, // 6  cyan
            {  64, 128, 255, 255 }, // 7  sky blue
            {  64,  64, 255, 255 }, // 8  blue
            { 128,  64, 255, 255 }, // 9  purple
            { 255,  64, 255, 255 }, // 10 magenta
            { 255,  64, 128, 255 }, // 11 pink
            { 220, 220, 220, 255 }, // 12 light gray
            { 140, 140, 140, 255 }, // 13 gray
            {  60,  60,  60, 255 }, // 14 dark gray
            { 255, 255, 255, 255 }, // 15 white
        };
    }

    // --------------------------------------------------------------------------
    // Layer — a named set of cells; can be hidden
    // --------------------------------------------------------------------------
    struct Layer
    {
        std::string name;
        bool        visible = true;

        // When the layer is hidden, we detach VoxelComponent from its entities.
        // The original data is saved here so it can be restored on show.
        struct HiddenCell
        {
            glm::ivec3 cell;
            glm::vec4  color;
        };
        std::vector<HiddenCell> hiddenData; // non-empty only while layer is hidden
    };

    // --------------------------------------------------------------------------
    // Canvas — fixed 3-D grid of blocks
    // --------------------------------------------------------------------------
    struct Canvas
    {
        int W, H, D;

        std::vector<glm::vec4> palette;
        std::vector<Layer>     layers;
        std::vector<EntityRef> cells;     // W*H*D, empty() == unoccupied
        std::vector<int>       cellLayer; // W*H*D, -1 == unoccupied

        Canvas(int W, int H, int D)
            : W(W), H(H), D(D)
            , palette(defaultPalette())
            , cells(static_cast<size_t>(W * H * D))
            , cellLayer(static_cast<size_t>(W * H * D), -1)
        {}

        bool inBounds(int x, int y, int z) const noexcept
        {
            return x >= 0 && x < W
                && y >= 0 && y < H
                && z >= 0 && z < D;
        }

        int index(int x, int y, int z) const noexcept
        {
            return x + W * (y + H * z);
        }

        EntityRef& at(int x, int y, int z)             { return cells[static_cast<size_t>(index(x, y, z))]; }
        const EntityRef& at(int x, int y, int z) const { return cells[static_cast<size_t>(index(x, y, z))]; }

        int  layerAt(int x, int y, int z) const noexcept { return cellLayer[static_cast<size_t>(index(x, y, z))]; }
        void setLayer(int x, int y, int z, int layer) noexcept { cellLayer[static_cast<size_t>(index(x, y, z))] = layer; }
    };

    // --------------------------------------------------------------------------
    // Commands for undo/redo
    // --------------------------------------------------------------------------
    struct PlaceCmd
    {
        glm::ivec3 pos;
        glm::vec4  color;
        int        layer;
    };

    struct RemoveCmd
    {
        glm::ivec3 pos;
        glm::vec4  color;
        int        layer;
    };

    using EditorCmd = std::variant<PlaceCmd, RemoveCmd>;

} // namespace pg
