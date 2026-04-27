#include "canvasgenerator.h"

#include <algorithm>
#include <cmath>

namespace
{
    // Tiny local xorshift32 RNG. Intentionally not using pg::RandomNumberGenerator
    // (a global singleton) so canvas generation is isolated and reproducible.
    struct XorShift32
    {
        uint32_t state;

        explicit XorShift32(uint32_t seed) : state(seed ? seed : 0x1u) {}

        uint32_t next()
        {
            uint32_t x = state;
            x ^= x << 13;
            x ^= x >> 17;
            x ^= x << 5;
            state = x;
            return x;
        }

        // Inclusive range.
        int rangeInt(int lo, int hi)
        {
            if (hi <= lo) return lo;
            return lo + static_cast<int>(next() % static_cast<uint32_t>(hi - lo + 1));
        }

        // [0, 1)
        float unitFloat()
        {
            return (next() & 0xFFFFFFu) / static_cast<float>(0x1000000);
        }
    };

    void paintBlob(TerrainGrid& terrain, const OrePatch& patch, XorShift32& rng)
    {
        const float r = static_cast<float>(patch.radius);
        const int minX = std::max(0, patch.centerX - patch.radius - 1);
        const int maxX = std::min(GridLayer::WIDTH  - 1, patch.centerX + patch.radius + 1);
        const int minY = std::max(0, patch.centerY - patch.radius - 1);
        const int maxY = std::min(GridLayer::HEIGHT - 1, patch.centerY + patch.radius + 1);

        for (int y = minY; y <= maxY; ++y)
        {
            for (int x = minX; x <= maxX; ++x)
            {
                float dx = static_cast<float>(x - patch.centerX);
                float dy = static_cast<float>(y - patch.centerY);
                float dist = std::sqrt(dx * dx + dy * dy);
                // Noisy threshold so patches aren't perfect circles.
                float noise = (rng.unitFloat() - 0.5f) * 1.5f;

                if (dist + noise <= r)
                    terrain[y][x] = patch.ore;
            }
        }
    }

    bool isNearOre(const TerrainGrid& terrain, int x, int y, int radius)
    {
        for (int dy = -radius; dy <= radius; ++dy)
        {
            for (int dx = -radius; dx <= radius; ++dx)
            {
                int nx = x + dx;
                int ny = y + dy;

                if (nx < 0 or ny < 0 or nx >= GridLayer::WIDTH or ny >= GridLayer::HEIGHT)
                    continue;

                if (isOre(terrain[ny][nx]))
                    return true;
            }
        }

        return false;
    }

    // Can a TREE_W x TREE_H tree fit with its top-left at (x, y)?
    // Requires in-bounds footprint, all cells being free grass, and no ore nearby.
    bool canPlaceTree(const TerrainGrid& terrain, int x, int y)
    {
        if (x < 0 or y < 0)
            return false;

        if (x + TREE_W > GridLayer::WIDTH)
            return false;

        if (y + TREE_H > GridLayer::HEIGHT)
            return false;

        for (int dy = 0; dy < TREE_H; ++dy)
        {
            for (int dx = 0; dx < TREE_W; ++dx)
            {
                if (terrain[y + dy][x + dx] != TerrainType::Grass)
                    return false;

                if (isNearOre(terrain, x + dx, y + dy, 1))
                    return false;
            }
        }

        return true;
    }
}

namespace canvasgen_detail
{
    uint32_t deriveSeed(uint32_t worldSeed, int cx, int cy)
    {
        uint32_t h = worldSeed * 2654435761u;

        h ^= static_cast<uint32_t>(cx) * 374761393u;
        h ^= static_cast<uint32_t>(cy) * 668265263u;
        h ^= h >> 13;
        h *= 1274126177u;

        return h ? h : 0x9E3779B9u;
    }
}

