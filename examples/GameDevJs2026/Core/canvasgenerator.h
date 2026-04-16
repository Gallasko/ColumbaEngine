#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "grid.h"
#include "terrain.h"

// A single ore patch on a canvas: center + radius + which ore type.
struct OrePatch
{
    TerrainType ore;
    int         centerX;
    int         centerY;
    int         radius;
    uint16_t    tileCount = 0;
};

// A tree instance occupies a 2x3 footprint anchored at (anchorX, anchorY) (top-left).
// All 6 cells are marked TerrainType::Tree in the terrain grid; the sprite is rendered
// exactly once from this instance list.
struct TreeInstance
{
    int anchorX;
    int anchorY;
};

// Tree footprint dimensions (must match Tree.png aspect: 32x48 = 2 tiles wide, 3 tall).
inline constexpr int TREE_W = 2;
inline constexpr int TREE_H = 3;

// Parameters driving canvas generation. All randomness derives from `seed`.
struct GenerationParams
{
    uint32_t                  seed          = 0;
    int                       minOrePatches = 1;
    int                       maxOrePatches = 3;
    int                       patchRadiusMin = 2;
    int                       patchRadiusMax = 4;
    float                     treeDensity   = 0.02f;
    float                     rockDensity   = 0.02f;
    int                       edgeMargin    = 3;
    std::vector<TerrainType>  allowedOres   = {
        TerrainType::OreIron,
        TerrainType::OreCopper,
        TerrainType::OreCoal,
        TerrainType::OreStone
    };
};

// Flat 32x32 terrain grid for one canvas.
using TerrainGrid = std::array<std::array<TerrainType, GridLayer::WIDTH>, GridLayer::HEIGHT>;

struct CanvasGenResult
{
    TerrainGrid                terrain{};
    std::vector<OrePatch>      orePatches;
    std::vector<TreeInstance>  trees;
};

namespace canvasgen_detail
{
    // Derive a per-canvas seed from world seed and canvas coord so previews stay stable.
    uint32_t deriveSeed(uint32_t worldSeed, int cx, int cy);
}

class CanvasGenerator
{
public:
    static CanvasGenResult generate(const GenerationParams& params);
};
