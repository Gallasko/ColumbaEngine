#include "stdafx.h"

#include <gtest/gtest.h>

#include <algorithm>

#include "Core/content.h"
#include "Core/scriptbrain.h"
#include "Core/tuning.h"
#include "Core/world.h"

using namespace arena;

// The arena run: waves, boons between them, a boss, a score.

namespace
{
    struct RunArena
    {
        Content content;
        Tuning tuning;
        ArenaWorld world;

        explicit RunArena(std::vector<WaveDef> waves = {})
        {
            content = loadContent(VOXELARENA_MODS_DIR);
            ContentReport r;
            tuning = readTuning(content.tuning, content.tuningFile, r);

            // A bare floor; the run comes in around its middle
            content.arena.size = glm::ivec3(32, 16, 32);
            content.arena.shapes = parseJson(R"([ { "block": "stone", "from": [0, 0, 0], "to": [31, 0, 31] } ])").value;
            content.arena.spawns.clear();
            content.arena.playerSpawn = glm::vec3(16, 1, 8);
            content.run.centre = glm::vec3(16, 1, 18);
            content.run.spawnRadius = 7.0f;

            if (not waves.empty())
                content.run.waves = waves;

            world.start(content, tuning);
            world.aiEnabled = false;
        }

        Fighter& player() { return *world.player(); }

        void run(int steps, const InputFrame& in = {})
        {
            for (int i = 0; i < steps; ++i)
                world.step(in);
        }

        int enemies() const
        {
            return static_cast<int>(std::count_if(world.fighters.begin(), world.fighters.end(), [](const Fighter& f) { return f.team == Team::Enemy and f.alive(); }));
        }

        /** Every enemy burns to death this step (a real death: it counts for the score). */
        void killAll()
        {
            for (auto& f : world.fighters)
                if (f.team == Team::Enemy and f.alive())
                {
                    f.health = 0.01f;
                    world.applyStatus(f, "base:burn");
                }
            run(2);
        }

        void pick(int n)
        {
            InputFrame in;
            in.pick = n;
            world.step(in);
        }

        void past(float seconds) { run(static_cast<int>(seconds * 60.0f) + 2); }
    };

    WaveDef wave(const std::string& enemy, int count, const std::string& boss = "")
    {
        WaveDef w;
        w.enemies.emplace_back(enemy, glm::ivec2(count));
        w.boss = boss;
        return w;
    }
}

TEST(arena_run, waves_come_after_a_warning_and_a_cleared_one_offers_boons)
{
    RunArena r({wave("base:skulker", 2), wave("base:brute", 1)});
    r.world.startRun(42);

    EXPECT_EQ(r.world.run.phase, RunPhase::Warn);
    EXPECT_EQ(r.world.run.wave, 1);
    EXPECT_EQ(r.enemies(), 0);   // not yet: the spawn points show first
    EXPECT_EQ(r.world.run.pending.size(), 2u);

    r.past(r.content.run.warn);
    EXPECT_EQ(r.world.run.phase, RunPhase::Fighting);
    EXPECT_EQ(r.enemies(), 2);

    r.killAll();
    EXPECT_EQ(r.world.run.phase, RunPhase::Choosing);
    ASSERT_EQ(r.player().offer.size(), static_cast<size_t>(r.content.run.choices));
    EXPECT_EQ(r.world.run.kills, 2);
    EXPECT_EQ(r.world.run.score, 2 * r.content.enemies.at("base:skulker").score + r.content.run.waveScore);

    // Their bodies leave the arena
    r.past(2.0f);
    EXPECT_TRUE(std::none_of(r.world.fighters.begin(), r.world.fighters.end(), [](const Fighter& f) { return f.team == Team::Enemy; }));

    // A pick, a breather, the next wave
    const std::string chosen = r.player().offer[1];
    r.pick(2);
    EXPECT_EQ(r.player().boons, std::vector<std::string>{chosen});
    EXPECT_TRUE(r.player().offer.empty());
    EXPECT_EQ(r.world.run.phase, RunPhase::Breather);
    r.past(r.content.run.breather + r.content.run.warn);
    EXPECT_EQ(r.world.run.wave, 2);
    EXPECT_EQ(r.enemies(), 1);
}

