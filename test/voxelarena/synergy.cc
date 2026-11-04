#include "stdafx.h"

#include <gtest/gtest.h>

#include <algorithm>

#include "Core/content.h"
#include "Core/tuning.h"
#include "Core/world.h"

using namespace arena;

// What makes things work together: element combos (combos.json), the payoff branches of spells (against
// statuses, detonate, a weapon imbued on casting) and the items for statuses and combos.

namespace
{
    /** The shipped content on a bare floor, two dummies in front of the player. */
    struct Synergy
    {
        Content content;
        Tuning tuning;
        ArenaWorld world;
        int a = 0, b = 0;

        explicit Synergy(bool run = false)
        {
            content = loadContent(VOXELARENA_MODS_DIR);
            ContentReport r;
            tuning = readTuning(content.tuning, content.tuningFile, r);

            content.arena.size = glm::ivec3(32, 16, 32);
            content.arena.shapes = parseJson(R"([ { "block": "stone", "from": [0, 0, 0], "to": [31, 0, 31] } ])").value;
            content.arena.spawns.clear();
            content.arena.playerSpawn = glm::vec3(16, 1, 8);

            if (run)
            {
                content.run.chooseWeapon = false;
                content.run.intermission = 0.0f;
                content.run.startSpells = {"base:firebolt"};
                content.run.startChoices.clear();
                content.run.difficulties.clear();
            }

            world.start(content, tuning);
            world.aiEnabled = false;
            if (run)
                world.startRun(7);
            step(30);

            if (not run)
            {
                a = world.spawnEnemy("base:dummy", player().body.position + glm::vec3(0, 0, 4.0f), 180.0f);
                b = world.spawnEnemy("base:dummy", player().body.position + glm::vec3(1.5f, 0, 4.0f), 180.0f);
                step(3);
            }
        }

        Fighter& player() { return *world.player(); }
        Fighter& dummy(int id) { return *world.find(id); }

        void step(int n)
        {
            for (int i = 0; i < n; ++i)
                world.step(InputFrame{});
        }

        static AttackDef blow(float damage, const std::string& status = "", Element element = Element::None)
        {
            AttackDef hit;
            hit.name = "test";
            hit.damage = damage;
            hit.poiseDamage = 0.0f;
            hit.knockback = 0.0f;
            hit.hitstop = 0;
            hit.element = element;
            hit.status.status = status;
            return hit;
        }

        float lost(int id, const AttackDef& hit)
        {
            Fighter& t = dummy(id);
            const float before = t.health;
            world.applyHit(player(), t, hit);
            return before - dummy(id).health;
        }

        int count(const std::string& kind) const
        {
            return static_cast<int>(std::count_if(world.events.begin(), world.events.end(), [&](const WorldEvent& e) { return e.kind == kind; }));
        }

        bool has(int id, const std::string& status) { return ArenaWorld::hasStatus(dummy(id), status); }
    };
}

TEST(arena_synergy, the_base_combos_load)
{
    Synergy s;
    EXPECT_TRUE(s.content.report.errors.empty());
    for (const char* id : {"base:melt", "base:overload", "base:shatter"})
        EXPECT_TRUE(s.content.combos.count(id)) << id;
    EXPECT_EQ(s.content.statuses.at("base:flame_weapon").imbue, "base:burn");
    EXPECT_EQ(s.content.statuses.at("base:burn").name, "burning");
}

TEST(arena_synergy, fire_on_a_slowed_enemy_melts_and_uses_up_both)
{
    Synergy s;
    const ComboDef& melt = s.content.combos.at("base:melt");

    // On its own, a fire hit burns
    EXPECT_FLOAT_EQ(s.lost(s.b, Synergy::blow(10.0f, "base:burn", Element::Fire)), 10.0f);
    EXPECT_TRUE(s.has(s.b, "base:burn"));

    // On a slowed one: the hit, then the melt (as much again plus its base), and neither status is left
    s.world.applyStatus(s.dummy(s.a), "base:chill");
    EXPECT_FLOAT_EQ(s.lost(s.a, Synergy::blow(10.0f, "base:burn", Element::Fire)), 10.0f + melt.damage * 10.0f + melt.base);
    EXPECT_FALSE(s.has(s.a, "base:chill"));
    EXPECT_FALSE(s.has(s.a, "base:burn"));
    EXPECT_EQ(s.count("base:melt"), 1);
}

