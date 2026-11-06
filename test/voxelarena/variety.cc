#include "stdafx.h"

#include <gtest/gtest.h>

#include <algorithm>

#include "Core/content.h"
#include "Core/tuning.h"
#include "Core/world.h"
#include "Core/net/protocol.h"

using namespace arena;

// What keeps runs from playing the same: elites' affixes (affixes.json), the enemies each arena brings (a sapper's
// blast, a mender's mending, a rimecaller's frost, a golem's pebbles), the level ups' rerolls and banishes, the
// recap's damage by source, and the trigger and trade-off items.

namespace
{
    /** The shipped content in a run (no enemies coming in by themselves), or on a bare training floor. */
    struct Variety
    {
        Content content;
        Tuning tuning;
        ArenaWorld world;

        explicit Variety(bool run = true)
        {
            content = loadContent(VOXELARENA_MODS_DIR);
            ContentReport r;
            tuning = readTuning(content.tuning, content.tuningFile, r);

            content.arena.size = glm::ivec3(32, 16, 32);
            content.arena.shapes = parseJson(R"([ { "block": "stone", "from": [0, 0, 0], "to": [31, 0, 31] } ])").value;
            content.arena.spawns.clear();
            content.arena.playerSpawn = glm::vec3(16, 1, 8);

            content.run.chooseWeapon = false;
            content.run.intermission = 0.0f;
            content.run.startSpells = {"base:firebolt"};
            content.run.startChoices.clear();
            content.run.difficulties.clear();
            content.run.spawnRate = glm::vec2(0.0f);   // only the enemies a test puts there

            world.start(content, tuning);
            world.aiEnabled = false;
            if (run)
                world.startRun(7);
            step(30);
        }

        Fighter& player() { return *world.player(); }
        Fighter& f(int id) { return *world.find(id); }

        void step(int n, const InputFrame& in = {})
        {
            for (int i = 0; i < n; ++i)
                world.step(in);
        }

        /** An enemy this far from the player (x across, z ahead), on the ground there. */
        int spawn(const std::string& type, glm::vec3 offset, const std::vector<std::string>& affixes = {})
        {
            const glm::vec3 at = world.groundNear(player().body.position + offset);
            const int id = world.spawnEnemy(type, at, 180.0f);
            if (not affixes.empty())
            {
                f(id).elite = true;
                f(id).affixes = affixes;
            }
            return id;
        }

        static AttackDef blow(float damage, const std::string& name = "test")
        {
            AttackDef hit;
            hit.name = name;
            hit.damage = damage;
            hit.poiseDamage = 0.0f;
            hit.knockback = 0.0f;
            hit.hitstop = 0;
            return hit;
        }

        float lost(int id, const AttackDef& hit)
        {
            const float before = f(id).health;
            world.applyHit(player(), f(id), hit);
            return before - f(id).health;
        }

        float took(int from, const AttackDef& hit)
        {
            const float before = player().health;
            world.applyHit(f(from), player(), hit);
            return before - player().health;
        }

        void kill(int id) { world.applyHit(player(), f(id), blow(1e6f)); }

        int count(const std::string& kind) const
        {
            return static_cast<int>(std::count_if(world.events.begin(), world.events.end(), [&](const WorldEvent& e) { return e.kind == kind; }));
        }

        int alive(const std::string& type) const
        {
            return static_cast<int>(std::count_if(world.fighters.begin(), world.fighters.end(), [&](const Fighter& o) { return o.type == type and o.alive(); }));
        }

        bool has(int id, const std::string& status) { return ArenaWorld::hasStatus(f(id), status); }
    };
}

// ---- Elites and the arenas' own enemies --------------------------------------------------------

