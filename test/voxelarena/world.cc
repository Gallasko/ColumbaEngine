#include "stdafx.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cmath>
#include <iostream>
#include <filesystem>
#include <fstream>

#include "Core/content.h"
#include "Core/scriptbrain.h"
#include "Core/tuning.h"
#include "Core/world.h"

using namespace arena;

namespace
{
    /** The shipped content with an empty arena floor: tests place what they need. */
    struct Arena
    {
        Content content;
        Tuning tuning;
        ArenaWorld world;

        Arena()
        {
            content = loadContent(VOXELARENA_MODS_DIR);
            ContentReport r;
            tuning = readTuning(content.tuning, content.tuningFile, r);

            // A bare 32 x 16 x 32 floor, nobody but the player
            content.arena.size = glm::ivec3(32, 16, 32);
            content.arena.shapes = parseJson(R"([ { "block": "stone", "from": [0, 0, 0], "to": [31, 0, 31] } ])").value;
            content.arena.spawns.clear();
            content.arena.playerSpawn = glm::vec3(16, 1, 8);

            world.start(content, tuning);
            settle();
        }

        Fighter& player() { return *world.player(); }

        void settle(int steps = 30)
        {
            for (int i = 0; i < steps; ++i)
                world.step(InputFrame{});
        }

        void run(int steps, const InputFrame& in = {})
        {
            for (int i = 0; i < steps; ++i)
                world.step(in);
        }

        InputFrame press(bool InputFrame::* button)
        {
            InputFrame in;
            in.*button = true;
            return in;
        }

        int dummyAhead(float distance = 1.4f)
        {
            const glm::vec3 p = player().body.position;
            return world.spawnEnemy("base:dummy", p + glm::vec3(0, 0, distance), 180.0f);
        }
    };
}

TEST(arena_world, the_player_lands_on_the_floor)
{
    Arena a;
    EXPECT_TRUE(a.player().body.onGround);
    EXPECT_NEAR(a.player().body.position.y, 1.0f, 0.01f);
}

TEST(arena_world, walking_reaches_walk_speed_and_stops)
{
    Arena a;
    InputFrame in;
    in.move = glm::vec2(0, 1);   // camera yaw 0: forward is +Z

    a.run(60, in);
    EXPECT_NEAR(a.player().body.velocity.z, a.tuning.player.walkSpeed, 0.01f);
    EXPECT_NEAR(a.player().yaw, 0.0f, 1.0f);

    a.run(30);
    EXPECT_NEAR(glm::length(a.player().body.velocity), 0.0f, 1e-3f);
}

TEST(arena_world, a_held_jump_reaches_the_tuned_height_and_a_tapped_one_less)
{
    auto apex = [](bool hold) {
        Arena a;
        const float ground = a.player().body.position.y;
        InputFrame in;
        in.jumpPressed = true;
        in.jumpHeld = true;
        a.world.step(in);

        float top = ground;
        InputFrame held;
        held.jumpHeld = hold;

        for (int i = 0; i < 90; ++i)
        {
            a.world.step(held);
            top = std::max(top, a.player().body.position.y);
        }

        EXPECT_TRUE(a.player().body.onGround);
        return top - ground;
    };

    const float full = apex(true);
    const float tapped = apex(false);

    Arena ref;
    EXPECT_NEAR(full, ref.tuning.player.jumpHeight, 0.12f);
    EXPECT_LT(tapped, full * 0.7f);
}

TEST(arena_world, a_jump_pressed_just_before_landing_is_kept)
{
    Arena a;
    a.player().body.position.y = 3.0f;
    a.player().body.onGround = false;

    // Fall until about to land, then press
    while (a.player().body.position.y > 1.25f)
        a.world.step(InputFrame{});

    InputFrame in;
    in.jumpPressed = true;
    in.jumpHeld = true;
    a.world.step(in);

    InputFrame held;
    held.jumpHeld = true;
    bool rose = false;

    for (int i = 0; i < 20; ++i)
    {
        a.world.step(held);
        rose = rose or a.player().body.velocity.y > 1.0f;
    }

    EXPECT_TRUE(rose);
}