TEST(arena_run, boons_change_the_player)
{
    RunArena r;
    Fighter& p = r.player();
    const float health = p.maxHealth;

    r.world.startRun(7);
    p.offer = {"base:tough", "base:nimble", "base:leech"};
    r.world.run.phase = RunPhase::Choosing;
    r.pick(1);
    EXPECT_FLOAT_EQ(r.player().maxHealth, health + r.content.upgrades.at("base:tough").maxHealth);

    r.player().offer = {"base:nimble"};
    r.world.run.phase = RunPhase::Choosing;
    r.pick(1);
    EXPECT_EQ(r.world.maxDashes(r.player()), r.tuning.dash.charges + 1);

    // Picked to its most, it is not offered again
    for (int i = 0; i < 10; ++i)
        r.player().boons.push_back("base:nimble");
    r.world.sumBoons(r.player());
    r.world.run.phase = RunPhase::Fighting;
    r.world.run.wave = 1;
    r.world.run.waves = 5;
    r.killAll();
    r.run(2);
    for (const auto& id : r.player().offer)
        EXPECT_NE(id, "base:nimble");
}

TEST(arena_run, falling_is_a_defeat_and_start_plays_again_from_the_beginning)
{
    RunArena r({wave("base:skulker", 1), wave("base:skulker", 1)});
    r.world.startRun(3);
    r.past(r.content.run.warn);
    r.player().boons = {"base:keen"};

    // Down, alone: lost
    r.player().health = 0.01f;
    r.world.applyStatus(r.player(), "base:burn");
    r.run(3);
    EXPECT_EQ(r.world.run.phase, RunPhase::Defeat);
    r.past(5.0f);
    EXPECT_FALSE(r.player().alive());   // no getting up in a run

    // Again: a fresh run
    r.pick(InputFrame::START);
    EXPECT_EQ(r.world.run.phase, RunPhase::Warn);
    EXPECT_EQ(r.world.run.wave, 1);
    EXPECT_EQ(r.world.run.score, 0);
    EXPECT_TRUE(r.player().alive());
    EXPECT_TRUE(r.player().boons.empty());
}

TEST(arena_run, the_boss_wave_ends_in_victory_with_a_time_bonus)
{
    RunArena r({wave("base:skulker", 1, "base:warden")});
    r.world.startRun(11);
    r.past(r.content.run.warn);

    const Fighter* boss = r.world.find(r.world.run.bossId);
    ASSERT_TRUE(boss);
    EXPECT_EQ(boss->type, "base:warden");

    r.killAll();
    EXPECT_EQ(r.world.run.phase, RunPhase::Victory);
    const int noBonus = r.content.enemies.at("base:skulker").score + r.content.enemies.at("base:warden").score + r.content.run.waveScore;
    EXPECT_GT(r.world.run.score, noBonus);   // quick: most of the time bonus
    EXPECT_LE(r.world.run.score, noBonus + r.content.run.timeBonus);
}