TEST(arena_variety, each_arena_after_the_first_brings_an_enemy_of_its_own)
{
    Variety v;
    for (const char* id : {"base:sapper", "base:mender", "base:rimecaller", "base:golem", "base:pebble"})
        EXPECT_TRUE(v.content.enemies.count(id)) << id;
    EXPECT_TRUE(v.content.enemies.at("base:sapper").blast.everyone);
    EXPECT_EQ(v.content.enemies.at("base:golem").split.enemy, "base:pebble");
    EXPECT_TRUE(v.content.enemies.at("base:rimecaller").moveset.attacks.at("shot").projectile.ground.any());

    // A pool opens with each arena's first wave, of an enemy no earlier wave had
    const auto& pools = v.content.run.pools;
    for (const auto& a : v.content.run.arenas)
    {
        if (a.from <= 1)
            continue;
        const bool own = std::any_of(pools.begin(), pools.end(), [&](const SpawnPool& p) {
            return p.from == a.from and std::none_of(pools.begin(), pools.end(), [&](const SpawnPool& q) { return q.enemy == p.enemy and q.from < a.from; });
        });
        EXPECT_TRUE(own) << a.arena;
    }
}

TEST(arena_variety, an_elite_comes_with_its_waves_affixes)
{
    Variety v;
    EXPECT_GE(v.content.affixes.size(), 6u);
    EXPECT_EQ(v.content.run.eliteAffixesAt(6), 0);
    EXPECT_EQ(v.content.run.eliteAffixesAt(7), 1);
    EXPECT_EQ(v.content.run.eliteAffixesAt(20), 2);

    v.world.run.wave = 15;
    v.world.run.pending.push_back({"base:brute", v.world.groundNear(v.player().body.position + glm::vec3(0, 0, 6)), 0.0f, false, true});
    v.step(2);

    const auto e = std::find_if(v.world.fighters.begin(), v.world.fighters.end(), [](const Fighter& o) { return o.type == "base:brute"; });
    ASSERT_NE(e, v.world.fighters.end());
    EXPECT_TRUE(e->elite);
    ASSERT_EQ(e->affixes.size(), 2u);
    EXPECT_NE(e->affixes[0], e->affixes[1]);
    for (const auto& id : e->affixes)
        EXPECT_TRUE(v.content.affixes.count(id)) << id;

    float more = 0.0f;
    for (const auto& id : e->affixes)
        more += v.content.affixes.at(id).health;
    EXPECT_NEAR(e->maxHealth, v.content.enemies.at("base:brute").health * v.world.enemyHealthScale(15) * 2.5f * (1.0f + more), 0.5f);
}

TEST(arena_variety, a_swift_elite_moves_faster)
{
    Variety v;
    const int plain = v.spawn("base:brute", glm::vec3(-3, 0, 6));
    const int swift = v.spawn("base:brute", glm::vec3(3, 0, 6), {"base:swift"});
    EXPECT_NEAR(v.world.speedScale(v.f(swift)), v.world.speedScale(v.f(plain)) * (1.0f + v.content.affixes.at("base:swift").speed), 1e-4f);
}

TEST(arena_variety, a_guardian_wards_the_enemies_close_to_it_and_not_itself)
{
    Variety v;
    const int guardian = v.spawn("base:brute", glm::vec3(0, 0, 8), {"base:guardian"});
    const int close = v.spawn("base:brute", glm::vec3(2, 0, 8));
    const int far = v.spawn("base:brute", glm::vec3(-9, 0, 8));
    v.step(2);

    EXPECT_TRUE(v.has(close, "base:warded"));
    EXPECT_FALSE(v.has(far, "base:warded"));
    EXPECT_FALSE(v.has(guardian, "base:warded"));
    EXPECT_NEAR(v.lost(close, Variety::blow(20.0f)), 10.0f, 0.01f);
    EXPECT_NEAR(v.lost(far, Variety::blow(20.0f)), 20.0f, 0.01f);

    // The ward stays on while it stays close, a moment after it leaves
    v.step(60);
    EXPECT_TRUE(v.has(close, "base:warded"));
    v.f(close).body.position.x += 12.0f;
    v.step(80);
    EXPECT_FALSE(v.has(close, "base:warded"));
}