CanvasGenResult CanvasGenerator::generate(const GenerationParams& params)
{
    CanvasGenResult result;
    XorShift32 rng(params.seed);

    // 1. Fill with grass.
    for (int y = 0; y < GridLayer::HEIGHT; ++y)
    {
        for (int x = 0; x < GridLayer::WIDTH; ++x)
        {
            result.terrain[y][x] = TerrainType::Grass;
        }
    }

    // 2a. Place required ore patches first (guaranteed on this canvas).
    std::vector<TerrainType> pool = params.allowedOres;

    for (const auto& reqOre : params.requiredOres)
    {
        OrePatch patch;
        patch.ore     = reqOre;
        patch.radius  = rng.rangeInt(params.patchRadiusMin, params.patchRadiusMax);
        patch.centerX = rng.rangeInt(params.edgeMargin, GridLayer::WIDTH  - 1 - params.edgeMargin);
        patch.centerY = rng.rangeInt(params.edgeMargin, GridLayer::HEIGHT - 1 - params.edgeMargin);

        paintBlob(result.terrain, patch, rng);
        result.orePatches.push_back(patch);

        // Remove from pool so it won't be duplicated by random selection.
        auto it = std::find(pool.begin(), pool.end(), reqOre);

        if (it != pool.end())
            pool.erase(it);
    }

    // 2b. Fill remaining budget with random ore patches.
    int desiredCount = rng.rangeInt(params.minOrePatches, params.maxOrePatches);
    int remaining = desiredCount - static_cast<int>(params.requiredOres.size());

    if (remaining > static_cast<int>(pool.size()))
        remaining = static_cast<int>(pool.size());

    for (int i = 0; i < remaining and not pool.empty(); ++i)
    {
        int idx = rng.rangeInt(0, static_cast<int>(pool.size()) - 1);
        TerrainType ore = pool[idx];
        pool.erase(pool.begin() + idx);

        OrePatch patch;
        patch.ore     = ore;
        patch.radius  = rng.rangeInt(params.patchRadiusMin, params.patchRadiusMax);
        patch.centerX = rng.rangeInt(params.edgeMargin, GridLayer::WIDTH  - 1 - params.edgeMargin);
        patch.centerY = rng.rangeInt(params.edgeMargin, GridLayer::HEIGHT - 1 - params.edgeMargin);

        paintBlob(result.terrain, patch, rng);
        result.orePatches.push_back(patch);
    }

    // 3a. Scatter trees as whole 2x3 structures. A tree is only placed if the
    //     entire footprint is in bounds, all 6 cells are free grass, and none of
    //     them is adjacent to ore. This guarantees trees never clip off-canvas.
    for (int y = 0; y + TREE_H <= GridLayer::HEIGHT; ++y)
    {
        for (int x = 0; x + TREE_W <= GridLayer::WIDTH; ++x)
        {
            if (rng.unitFloat() >= params.treeDensity)
                continue;

            if (not canPlaceTree(result.terrain, x, y))
                continue;

            for (int dy = 0; dy < TREE_H; ++dy)
            {
                for (int dx = 0; dx < TREE_W; ++dx)
                {
                    result.terrain[y + dy][x + dx] = TerrainType::Tree;
                }
            }

            result.trees.push_back({x, y});
        }
    }

    // 3b. Scatter rocks (1x1) on remaining grass cells not adjacent to ore.
    for (int y = 0; y < GridLayer::HEIGHT; ++y)
    {
        for (int x = 0; x < GridLayer::WIDTH; ++x)
        {
            if (result.terrain[y][x] != TerrainType::Grass)
                continue;

            if (isNearOre(result.terrain, x, y, 1))
                continue;

            if (rng.unitFloat() < params.rockDensity)
                result.terrain[y][x] = TerrainType::Rock;
        }
    }

    // 4. Count tiles per patch (useful later for previews).
    for (auto& patch : result.orePatches)
    {
        int bbMin = std::max(0, patch.centerX - patch.radius - 1);
        int bbMax = std::min(GridLayer::WIDTH - 1, patch.centerX + patch.radius + 1);
        int byMin = std::max(0, patch.centerY - patch.radius - 1);
        int byMax = std::min(GridLayer::HEIGHT - 1, patch.centerY + patch.radius + 1);

        uint16_t count = 0;

        for (int y = byMin; y <= byMax; ++y)
        {
            for (int x = bbMin; x <= bbMax; ++x)
            {
                if (result.terrain[y][x] == patch.ore)
                    ++count;
            }
        }

        patch.tileCount = count;
    }

    return result;
}
