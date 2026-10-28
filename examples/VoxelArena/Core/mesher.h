#pragma once

#include <cstdint>
#include <functional>
#include <vector>

#include "voxelgrid.h"

namespace arena
{
    /**
     * CPU side of a voxel mesh, ready to upload.
     *
     * Vertex layout (FLOATS_PER_VERTEX floats):
     *   position (3), normal (3), color rgb 0..1 (3), ambient occlusion 0..1 (1)
     */
    struct MeshData
    {
        static constexpr int FLOATS_PER_VERTEX = 10;

        std::vector<float> vertices;
        std::vector<uint32_t> indices;

        size_t vertexCount() const { return vertices.size() / FLOATS_PER_VERTEX; }
        size_t faceCount() const   { return indices.size() / 6; }
    };

    struct MeshOptions
    {
        /** Size of one cell in mesh units (models are authored in small voxels). */
        float cellSize = 1.0f;
        /** Subtracted from every vertex position before scaling, e.g. to put a model's feet at the origin. */
        glm::vec3 origin = glm::vec3(0.0f);
        /** Strength of the corner darkening, 0 = off. */
        float aoStrength = 0.6f;
        /** Optional brightness per cell (1 = as is), e.g. to darken damaged blocks. */
        std::function<float(int, int, int)> brightness;
    };

    /**
     * Builds one mesh for the whole grid. A face is emitted only when the neighbouring
     * cell is not a visible block (face culling); cells outside the grid count as empty,
     * so the arena's outer faces are drawn.
     */
    MeshData buildMesh(const VoxelGrid& grid, const BlockTable& table, const MeshOptions& options = {});
}