TEST(arena_variety, a_blazing_elite_leaves_fire_that_burns_the_players)
{
    Variety v;
    const int e = v.spawn("base:brute", glm::vec3(0, 0, 8), {"base:blazing"});
    v.step(30);

    const auto fire = std::find_if(v.world.zones.begin(), v.world.zones.end(), [](const Zone& z) { return z.team == Team::Enemy and z.element == Element::Fire; });
    ASSERT_NE(fire, v.world.zones.end());
    EXPECT_EQ(fire->status.status, "base:burn");

    // It walks off; the player stands in what it left
    const glm::vec3 spot = fire->position;
    v.f(e).body.position.x += 10.0f;
    v.player().body.position = spot;
    const float before = v.player().health;
    v.step(40);
    EXPECT_LT(v.player().health, before);
    EXPECT_TRUE(ArenaWorld::hasStatus(v.player(), "base:burn"));

    // Its hits burn too
    v.player().statuses.clear();
    v.took(e, Variety::blow(1.0f));
    EXPECT_TRUE(ArenaWorld::hasStatus(v.player(), "base:burn"));
}

TEST(arena_variety, a_frosty_elites_hits_slow_the_player_and_never_freeze_it)
{
    Variety v;
    const int e = v.spawn("base:brute", glm::vec3(0, 0, 3), {"base:frosty"});
    const float speed = v.world.speedScale(v.player());

    for (int k = 0; k < 6; ++k)
        v.took(e, Variety::blow(1.0f));

    EXPECT_TRUE(ArenaWorld::hasStatus(v.player(), "base:rime"));
    EXPECT_FALSE(ArenaWorld::hasStatus(v.player(), "base:frozen"));
    EXPECT_LT(v.world.speedScale(v.player()), speed);
}

TEST(arena_variety, a_volatile_elite_explodes_a_moment_after_it_dies)
{
    Variety v;
    const int e = v.spawn("base:brute", glm::vec3(0, 0, 2), {"base:volatile"});
    const float health = v.player().health;
    v.kill(e);
    v.step(1);

    // First the ring, hurting nobody
    EXPECT_TRUE(std::any_of(v.world.zones.begin(), v.world.zones.end(), [](const Zone& z) { return z.blasting(); }));
    EXPECT_EQ(v.count("blast_warn"), 1);
    EXPECT_FLOAT_EQ(v.player().health, health);

    // Then the blast
    v.step(static_cast<int>(v.content.affixes.at("base:volatile").blast.delay * 60.0f) + 2);
    EXPECT_EQ(v.count("blast"), 1);
    EXPECT_LT(v.player().health, health);
    EXPECT_FALSE(std::any_of(v.world.zones.begin(), v.world.zones.end(), [](const Zone& z) { return z.blasting(); }));
}

TEST(arena_variety, a_sapper_the_player_kills_does_not_go_off)
{
    // Killed by a player before its fuse burns down: no blast (its keg is put out), whatever stands by it
    Variety v;
    const int sapper = v.spawn("base:sapper", glm::vec3(0, 0, 8));
    const int brute = v.spawn("base:brute", glm::vec3(1.5f, 0, 8));
    v.kill(sapper);
    v.step(70);

    EXPECT_FALSE(v.f(sapper).alive());
    EXPECT_FALSE(v.f(sapper).blownUp);
    EXPECT_FLOAT_EQ(v.f(brute).health, v.f(brute).maxHealth);
    EXPECT_FALSE(v.player().stats.bySource.count("base:sapper"));
}

TEST(arena_variety, a_sapper_s_blast_sets_off_another_sapper_and_hurts_the_enemies_too)
{
    // Its fuse burnt down: the blast hurts the enemies by it, and a sapper it kills goes off in turn (no player killed it)
    Variety v;
    const int first = v.spawn("base:sapper", glm::vec3(0, 0, 8));
    const int second = v.spawn("base:sapper", glm::vec3(1.2f, 0, 8));
    const int brute = v.spawn("base:brute", glm::vec3(-1.4f, 0, 8));
    v.f(second).health = 1.0f;

    Fighter& s = v.f(first);
    s.action = Action::Attack;
    s.attack = "fuse";
    s.frame = 0;
    v.step(v.content.enemies.at("base:sapper").moveset.attacks.at("fuse").total() + 70);

    EXPECT_LT(v.f(brute).health, v.f(brute).maxHealth);
    EXPECT_FALSE(v.f(second).alive());
    v.step(70);
    EXPECT_TRUE(v.f(second).blownUp);
}