TEST(arena_world, coyote_time_allows_a_jump_just_after_a_ledge)
{
    Arena a;
    // A one block high platform the player stands on, ending at z = 10
    a.world.grid.fill({0, 1, 0}, {31, 1, 9}, a.content.block("base:stone"));
    a.player().body.position = glm::vec3(16, 2, 9.5f);
    a.settle(10);
    ASSERT_NEAR(a.player().body.position.y, 2.0f, 0.01f);

    InputFrame walk;
    walk.move = glm::vec2(0, 1);

    int airborne = 0;
    while (airborne < 3)
    {
        a.world.step(walk);
        airborne = a.player().body.onGround ? 0 : airborne + 1;
    }

    // Three frames off the ledge (0.05 s < 0.1 s coyote): the jump still happens
    InputFrame jump = walk;
    jump.jumpPressed = true;
    jump.jumpHeld = true;
    a.world.step(jump);
    EXPECT_GT(a.player().body.velocity.y, 3.0f);
}

TEST(arena_world, the_dodge_is_invulnerable_then_not)
{
    Arena a;
    a.world.step(a.press(&InputFrame::dodgePressed));

    ASSERT_EQ(a.player().action, Action::Dodge);

    int safeFrames = 0;
    for (int i = 0; i < a.tuning.dodge.frames + 2; ++i)
    {
        safeFrames += a.player().invulnerable(a.tuning) ? 1 : 0;
        a.world.step(InputFrame{});
    }

    EXPECT_EQ(safeFrames, a.tuning.dodge.iframeEnd - a.tuning.dodge.iframeStart);
    EXPECT_EQ(a.player().action, Action::Free);
    EXPECT_LT(a.player().stamina, a.tuning.stamina.max);
}

TEST(arena_world, three_light_presses_make_the_three_hit_combo)
{
    Arena a;
    const int dummy = a.dummyAhead();
    a.settle(5);

    std::vector<std::string> landed;

    for (int i = 0; i < 160; ++i)
    {
        InputFrame in;
        // Mash light every 8 frames, like a player would
        in.lightPressed = (i % 8 == 0) and i < 64;
        a.world.step(in);

        for (const auto& h : a.world.hits)
            if (h.target == dummy)
                landed.push_back(h.attack);
        a.world.hits.clear();
    }

    ASSERT_EQ(landed.size(), 3u);
    EXPECT_EQ(landed[0], "light1");
    EXPECT_EQ(landed[1], "light2");
    EXPECT_EQ(landed[2], "light3");
}

TEST(arena_world, a_hit_freezes_both_fighters_for_its_hitstop)
{
    Arena a;
    const int dummy = a.dummyAhead();
    a.settle(5);

    a.world.step(a.press(&InputFrame::lightPressed));

    int hitStep = -1;
    for (int i = 0; i < 30 and hitStep < 0; ++i)
    {
        a.world.step(InputFrame{});
        if (not a.world.hits.empty())
            hitStep = i;
    }

    ASSERT_GE(hitStep, 0);
    const int frame = a.player().frame;
    const int hitstop = a.world.hits[0].hitstop;
    EXPECT_GT(hitstop, 0);
    EXPECT_EQ(a.world.find(dummy)->hitstop, hitstop);

    a.run(hitstop);
    EXPECT_EQ(a.player().frame, frame);   // frozen
    a.world.step(InputFrame{});
    EXPECT_EQ(a.player().frame, frame + 1);
}

TEST(arena_world, a_heavy_breaks_the_brute_s_poise)
{
    Arena a;
    a.world.aiEnabled = false;
    const glm::vec3 p = a.player().body.position;
    const int brute = a.world.spawnEnemy("base:brute", p + glm::vec3(0, 0, 1.6f), 180.0f);
    a.settle(5);

    a.world.step(a.press(&InputFrame::heavyPressed));
    bool staggered = false;

    for (int i = 0; i < 60; ++i)
    {
        a.world.step(InputFrame{});
        staggered = staggered or a.world.find(brute)->action == Action::Stagger;
    }

    EXPECT_TRUE(staggered);
    EXPECT_LT(a.world.find(brute)->health, a.world.find(brute)->maxHealth);
}

