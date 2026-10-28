#include "terrain.h"

#include <algorithm>
#include <cmath>
#include <deque>

namespace arena
{
    namespace
    {
        // A group bigger than this is taken as held up: it is a wall or the floor, not debris
        constexpr size_t MAX_PIECE = 4096;

        const glm::ivec3 NEIGHBOURS[6] = { {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1} };
    }

    void Terrain::reset(VoxelGrid* g, const BlockTable* t)
    {
        grid = g;
        table = t;
        damageTaken.clear();
        temporary.clear();
        removedSinceCheck.clear();
        brokenSinceStep.clear();
        brokenBlocksSinceStep.clear();
        burning.clear();
        ignitedSinceStep.clear();
        falling.clear();
        ++rev;
    }

    bool Terrain::breakable(BlockId id) const
    {
        if (id == AIR)
            return false;

        return not table or id >= table->size() or (*table)[id].breakable;
    }

    void Terrain::forget(size_t index)
    {
        if (damageTaken.erase(index))
        {
            ++rev;

            if (damageLog)
                damageLog->push_back(static_cast<uint32_t>(index));
        }
    }

    void Terrain::setDamageRatio(size_t index, float ratio)
    {
        if (not grid)
            return;

        const auto& cells = grid->raw();

        if (index >= cells.size())
            return;

        if (ratio <= 0.0f)
        {
            damageTaken.erase(index);
        }
        else
        {
            const BlockId id = cells[index];
            const float hardness = (table and id < table->size()) ? std::max(1.0f, (*table)[id].hardness) : 50.0f;
            damageTaken[index] = ratio * hardness;
        }

        ++rev;
    }

    std::vector<std::pair<uint32_t, float>> Terrain::damagedCells() const
    {
        std::vector<std::pair<uint32_t, float>> out;

        for (const auto& [index, taken] : damageTaken)
        {
            const BlockId id = grid->raw()[index];
            const float hardness = (table and id < table->size()) ? std::max(1.0f, (*table)[id].hardness) : 50.0f;
            out.emplace_back(static_cast<uint32_t>(index), std::clamp(taken / hardness, 0.0f, 1.0f));
        }

        return out;
    }

    bool Terrain::damage(const glm::ivec3& c, float amount)
    {
        if (not grid or not grid->inBounds(c.x, c.y, c.z) or amount <= 0.0f)
            return false;

        const BlockId id = grid->get(c.x, c.y, c.z);

        if (not breakable(id))
            return false;

        const float hardness = (table and id < table->size()) ? std::max(1.0f, (*table)[id].hardness) : 50.0f;
        float& taken = damageTaken[grid->index(c.x, c.y, c.z)];
        taken += amount;
        ++rev;

        if (damageLog)
            damageLog->push_back(static_cast<uint32_t>(grid->index(c.x, c.y, c.z)));

        if (taken < hardness)
            return false;

        remove(c);
        brokenSinceStep.push_back(c);
        brokenBlocksSinceStep.push_back(id);
        return true;
    }

    void Terrain::remove(const glm::ivec3& c)
    {
        if (not grid or not grid->inBounds(c.x, c.y, c.z) or grid->get(c.x, c.y, c.z) == AIR)
            return;

        grid->set(c.x, c.y, c.z, AIR);
        forget(grid->index(c.x, c.y, c.z));
        removedSinceCheck.push_back(c);
    }

    void Terrain::placeFor(const glm::ivec3& c, BlockId id, float lifetime)
    {
        if (not grid or not grid->inBounds(c.x, c.y, c.z))
            return;

        const BlockId before = grid->get(c.x, c.y, c.z);
        grid->set(c.x, c.y, c.z, id);
        forget(grid->index(c.x, c.y, c.z));

        if (lifetime > 0.0f)
            temporary.push_back({c, id, before, now + lifetime});
    }

    void Terrain::digFor(const glm::ivec3& c, float lifetime)
    {
        if (not grid or not grid->inBounds(c.x, c.y, c.z))
            return;

        const BlockId before = grid->get(c.x, c.y, c.z);

        if (not breakable(before))
            return;

        remove(c);

        if (lifetime > 0.0f)
            temporary.push_back({c, AIR, before, now + lifetime});
    }