TEST(arena_variety, a_sapper_that_lights_its_fuse_goes_up_and_gives_nothing)
{
    Variety v;
    const int sapper = v.spawn("base:sapper", glm::vec3(0, 0, 2));
    const int kills = v.world.run.kills, score = v.world.run.score;

    Fighter& s = v.f(sapper);
    s.action = Action::Attack;
    s.attack = "fuse";
    s.frame = 0;
    const float health = v.player().health;
    v.step(v.content.enemies.at("base:sapper").moveset.attacks.at("fuse").total() + 2);

    EXPECT_FALSE(v.f(sapper).alive());
    EXPECT_EQ(v.world.run.kills, kills);
    EXPECT_EQ(v.world.run.score, score);
    EXPECT_TRUE(v.world.pickups.empty());

    v.step(70);
    EXPECT_LT(v.player().health, health);   // it was right by the player
}

TEST(arena_variety, a_golem_breaks_into_pebbles_that_split_no_more)
{
    Variety v;
    const int golem = v.spawn("base:golem", glm::vec3(0, 0, 8));
    v.kill(golem);
    v.step(2);
    EXPECT_EQ(v.alive("base:pebble"), 3);
    EXPECT_EQ(v.count("split"), 1);

    for (const auto& o : v.world.fighters)
        if (o.type == "base:pebble")
            EXPECT_TRUE(o.split);

    // A splitting elite: two of its own kind, plain, with less health
    const int slime = v.spawn("base:slime", glm::vec3(4, 0, 8), {"base:splitting"});
    v.kill(slime);
    v.step(2);
    int slimes = 0;
    for (const auto& o : v.world.fighters)
        if (o.type == "base:slime" and o.alive())
        {
            ++slimes;
            EXPECT_FALSE(o.elite);
            EXPECT_TRUE(o.affixes.empty());
            EXPECT_NEAR(o.maxHealth, v.content.enemies.at("base:slime").health * v.world.enemyHealthScale(1) * v.content.affixes.at("base:splitting").split.health, 0.01f);
        }
    EXPECT_EQ(slimes, 2);

    // Those split no more
    for (auto& o : v.world.fighters)
        if (o.type == "base:slime" and o.alive())
            v.kill(o.id);
    v.step(2);
    EXPECT_EQ(v.alive("base:slime"), 0);
}

TEST(arena_variety, what_dies_as_the_wave_ends_does_not_split_into_the_shop)
{
    Variety v;
    const int golem = v.spawn("base:golem", glm::vec3(0, 0, 8));
    v.f(golem).health = 0.01f;
    v.world.applyStatus(v.f(golem), "base:burn", 1, v.player().id);   // it burns to death in the next step, before the wave ends
    v.world.run.timer = 0.0f;
    v.step(2);
    EXPECT_EQ(v.world.run.phase, RunPhase::Shop);
    EXPECT_EQ(v.alive("base:pebble"), 0);
    EXPECT_TRUE(std::none_of(v.world.fighters.begin(), v.world.fighters.end(), [](const Fighter& o) { return o.team == Team::Enemy; }));
}

TEST(arena_variety, outside_a_run_nothing_splits_its_enemies_come_back)
{
    Variety v(false);
    v.spawn("base:golem", glm::vec3(0, 0, 8));
    for (auto& o : v.world.fighters)
        if (o.type == "base:golem")
            v.kill(o.id);
    v.step(2);
    EXPECT_EQ(v.alive("base:pebble"), 0);
}

TEST(arena_variety, a_mender_mends_its_allies_but_never_a_crystal)
{
    Variety v;
    const int mender = v.spawn("base:mender", glm::vec3(0, 0, 8));
    const int brute = v.spawn("base:brute", glm::vec3(2, 0, 8));
    const int crystal = v.spawn("base:crystal", glm::vec3(-2, 0, 8));
    v.f(brute).health = v.f(brute).maxHealth * 0.5f;
    v.f(crystal).health = v.f(crystal).maxHealth * 0.5f;
    const float bruteBefore = v.f(brute).health, crystalBefore = v.f(crystal).health;

    Fighter& m = v.f(mender);
    m.action = Action::Attack;
    m.attack = "mend";
    m.frame = 0;
    v.step(v.content.enemies.at("base:mender").moveset.attacks.at("mend").startup + 3);

    EXPECT_GT(v.f(brute).health, bruteBefore);
    EXPECT_FLOAT_EQ(v.f(crystal).health, crystalBefore);
    EXPECT_EQ(v.count("enemy_heal"), 1);
    EXPECT_EQ(v.count("heal_burst"), 0);
}

