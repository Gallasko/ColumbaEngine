#include "mesher.h"

#include <cstdint>

namespace arena
{
    namespace
    {
        // For each of the 6 faces: normal, then the two in-plane axes (u, v) that walk its corners.
        struct FaceDef
        {
            glm::ivec3 normal;
            glm::ivec3 u;
            glm::ivec3 v;
        };

        // Corners are wound counter clockwise seen from outside (front faces for GL's default).
        const FaceDef FACES[6] = {
            { { 1, 0, 0}, {0, 0, -1}, {0, 1, 0} },  // +X
            { {-1, 0, 0}, {0, 0,  1}, {0, 1, 0} },  // -X
            { { 0, 1, 0}, {1, 0,  0}, {0, 0, -1} }, // +Y
            { { 0,-1, 0}, {1, 0,  0}, {0, 0,  1} }, // -Y
            { { 0, 0, 1}, {1, 0,  0}, {0, 1, 0} },  // +Z
            { { 0, 0,-1}, {-1, 0, 0}, {0, 1, 0} },  // -Z
        };

        bool opaqueAt(const VoxelGrid& grid, const BlockTable& table, const glm::ivec3& p)
        {
            BlockId id = grid.get(p.x, p.y, p.z);

            if (id == AIR)
                return false;

            return id < table.size() ? table[id].visible : true;
        }

        // Stable per-cell hash in 0..1, so a block's jitter does not change when its neighbours are edited
        float cellHash(int x, int y, int z)
        {
            uint32_t h = static_cast<uint32_t>(x) * 73856093u ^ static_cast<uint32_t>(y) * 19349663u ^ static_cast<uint32_t>(z) * 83492791u;
            h ^= h >> 13;
            h *= 0x5bd1e995u;
            h ^= h >> 15;
            return static_cast<float>(h & 0xFFFF) / 65535.0f;
        }
    }

    MeshData buildMesh(const VoxelGrid& grid, const BlockTable& table, const MeshOptions& options)
    {
        MeshData mesh;

        const int W = grid.width(), H = grid.height(), D = grid.depth();

        // Rough reserve: surfaces are about two faces per column
        mesh.vertices.reserve(static_cast<size_t>(W * D) * 8 * MeshData::FLOATS_PER_VERTEX);
        mesh.indices.reserve(static_cast<size_t>(W * D) * 12);

        for (int y = 0; y < H; ++y)
        for (int z = 0; z < D; ++z)
        for (int x = 0; x < W; ++x)
        {
            const BlockId id = grid.get(x, y, z);

            if (id == AIR)
                continue;

            BlockLook look;
            if (id < table.size())
                look = table[id];

            if (not look.visible)
                continue;

            glm::vec3 color = look.color;

            if (look.colorNoise > 0.0f)
                color *= 1.0f + (cellHash(x, y, z) - 0.5f) * look.colorNoise;

            const glm::ivec3 p(x, y, z);

            for (const auto& face : FACES)
            {
                const glm::ivec3 n = p + face.normal;

                if (opaqueAt(grid, table, n))
                    continue;

                // Face centre sits half a cell out along the normal; corners at centre +- u/2 +- v/2
                const glm::vec3 centre = glm::vec3(p) + glm::vec3(0.5f) + glm::vec3(face.normal) * 0.5f;

                const glm::ivec3 cornerSigns[4] = { {-1, -1, 0}, {1, -1, 0}, {1, 1, 0}, {-1, 1, 0} };

                float ao[4];
                const uint32_t base = static_cast<uint32_t>(mesh.vertexCount());

                for (int c = 0; c < 4; ++c)
                {
                    const int su = cornerSigns[c].x;
                    const int sv = cornerSigns[c].y;

                    // Classic voxel AO: the two side cells and the corner cell in front of the face
                    const bool side1 = opaqueAt(grid, table, n + face.u * su);
                    const bool side2 = opaqueAt(grid, table, n + face.v * sv);
                    const bool corner = opaqueAt(grid, table, n + face.u * su + face.v * sv);

                    const int occlusion = (side1 and side2) ? 3 : (int(side1) + int(side2) + int(corner));
                    ao[c] = 1.0f - options.aoStrength * (static_cast<float>(occlusion) / 3.0f);

                    const glm::vec3 corner3 = centre + glm::vec3(face.u) * (0.5f * su) + glm::vec3(face.v) * (0.5f * sv);
                    const glm::vec3 pos = (corner3 - options.origin) * options.cellSize;

                    mesh.vertices.insert(mesh.vertices.end(), {
                        pos.x, pos.y, pos.z,
                        static_cast<float>(face.normal.x), static_cast<float>(face.normal.y), static_cast<float>(face.normal.z),
                        color.r, color.g, color.b,
                        ao[c]
                    });
                }

                // Split the quad along the diagonal that keeps AO gradients smooth
                if (ao[0] + ao[2] >= ao[1] + ao[3])
                    mesh.indices.insert(mesh.indices.end(), { base, base + 1, base + 2, base, base + 2, base + 3 });
                else
                    mesh.indices.insert(mesh.indices.end(), { base + 1, base + 2, base + 3, base + 1, base + 3, base });
            }
        }

        return mesh;
    }
}
