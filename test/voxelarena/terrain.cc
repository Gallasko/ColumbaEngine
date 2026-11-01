#include "stdafx.h"

#include <gtest/gtest.h>

#include <algorithm>

#include "Core/terrain.h"

using namespace arena;

namespace
{
    // 0 air, 1 stone (hardness 100), 2 bedrock (unbreakable), 3 rope (hardness 10)
    BlockTable table()
    {
        BlockTable t(4);
        t[0].solid = false;
        t[0].visible = false;
        t[1].hardness = 100.0f;
        t[2].breakable = false;
        t[3].hardness = 10.0f;
        return t;
    }

    void run(Terrain& t, float seconds, float& time, TerrainEvents& events)
    {
        for (int i = 0; i < static_cast<int>(seconds * 60.0f); ++i)
        {
            time += 1.0f / 60.0f;
            t.step(1.0f / 60.0f, time, 30.0f, events);
        }
    }
}

TEST(arena_terrain, damage_adds_up_to_the_hardness_and_bedrock_takes_none)
{
    VoxelGrid g(8, 8, 8);
    const auto tab = table();
    g.set(2, 1, 2, 1);
    g.set(3, 0, 3, 2);
    Terrain t;
    t.reset(&g, &tab);

    EXPECT_FALSE(t.damage({2, 1, 2}, 60.0f));
    EXPECT_NEAR(t.damageRatio(2, 1, 2), 0.6f, 1e-4f);
    EXPECT_TRUE(t.damage({2, 1, 2}, 60.0f));
    EXPECT_EQ(g.get(2, 1, 2), AIR);
    EXPECT_EQ(t.damageRatio(2, 1, 2), 0.0f);

    EXPECT_FALSE(t.damage({3, 0, 3}, 1e6f));
    EXPECT_EQ(g.get(3, 0, 3), 2);
}

TEST(arena_terrain, a_boulder_on_a_cut_rope_falls_and_lands_in_one_piece)
{
    VoxelGrid g(16, 16, 16);
    const auto tab = table();
    g.fill({0, 0, 0}, {15, 0, 15}, 2);       // bedrock floor
    g.fill({2, 1, 2}, {2, 10, 2}, 1);        // a pillar
    g.fill({2, 10, 2}, {8, 10, 2}, 1);       // a beam from its top
    g.fill({8, 8, 2}, {8, 9, 2}, 3);         // a rope down from the beam
    g.fill({7, 5, 1}, {9, 7, 3}, 1);         // a 3x3x3 boulder on the rope

    Terrain t;
    t.reset(&g, &tab);
    TerrainEvents events;
    float time = 0.0f;

    run(t, 0.1f, time, events);
    EXPECT_TRUE(t.falling.empty());   // all held up through the rope, beam and pillar

    EXPECT_TRUE(t.damage({8, 9, 2}, 10.0f));   // cut the rope at the beam
    run(t, 0.05f, time, events);

    // The lower rope cell and the boulder broke off together
    ASSERT_EQ(t.falling.size(), 1u);
    EXPECT_EQ(t.falling[0].occupied.size(), 28u);
    EXPECT_EQ(g.get(8, 6, 2), AIR);

    run(t, 2.0f, time, events);
    EXPECT_TRUE(t.falling.empty());
    ASSERT_EQ(events.landed.size(), 1u);

    // On the floor now: the boulder's bottom at y = 1, the rope cell on top of it
    EXPECT_EQ(g.get(8, 1, 2), 1);
    EXPECT_EQ(g.get(7, 3, 1), 1);
    EXPECT_EQ(g.get(8, 4, 2), 3);
    EXPECT_EQ(g.get(8, 5, 2), AIR);
    EXPECT_EQ(g.get(2, 10, 2), 1);   // the beam stays
}