TEST(arena_variety, a_rimecallers_orb_leaves_frost_on_the_ground)
{
    Variety v;
    const int caster = v.spawn("base:rimecaller", glm::vec3(0, 0, 12));

    Projectile p;
    p.id = 900;
    p.owner = caster;
    p.team = Team::Enemy;
    p.source = "base:rimecaller";
    p.attack = "shot";
    p.def = v.content.enemies.at("base:rimecaller").moveset.attacks.at("shot").projectile;
    p.position = v.player().body.position + glm::vec3(3, 3, 3);
    p.velocity = glm::vec3(0, -12, 0);
    v.world.projectiles.push_back(p);
    v.step(30);

    const auto frost = std::find_if(v.world.zones.begin(), v.world.zones.end(), [](const Zone& z) { return z.team == Team::Enemy and z.element == Element::Ice; });
    ASSERT_NE(frost, v.world.zones.end());
    EXPECT_EQ(frost->status.status, "base:rime");
}

// ---- The level ups: rerolls and banishes -------------------------------------------------------

TEST(arena_variety, a_level_up_can_be_rerolled_a_few_times_a_run)
{
    Variety v;
    Fighter& p = v.player();
    EXPECT_EQ(p.levelRerolls, v.content.run.levelRerolls);
    EXPECT_EQ(p.banishes, v.content.run.levelBanishes);

    p.levelUps = 1;
    v.world.run.timer = 0.0f;
    v.step(1);
    ASSERT_EQ(v.world.run.phase, RunPhase::Shop);
    ASSERT_FALSE(p.levelOffer.empty());
    const size_t picks = p.levelOffer.size();

    InputFrame reroll;
    reroll.pick = InputFrame::LEVEL_REROLL;
    v.world.step(reroll);
    EXPECT_EQ(p.levelRerolls, v.content.run.levelRerolls - 1);
    EXPECT_EQ(p.levelOffer.size(), picks);
    EXPECT_EQ(p.levelUps, 1);
    EXPECT_EQ(v.count("level_reroll"), 1);

    // None left: nothing happens
    p.levelRerolls = 0;
    const auto offer = p.levelOffer;
    v.world.step(reroll);
    EXPECT_EQ(p.levelOffer, offer);
}

TEST(arena_variety, a_banished_pick_is_replaced_and_never_offered_again)
{
    Variety v;
    Fighter& p = v.player();
    p.levelUps = 1;
    v.world.run.timer = 0.0f;
    v.step(1);
    ASSERT_EQ(v.world.run.phase, RunPhase::Shop);

    // A new spell on the first card, banished: another new thing in its place
    p.levelOffer = {"base:fireball", p.weapon, "base:glacier"};
    InputFrame banish;
    banish.pick = InputFrame::BANISH;
    v.world.step(banish);
    EXPECT_EQ(p.banishes, v.content.run.levelBanishes - 1);
    ASSERT_EQ(p.banished, std::vector<std::string>({"base:fireball"}));
    ASSERT_EQ(p.levelOffer.size(), 3u);
    EXPECT_NE(p.levelOffer[0], "base:fireball");
    EXPECT_NE(p.levelOffer[0], "base:glacier");
    EXPECT_TRUE(v.world.runSkill(p, p.levelOffer[0]) == nullptr) << p.levelOffer[0];

    for (int k = 0; k < 40; ++k)
    {
        v.world.rollLevel(p);
        EXPECT_EQ(std::count(p.levelOffer.begin(), p.levelOffer.end(), "base:fireball"), 0);
    }

    // A branch is what something grows into: no banishing it
    p.levelOffer = {"base:firebolt#0", "base:firebolt#1"};
    v.world.step(banish);
    EXPECT_EQ(p.banishes, v.content.run.levelBanishes - 1);
    EXPECT_EQ(p.levelOffer.size(), 2u);
}

