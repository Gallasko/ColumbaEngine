#include "voxelgrid.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace arena
{
    VoxelGrid::VoxelGrid(int width, int height, int depth) : w(width), h(height), d(depth)
    {
        cells.assign(static_cast<size_t>(w) * static_cast<size_t>(h) * static_cast<size_t>(d), AIR);
    }

    bool VoxelGrid::set(int x, int y, int z, BlockId id)
    {
        if (not inBounds(x, y, z))
            return false;

        auto& cell = cells[index(x, y, z)];

        if (cell == id)
            return false;

        cell = id;
        ++rev;

        if (changeLog)
            changeLog->emplace_back(static_cast<uint32_t>(index(x, y, z)), id);

        return true;
    }

    void VoxelGrid::fill(const glm::ivec3& from, const glm::ivec3& to, BlockId id)
    {
        const glm::ivec3 lo = glm::max(glm::min(from, to), glm::ivec3(0));
        const glm::ivec3 hi = glm::min(glm::max(from, to), glm::ivec3(w - 1, h - 1, d - 1));

        for (int y = lo.y; y <= hi.y; ++y)
            for (int z = lo.z; z <= hi.z; ++z)
                for (int x = lo.x; x <= hi.x; ++x)
                {
                    BlockId& c = cells[index(x, y, z)];

                    if (changeLog and c != id)
                        changeLog->emplace_back(static_cast<uint32_t>(index(x, y, z)), id);

                    c = id;
                }

        ++rev;
    }

    void VoxelGrid::clear()
    {
        std::fill(cells.begin(), cells.end(), AIR);
        ++rev;
    }

    RayHit raycast(const VoxelGrid& grid, const BlockTable& table, const glm::vec3& origin, const glm::vec3& dir, float maxDist)
    {
        RayHit result;

        // Far beyond any arena. A ray from further (an aim sent over the network) finds nothing: past
        // 2^24 a float no longer moves by one cell, and the walk below would never end
        constexpr float FARTHEST = 4096.0f;

        const bool finite = std::isfinite(origin.x) and std::isfinite(origin.y) and std::isfinite(origin.z) and
                            std::isfinite(dir.x) and std::isfinite(dir.y) and std::isfinite(dir.z) and std::isfinite(maxDist);

        if (not finite or std::fabs(origin.x) > FARTHEST or std::fabs(origin.y) > FARTHEST or std::fabs(origin.z) > FARTHEST)
            return result;

        maxDist = std::min(maxDist, FARTHEST);

        // A cell boundary is crossed at most every 1/sqrt(3) along a unit ray: more steps than this is a bug
        const int maxSteps = static_cast<int>(maxDist * 2.0f * std::max(1.0f, glm::length(dir))) + 8;

        glm::ivec3 cell = glm::ivec3(glm::floor(origin));
        const glm::ivec3 step(dir.x > 0 ? 1 : -1, dir.y > 0 ? 1 : -1, dir.z > 0 ? 1 : -1);

        constexpr float inf = std::numeric_limits<float>::infinity();

        glm::vec3 tDelta, tMax;

        for (int i = 0; i < 3; ++i)
        {
            if (std::fabs(dir[i]) < 1e-8f)
            {
                tDelta[i] = inf;
                tMax[i] = inf;
            }
            else
            {
                tDelta[i] = std::fabs(1.0f / dir[i]);
                const float boundary = step[i] > 0 ? static_cast<float>(cell[i] + 1) : static_cast<float>(cell[i]);
                tMax[i] = (boundary - origin[i]) / dir[i];
            }
        }

        // Starting inside a solid cell counts as a hit at distance 0
        if (grid.isSolid(table, cell.x, cell.y, cell.z))
        {
            result.hit = true;
            result.cell = cell;
            return result;
        }

        float t = 0.0f;

        for (int n = 0; t <= maxDist and n < maxSteps; ++n)
        {
            int axis = 0;
            if (tMax.y < tMax[axis]) axis = 1;
            if (tMax.z < tMax[axis]) axis = 2;

            t = tMax[axis];

            if (t > maxDist)
                break;

            cell[axis] += step[axis];
            tMax[axis] += tDelta[axis];

            if (grid.isSolid(table, cell.x, cell.y, cell.z))
            {
                result.hit = true;
                result.cell = cell;
                result.normal = glm::ivec3(0);
                result.normal[axis] = -step[axis];
                result.distance = t;
                return result;
            }
        }

        return result;
    }
}