TEST(arena_synergy, a_combo_goes_either_way_round_but_not_twice_in_a_row)
{
    Synergy s;

    // Ice on a burning one melts too
    s.world.applyStatus(s.dummy(s.a), "base:burn");
    s.lost(s.a, Synergy::blow(10.0f, "base:chill", Element::Ice));
    EXPECT_EQ(s.count("base:melt"), 1);

    // At once again: the target's combo cooldown holds it, the statuses just land
    s.world.applyStatus(s.dummy(s.a), "base:burn");
    s.lost(s.a, Synergy::blow(10.0f, "base:chill", Element::Ice));
    EXPECT_EQ(s.count("base:melt"), 1);
    EXPECT_TRUE(s.has(s.a, "base:chill"));

    // Once it is over, it goes again (a few steps more: the melt's hitstop holds the dummy's clock)
    s.step(static_cast<int>(s.content.combos.at("base:melt").cooldown * 60.0f) + 12);
    s.world.applyStatus(s.dummy(s.a), "base:burn");
    s.lost(s.a, Synergy::blow(10.0f, "base:chill", Element::Ice));
    EXPECT_GE(s.count("base:melt"), 1);
    EXPECT_FALSE(s.has(s.a, "base:chill"));
}

TEST(arena_synergy, lightning_on_a_burning_enemy_overloads_and_sets_those_near_on_fire)
{
    Synergy s;
    s.world.applyStatus(s.dummy(s.a), "base:burn");
    const float bBefore = s.dummy(s.b).health;

    s.lost(s.a, Synergy::blow(10.0f, "base:shock", Element::Volt));
    EXPECT_EQ(s.count("base:overload"), 1);
    EXPECT_LT(s.dummy(s.b).health, bBefore);     // 1.5 m away, inside the blast
    EXPECT_TRUE(s.has(s.b, "base:burn"));
    EXPECT_TRUE(s.has(s.a, "base:burn"));        // not used up
}

TEST(arena_synergy, a_combos_own_hit_sets_off_no_combo)
{
    Synergy s;

    // b is stunned: the overload's burn landing on it would be another overload, but a combo's hit sets none off
    s.world.applyStatus(s.dummy(s.a), "base:burn");
    s.world.applyStatus(s.dummy(s.b), "base:shock");
    s.lost(s.a, Synergy::blow(10.0f, "base:shock", Element::Volt));
    EXPECT_EQ(s.count("base:overload"), 1);
}

TEST(arena_synergy, lightning_shatters_a_frozen_enemy)
{
    Synergy s;
    const ComboDef& shatter = s.content.combos.at("base:shatter");
    const float weak = s.content.enemies.at("base:dummy").weakness[static_cast<int>(Element::Volt)];

    s.world.applyStatus(s.dummy(s.a), "base:chill", 4);
    ASSERT_TRUE(s.has(s.a, "base:frozen"));

    const float lost = s.lost(s.a, Synergy::blow(10.0f, "base:shock", Element::Volt));
    EXPECT_FLOAT_EQ(lost, (10.0f + shatter.damage * 10.0f + shatter.base) * weak);
    EXPECT_FALSE(s.has(s.a, "base:frozen"));
    EXPECT_EQ(s.count("base:shatter"), 1);
}

