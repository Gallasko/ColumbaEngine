#include "stdafx.h"

#include <gtest/gtest.h>

#include "Core/mesher.h"
#include "Core/voxelgrid.h"

using namespace arena;

namespace
{
    BlockTable twoBlocks()
    {
        BlockTable t(3);
        t[0].solid = false;
        t[0].visible = false;
        t[1].color = glm::vec3(0.5f);
        t[2].color = glm::vec3(1.0f, 0.0f, 0.0f);
        return t;
    }
}

TEST(arena_grid, set_get_and_revision)
{
    VoxelGrid g(4, 3, 5);

    EXPECT_EQ(g.get(1, 1, 1), AIR);
    const auto rev = g.revision();

    EXPECT_TRUE(g.set(1, 1, 1, 2));
    EXPECT_EQ(g.get(1, 1, 1), 2);
    EXPECT_GT(g.revision(), rev);

    // Writing the same value is not an edit
    const auto rev2 = g.revision();
    EXPECT_FALSE(g.set(1, 1, 1, 2));
    EXPECT_EQ(g.revision(), rev2);

    EXPECT_FALSE(g.set(-1, 0, 0, 1));
    EXPECT_EQ(g.get(10, 0, 0), AIR);
}

TEST(arena_grid, outside_is_closed_on_the_sides_and_open_above)
{
    VoxelGrid g(4, 4, 4);
    const auto t = twoBlocks();

    EXPECT_TRUE(g.isSolid(t, 1, -1, 1));   // under the floor
    EXPECT_TRUE(g.isSolid(t, -1, 1, 1));   // past a side
    EXPECT_FALSE(g.isSolid(t, 1, 4, 1));   // the sky
    EXPECT_FALSE(g.isSolid(t, 1, 1, 1));   // air inside

    g.outsideSolid = false;
    EXPECT_FALSE(g.isSolid(t, 1, -1, 1));
}

TEST(arena_mesher, a_lone_block_has_six_faces)
{
    VoxelGrid g(3, 3, 3);
    g.set(1, 1, 1, 1);

    const auto mesh = buildMesh(g, twoBlocks());
    EXPECT_EQ(mesh.faceCount(), 6u);
    EXPECT_EQ(mesh.vertexCount(), 24u);
    EXPECT_EQ(mesh.indices.size(), 36u);
}

TEST(arena_mesher, touching_blocks_cull_their_shared_faces)
{
    VoxelGrid g(4, 4, 4);
    g.set(1, 1, 1, 1);
    g.set(2, 1, 1, 2);

    EXPECT_EQ(buildMesh(g, twoBlocks()).faceCount(), 10u);

    // A 2x2x2 cube is 24 outer faces
    VoxelGrid c(4, 4, 4);
    c.fill({0, 0, 0}, {1, 1, 1}, 1);
    EXPECT_EQ(buildMesh(c, twoBlocks()).faceCount(), 24u);
}

TEST(arena_mesher, faces_point_outward_and_wind_counter_clockwise)
{
    VoxelGrid g(3, 3, 3);
    g.set(1, 1, 1, 1);
    const auto mesh = buildMesh(g, twoBlocks());

    for (size_t t = 0; t < mesh.indices.size(); t += 3)
    {
        auto vtx = [&](size_t i) {
            const float* v = &mesh.vertices[mesh.indices[t + i] * MeshData::FLOATS_PER_VERTEX];
            return glm::vec3(v[0], v[1], v[2]);
        };
        const float* n = &mesh.vertices[mesh.indices[t] * MeshData::FLOATS_PER_VERTEX + 3];

        const glm::vec3 geometric = glm::cross(vtx(1) - vtx(0), vtx(2) - vtx(0));
        EXPECT_GT(glm::dot(geometric, glm::vec3(n[0], n[1], n[2])), 0.0f);

        // And the normal points away from the block centre
        const glm::vec3 centre = (vtx(0) + vtx(1) + vtx(2)) / 3.0f;
        EXPECT_GT(glm::dot(centre - glm::vec3(1.5f), glm::vec3(n[0], n[1], n[2])), 0.0f);
    }
}

TEST(arena_mesher, a_corner_in_front_of_a_face_darkens_it)
{
    VoxelGrid g(4, 4, 4);
    g.set(1, 0, 1, 1);
    g.set(2, 1, 1, 1);   // a wall next to the top face of the first block

    const auto mesh = buildMesh(g, twoBlocks());

    float minAo = 1.0f;
    for (size_t v = 0; v < mesh.vertexCount(); ++v)
    {
        const float* p = &mesh.vertices[v * MeshData::FLOATS_PER_VERTEX];
        // Top face of the lower block (y == 1, normal +Y, x within the block)
        if (p[1] == 1.0f and p[4] == 1.0f and p[0] <= 2.0f)
            minAo = std::min(minAo, p[9]);
    }

    EXPECT_LT(minAo, 1.0f);
}

TEST(arena_grid, raycast_finds_the_first_solid_cell_and_its_face)
{
    VoxelGrid g(8, 8, 8);
    const auto t = twoBlocks();
    g.set(5, 2, 2, 1);

    const auto hit = raycast(g, t, glm::vec3(0.5f, 2.5f, 2.5f), glm::vec3(1, 0, 0), 20.0f);
    ASSERT_TRUE(hit.hit);
    EXPECT_EQ(hit.cell, glm::ivec3(5, 2, 2));
    EXPECT_EQ(hit.normal, glm::ivec3(-1, 0, 0));
    EXPECT_NEAR(hit.distance, 4.5f, 1e-4f);

    EXPECT_FALSE(raycast(g, t, glm::vec3(0.5f, 5.5f, 2.5f), glm::vec3(1, 0, 0), 3.0f).hit);
}