// ---- The recap: damage by what dealt it --------------------------------------------------------

TEST(arena_variety, the_recap_counts_damage_by_what_dealt_it)
{
    Variety v;
    const int brute = v.spawn("base:brute", glm::vec3(0, 0, 3));
    v.f(brute).maxHealth = v.f(brute).health = 1e5f;
    Fighter& p = v.player();   // after the spawns: they may move the fighters in memory
    auto dealt = [&](const std::string& source) {
        auto it = v.player().stats.bySource.find(source);
        return it == v.player().stats.bySource.end() ? 0.0f : it->second;
    };

    // The weapon's attacks, a weapon skill, a spell, a combo
    const Moveset* m = v.world.movesetOf(p);
    ASSERT_TRUE(m);
    AttackDef light = m->attacks.at(m->light);
    const float byWeapon = v.lost(brute, light);
    EXPECT_NEAR(dealt(p.weapon), byWeapon, 0.01f);
    v.lost(brute, Variety::blow(10.0f, "@base:whirlwind"));
    EXPECT_NEAR(dealt("base:whirlwind"), 10.0f, 0.01f);
    AttackDef spell = Variety::blow(12.0f, "base:bolt");
    spell.source = "base:firebolt";
    v.lost(brute, spell);
    EXPECT_NEAR(dealt("base:firebolt"), 12.0f, 0.01f);
    AttackDef melt = Variety::blow(7.0f, "base:melt");
    melt.combo = true;
    v.lost(brute, melt);
    EXPECT_NEAR(dealt("base:melt"), 7.0f, 0.01f);

    // A burn left by the player: it counts too, under the status
    const float before = p.stats.damage;
    v.world.applyStatus(v.f(brute), "base:burn", 1, p.id);
    v.step(60);
    EXPECT_GT(dealt("base:burn"), 0.0f);
    EXPECT_NEAR(v.player().stats.damage - before, dealt("base:burn"), 0.01f);

    // A spell cast for real: its bolt's hit counts under the spell
    v.f(brute).statuses.clear();
    int slot = -1;
    for (size_t i = 0; i < v.tuning.abilities.size(); ++i)
        if (std::find(v.tuning.abilities[i].begin(), v.tuning.abilities[i].end(), "spell:1") != v.tuning.abilities[i].end())
            slot = static_cast<int>(i);
    ASSERT_GE(slot, 0);
    const float firebolt = dealt("base:firebolt");
    InputFrame cast;
    cast.abilityPressed[slot] = true;
    cast.aimOrigin = v.player().body.position + glm::vec3(0, 1.2f, 0);
    cast.aimDir = glm::normalize(v.f(brute).body.position + glm::vec3(0, 1.2f, 0) - cast.aimOrigin);
    v.world.step(cast);
    v.step(30);
    EXPECT_GT(dealt("base:firebolt"), firebolt);
}

// ---- Trade-off and trigger items ---------------------------------------------------------------

TEST(arena_variety, a_rarer_trade_off_is_better_never_worse)
{
    Variety v;
    Fighter& p = v.player();
    const int legendary = static_cast<int>(v.content.run.rarities.size()) - 1;
    const float k = v.world.rarityPower(legendary);
    ASSERT_GT(k, 1.0f);

    v.world.takeBoon(p, {"base:glass_cannon", legendary});
    EXPECT_FLOAT_EQ(p.boon.healthScale, -0.3f);
    EXPECT_NEAR(p.boon.damage, v.world.damageFalloff(0.35f * k), 1e-4f);
    EXPECT_NEAR(p.maxHealth, v.tuning.player.health * 0.7f, 0.01f);
    EXPECT_LE(p.health, p.maxHealth);

    v.world.takeBoon(p, {"base:juggernaut", legendary});
    EXPECT_FLOAT_EQ(p.boon.speed, -0.12f);
    EXPECT_NEAR(p.boon.armor, 0.12f * k, 1e-4f);

    v.world.takeBoon(p, {"base:cursed_coin", legendary});
    EXPECT_FLOAT_EQ(p.boon.damageTaken, 0.2f);

    v.world.takeBoon(p, {"base:overclock", legendary});
    EXPECT_FLOAT_EQ(p.boon.manaCost, -0.35f);
    EXPECT_NEAR(p.boon.cooldown, 0.25f * k, 1e-4f);
}