TEST(arena_synergy, more_damage_against_a_status_from_a_branch_or_an_item)
{
    Synergy s;

    // A branch's: three times on a frozen one, nothing more on the other
    AttackDef breaker = Synergy::blow(10.0f);
    breaker.payoff.against["base:frozen"] = 2.0f;
    EXPECT_FLOAT_EQ(s.lost(s.b, breaker), 10.0f);
    s.world.applyStatus(s.dummy(s.a), "base:chill", 4);
    EXPECT_FLOAT_EQ(s.lost(s.a, breaker), 30.0f);

    // "any": whatever status it has
    AttackDef any = Synergy::blow(10.0f);
    any.payoff.against["any"] = 0.5f;
    s.world.applyStatus(s.dummy(s.b), "base:burn");
    EXPECT_FLOAT_EQ(s.lost(s.b, any), 15.0f);

    // An item's: Kindling on everything of the player against burning ones
    s.player().boons = {Fighter::Boon("base:kindling")};
    s.world.sumBoons(s.player());
    const float more = s.content.upgrades.at("base:kindling").payoff.against.at("base:burn");
    EXPECT_FLOAT_EQ(s.lost(s.b, Synergy::blow(10.0f)), 10.0f * (1.0f + more));
}

TEST(arena_synergy, detonate_deals_the_burn_left_at_once_but_not_a_burn_just_landed)
{
    Synergy s;
    AttackDef det = Synergy::blow(10.0f);
    det.payoff.detonate = 1.5f;
    const StatusDef& burn = s.content.statuses.at("base:burn");

    // Just landed: left to burn
    s.world.applyStatus(s.dummy(s.a), "base:burn");
    EXPECT_FLOAT_EQ(s.lost(s.a, det), 10.0f);
    EXPECT_TRUE(s.has(s.a, "base:burn"));

    // Burning a while: what is left of it, times 1.5, and the burn is gone
    s.step(30);
    const float left = s.dummy(s.a).statuses.front().remaining;
    EXPECT_NEAR(s.lost(s.a, det), 10.0f + burn.damagePerSecond * left * 1.5f, 1e-3f);
    EXPECT_FALSE(s.has(s.a, "base:burn"));
    EXPECT_EQ(s.count("detonate"), 1);
}

TEST(arena_synergy, a_spell_branch_imbues_the_weapon_and_its_hits_set_off_combos)
{
    Synergy s;
    Fighter& p = s.player();

    // Frostbolt's Frost weapon: in its recipe once taken, and a status on the caster as it casts
    const SpellDef& frostbolt = s.content.spells.at("base:frostbolt");
    int branch = -1;
    for (size_t k = 0; k < frostbolt.branches.size(); ++k)
        if (frostbolt.branches[k].payoff.onCast == "base:frost_weapon")
            branch = static_cast<int>(k);
    ASSERT_GE(branch, 0);
    Fighter::RunSpell rs{"base:frostbolt", frostbolt.maxLevel() + 1, branch};
    const SpellRecipe recipe = s.world.spellAt(rs);
    EXPECT_EQ(recipe.payoff.onCast, "base:frost_weapon");

    p.spells.resize(ArenaWorld::SPELL_SLOTS);
    p.spells[0] = recipe;
    int slot = -1;
    for (size_t i = 0; i < s.tuning.abilities.size(); ++i)
        if (std::find(s.tuning.abilities[i].begin(), s.tuning.abilities[i].end(), "spell:1") != s.tuning.abilities[i].end())
            slot = static_cast<int>(i);
    ASSERT_GE(slot, 0);
    InputFrame in;
    in.abilityPressed[slot] = true;
    in.aimOrigin = p.body.position + glm::vec3(0, 1.6f, 0);
    in.aimDir = glm::vec3(0, 0, -1);   // away from the dummies
    s.world.step(in);
    EXPECT_TRUE(ArenaWorld::hasStatus(s.player(), "base:frost_weapon"));

    // A weapon hit now slows; another kind of hit does not
    const Moveset* m = s.world.movesetOf(s.player());
    ASSERT_TRUE(m);
    AttackDef swing = m->attacks.at(m->light);
    swing.poiseDamage = 0.0f;
    s.world.applyHit(s.player(), s.dummy(s.b), Synergy::blow(5.0f));
    EXPECT_FALSE(s.has(s.b, "base:chill"));
    s.world.applyHit(s.player(), s.dummy(s.b), swing);
    EXPECT_TRUE(s.has(s.b, "base:chill"));

    // On a burning one, the weapon's frost melts it
    s.world.applyStatus(s.dummy(s.a), "base:burn");
    s.world.applyHit(s.player(), s.dummy(s.a), swing);
    EXPECT_EQ(s.count("base:melt"), 1);

    // It wears off (a few steps more: the swings' hitstop holds the player's clock)
    s.step(static_cast<int>(s.content.statuses.at("base:frost_weapon").duration * 60.0f) + 30);
    EXPECT_FALSE(ArenaWorld::hasStatus(s.player(), "base:frost_weapon"));
}

