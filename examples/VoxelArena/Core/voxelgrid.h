#pragma once

#include <cstdint>
#include <vector>

#include "glm/glm.hpp"

namespace arena
{
    using BlockId = uint16_t;

    /** Block 0 is always air. */
    constexpr BlockId AIR = 0;

    /** What the mesher and the physics need to know about a block, indexed by runtime BlockId. */
    struct BlockLook
    {
        glm::vec3 color = glm::vec3(1.0f);  // 0..1
        bool solid = true;                   // collides
        bool visible = true;                 // gets faces
        float colorNoise = 0.0f;             // 0..1, per-block brightness jitter so flat ground reads as voxels
        float hardness = 50.0f;              // hit points: damage needed to break it
        bool breakable = true;               // false: nothing breaks it, and it anchors what rests on it
        bool flammable = false;              // fire sets it alight: it burns away, setting its neighbours alight
        float weakness[4] = {1, 1, 1, 1};    // terrain damage scale by element (none, fire, ice, volt)
    };

    using BlockTable = std::vector<BlockLook>;

    /**
     * A fixed-size voxel volume stored as one flat array, one BlockId per cell.
     * Seed of the future chunk: a chunk will be this with a world offset and a palette.
     *
     * Index order is x fastest, then z, then y (a horizontal slice is contiguous).
     */
    class VoxelGrid
    {
    public:
        VoxelGrid() = default;
        VoxelGrid(int width, int height, int depth);

        int width() const  { return w; }
        int height() const { return h; }
        int depth() const  { return d; }

        bool inBounds(int x, int y, int z) const
        {
            return x >= 0 and y >= 0 and z >= 0 and x < w and y < h and z < d;
        }

        BlockId get(int x, int y, int z) const
        {
            return inBounds(x, y, z) ? cells[index(x, y, z)] : AIR;
        }

        /** Returns true when the cell changed. Bumps revision() so the renderer re-meshes. */
        bool set(int x, int y, int z, BlockId id);

        void fill(const glm::ivec3& from, const glm::ivec3& to, BlockId id);

        void clear();

        /** Changes on every edit. Compare with a stored value to know if a re-mesh is due. */
        uint64_t revision() const { return rev; }

        /**
         * Outside the grid the arena is closed: the floor and the four sides are solid,
         * the sky is open. Models use outsideSolid = false (nothing around them).
         */
        bool outsideSolid = true;

        bool isSolid(const BlockTable& table, int x, int y, int z) const
        {
            if (not inBounds(x, y, z))
                return outsideSolid and y < h;

            BlockId id = cells[index(x, y, z)];
            return id < table.size() ? table[id].solid : (id != AIR);
        }

        const std::vector<BlockId>& raw() const { return cells; }

        /** When set, every changed cell is appended here (flat index, new block): what a server sends. */
        std::vector<std::pair<uint32_t, BlockId>>* changeLog = nullptr;

        /** Flat index of a cell (inBounds must hold). */
        size_t index(int x, int y, int z) const
        {
            return static_cast<size_t>(x) + static_cast<size_t>(w) * (static_cast<size_t>(z) + static_cast<size_t>(d) * static_cast<size_t>(y));
        }

    private:
        int w = 0, h = 0, d = 0;
        std::vector<BlockId> cells;
        uint64_t rev = 0;
    };

    /** Result of a voxel ray cast (DDA). */
    struct RayHit
    {
        bool hit = false;
        glm::ivec3 cell = glm::ivec3(0);
        glm::ivec3 normal = glm::ivec3(0);  // face that was entered
        float distance = 0.0f;
    };

    /** Walks the grid from origin along dir (normalized) up to maxDist and stops at the first solid cell. */
    RayHit raycast(const VoxelGrid& grid, const BlockTable& table, const glm::vec3& origin, const glm::vec3& dir, float maxDist);
}
