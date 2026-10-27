#include "physics.h"

#include <cmath>

namespace arena
{
    namespace
    {
        // Distance kept between a body and the face it rests against, so floor() of the
        // box's edge never lands inside the blocking cell on the next step.
        constexpr float SKIN = 1e-3f;

        int cellLo(float v) { return static_cast<int>(std::floor(v)); }
        // Last cell touched by an edge at v (an edge exactly on a boundary does not touch the next cell)
        int cellHi(float v) { return static_cast<int>(std::ceil(v)) - 1; }

        /**
         * Moves the box [min, max) by d along one axis and returns the distance actually travelled.
         * Scans the slabs of cells the leading face crosses, nearest first.
         */
        float sweepAxis(const VoxelGrid& grid, const BlockTable& table, const glm::vec3& min, const glm::vec3& max, int axis, float d)
        {
            if (d == 0.0f)
                return 0.0f;

            const int a1 = (axis + 1) % 3;
            const int a2 = (axis + 2) % 3;

            const int lo1 = cellLo(min[a1]), hi1 = cellHi(max[a1]);
            const int lo2 = cellLo(min[a2]), hi2 = cellHi(max[a2]);

            auto slabSolid = [&](int c) {
                for (int i = lo1; i <= hi1; ++i)
                {
                    for (int j = lo2; j <= hi2; ++j)
                    {
                        glm::ivec3 p;
                        p[axis] = c;
                        p[a1] = i;
                        p[a2] = j;

                        if (grid.isSolid(table, p.x, p.y, p.z))
                            return true;
                    }
                }

                return false;
            };

            if (d > 0.0f)
            {
                const int first = cellHi(max[axis]) + 1;
                const int last = cellHi(max[axis] + d);

                for (int c = first; c <= last; ++c)
                {
                    if (slabSolid(c))
                        return std::max(0.0f, static_cast<float>(c) - max[axis] - SKIN);
                }
            }
            else
            {
                const int first = cellLo(min[axis]) - 1;
                const int last = cellLo(min[axis] + d);

                for (int c = first; c >= last; --c)
                {
                    if (slabSolid(c))
                        return std::min(0.0f, static_cast<float>(c + 1) - min[axis] + SKIN);
                }
            }

            return d;
        }
    }

    bool overlapsSolid(const VoxelGrid& grid, const BlockTable& table, const glm::vec3& min, const glm::vec3& max)
    {
        for (int y = cellLo(min.y); y <= cellHi(max.y); ++y)
            for (int z = cellLo(min.z); z <= cellHi(max.z); ++z)
                for (int x = cellLo(min.x); x <= cellHi(max.x); ++x)
                    if (grid.isSolid(table, x, y, z))
                        return true;

        return false;
    }

    void moveBody(const VoxelGrid& grid, const BlockTable& table, Body& body, const glm::vec3& delta, const MoveOptions& options)
    {
        body.hitX = body.hitY = body.hitZ = false;
        body.steppedUp = 0.0f;
        body.mantled = false;
        body.cornerNudged = false;

        const bool wasOnGround = body.onGround;

        // Vertical first: landing this frame lets the horizontal pass treat the body as grounded
        float dy = sweepAxis(grid, table, body.boxMin(), body.boxMax(), 1, delta.y);

        // Corner correction: the head caught an edge on the way up. Find the smallest sideways slide
        // (in 5 cm steps) after which the whole rise is free, and take it
        if (delta.y > 0.0f and dy < delta.y - SKIN and options.cornerNudge > 0.0f)
        {
            bool found = false;

            for (float step = 0.05f; step <= options.cornerNudge + 1e-4f and not found; step += 0.05f)
            {
                for (int axis : {0, 2})
                {
                    for (float sign : {1.0f, -1.0f})
                    {
                        const float shift = step * sign;

                        if (std::fabs(sweepAxis(grid, table, body.boxMin(), body.boxMax(), axis, shift) - shift) > 1e-5f)
                            continue;

                        Body shifted = body;
                        shifted.position[axis] += shift;
                        const float rise = sweepAxis(grid, table, shifted.boxMin(), shifted.boxMax(), 1, delta.y);

                        if (rise >= delta.y - SKIN)
                        {
                            body.position[axis] += shift;
                            dy = rise;
                            body.cornerNudged = true;
                            found = true;
                            break;
                        }
                    }

                    if (found)
                        break;
                }
            }
        }

        body.position.y += dy;

        if (dy != delta.y)
        {
            body.hitY = true;
            body.velocity.y = 0.0f;
        }

        body.onGround = (delta.y <= 0.0f and body.hitY);

        const bool grounded = body.onGround or wasOnGround;

        for (int axis : {0, 2})
        {
            const float want = delta[axis];

            if (want == 0.0f)
                continue;

            float moved = sweepAxis(grid, table, body.boxMin(), body.boxMax(), axis, want);

            // Blocked: try the same move from higher up, then settle back down. On the ground this is the
            // step-up; in the air it is the ledge assist, which must land on the ledge to count
            const float lift = grounded ? options.stepHeight : options.airStepHeight;

            if (moved != want and lift > 0.0f)
            {
                const float up = sweepAxis(grid, table, body.boxMin(), body.boxMax(), 1, lift);

                if (up > 0.0f)
                {
                    Body raised = body;
                    raised.position.y += up;

                    const float movedRaised = sweepAxis(grid, table, raised.boxMin(), raised.boxMax(), axis, want);

                    if (std::fabs(movedRaised) > std::fabs(moved) + SKIN)
                    {
                        raised.position[axis] += movedRaised;

                        const float down = sweepAxis(grid, table, raised.boxMin(), raised.boxMax(), 1, -up);
                        raised.position.y += down;

                        // A mantle stands on the ledge, higher than it started; anything else is no mantle
                        const bool landed = down > -up + SKIN;

                        if (not grounded and (not landed or raised.position.y <= body.position.y + SKIN))
                        {
                            body.position[axis] += moved;
                            (axis == 0 ? body.hitX : body.hitZ) = true;
                            body.velocity[axis] = 0.0f;
                            continue;
                        }

                        if (not grounded)
                        {
                            body.mantled = true;
                            body.velocity.y = std::min(body.velocity.y, 0.0f);
                        }

                        body.steppedUp += raised.position.y - body.position.y;
                        body.position = raised.position;
                        body.onGround = true;

                        if (movedRaised != want)
                        {
                            (axis == 0 ? body.hitX : body.hitZ) = true;
                            body.velocity[axis] = 0.0f;
                        }

                        continue;
                    }
                }
            }

            body.position[axis] += moved;

            if (moved != want)
            {
                (axis == 0 ? body.hitX : body.hitZ) = true;
                body.velocity[axis] = 0.0f;
            }
        }
    }
}
