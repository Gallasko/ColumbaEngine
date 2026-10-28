#pragma once

#include <map>
#include <set>
#include <vector>

#include "voxelgrid.h"

namespace arena
{
    /**
     * A piece of the arena that lost its support and falls as one rigid block of cells.
     * Its cells are out of the grid while it falls and written back where it lands.
     */
    struct FallingPiece
    {
        int id = 0;
        glm::ivec3 origin = glm::ivec3(0);   // world cell of the piece's local (0, 0, 0) when it broke off
        VoxelGrid cells;                      // the piece itself (outsideSolid off), for meshing
        std::vector<glm::ivec3> occupied;     // local cells holding a block
        float drop = 0.0f;                    // metres fallen
        float speed = 0.0f;                   // m/s
        std::vector<int> crushed;             // fighters it already hit

        /** World box of a local cell at the current drop. */
        glm::vec3 cellMin(const glm::ivec3& local) const { return glm::vec3(origin + local) - glm::vec3(0, drop, 0); }
    };

    /** What happened to the terrain during a step, for the game and the fighters. */
    struct TerrainEvents
    {
        std::vector<glm::ivec3> broken;       // cells broken by damage
        std::vector<BlockId> brokenBlocks;    // what each of them was
        std::vector<int> fell;                // pieces that broke off
        std::vector<int> landed;              // pieces that landed (their cells are back in the grid)
        std::vector<glm::ivec3> filled;       // cells filled back in (pit refills, landings): fighters inside need lifting
        std::vector<glm::ivec3> ignited;      // cells set alight
    };

    /**
     * The destructible side of the arena: block damage, blocks that only last a while
     * (raised walls, dug pits that fill back in), and collapse.
     *
     * Collapse: after blocks are removed, every neighbouring group of blocks is checked for a path
     * to an anchor (the bottom layer or an unbreakable block). A group with none breaks off and falls.
     */
    class Terrain
    {
    public:
        void reset(VoxelGrid* grid, const BlockTable* table);
        void setTable(const BlockTable* table) { this->table = table; }

        /** Adds damage to a cell. Returns true when it broke. Unbreakable cells and air take none. */
        bool damage(const glm::ivec3& cell, float amount);

        /** Removes a block (no damage needed) and checks what it was holding up. */
        void remove(const glm::ivec3& cell);

        /**
         * A block that lasts lifetime seconds (0 = for good). When it runs out the cell goes back to
         * what it was: a raised wall crumbles to air, a dug cell fills back with its old block.
         */
        void placeFor(const glm::ivec3& cell, BlockId id, float lifetime);
        void digFor(const glm::ivec3& cell, float lifetime);

        void step(float dt, float time, float gravity, TerrainEvents& events);

        /** 0 (intact) to 1 (about to break), for darkening damaged blocks. */
        float damageRatio(int x, int y, int z) const;
        float damageRatioAt(size_t index) const;

        /** Network client: the damage the server reports (0..1 of the block's hardness). */
        void setDamageRatio(size_t index, float ratio);

        /** When set, every cell whose damage changed is appended here (flat index): what a server sends. */
        std::vector<uint32_t>* damageLog = nullptr;

        /** Network client: a piece the server says is falling. */
        void addFalling(FallingPiece piece) { falling.push_back(std::move(piece)); }

        /** Every damaged cell and its ratio, for a client joining. */
        std::vector<std::pair<uint32_t, float>> damagedCells() const;

        /** Changes when damage changes (the grid's own revision covers block changes). */
        uint64_t revision() const { return rev; }

        bool breakable(BlockId id) const;

        /**
         * Sets a flammable block alight: it burns `seconds`, then is gone, and after `spread` seconds its
         * flammable neighbours catch. False when it is not flammable or already burning.
         */
        bool ignite(const glm::ivec3& cell, float seconds, float spread);
        bool isBurning(const glm::ivec3& cell) const;
        size_t burningCount() const { return burning.size(); }

        /** Seconds a dug cell has left before it fills back in; -1 when the cell is not a dug one. */
        float dugRemaining(const glm::ivec3& cell) const;

        std::vector<FallingPiece> falling;

        size_t damagedCount() const { return damageTaken.size(); }
        size_t temporaryCount() const { return temporary.size(); }

    private:
        struct Temporary
        {
            glm::ivec3 cell;
            BlockId placed;      // what the cell holds while it lasts
            BlockId restore;     // what it goes back to
            float expires;
        };

        bool anchored(const glm::ivec3& cell) const;
        void checkSupport(TerrainEvents& events);
        void forget(size_t index);

        VoxelGrid* grid = nullptr;
        const BlockTable* table = nullptr;

        std::map<size_t, float> damageTaken;   // cell index -> damage so far
        std::vector<Temporary> temporary;

        struct Burning
        {
            glm::ivec3 cell;
            BlockId block;
            float spreadAt, endsAt;
            float seconds, spreadSeconds;
            bool spread = false;
        };
        std::vector<Burning> burning;
        std::vector<glm::ivec3> ignitedSinceStep;
        std::vector<glm::ivec3> removedSinceCheck;
        std::vector<glm::ivec3> brokenSinceStep;
        std::vector<BlockId> brokenBlocksSinceStep;

        float now = 0.0f;
        int nextPiece = 1;
        uint64_t rev = 0;
    };
}