TEST(arena_variety, the_moment_items_add_damage_only_in_their_moment)
{
    auto with = [](const std::string& item, auto&& moment) {
        Variety v;
        const int brute = v.spawn("base:brute", glm::vec3(0, 0, 3));
        v.f(brute).maxHealth = v.f(brute).health = 1e5f;
        v.world.takeBoon(v.player(), {item, 0});
        const float plain = v.lost(brute, Variety::blow(10.0f));
        moment(v, brute);
        return std::make_pair(plain, v.lost(brute, Variety::blow(10.0f)));
    };

    auto [full, hurt] = with("base:last_stand", [](Variety& v, int) { v.player().health = v.player().maxHealth * 0.4f; });
    EXPECT_NEAR(full, 10.0f, 0.01f);
    EXPECT_NEAR(hurt, 14.0f, 0.01f);

    auto [ground, air] = with("base:sky_striker", [](Variety& v, int) { v.player().body.onGround = false; });
    EXPECT_NEAR(ground, 10.0f, 0.01f);
    EXPECT_NEAR(air, 13.5f, 0.01f);

    auto [healthy, low] = with("base:executioner", [](Variety& v, int b) { v.f(b).health = v.f(b).maxHealth * 0.2f; });
    EXPECT_NEAR(healthy, 10.0f, 0.01f);
    EXPECT_NEAR(low, 15.0f, 0.01f);

    auto [calm, struck] = with("base:grudge", [](Variety& v, int) { v.player().sinceHit = 1.0f; });
    EXPECT_NEAR(calm, 10.0f, 0.01f);
    EXPECT_NEAR(struck, 14.0f, 0.01f);
}

TEST(arena_variety, thorns_hit_back_and_a_cursed_coin_makes_hits_hurt_more)
{
    Variety v;
    const int brute = v.spawn("base:brute", glm::vec3(0, 0, 3));
    EXPECT_NEAR(v.took(brute, Variety::blow(10.0f)), 10.0f, 0.01f);

    v.world.takeBoon(v.player(), {"base:thorns", 0});
    const float before = v.f(brute).health;
    v.took(brute, Variety::blow(10.0f));
    EXPECT_NEAR(before - v.f(brute).health, 8.0f, 0.01f);
    EXPECT_EQ(v.count("thorns"), 1);
    EXPECT_NEAR(v.player().stats.bySource.at("base:thorns"), 8.0f, 0.01f);

    v.world.takeBoon(v.player(), {"base:cursed_coin", 0});
    EXPECT_NEAR(v.took(brute, Variety::blow(10.0f)), 12.0f, 0.01f);
}

TEST(arena_variety, soul_siphon_heals_on_each_kill)
{
    Variety v;
    v.world.takeBoon(v.player(), {"base:soul_siphon", 0});
    v.player().health = 50.0f;
    v.kill(v.spawn("base:slime", glm::vec3(0, 0, 3)));
    EXPECT_FLOAT_EQ(v.player().health, 52.0f);
}

TEST(arena_variety, ember_trail_leaves_burning_ground_where_a_roll_starts)
{
    Variety v;
    v.world.takeBoon(v.player(), {"base:ember_trail", 0});
    InputFrame roll;
    roll.dodgePressed = true;
    roll.move = glm::vec2(0, 1);
    v.world.step(roll);
    v.step(1);

    const auto fire = std::find_if(v.world.zones.begin(), v.world.zones.end(), [](const Zone& z) { return z.team == Team::Player and z.element == Element::Fire; });
    ASSERT_NE(fire, v.world.zones.end());
    EXPECT_EQ(fire->status.status, "base:burn");
    EXPECT_EQ(fire->source, "base:ember_trail");
}

