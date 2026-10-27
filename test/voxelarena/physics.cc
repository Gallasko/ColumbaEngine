#include "stdafx.h"

#include <gtest/gtest.h>

#include "Core/physics.h"

using namespace arena;

namespace
{
    BlockTable table()
    {
        BlockTable t(2);
        t[0].solid = false;
        return t;
    }

    VoxelGrid flatGround()
    {
        VoxelGrid g(16, 8, 16);
        g.fill({0, 0, 0}, {15, 0, 15}, 1);   // floor top at y = 1
        return g;
    }
}

TEST(arena_physics, falls_and_lands_flush_on_the_floor)
{
    auto g = flatGround();
    Body b;
    b.position = glm::vec3(4.5f, 3.0f, 4.5f);

    moveBody(g, table(), b, glm::vec3(0, -5.0f, 0));

    EXPECT_TRUE(b.onGround);
    EXPECT_TRUE(b.hitY);
    EXPECT_NEAR(b.position.y, 1.0f, 0.01f);
    EXPECT_FALSE(overlapsSolid(g, table(), b.boxMin(), b.boxMax()));
}

TEST(arena_physics, a_wall_stops_the_body_on_that_axis_only)
{
    auto g = flatGround();
    g.fill({8, 1, 0}, {8, 3, 15}, 1);   // wall at x = 8, three high

    Body b;
    b.position = glm::vec3(6.5f, 1.0f, 4.5f);
    b.onGround = true;

    moveBody(g, table(), b, glm::vec3(3.0f, -0.01f, 1.0f));

    EXPECT_TRUE(b.hitX);
    EXPECT_FALSE(b.hitZ);
    EXPECT_NEAR(b.position.x + b.width * 0.5f, 8.0f, 0.01f);
    EXPECT_NEAR(b.position.z, 5.5f, 1e-4f);
}

TEST(arena_physics, fast_moves_do_not_tunnel)
{
    auto g = flatGround();
    g.fill({8, 1, 0}, {8, 3, 15}, 1);

    Body b;
    b.position = glm::vec3(2.5f, 1.0f, 4.5f);

    moveBody(g, table(), b, glm::vec3(40.0f, 0, 0));
    EXPECT_LT(b.position.x, 8.0f);
}

TEST(arena_physics, steps_up_one_block_but_not_two)
{
    MoveOptions opt;
    opt.stepHeight = 1.05f;

    {
        auto g = flatGround();
        g.fill({8, 1, 0}, {15, 1, 15}, 1);   // one block ledge

        Body b;
        b.position = glm::vec3(7.5f, 1.0f, 4.5f);
        b.onGround = true;

        moveBody(g, table(), b, glm::vec3(0.5f, -0.01f, 0), opt);
        EXPECT_NEAR(b.position.y, 2.0f, 0.01f);
        EXPECT_GT(b.position.x, 7.9f);
        EXPECT_GT(b.steppedUp, 0.9f);
        EXPECT_TRUE(b.onGround);
    }

    {
        auto g = flatGround();
        g.fill({8, 1, 0}, {15, 2, 15}, 1);   // two block wall

        Body b;
        b.position = glm::vec3(7.5f, 1.0f, 4.5f);
        b.onGround = true;

        moveBody(g, table(), b, glm::vec3(0.5f, -0.01f, 0), opt);
        EXPECT_NEAR(b.position.y, 1.0f, 0.01f);
        EXPECT_TRUE(b.hitX);
    }
}

TEST(arena_physics, no_step_up_in_the_air)
{
    MoveOptions opt;
    opt.stepHeight = 1.05f;

    auto g = flatGround();
    g.fill({8, 1, 0}, {15, 3, 15}, 1);

    Body b;
    b.position = glm::vec3(7.5f, 2.5f, 4.5f);
    b.onGround = false;

    moveBody(g, table(), b, glm::vec3(0.5f, 0.1f, 0), opt);
    EXPECT_TRUE(b.hitX);
    EXPECT_NEAR(b.position.y, 2.6f, 1e-3f);
}

TEST(arena_physics, the_head_bumps_the_ceiling)
{
    auto g = flatGround();
    g.fill({0, 4, 0}, {15, 4, 15}, 1);   // ceiling bottom at y = 4

    Body b;
    b.position = glm::vec3(4.5f, 1.0f, 4.5f);
    b.velocity.y = 10.0f;

    moveBody(g, table(), b, glm::vec3(0, 3.0f, 0));
    EXPECT_TRUE(b.hitY);
    EXPECT_FALSE(b.onGround);
    EXPECT_EQ(b.velocity.y, 0.0f);
    EXPECT_NEAR(b.position.y + b.height, 4.0f, 0.01f);
}

TEST(arena_physics, ledge_assist_climbs_a_ledge_just_above_the_feet_but_not_higher)
{
    MoveOptions opt;
    opt.airStepHeight = 0.5f;

    auto g = flatGround();
    g.fill({8, 1, 0}, {15, 2, 15}, 1);   // top at y = 3

    // Feet 0.3 under the top: climbed
    Body b;
    b.position = glm::vec3(7.65f, 2.7f, 4.5f);
    b.velocity = glm::vec3(4.0f, -1.0f, 0.0f);
    moveBody(g, table(), b, glm::vec3(0.3f, -0.02f, 0), opt);
    EXPECT_TRUE(b.mantled);
    EXPECT_TRUE(b.onGround);
    EXPECT_NEAR(b.position.y, 3.0f, 0.01f);
    EXPECT_GT(b.position.x, 7.8f);
    EXPECT_LE(b.velocity.y, 0.0f);

    // Feet 0.7 under the top: stopped against the side
    Body c;
    c.position = glm::vec3(7.65f, 2.3f, 4.5f);
    moveBody(g, table(), c, glm::vec3(0.3f, -0.02f, 0), opt);
    EXPECT_FALSE(c.mantled);
    EXPECT_TRUE(c.hitX);
    EXPECT_LT(c.position.y, 2.3f);
}

TEST(arena_physics, corner_correction_slides_a_rising_head_past_a_block_edge)
{
    MoveOptions opt;
    opt.cornerNudge = 0.3f;

    auto g = flatGround();
    g.set(8, 3, 4, 1);   // one block hanging over head height (bottom at y = 3)

    // The head overlaps that block by 0.15 on X
    Body b;
    b.position = glm::vec3(7.85f, 1.0f, 4.5f);
    moveBody(g, table(), b, glm::vec3(0, 1.5f, 0), opt);
    EXPECT_TRUE(b.cornerNudged);
    EXPECT_NEAR(b.position.y, 2.5f, 1e-3f);
    EXPECT_LE(b.position.x + b.width * 0.5f, 8.0f);

    // A head right under the middle of the block is not slid out: it bumps
    Body c;
    c.position = glm::vec3(8.5f, 1.0f, 4.5f);
    moveBody(g, table(), c, glm::vec3(0, 1.5f, 0), opt);
    EXPECT_FALSE(c.cornerNudged);
    EXPECT_TRUE(c.hitY);

    // And without the option the first head bumps too
    Body d;
    d.position = glm::vec3(7.85f, 1.0f, 4.5f);
    moveBody(g, table(), d, glm::vec3(0, 1.5f, 0));
    EXPECT_TRUE(d.hitY);
}
