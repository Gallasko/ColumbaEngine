#include "stdafx.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <set>

#include "Core/content.h"
#include "Core/progress.h"
#include "Core/tuning.h"
#include "Core/world.h"

using namespace arena;

// The achievements: goals over all of a player's runs (achievements.json), and the content they keep locked until then.

namespace
{
    bool lockedId(const std::set<std::string>& locked, const std::string& id)
    {
        return locked.count(id) > 0 or locked.count(id.substr(0, id.find('#'))) > 0;
    }
}

TEST(arena_progress, a_new_player_starts_with_the_core_set)
{
    const Content content = loadContent(VOXELARENA_MODS_DIR);
    ASSERT_TRUE(content.report.errors.empty()) << content.report.errors.front().str();
    ASSERT_FALSE(content.achievementOrder.empty());

    const auto locked = lockedContent(Progress{}, content);

    // Open: the sword and the bow, a first spell of each element, each weapon's first technique, the plain items
    for (const char* id : {"base:sword", "base:bow", "base:firebolt", "base:frostbolt", "base:spark", "base:leap_slam", "base:volley",
                           "base:shadow_dash", "base:sanctuary", "base:harvest", "base:tough", "base:keen", "base:searing"})
        EXPECT_FALSE(locked.count(id)) << id;

    // Locked: the other weapons, spells and techniques, the special items, and the content only goals give
    for (const char* id : {"base:dagger", "base:staff", "base:scythe", "base:fireball", "base:glacier", "base:whirlwind", "base:kindling",
                           "base:flame_burst", "base:frost_ray", "base:storm_field", "base:flint"})
        EXPECT_TRUE(locked.count(id)) << id;

    // Every first spell to choose from has one open of each element at least
    int open = 0;
    for (const auto& id : content.run.startChoices)
        open += locked.count(id) ? 0 : 1;
    EXPECT_GE(open, 3);

    // Unlock all: nothing locked
    Progress all;
    all.unlockAll = true;
    EXPECT_TRUE(lockedContent(all, content).empty());
}

TEST(arena_progress, a_goal_reached_unlocks_what_it_names)
{
    const Content content = loadContent(VOXELARENA_MODS_DIR);
    Progress p;

    p.stats["damage:fire"] = 4999.0f;
    EXPECT_TRUE(reachGoals(p, content).empty());
    EXPECT_TRUE(lockedContent(p, content).count("base:fireball"));

    p.stats["damage:fire"] += 1.0f;
    const auto reached = reachGoals(p, content);
    ASSERT_EQ(reached, std::vector<std::string>{"base:firestarter"});
    EXPECT_FALSE(lockedContent(p, content).count("base:fireball"));
    EXPECT_FALSE(lockedContent(p, content).count("base:kindling"));

    // Reached once: never again
    EXPECT_TRUE(reachGoals(p, content).empty());
    EXPECT_FLOAT_EQ(goalProgress(p, content.achievements.at("base:firestarter")), 5000.0f);
}

TEST(arena_progress, a_run_counts_what_the_player_did)
{
    Fighter f;
    f.weapon = "base:sword";
    f.level = 9;
    f.stats.kills = 40;
    f.stats.damage = 1500.0f;
    f.stats.presents = 2;
    f.stats.bySource["base:burn"] = 300.0f;
    f.stats.counts["elites:"] = 3.0f;
    f.stats.counts["elites:base:blazing"] = 1.0f;
    f.stats.counts["status:base:burn"] = 12.0f;
    f.stats.counts["combos:"] = 2.0f;
    f.stats.counts["damage:fire"] = 450.0f;

    RunState lost;
    lost.wave = 6;
    lost.waves = 10;

    Progress p;
    countRun(p, f, lost, false);
    EXPECT_EQ(p.stats["runs"], 1.0f);
    EXPECT_EQ(p.stats["kills"], 40.0f);
    EXPECT_EQ(p.stats["damage"], 1500.0f);
    EXPECT_EQ(p.stats["presents"], 2.0f);
    EXPECT_EQ(p.stats["damage:base:burn"], 300.0f);
    EXPECT_EQ(p.stats["elites"], 3.0f);
    EXPECT_EQ(p.stats["elites:base:blazing"], 1.0f);
    EXPECT_EQ(p.stats["status:base:burn"], 12.0f);
    EXPECT_EQ(p.stats["combos"], 2.0f);
    EXPECT_EQ(p.stats["damage:fire"], 450.0f);
    EXPECT_EQ(p.stats["waves"], 5.0f);         // wave 6 lost: 5 cleared
    EXPECT_EQ(p.stats["bestWave"], 5.0f);
    EXPECT_EQ(p.stats["bestWave:base:sword"], 5.0f);
    EXPECT_EQ(p.stats["bestLevel"], 9.0f);
    EXPECT_EQ(p.stats["wins"], 0.0f);

    // A win on the second level: it counts for the first one too; the bests stay the best
    RunState won = lost;
    won.wave = 10;
    won.difficulty = 1;
    f.level = 4;
    countRun(p, f, won, true);
    EXPECT_EQ(p.stats["runs"], 2.0f);
    EXPECT_EQ(p.stats["kills"], 80.0f);
    EXPECT_EQ(p.stats["waves"], 15.0f);
    EXPECT_EQ(p.stats["bestWave"], 10.0f);
    EXPECT_EQ(p.stats["bestLevel"], 9.0f);
    EXPECT_EQ(p.stats["wins"], 1.0f);
    EXPECT_EQ(p.stats["wins:base:sword"], 1.0f);
    EXPECT_EQ(p.stats["wins:level:1"], 1.0f);
    EXPECT_EQ(p.stats["wins:level:2"], 1.0f);
    EXPECT_EQ(p.stats["wins:level:3"], 0.0f);
}