    bool Terrain::ignite(const glm::ivec3& c, float seconds, float spread)
    {
        if (not grid or not table or not grid->inBounds(c.x, c.y, c.z))
            return false;

        const BlockId id = grid->get(c.x, c.y, c.z);

        if (id == AIR or id >= table->size() or not (*table)[id].flammable or isBurning(c))
            return false;

        burning.push_back({c, id, now + spread, now + seconds, seconds, spread, false});
        ignitedSinceStep.push_back(c);
        return true;
    }

    bool Terrain::isBurning(const glm::ivec3& c) const
    {
        return std::any_of(burning.begin(), burning.end(), [&](const Burning& b) { return b.cell == c; });
    }

    float Terrain::dugRemaining(const glm::ivec3& c) const
    {
        for (const auto& t : temporary)
            if (t.cell == c and t.placed == AIR)
                return std::max(0.0f, t.expires - now);

        return -1.0f;
    }

    float Terrain::damageRatio(int x, int y, int z) const
    {
        if (not grid or not grid->inBounds(x, y, z))
            return 0.0f;

        auto it = damageTaken.find(grid->index(x, y, z));

        if (it == damageTaken.end())
            return 0.0f;

        const BlockId id = grid->get(x, y, z);
        const float hardness = (table and id < table->size()) ? std::max(1.0f, (*table)[id].hardness) : 50.0f;
        return std::clamp(it->second / hardness, 0.0f, 1.0f);
    }

    float Terrain::damageRatioAt(size_t index) const
    {
        auto it = damageTaken.find(index);

        if (not grid or it == damageTaken.end() or index >= grid->raw().size())
            return 0.0f;

        const BlockId id = grid->raw()[index];
        const float hardness = (table and id < table->size()) ? std::max(1.0f, (*table)[id].hardness) : 50.0f;
        return std::clamp(it->second / hardness, 0.0f, 1.0f);
    }

    bool Terrain::anchored(const glm::ivec3& c) const
    {
        return c.y == 0 or not breakable(grid->get(c.x, c.y, c.z));
    }

    void Terrain::checkSupport(TerrainEvents& events)
    {
        if (removedSinceCheck.empty())
            return;

        std::set<size_t> settled;   // cells already found held up (or turned into a piece) this check

        for (const auto& removed : removedSinceCheck)
        {
            for (const auto& n : NEIGHBOURS)
            {
                const glm::ivec3 seed = removed + n;

                if (not grid->inBounds(seed.x, seed.y, seed.z) or grid->get(seed.x, seed.y, seed.z) == AIR)
                    continue;

                if (settled.count(grid->index(seed.x, seed.y, seed.z)))
                    continue;

                // Flood the group of blocks touching seed, until it finds an anchor
                std::vector<glm::ivec3> group;
                std::set<size_t> seen;
                std::deque<glm::ivec3> queue{seed};
                seen.insert(grid->index(seed.x, seed.y, seed.z));
                bool held = false;

                while (not queue.empty())
                {
                    const glm::ivec3 c = queue.front();
                    queue.pop_front();
                    group.push_back(c);

                    if (anchored(c) or group.size() > MAX_PIECE)
                    {
                        held = true;
                        break;
                    }

                    for (const auto& d : NEIGHBOURS)
                    {
                        const glm::ivec3 m = c + d;

                        if (not grid->inBounds(m.x, m.y, m.z) or grid->get(m.x, m.y, m.z) == AIR)
                            continue;

                        if (seen.insert(grid->index(m.x, m.y, m.z)).second)
                            queue.push_back(m);
                    }
                }

                for (const auto& c : group)
                    settled.insert(grid->index(c.x, c.y, c.z));

                if (held)
                    continue;

                // Nothing holds it: it breaks off as one piece
                glm::ivec3 lo = group.front(), hi = group.front();

                for (const auto& c : group)
                {
                    lo = glm::min(lo, c);
                    hi = glm::max(hi, c);
                }

                FallingPiece piece;
                piece.id = nextPiece++;
                piece.origin = lo;
                piece.cells = VoxelGrid(hi.x - lo.x + 1, hi.y - lo.y + 1, hi.z - lo.z + 1);
                piece.cells.outsideSolid = false;

                for (const auto& c : group)
                {
                    piece.cells.set(c.x - lo.x, c.y - lo.y, c.z - lo.z, grid->get(c.x, c.y, c.z));
                    piece.occupied.push_back(c - lo);
                    grid->set(c.x, c.y, c.z, AIR);
                    forget(grid->index(c.x, c.y, c.z));
                }

                // A temporary block that fell is not coming back where it was
                temporary.erase(std::remove_if(temporary.begin(), temporary.end(), [&](const Temporary& t) {
                    return std::find(group.begin(), group.end(), t.cell) != group.end();
                }), temporary.end());

                events.fell.push_back(piece.id);
                falling.push_back(std::move(piece));
            }
        }

        removedSinceCheck.clear();
    }