TEST(arena_synergy, pyre_makes_a_burning_kill_explode_even_when_the_burn_kills)
{
    Synergy s;
    s.player().boons = {Fighter::Boon("base:pyre")};
    s.world.sumBoons(s.player());
    const auto& burst = s.content.upgrades.at("base:pyre").burst;

    // Killed by a blow while burning: b, next to it, takes the burst and catches fire
    s.world.applyStatus(s.dummy(s.a), "base:burn", 1, s.player().id);
    s.dummy(s.a).health = 1.0f;
    float bBefore = s.dummy(s.b).health;
    s.lost(s.a, Synergy::blow(5.0f));
    EXPECT_FALSE(s.dummy(s.a).alive());
    EXPECT_EQ(s.count("burst"), 1);
    EXPECT_FLOAT_EQ(bBefore - s.dummy(s.b).health, burst.damage);
    EXPECT_TRUE(s.has(s.b, "base:burn"));

    // Killed by the burn itself: it is still the player's (who left it), so it bursts too
    s.step(int(s.content.enemies.at("base:dummy").respawnSeconds * 60.0f) + 10);
    Fighter& b = s.dummy(s.b);
    b.health = 0.01f;
    s.world.events.clear();
    s.step(2);
    EXPECT_FALSE(s.dummy(s.b).alive());
    EXPECT_EQ(s.count("burst"), 1);

    // Without a burn: no burst
    Synergy plain;
    plain.player().boons = {Fighter::Boon("base:pyre")};
    plain.world.sumBoons(plain.player());
    plain.dummy(plain.a).health = 1.0f;
    plain.lost(plain.a, Synergy::blow(5.0f));
    EXPECT_EQ(plain.count("burst"), 0);
}

TEST(arena_synergy, long_fuse_makes_your_statuses_last_longer)
{
    Synergy s;
    s.player().boons = {Fighter::Boon("base:long_fuse")};
    s.world.sumBoons(s.player());
    const float longer = s.content.upgrades.at("base:long_fuse").statusDuration;

    s.world.applyStatus(s.dummy(s.a), "base:burn", 1, s.player().id);
    s.world.applyStatus(s.dummy(s.b), "base:burn");   // nobody's
    const float d = s.content.statuses.at("base:burn").duration;
    EXPECT_FLOAT_EQ(s.dummy(s.a).statuses.front().remaining, d * (1.0f + longer));
    EXPECT_FLOAT_EQ(s.dummy(s.b).statuses.front().remaining, d);
}

TEST(arena_synergy, combo_items_come_only_with_what_makes_their_statuses)
{
    Synergy r(true);
    Fighter& p = r.player();
    ASSERT_EQ(p.runSpells.size(), 1u);   // Firebolt: fire, one element

    auto offered = [&](const std::string& id) {
        for (int k = 0; k < 200; ++k)
        {
            r.world.rollShop(p);
            for (const auto& o : p.shop)
                if (o.id == id)
                    return true;
        }
        return false;
    };

    EXPECT_TRUE(offered("base:kindling"));
    EXPECT_TRUE(offered("base:pyre"));
    EXPECT_FALSE(offered("base:brittle"));    // nothing of the player's is ice
    EXPECT_FALSE(offered("base:catalyst"));   // one element: no combos of its own

    // A second element: combos, and what goes with ice
    p.runSpells.push_back({"base:frostbolt", 1});
    EXPECT_TRUE(offered("base:brittle"));
    EXPECT_TRUE(offered("base:catalyst"));
}