TEST(arena_world, a_dodge_through_the_slam_takes_no_damage)
{
    Arena a;
    a.world.aiEnabled = false;
    const glm::vec3 p = a.player().body.position;
    Fighter* brute = a.world.find(a.world.spawnEnemy("base:brute", p + glm::vec3(0, 0, 2.0f), 180.0f));
    a.settle(5);

    // Make the brute slam by hand, and dodge just as it becomes active
    const AttackDef* slam = a.content.enemies.at("base:brute").moveset.find("slam");
    brute->action = Action::Attack;
    brute->attack = "slam";
    brute->frame = 0;

    a.run(slam->startup - 3);
    InputFrame dodge;
    dodge.dodgePressed = true;
    dodge.move = glm::vec2(1, 0);
    a.world.step(dodge);
    a.run(30);

    EXPECT_FLOAT_EQ(a.player().health, a.player().maxHealth);

    // The same slam without a dodge lands
    Arena b;
    b.world.aiEnabled = false;
    Fighter* brute2 = b.world.find(b.world.spawnEnemy("base:brute", b.player().body.position + glm::vec3(0, 0, 2.0f), 180.0f));
    b.settle(5);
    brute2->action = Action::Attack;
    brute2->attack = "slam";
    brute2->frame = 0;
    b.run(slam->total());

    EXPECT_LT(b.player().health, b.player().maxHealth);
}

TEST(arena_world, stamina_runs_out_and_comes_back)
{
    Arena a;

    for (int i = 0; i < 12; ++i)
    {
        a.world.step(a.press(&InputFrame::dodgePressed));
        a.run(a.tuning.dodge.frames);
    }

    EXPECT_LT(a.player().stamina, a.tuning.dodge.cost);

    a.run(60 * 6);
    EXPECT_FLOAT_EQ(a.player().stamina, a.tuning.stamina.max);
}

TEST(arena_world, the_spear_from_the_test_mod_reaches_further_than_the_sword)
{
    auto reachHits = [](const std::string& weapon) {
        Arena a;
        a.world.equip(weapon);
        const int dummy = a.dummyAhead(3.2f);   // sword: 1.75 m box + 0.47 m lunge falls short
        a.settle(5);

        a.world.step(a.press(&InputFrame::lightPressed));
        int hits = 0;
        for (int i = 0; i < 40; ++i)
        {
            a.world.step(InputFrame{});
            for (const auto& h : a.world.hits)
                hits += h.target == dummy;
            a.world.hits.clear();
        }
        return hits;
    };

    EXPECT_EQ(reachHits("base:sword"), 0);
    EXPECT_EQ(reachHits("testmod:spear"), 1);
}

// ---- Scripts ----

TEST(arena_brain, the_brute_script_chases_then_slams)
{
    Arena a;
    ScriptBrain brain;
    ContentReport report;
    brain.load(a.content, report);
    ASSERT_TRUE(report.errors.empty()) << (report.errors.empty() ? "" : report.errors[0].str());

    a.world.setBrain(&brain);
    const int id = a.world.spawnEnemy("base:brute", a.player().body.position + glm::vec3(0, 0, 9.0f), 180.0f);
    a.settle(5);

    Fighter* brute = a.world.find(id);
    EXPECT_EQ(brute->aiMove, "chase") << brute->aiLastError;

    bool slammed = false;
    for (int i = 0; i < 60 * 6 and not slammed; ++i)
    {
        a.world.step(InputFrame{});
        slammed = brute->action == Action::Attack and brute->attack == "slam";
    }

    EXPECT_TRUE(slammed) << brute->aiLastError;
    EXPECT_GT(brain.stats.calls, 0u);
}

TEST(arena_brain, a_broken_script_is_reported_and_the_enemy_stands_still)
{
    Arena a;
    const std::string dir = (std::filesystem::temp_directory_path() / "voxelarena-badscript").string();
    std::filesystem::create_directories(dir);

    {
        std::ofstream(dir + "/syntax.pg") << "fun think(me)\n{\n    me.move = \n}\n";
        std::ofstream(dir + "/runtime.pg") << "fun think(me)\n{\n    me.move = me.nothing.deeper\n}\n";
    }

    a.content.scripts["test:syntax"] = dir + "/syntax.pg";
    a.content.scripts["test:runtime"] = dir + "/runtime.pg";

    ScriptBrain brain;
    ContentReport report;
    brain.load(a.content, report);

    bool syntaxReported = false;
    for (const auto& e : report.errors)
        syntaxReported = syntaxReported or e.file.find("syntax.pg") != std::string::npos;
    EXPECT_TRUE(syntaxReported);

    // The runtime error comes back from think() without crashing, every time
    Fighter self, target;
    BrainView view;
    view.self = &self;
    view.target = &target;
    BrainOrder order;
    std::string error;

    for (int i = 0; i < 100; ++i)
        EXPECT_FALSE(brain.think("test:runtime", view, order, error));

    EXPECT_NE(error.find("runtime.pg"), std::string::npos);
}