    void Terrain::step(float dt, float time, float gravity, TerrainEvents& events)
    {
        if (not grid)
            return;

        now = time;
        events.broken.insert(events.broken.end(), brokenSinceStep.begin(), brokenSinceStep.end());
        events.brokenBlocks.insert(events.brokenBlocks.end(), brokenBlocksSinceStep.begin(), brokenBlocksSinceStep.end());
        brokenSinceStep.clear();
        brokenBlocksSinceStep.clear();

        // Temporary blocks that ran out, oldest first
        for (auto it = temporary.begin(); it != temporary.end();)
        {
            if (it->expires > time)
            {
                ++it;
                continue;
            }

            const Temporary t = *it;
            it = temporary.erase(it);

            // Something else changed the cell since (broken, built over): leave it
            if (grid->get(t.cell.x, t.cell.y, t.cell.z) != t.placed)
                continue;

            if (t.restore == AIR)
            {
                remove(t.cell);
            }
            else
            {
                grid->set(t.cell.x, t.cell.y, t.cell.z, t.restore);
                forget(grid->index(t.cell.x, t.cell.y, t.cell.z));
                events.filled.push_back(t.cell);
            }
        }

        // Burning blocks: they catch their neighbours, then burn away
        for (size_t i = 0; i < burning.size();)
        {
            const Burning b = burning[i];

            // Gone some other way
            if (grid->get(b.cell.x, b.cell.y, b.cell.z) != b.block)
            {
                burning.erase(burning.begin() + static_cast<std::ptrdiff_t>(i));
                continue;
            }

            if (not b.spread and now >= b.spreadAt)
            {
                burning[i].spread = true;

                for (const glm::ivec3 d : {glm::ivec3(1, 0, 0), glm::ivec3(-1, 0, 0), glm::ivec3(0, 1, 0), glm::ivec3(0, -1, 0), glm::ivec3(0, 0, 1), glm::ivec3(0, 0, -1)})
                    ignite(b.cell + d, b.seconds, b.spreadSeconds);
            }

            if (now >= b.endsAt)
            {
                burning.erase(burning.begin() + static_cast<std::ptrdiff_t>(i));
                damage(b.cell, 1e9f);
                continue;
            }

            ++i;
        }

        // What burned away this step is broken too
        events.broken.insert(events.broken.end(), brokenSinceStep.begin(), brokenSinceStep.end());
        events.brokenBlocks.insert(events.brokenBlocks.end(), brokenBlocksSinceStep.begin(), brokenBlocksSinceStep.end());
        brokenSinceStep.clear();
        brokenBlocksSinceStep.clear();
        events.ignited.insert(events.ignited.end(), ignitedSinceStep.begin(), ignitedSinceStep.end());
        ignitedSinceStep.clear();

        checkSupport(events);

        // Falling pieces: down until any of their cells would enter a block, then back into the grid
        for (auto it = falling.begin(); it != falling.end();)
        {
            FallingPiece& p = *it;
            p.speed += gravity * dt;
            const float target = p.drop + p.speed * dt;

            // Cells fallen through so far (whole), and the first whole drop that would collide
            int landAt = -1;

            for (int k = static_cast<int>(std::floor(p.drop)) + 1; k <= static_cast<int>(std::ceil(target)); ++k)
            {
                bool hit = false;

                for (const auto& l : p.occupied)
                {
                    const glm::ivec3 w = p.origin + l - glm::ivec3(0, k, 0);

                    if (w.y < 0 or grid->isSolid(*table, w.x, w.y, w.z))
                    {
                        hit = true;
                        break;
                    }
                }

                if (hit)
                {
                    landAt = k - 1;
                    break;
                }
            }

            if (landAt < 0)
            {
                p.drop = target;
                ++it;
                continue;
            }

            for (const auto& l : p.occupied)
            {
                const glm::ivec3 w = p.origin + l - glm::ivec3(0, landAt, 0);
                grid->set(w.x, w.y, w.z, p.cells.get(l.x, l.y, l.z));
                events.filled.push_back(w);
            }

            events.landed.push_back(p.id);
            it = falling.erase(it);
        }
    }
}