TEST(arena_variety, static_charge_zaps_on_every_fifth_weapon_hit)
{
    Variety v;
    v.world.takeBoon(v.player(), {"base:static_charge", 0});
    const int a = v.spawn("base:brute", glm::vec3(0, 0, 3));
    const int b = v.spawn("base:brute", glm::vec3(2, 0, 3));
    v.f(a).maxHealth = v.f(a).health = 1e5f;

    const Moveset* m = v.world.movesetOf(v.player());
    AttackDef light = m->attacks.at(m->light);
    for (int k = 0; k < 4; ++k)
        v.lost(a, light);
    EXPECT_EQ(v.count("proc"), 0);
    EXPECT_FLOAT_EQ(v.f(b).health, v.f(b).maxHealth);

    v.lost(a, light);
    EXPECT_EQ(v.count("proc"), 1);
    EXPECT_LT(v.f(b).health, v.f(b).maxHealth);   // it jumped on
    EXPECT_GT(v.player().stats.bySource.at("base:static_charge"), 0.0f);
}

TEST(arena_variety, an_item_s_landing_turns_a_hard_landing_into_a_hit_around)
{
    // No base item has one any more (Meteor boots went): a mod's may, so one made here
    Variety v;
    UpgradeDef boots;
    boots.id = "test:boots";
    boots.name = "Boots";
    boots.landing.radius = 3.5f;
    boots.landing.damage = 30.0f;
    v.content.upgrades[boots.id] = boots;
    v.world.takeBoon(v.player(), {"test:boots", 0});
    const int brute = v.spawn("base:brute", glm::vec3(1.8f, 0, 0));

    v.player().body.position.y += 9.0f;
    v.player().body.onGround = false;
    v.step(90);

    EXPECT_GE(v.count("quake"), 1);
    EXPECT_LT(v.f(brute).health, v.f(brute).maxHealth);
}

// ---- Over the network ---------------------------------------------------------------------------

TEST(arena_variety, affixes_level_up_choices_and_damage_by_source_go_to_the_clients)
{
    Variety v;
    net::Names names;
    names.build(v.content);

    Fighter me = v.player();
    me.levelRerolls = 2;
    me.banishes = 1;
    me.banished = {"base:fireball"};
    me.stats.bySource = {{"base:sword", 120.0f}, {"base:burn", 33.0f}, {"base:sapper", 5.0f}};
    Fighter elite;
    elite.id = 40;
    elite.team = Team::Enemy;
    elite.type = "base:brute";
    elite.elite = true;
    elite.affixes = {"base:blazing", "base:guardian"};

    net::SnapshotMsg snap;
    snap.tick = 10;
    snap.playerId = me.id;
    snap.fighters = {me, elite};
    snap.zones.push_back({5, Element::Fire, glm::vec3(1, 2, 3), 3.0f, 1.0f, 1.0f, 0.0f, true, true});

    net::SnapshotMsg back;
    ASSERT_TRUE(net::decode(net::encode(snap, names), back, names));
    ASSERT_EQ(back.fighters.size(), 2u);
    EXPECT_EQ(back.fighters[0].levelRerolls, 2);
    EXPECT_EQ(back.fighters[0].banishes, 1);
    EXPECT_EQ(back.fighters[0].banished, std::vector<std::string>({"base:fireball"}));
    EXPECT_FLOAT_EQ(back.fighters[0].stats.bySource.at("base:burn"), 33.0f);
    EXPECT_FLOAT_EQ(back.fighters[0].stats.bySource.at("base:sapper"), 5.0f);
    EXPECT_EQ(back.fighters[1].affixes, elite.affixes);
    ASSERT_EQ(back.zones.size(), 1u);
    EXPECT_TRUE(back.zones[0].blast);
    EXPECT_TRUE(back.zones[0].enemy);

    // Every event the new things make has a name over the network
    for (const char* kind : {"blast_warn", "blast", "split", "quake", "thorns", "proc", "level_reroll", "banish", "enemy_heal", "heal", "heal_burst", "duel_start"})
        EXPECT_NE(names.id(kind), 0) << kind;
}