TEST(arena_brain, two_hundred_brutes_think_in_a_frame_budget)
{
    Arena a;
    ScriptBrain brain;
    ContentReport report;
    brain.load(a.content, report);
    a.world.setBrain(&brain);
    a.world.godMode = true;

    // 200 brutes on a 32 x 32 floor, all awake (within engage range of the player)
    for (int i = 0; i < 200; ++i)
        a.world.spawnEnemy("base:brute", glm::vec3(2.5f + (i % 14) * 2.0f, 1.0f, 2.5f + (i / 14) * 2.0f), 0.0f);

    a.settle(10);
    const auto calls0 = brain.stats.calls;
    const double seconds0 = brain.stats.seconds;
    const auto start = std::chrono::steady_clock::now();

    a.run(120);

    const double stepMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count() / 120.0;
    const double thinkUs = (brain.stats.seconds - seconds0) * 1e6 / static_cast<double>(std::max<uint64_t>(1, brain.stats.calls - calls0));

    std::cout << "[ perf     ] 200 brutes: " << stepMs << " ms per step (scripts and all), "
              << thinkUs << " us per think() call, " << (brain.stats.calls - calls0) / 120 << " calls per step" << std::endl;

    // A 60 Hz step has 16.7 ms; the whole world (200 scripted enemies, physics, hits) must fit well inside
    EXPECT_LT(stepMs, 8.0);
}

TEST(arena_world, a_jump_climbs_a_two_block_wall_and_walking_climbs_one)
{
    Arena a;
    const BlockId stone = a.content.block("base:stone");
    a.world.grid.fill({0, 1, 12}, {31, 1, 12}, stone);   // one block high at z = 12
    a.world.grid.fill({0, 1, 16}, {31, 2, 31}, stone);   // two blocks high from z = 16
    a.player().body.position = glm::vec3(16, 1, 10);
    a.settle(10);

    InputFrame walk;
    walk.move = glm::vec2(0, 1);
    a.run(90, walk);

    // Over the single block by walking, then stopped by the wall
    EXPECT_NEAR(a.player().body.position.y, 1.0f, 0.05f);
    EXPECT_NEAR(a.player().body.position.z + a.player().body.width * 0.5f, 16.0f, 0.05f);

    InputFrame jump = walk;
    jump.jumpPressed = true;
    jump.jumpHeld = true;
    a.world.step(jump);

    InputFrame holdJump = walk;
    holdJump.jumpHeld = true;
    a.run(60, holdJump);

    EXPECT_NEAR(a.player().body.position.y, 3.0f, 0.05f);
    EXPECT_TRUE(a.player().body.onGround);
}

TEST(arena_world, a_dodge_covers_about_four_metres)
{
    Arena a;
    const glm::vec3 start = a.player().body.position;

    InputFrame dodge;
    dodge.dodgePressed = true;
    dodge.move = glm::vec2(0, 1);
    a.world.step(dodge);
    a.run(a.tuning.dodge.frames + 20);

    const float distance = glm::length(glm::vec2(a.player().body.position.x - start.x, a.player().body.position.z - start.z));
    EXPECT_GT(distance, 4.0f);
    EXPECT_LT(distance, 6.0f);
}

TEST(arena_world, in_first_person_the_body_faces_the_view_and_attacks_go_there)
{
    Arena a;

    // Strafe right while looking at yaw 90: the body keeps facing the view instead of the walk direction
    InputFrame strafe;
    strafe.cameraYaw = 90.0f;
    strafe.faceCamera = true;
    strafe.move = glm::vec2(1, 0);
    a.run(30, strafe);
    EXPECT_NEAR(a.player().yaw, 90.0f, 0.01f);

    // Third person: the same input turns the body to the walk direction (right of yaw 90 is yaw 0)
    InputFrame third = strafe;
    third.faceCamera = false;
    a.run(30, third);
    EXPECT_NEAR(a.player().yaw, 0.0f, 1.0f);

    // An attack pressed while walking sideways in first person goes where the camera looks
    InputFrame attack = strafe;
    attack.cameraYaw = 45.0f;
    attack.lightPressed = true;
    a.world.step(attack);
    EXPECT_EQ(a.player().action, Action::Attack);
    EXPECT_NEAR(a.player().yaw, 45.0f, 0.01f);
}
