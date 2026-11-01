#include "mesher.h"

#include <cmath>
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

            return id < table.size() ? table[id].visible and not table[id].decoration() : true;
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

        void pushVertex(MeshData& mesh, const glm::vec3& pos, const glm::vec3& normal, const glm::vec3& color, float ao, const glm::vec4& surface)
        {
            mesh.vertices.insert(mesh.vertices.end(), {
                pos.x, pos.y, pos.z,
                normal.x, normal.y, normal.z,
                color.r, color.g, color.b,
                ao,
                surface.x, surface.y, surface.z, surface.w
            });
        }

        // Two pictures crossed along the cell's diagonals, each seen from both sides. Lit as the
        // ground under it, so a field of grass takes the light of the ground it grows on.
        void addCross(MeshData& mesh, const glm::ivec3& p, const glm::vec3& color, int layer, const MeshOptions& options)
        {
            // Moved, turned and sized a little per cell, so a field of them does not stand in rows
            const float h1 = cellHash(p.x, p.y, p.z);
            const float h2 = cellHash(p.z + 31, p.y + 7, p.x - 13);
            const float h3 = cellHash(p.y - 3, p.x + 11, p.z + 5);

            const glm::vec3 foot = glm::vec3(p) + glm::vec3(0.5f + (h1 - 0.5f) * 0.3f, 0.0f, 0.5f + (h2 - 0.5f) * 0.3f);
            const float height = 0.55f + 0.25f * h3;
            const float turn = (h1 - 0.5f) * 0.6f;
            const float bottomAo = 1.0f - options.aoStrength * 0.5f;
            const glm::vec3 up(0.0f, 1.0f, 0.0f);

            for (const float diagonal : {0.785398f, -0.785398f})
            {
                const glm::vec3 half = glm::vec3(std::cos(diagonal + turn), 0.0f, std::sin(diagonal + turn)) * 0.5f;
                const glm::vec3 corners[4] = { foot - half, foot + half, foot + half + up * height, foot - half + up * height };
                const glm::vec2 uv[4] = { {0, 0}, {1, 0}, {1, 1}, {0, 1} };

                const uint32_t base = static_cast<uint32_t>(mesh.vertexCount());

                for (int c = 0; c < 4; ++c)
                    pushVertex(mesh, (corners[c] - options.origin) * options.cellSize, up, color, c < 2 ? bottomAo : 1.0f, glm::vec4(uv[c], static_cast<float>(layer), 0.0f));

                mesh.indices.insert(mesh.indices.end(), { base, base + 1, base + 2, base, base + 2, base + 3 });
                mesh.indices.insert(mesh.indices.end(), { base, base + 2, base + 1, base, base + 3, base + 2 });
            }
        }

        // A copy of the block's model, turned a quarter turn at random per cell
        void addModel(MeshData& mesh, const glm::ivec3& p, const MeshData& model, float brightness, const MeshOptions& options)
        {
            const int turns = static_cast<int>(cellHash(p.x, p.y, p.z) * 4.0f) & 3;
            const glm::vec3 centre = glm::vec3(p) + glm::vec3(0.5f, 0.0f, 0.5f);

            auto turn = [turns](glm::vec3 v) {
                for (int k = 0; k < turns; ++k)
                    v = glm::vec3(-v.z, v.y, v.x);
                return v;
            };

            const uint32_t base = static_cast<uint32_t>(mesh.vertexCount());
            const size_t n = model.vertexCount();

            for (size_t i = 0; i < n; ++i)
            {
                const float* v = &model.vertices[i * MeshData::FLOATS_PER_VERTEX];
                const glm::vec3 pos = centre + turn(glm::vec3(v[0], v[1], v[2]));

                pushVertex(mesh, (pos - options.origin) * options.cellSize, turn(glm::vec3(v[3], v[4], v[5])),
                           glm::vec3(v[6], v[7], v[8]) * brightness, v[9], glm::vec4(v[10], v[11], v[12], v[13]));
            }

            for (uint32_t index : model.indices)
                mesh.indices.push_back(base + index);
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

            const glm::ivec3 p(x, y, z);

            // A textured block shows its texture as painted: the colour is for the debris and the fallback
            const bool textured = look.texture[FACE_TOP] >= 0 or look.texture[FACE_SIDE] >= 0 or look.texture[FACE_BOTTOM] >= 0;
            glm::vec3 color = textured ? glm::vec3(1.0f) : look.color;
            float brightness = options.brightness ? options.brightness(x, y, z) : 1.0f;

            if (look.colorNoise > 0.0f)
                color *= 1.0f + (cellHash(x, y, z) - 0.5f) * look.colorNoise;

            color *= brightness;

            if (look.shape == BlockShape::Cross)
            {
                addCross(mesh, p, color, look.texture[FACE_SIDE], options);
                continue;
            }

            if (look.shape == BlockShape::Model)
            {
                if (options.blockModels and id < options.blockModels->size())
                    addModel(mesh, p, (*options.blockModels)[id], brightness, options);
                continue;
            }

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
                const int layer = look.texture[face.normal.y > 0 ? FACE_TOP : (face.normal.y < 0 ? FACE_BOTTOM : FACE_SIDE)];

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

                    const glm::vec4 surface((su + 1) * 0.5f, (sv + 1) * 0.5f, static_cast<float>(layer), options.edges);
                    pushVertex(mesh, pos, glm::vec3(face.normal), color, ao[c], surface);
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
