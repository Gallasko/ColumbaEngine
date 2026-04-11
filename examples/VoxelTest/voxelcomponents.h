#pragma once

#include "ECS/system.h"

#include "glm/glm.hpp"

namespace pg
{
    /**
     * A single voxel (axis-aligned cube) in the world.
     *
     * position: world-space origin of the cube (lower-near-left corner of
     *           its local-space unit cube, before scaling).
     * size:     per-axis dimensions in world units.
     * color:    RGBA in 0..255 range (the shader divides by 255).
     */
    struct VoxelComponent : public Component
    {
        VoxelComponent(const glm::vec3& position = glm::vec3(0.0f),
                       const glm::vec3& size     = glm::vec3(1.0f),
                       const glm::vec4& color    = glm::vec4(255.0f, 255.0f, 255.0f, 255.0f))
            : position(position), size(size), color(color)
        {}

        DEFAULT_COMPONENT_MEMBERS(VoxelComponent)

        glm::vec3 position;
        glm::vec3 size;
        glm::vec4 color;
    };
}
