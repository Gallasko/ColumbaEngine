#pragma once

#include "voxelgrid.h"

namespace arena
{
    /**
     * An upright box standing on its feet: position is the centre of the bottom face.
     * Width is the full size on X and Z, height on Y.
     */
    struct Body
    {
        glm::vec3 position = glm::vec3(0.0f);
        glm::vec3 velocity = glm::vec3(0.0f);

        float width = 0.6f;
        float height = 1.8f;

        bool onGround = false;

        // Set by move(): which axes hit something this step
        bool hitX = false, hitY = false, hitZ = false;

        /** Height climbed by the last auto step-up, so the renderer can smooth the pop. */
        float steppedUp = 0.0f;

        /** The last move climbed a ledge from the air (ledge assist). */
        bool mantled = false;

        /** The last move slid sideways around a block edge on the way up (corner correction). */
        bool cornerNudged = false;

        glm::vec3 boxMin() const { return position - glm::vec3(width * 0.5f, 0.0f, width * 0.5f); }
        glm::vec3 boxMax() const { return position + glm::vec3(width * 0.5f, height, width * 0.5f); }
    };

    struct MoveOptions
    {
        /** Highest ledge climbed without jumping (0 = off). Only while on the ground. */
        float stepHeight = 0.0f;

        /**
         * Ledge assist: in the air, a ledge whose top is at most this far above the feet is climbed
         * onto instead of stopping the body against its side (0 = off). A jump that almost made it, makes it.
         */
        float airStepHeight = 0.0f;

        /**
         * Corner correction: a body moving up whose head clips the edge of a block is slid sideways
         * by up to this much to get past it, instead of stopping dead (0 = off).
         */
        float cornerNudge = 0.0f;
    };

    /** True when the box [min, max) overlaps any solid cell. */
    bool overlapsSolid(const VoxelGrid& grid, const BlockTable& table, const glm::vec3& min, const glm::vec3& max);

    /**
     * Moves the body by delta against the grid, one axis at a time (Y, then X, then Z).
     * Body::mantled tells when the ledge assist climbed this step.
     * Each axis stops flush against the first solid cell in its way, so fast moves cannot
     * tunnel. Velocity on a blocked axis is zeroed. Updates onGround.
     */
    void moveBody(const VoxelGrid& grid, const BlockTable& table, Body& body, const glm::vec3& delta, const MoveOptions& options = {});
}