TEST(arena_progress, progress_is_saved_and_read_back)
{
    const auto path = (std::filesystem::temp_directory_path() / "voxelarena-progress-test.json").string();
    Progress p;
    p.stats["damage:base:burn"] = 1234.5f;
    p.stats["wins"] = 2.0f;
    p.done = {"base:victor", "base:firestarter"};
    ASSERT_TRUE(saveProgress(path, p));

    Progress back;
    ASSERT_TRUE(loadProgress(path, back));
    EXPECT_EQ(back.stats, p.stats);
    EXPECT_EQ(back.done, p.done);
    std::filesystem::remove(path);

    EXPECT_FALSE(loadProgress(path, back));   // none yet: a new player
}

TEST(arena_progress, locked_content_never_comes_up_in_a_run)
{
    Content content = loadContent(VOXELARENA_MODS_DIR);
    ContentReport report;
    const Tuning tuning = readTuning(content.tuning, content.tuningFile, report);
    content.run.chooseWeapon = false;
    content.run.intermission = 0.0f;
    content.run.difficulties.clear();
    content.arena.size = glm::ivec3(32, 16, 32);
    content.arena.shapes = parseJson(R"([ { "block": "stone", "from": [0, 0, 0], "to": [31, 0, 31] } ])").value;
    content.arena.spawns.clear();
    content.arena.playerSpawn = glm::vec3(16, 1, 8);

    ArenaWorld world;
    world.start(content, tuning);
    world.aiEnabled = false;

    const auto locked = lockedContent(Progress{}, content);
    Fighter& p = *world.player();
    p.locked = locked;

    // The first spells: only open ones
    for (int k = 0; k < 20; ++k)
        for (const auto& id : world.rollFirstSpells(p))
            EXPECT_FALSE(locked.count(id)) << id;

    // A locked weapon cannot be taken
    world.chooseWeapon(p, "base:dagger");
    EXPECT_NE(p.weapon, "base:dagger");

    world.startRun(3);
    ASSERT_EQ(world.run.phase, RunPhase::Armory);   // the first spell to pick: among open ones only
    for (const auto& id : p.firstSpellOffer)
        EXPECT_FALSE(locked.count(id)) << id;
    {
        InputFrame in;
        in.pick = InputFrame::FIRST_SPELL;
        world.step(in);
    }
    ASSERT_EQ(world.run.phase, RunPhase::Fighting);
    world.run.timer = 0.0f;
    world.step(InputFrame{});
    ASSERT_EQ(world.run.phase, RunPhase::Shop);

    // Many level ups and shop rolls: nothing locked in them
    for (int k = 0; k < 60; ++k)
    {
        p.levelUps = 1;
        world.rollLevel(p);
        for (const auto& id : p.levelOffer)
            EXPECT_FALSE(lockedId(locked, id)) << id;

        p.shards = 1000;
        InputFrame in;
        in.pick = InputFrame::REROLL;
        world.step(in);
        for (const auto& o : p.shop)
            EXPECT_FALSE(lockedId(locked, o.id)) << o.id;
    }
}