TEST(arena_terrain, what_still_touches_the_ground_stays_up)
{
    VoxelGrid g(16, 16, 16);
    const auto tab = table();
    g.fill({0, 0, 0}, {15, 0, 15}, 2);
    g.fill({4, 1, 4}, {5, 6, 5}, 1);   // a 2x2 pillar

    Terrain t;
    t.reset(&g, &tab);
    TerrainEvents events;
    float time = 0.0f;

    // Three of its four feet gone: still standing
    t.remove({4, 1, 4});
    t.remove({5, 1, 4});
    t.remove({4, 1, 5});
    run(t, 0.1f, time, events);
    EXPECT_TRUE(t.falling.empty());

    // The last one: down it comes, one block, onto the floor
    t.remove({5, 1, 5});
    run(t, 0.05f, time, events);
    ASSERT_EQ(t.falling.size(), 1u);
    run(t, 1.0f, time, events);
    EXPECT_EQ(g.get(4, 1, 4), 1);
    EXPECT_EQ(g.get(4, 6, 4), AIR);
    EXPECT_EQ(g.get(4, 5, 4), 1);
}

TEST(arena_terrain, raised_blocks_crumble_and_dug_cells_fill_back_in)
{
    VoxelGrid g(8, 8, 8);
    const auto tab = table();
    g.fill({0, 0, 0}, {7, 1, 7}, 1);

    Terrain t;
    t.reset(&g, &tab);
    TerrainEvents events;
    float time = 0.0f;

    t.placeFor({3, 2, 3}, 1, 1.0f);
    t.digFor({5, 1, 5}, 1.5f);
    EXPECT_EQ(g.get(3, 2, 3), 1);
    EXPECT_EQ(g.get(5, 1, 5), AIR);

    run(t, 1.2f, time, events);
    EXPECT_EQ(g.get(3, 2, 3), AIR);
    EXPECT_EQ(g.get(5, 1, 5), AIR);

    run(t, 0.5f, time, events);
    EXPECT_EQ(g.get(5, 1, 5), 1);
    EXPECT_FALSE(events.filled.empty());

    // A cell changed by something else in the meantime is left as it is
    t.placeFor({1, 2, 1}, 1, 0.5f);
    t.damage({1, 2, 1}, 1000.0f);
    t.placeFor({1, 2, 1}, 3, 0.0f);   // built over, for good
    run(t, 1.0f, time, events);
    EXPECT_EQ(g.get(1, 2, 1), 3);
}

TEST(arena_terrain, a_decoration_breaks_with_its_ground_and_holds_nothing_up)
{
    VoxelGrid g(8, 8, 8);
    auto tab = table();
    tab.emplace_back();                       // 4: a flower
    tab[4].shape = BlockShape::Cross;
    tab[4].solid = false;
    tab[4].hardness = 1.0f;

    g.fill({0, 0, 0}, {7, 0, 7}, 2);          // bedrock floor
    g.set(1, 1, 1, 1);                        // a stone with a flower on it
    g.set(1, 2, 1, 4);
    g.set(4, 1, 4, 1);                        // a stone, a flower, and a beam over the flower
    g.set(4, 2, 4, 4);
    g.fill({4, 3, 4}, {6, 3, 4}, 1);
    g.fill({6, 1, 4}, {6, 2, 4}, 1);          // the beam's only pillar

    Terrain t;
    t.reset(&g, &tab);
    TerrainEvents events;
    float time = 0.0f;

    // The flower's ground goes: the flower breaks too, and is reported broken
    EXPECT_TRUE(t.damage({1, 1, 1}, 1e9f));
    run(t, 0.1f, time, events);
    EXPECT_EQ(g.get(1, 2, 1), AIR);
    EXPECT_NE(std::find(events.broken.begin(), events.broken.end(), glm::ivec3(1, 2, 1)), events.broken.end());
    EXPECT_TRUE(t.falling.empty());

    // The pillar goes: the beam is not held up through the flower under it
    EXPECT_TRUE(t.damage({6, 1, 4}, 1e9f));
    run(t, 0.05f, time, events);
    ASSERT_EQ(t.falling.size(), 1u);
    EXPECT_EQ(t.falling[0].occupied.size(), 4u);   // the beam and the pillar's top
    EXPECT_EQ(g.get(4, 2, 4), 4);                   // the flower is still on its stone
}
