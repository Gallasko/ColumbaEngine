#include "stdafx.h"

#include <gtest/gtest.h>

#include <algorithm>

#include "Core/content.h"
#include "Core/scriptbrain.h"
#include "Core/tuning.h"
#include "Core/world.h"

using namespace arena;

// The arena run: timed waves with enemies coming in all the while, shards, the shop, bosses, a score.

namespace
{
    struct RunArena
    {
        Content content;
        Tuning tuning;
        ArenaWorld world;

        RunArena()
        {
            content = loadContent(VOXELARENA_MODS_DIR);
            ContentReport r;
            tuning = readTuning(content.tuning, content.tuningFile, r);

            // Training: a bare floor (the run has its own arena, run.json's)
            content.arena.size = glm::ivec3(32, 16, 32);
            content.arena.shapes = parseJson(R"([ { "block": "stone", "from": [0, 0, 0], "to": [31, 0, 31] } ])").value;
            content.arena.spawns.clear();
            content.arena.playerSpawn = glm::vec3(16, 1, 8);

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

        /** Every enemy burns to death this step (a real death: it counts and drops its shards). */
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

        /** The wave's time is up: on to the shop. */
        void timeUp()
        {
            world.run.timer = 0.0f;
            run(1);
        }

        /** An enemy right by the player. */
        Fighter& near(const std::string& type, float distance = 3.0f)
        {
            const int id = world.spawnEnemy(type, player().body.position + glm::vec3(0, 0, distance), 180.0f);
            return *world.find(id);
        }
    };
}

TEST(arena_run, a_wave_is_timed_and_enemies_keep_coming_from_the_edges)
{
    RunArena r;
    r.world.startRun(42);

    EXPECT_EQ(r.world.run.phase, RunPhase::Fighting);
    EXPECT_EQ(r.world.run.wave, 1);
    EXPECT_EQ(r.world.run.waves, r.content.run.waves);
    EXPECT_FLOAT_EQ(r.world.run.timer, r.content.run.duration.x);
    EXPECT_EQ(r.enemies(), 0);

    // A spawn point shows first, then they come; more as the wave goes on
    r.past(1.0f / r.content.run.spawnRate.x + 0.1f);
    EXPECT_FALSE(r.world.run.pending.empty());
    r.past(r.content.run.warn);
    const int early = r.enemies();
    EXPECT_GE(early, 1);
    r.past(6.0f);
    EXPECT_GT(r.enemies(), early);

    for (const auto& f : r.world.fighters)
        if (f.team == Team::Enemy)
            EXPECT_GT(glm::length(f.body.position - r.player().body.position), 4.0f);   // not on top of the player

    // The time is up: whoever is left goes, the shop opens
    r.past(r.content.run.duration.x);
    EXPECT_EQ(r.world.run.phase, RunPhase::Shop);
    EXPECT_EQ(r.enemies(), 0);
    EXPECT_EQ(r.player().shop.size(), static_cast<size_t>(r.content.run.shopSlots));
    EXPECT_EQ(r.world.run.score, r.content.run.waveScore);
}

TEST(arena_run, kills_drop_shards_that_fly_to_the_player_and_level_it_up)
{
    RunArena r;
    r.world.startRun(3);
    const int drops = r.content.enemies.at("base:brute").drops;
    ASSERT_GT(drops, 0);

    r.near("base:brute");
    r.killAll();
    int lying = 0;
    for (const auto& p : r.world.pickups)
        lying += p.value;
    EXPECT_EQ(lying, drops);
    EXPECT_EQ(r.world.run.kills, 1);

    // In pickup range: they come to the player
    r.past(1.5f);
    EXPECT_TRUE(r.world.pickups.empty());
    EXPECT_EQ(r.player().shards, drops);
    EXPECT_EQ(r.player().xp, drops);

    // Out of range they stay where they fell, until the wave ends: then they are the player's anyway
    r.near("base:brute", 12.0f);
    r.killAll();
    r.past(1.5f);
    EXPECT_FALSE(r.world.pickups.empty());

    // Enough for a level: a free stat item to pick in the shop
    r.player().xp = r.world.xpFor(1) - drops;
    r.timeUp();
    EXPECT_TRUE(r.world.pickups.empty());
    EXPECT_EQ(r.player().shards, 2 * drops);
    EXPECT_EQ(r.player().level, 2);
    EXPECT_EQ(r.player().levelUps, 1);
    ASSERT_EQ(r.player().levelOffer.size(), static_cast<size_t>(r.content.run.levelChoices));

    const std::string chosen = r.player().levelOffer[2];
    r.pick(InputFrame::LEVEL + 2);
    EXPECT_EQ(r.player().boons, std::vector<std::string>{chosen});
    EXPECT_EQ(r.player().levelUps, 0);
    EXPECT_TRUE(r.player().levelOffer.empty());
    EXPECT_EQ(r.player().shards, 2 * drops);   // free
}

TEST(arena_run, the_shop_sells_locks_and_rerolls)
{
    RunArena r;
    r.world.startRun(7);
    r.timeUp();
    ASSERT_EQ(r.world.run.phase, RunPhase::Shop);

    Fighter& p = r.player();
    p.shards = 500;

    // Locked: still there after a reroll
    const std::string locked = p.shop[0].id;
    r.pick(InputFrame::LOCK + 0);
    EXPECT_TRUE(p.shop[0].locked);

    const int price = r.world.rerollPrice(p);
    r.pick(InputFrame::REROLL);
    EXPECT_EQ(p.shards, 500 - price);
    EXPECT_GT(r.world.rerollPrice(p), price);   // dearer each time
    EXPECT_TRUE(std::any_of(p.shop.begin(), p.shop.end(), [&](const Fighter::ShopOffer& o) { return o.id == locked and o.locked; }));
    EXPECT_EQ(p.shop.size(), static_cast<size_t>(r.content.run.shopSlots));

    // Bought: a stat item is a boon, an unlock is owned
    const int before = p.shards;
    const Fighter::ShopOffer offer = p.shop[1];
    r.pick(InputFrame::BUY + 1);
    EXPECT_EQ(p.shards, before - offer.price);
    EXPECT_TRUE(p.shop[1].bought);

    if (r.content.upgrades.count(offer.id))
        EXPECT_EQ(std::count(p.boons.begin(), p.boons.end(), offer.id), 1);
    else
        EXPECT_TRUE(r.world.owns(p, offer.id));

    // Bought once is bought: no second time
    r.pick(InputFrame::BUY + 1);
    EXPECT_EQ(p.shards, before - offer.price);

    // Too dear: nothing
    p.shards = 0;
    r.pick(InputFrame::BUY + 2);
    EXPECT_FALSE(p.shop[2].bought);

    // Ready: the next wave
    r.pick(InputFrame::START);
    EXPECT_EQ(r.world.run.phase, RunPhase::Fighting);
    EXPECT_EQ(r.world.run.wave, 2);
    EXPECT_GT(r.world.run.timer, r.content.run.duration.x);   // longer each wave
}

TEST(arena_run, prices_grow_and_owned_unlocks_are_not_offered_again)
{
    RunArena r;
    r.world.startRun(11);
    Fighter& p = r.player();

    for (const auto& id : r.content.run.startOwned)
        EXPECT_TRUE(r.world.owns(p, id));

    // Everything to unlock owned: the shop has stat items only, and dearer later on
    for (const auto& [id, price] : r.content.run.unlocks)
        p.owned.push_back(id);

    r.world.skipToWave(9);
    r.timeUp();
    ASSERT_EQ(r.world.run.phase, RunPhase::Shop);

    for (const auto& o : p.shop)
    {
        ASSERT_TRUE(r.content.upgrades.count(o.id)) << o.id;
        EXPECT_GT(o.price, r.content.upgrades.at(o.id).price);
    }
}

TEST(arena_run, what_is_not_owned_cannot_be_used)
{
    RunArena r;
    r.world.startRun(5);
    Fighter& p = r.player();
    ASSERT_EQ(p.weapon, r.content.run.startWeapon);

    // Not the dagger, until bought
    const auto dagger = std::find(r.content.weaponOrder.begin(), r.content.weaponOrder.end(), "base:dagger");
    ASSERT_NE(dagger, r.content.weaponOrder.end());
    InputFrame in;
    in.equip = static_cast<int>(dagger - r.content.weaponOrder.begin()) + 1;
    r.world.step(in);
    EXPECT_EQ(p.weapon, r.content.run.startWeapon);

    // No weapon skill until bought
    for (int i = 0; i < InputFrame::ABILITIES; ++i)
        if (const AbilityDef* a = r.world.ability(i, &p))
            EXPECT_NE(a->kind, "attack") << a->id;

    // A spell: only of what is owned
    auto cast = [&](const std::string& element, const std::string& keyword) {
        SpellRecipe recipe;
        recipe.element = element;
        recipe.keywords = {keyword};
        r.world.setSpells(p, {recipe, {}, {}});
        p.mana = r.tuning.mana.max;
        std::fill(std::begin(p.abilityCooldown), std::end(p.abilityCooldown), 0.0f);
        int key = -1;
        for (int i = 0; i < InputFrame::ABILITIES; ++i)
            if (r.world.spellSlotAt(i) == 0)
                key = i;
        InputFrame c;
        c.abilityPressed[key] = true;
        c.aimDir = glm::vec3(0, 0, 1);
        r.world.step(c);
        r.run(2);
        return p.mana < r.tuning.mana.max - 1.0f;
    };

    EXPECT_TRUE(cast("fire", "base:bolt"));
    EXPECT_FALSE(cast("ice", "base:bolt"));
    EXPECT_FALSE(cast("fire", "base:nova"));

    // Bought in the shop: in hand at once
    r.timeUp();
    p.shop = {{"base:dagger", 5, false, false}};
    p.shards = 5;
    r.pick(InputFrame::BUY);
    EXPECT_EQ(p.weapon, "base:dagger");
    EXPECT_TRUE(r.world.owns(p, "base:dagger"));

    // Outside a run: everything again
    r.world.stopRun();
    EXPECT_TRUE(r.world.owns(r.player(), "element:ice"));
}

TEST(arena_run, elites_are_tougher_and_drop_more)
{
    RunArena r;
    r.content.run.eliteFrom = 1;
    r.content.run.eliteChance = 1.0f;
    r.content.run.pools = {{"base:skulker", 1, 1000, 1.0f}};
    r.world.startRun(9);
    r.past(1.0f / r.content.run.spawnRate.x + r.content.run.warn + 0.5f);

    const Fighter* elite = nullptr;
    for (const auto& f : r.world.fighters)
        if (f.team == Team::Enemy)
            elite = &f;

    ASSERT_TRUE(elite);
    EXPECT_TRUE(elite->elite);
    EXPECT_FLOAT_EQ(elite->maxHealth, r.content.enemies.at("base:skulker").health * 2.5f);

    r.killAll();
    int lying = 0;
    for (const auto& p : r.world.pickups)
        lying += p.value;
    EXPECT_EQ(lying, 3 * r.content.enemies.at("base:skulker").drops);
}

TEST(arena_run, boss_waves_last_until_the_boss_falls_and_the_last_is_a_victory)
{
    RunArena r;
    ASSERT_EQ(r.content.run.bosses.at(10), "base:chieftain");
    ASSERT_EQ(r.content.run.bosses.at(20), "base:warden");
    r.world.startRun(13);

    // Midway: the Chieftain, and the time does not end it
    r.world.skipToWave(10);
    EXPECT_TRUE(r.world.run.bossWave);
    r.past(2.5f);
    const Fighter* boss = r.world.find(r.world.run.bossId);
    ASSERT_TRUE(boss);
    EXPECT_EQ(boss->type, "base:chieftain");
    r.world.run.timer = 0.0f;
    r.run(5);
    EXPECT_EQ(r.world.run.phase, RunPhase::Fighting);

    r.killAll();
    EXPECT_EQ(r.world.run.phase, RunPhase::Shop);

    // The last: the Warden, then the victory
    r.world.skipToWave(20);
    r.past(2.5f);
    boss = r.world.find(r.world.run.bossId);
    ASSERT_TRUE(boss);
    EXPECT_EQ(boss->type, "base:warden");
    const int score = r.world.run.score;
    r.killAll();
    EXPECT_EQ(r.world.run.phase, RunPhase::Victory);
    EXPECT_GE(r.world.run.score, score + r.content.run.victoryScore + r.content.run.waveScore * 20);
}

TEST(arena_run, falling_is_a_defeat_and_start_plays_again_from_the_beginning)
{
    RunArena r;
    r.world.startRun(3);
    r.player().boons = {"base:keen"};
    r.player().shards = 40;
    r.player().owned.push_back("base:dagger");

    // Down, alone: lost
    r.player().health = 0.01f;
    r.world.applyStatus(r.player(), "base:burn");
    r.run(3);
    EXPECT_EQ(r.world.run.phase, RunPhase::Defeat);
    r.past(5.0f);
    EXPECT_FALSE(r.player().alive());   // no getting up in a run

    // Again: a fresh run
    r.pick(InputFrame::START);
    EXPECT_EQ(r.world.run.phase, RunPhase::Fighting);
    EXPECT_EQ(r.world.run.wave, 1);
    EXPECT_EQ(r.world.run.score, 0);
    EXPECT_TRUE(r.player().alive());
    EXPECT_TRUE(r.player().boons.empty());
    EXPECT_EQ(r.player().shards, 0);
    EXPECT_FALSE(r.world.owns(r.player(), "base:dagger"));
}

TEST(arena_run, stat_items_change_the_player)
{
    RunArena r;
    r.world.startRun(7);
    Fighter& p = r.player();
    const float health = p.maxHealth;

    r.timeUp();
    p.shards = 1000;
    p.shop = {{"base:tough", 1}, {"base:nimble", 1}, {"base:mending", 1}, {"base:plated", 1}};
    for (int i = 0; i < 4; ++i)
        r.pick(InputFrame::BUY + i);

    EXPECT_FLOAT_EQ(p.maxHealth, health + r.content.upgrades.at("base:tough").maxHealth);
    EXPECT_EQ(r.world.maxDashes(p), r.tuning.dash.charges + 1);
    EXPECT_FLOAT_EQ(p.boon.armor, r.content.upgrades.at("base:plated").armor);

    // Regeneration
    p.health = 10.0f;
    r.past(1.0f);
    EXPECT_NEAR(p.health, 10.0f + r.content.upgrades.at("base:mending").regen, 0.1f);

    // At its most, it is not offered again
    for (int i = 0; i < 10; ++i)
        p.boons.push_back("base:nimble");
    r.world.sumBoons(p);
    r.pick(InputFrame::REROLL);
    for (const auto& o : p.shop)
        EXPECT_NE(o.id, "base:nimble");
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

TEST(arena_run, the_bosses_brains_run_and_they_attack)
{
    for (const auto& [type, expected] : std::vector<std::pair<std::string, std::string>>{{"base:warden", "slam"}, {"base:chieftain", "slam"}})
    {
        RunArena r;
        ScriptBrain brain;
        ContentReport report;
        brain.load(r.content, report);
        ASSERT_TRUE(report.errors.empty()) << report.errors.front().text;
        r.world.setBrain(&brain);
        r.world.aiEnabled = true;

        const int id = r.world.spawnEnemy(type, r.player().body.position + glm::vec3(0, 0, 3.0f), 180.0f);
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

        EXPECT_FALSE(attacks.empty()) << type;
        EXPECT_TRUE(std::find(attacks.begin(), attacks.end(), expected) != attacks.end()) << type;
    }
}

TEST(arena_run, enemies_in_a_run_come_for_the_player_from_anywhere)
{
    RunArena r;
    ScriptBrain brain;
    ContentReport report;
    brain.load(r.content, report);
    r.world.setBrain(&brain);
    r.world.aiEnabled = true;
    r.world.godMode = true;
    r.world.startRun(21);

    // Far past its engage range
    const float range = static_cast<float>(r.content.enemies.at("base:brute").params.find("engageRange")->number);
    Fighter& brute = r.near("base:brute", std::min(range + 6.0f, 28.0f));
    const int id = brute.id;
    const float start = glm::length(brute.body.position - r.player().body.position);
    r.past(2.0f);
    EXPECT_LT(glm::length(r.world.find(id)->body.position - r.player().body.position), start - 2.0f);
}

TEST(arena_run, it_plays_in_its_own_arena_built_anew_for_each_wave)
{
    RunArena r;
    ASSERT_TRUE(r.content.run.hasArena);
    const glm::ivec3 training(r.world.grid.width(), r.world.grid.height(), r.world.grid.depth());

    r.world.startRun(9);
    const glm::ivec3 size = r.content.run.arena.size;
    EXPECT_EQ(glm::ivec3(r.world.grid.width(), r.world.grid.height(), r.world.grid.depth()), size);
    EXPECT_NEAR(glm::length(r.player().body.position - r.content.run.arena.playerSpawn), 0.0f, 0.5f);

    // Wave 1: a pillar broken, the player wanders off
    const glm::ivec3 pillar(9, 5, 13);
    const BlockId stone = r.world.grid.get(pillar.x, pillar.y, pillar.z);
    ASSERT_NE(stone, AIR);
    r.world.terrain.damage(pillar, 1e6f);
    r.run(2);
    EXPECT_EQ(r.world.grid.get(pillar.x, pillar.y, pillar.z), AIR);
    r.player().body.position = glm::vec3(30, 4, 30);

    // Through the shop: wave 2 on a whole arena, from the start
    r.timeUp();
    const int builds = r.world.arenaBuilds;
    r.pick(InputFrame::START);
    EXPECT_EQ(r.world.run.wave, 2);
    EXPECT_GT(r.world.arenaBuilds, builds);
    EXPECT_EQ(r.world.grid.get(pillar.x, pillar.y, pillar.z), stone);
    EXPECT_LT(glm::length(r.player().body.position - r.player().spawnPosition), 1.0f);

    // Training: back to the big arena
    r.world.stopRun();
    EXPECT_EQ(glm::ivec3(r.world.grid.width(), r.world.grid.height(), r.world.grid.depth()), training);
}