TEST(arena_run, the_warden_takes_volt_hard_and_fire_lightly)
{
    auto damageBy = [](const std::string& element) {
        RunArena r;
        const int id = r.world.spawnEnemy("base:warden", r.player().body.position + glm::vec3(0, 0, 3.0f), 180.0f);
        r.run(2);
        Fighter* w = r.world.find(id);
        const float before = w->health;

        // A bolt of the element, straight at it
        SpellRecipe recipe;
        recipe.element = element;
        recipe.keywords = {"base:bolt"};
        r.player().spells = {recipe, {}, {}};
        int key = -1;
        for (int i = 0; i < InputFrame::ABILITIES; ++i)
            if (r.world.spellSlotAt(i) == 0)
                key = i;
        InputFrame in;
        in.abilityPressed[key] = true;
        in.aimOrigin = r.player().body.position + glm::vec3(0, 1.4f, 0);
        in.aimDir = glm::normalize(w->body.position + glm::vec3(0, 1.4f, 0) - in.aimOrigin);
        r.world.step(in);
        r.run(20);
        return before - r.world.find(id)->health;
    };

    const float volt = damageBy("volt"), fire = damageBy("fire"), ice = damageBy("ice");
    EXPECT_NEAR(volt / ice, 1.5f, 0.05f);
    EXPECT_LT(fire, ice);   // its weakness numbers, and burning on top would only add to fire
}

TEST(arena_run, the_wardens_brain_runs_and_it_attacks)
{
    RunArena r;
    ScriptBrain brain;
    ContentReport report;
    brain.load(r.content, report);
    ASSERT_TRUE(report.errors.empty()) << report.errors.front().text;
    r.world.setBrain(&brain);
    r.world.aiEnabled = true;

    const int id = r.world.spawnEnemy("base:warden", r.player().body.position + glm::vec3(0, 0, 3.0f), 180.0f);
    r.world.godMode = true;

    std::vector<std::string> attacks;
    for (int i = 0; i < 60 * 12; ++i)
    {
        r.world.step(InputFrame{});
        const Fighter* w = r.world.find(id);
        ASSERT_TRUE(w);
        EXPECT_TRUE(w->aiLastError.empty()) << w->aiLastError;
        if (w->action == Action::Attack and (attacks.empty() or attacks.back() != w->attack))
            attacks.push_back(w->attack);
    }

    EXPECT_FALSE(attacks.empty());
    EXPECT_TRUE(std::find(attacks.begin(), attacks.end(), "slam") != attacks.end());
}

TEST(arena_run, it_plays_in_its_own_arena_built_anew_for_each_wave)
{
    RunArena r({wave("base:skulker", 1), wave("base:skulker", 1)});
    ASSERT_TRUE(r.content.run.hasArena);
    const glm::ivec3 training(r.world.grid.width(), r.world.grid.height(), r.world.grid.depth());

    r.world.startRun(9);
    const glm::ivec3 size = r.content.run.arena.size;
    EXPECT_EQ(glm::ivec3(r.world.grid.width(), r.world.grid.height(), r.world.grid.depth()), size);
    EXPECT_NEAR(glm::length(r.player().body.position - r.content.run.arena.playerSpawn), 0.0f, 0.5f);

    // Wave 1: a pillar broken, the player wanders off
    r.past(r.content.run.warn);
    const glm::ivec3 pillar(9, 5, 13);
    const BlockId stone = r.world.grid.get(pillar.x, pillar.y, pillar.z);
    ASSERT_NE(stone, AIR);
    r.world.terrain.damage(pillar, 1e6f);
    r.run(2);
    EXPECT_EQ(r.world.grid.get(pillar.x, pillar.y, pillar.z), AIR);
    r.player().body.position = glm::vec3(30, 4, 30);

    // Cleared, a boon, the breather: wave 2 on a whole arena, from the start
    r.killAll();
    r.pick(1);
    const int builds = r.world.arenaBuilds;
    r.past(r.content.run.breather);
    EXPECT_EQ(r.world.run.wave, 2);
    EXPECT_GT(r.world.arenaBuilds, builds);
    EXPECT_EQ(r.world.grid.get(pillar.x, pillar.y, pillar.z), stone);
    EXPECT_LT(glm::length(r.player().body.position - r.player().spawnPosition), 1.0f);

    // Training: back to the big arena
    r.world.stopRun();
    EXPECT_EQ(glm::ivec3(r.world.grid.width(), r.world.grid.height(), r.world.grid.depth()), training);
}
