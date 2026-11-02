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

            // Straight to the first wave with the start kit's weapon (the weapon choice has its own tests),
            // straight to the shop as a wave ends (the moment before it has its own)
            content.run.chooseWeapon = false;
            content.run.intermission = 0.0f;
            content.run.startSpells = {"base:firebolt"};   // no first spell to pick either (its own test)
            content.run.startChoices.clear();
            content.run.difficulties.clear();   // the run as written: the difficulty levels have their own tests

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
    r.past(1.0f / (r.content.run.spawnRate.x * r.content.run.spawnScale) + 0.1f);
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

    // Enough for a level: a spell, a spell's next level or a weapon skill to pick as the wave ends
    r.player().xp = r.world.xpFor(1) - drops;
    r.timeUp();
    EXPECT_TRUE(r.world.pickups.empty());
    EXPECT_EQ(r.player().shards, 2 * drops);
    EXPECT_EQ(r.player().level, 2);
    EXPECT_EQ(r.player().levelUps, 1);
    ASSERT_EQ(r.player().levelOffer.size(), static_cast<size_t>(r.content.run.levelChoices));

    const auto spellsBefore = r.player().runSpells;
    const auto ownedBefore = r.player().owned;
    const std::string chosen = r.player().levelOffer[2];
    r.pick(InputFrame::LEVEL + 2);
    EXPECT_TRUE(r.player().runSpells.size() != spellsBefore.size() or r.player().owned.size() != ownedBefore.size() or
                std::any_of(r.player().runSpells.begin(), r.player().runSpells.end(), [&](const Fighter::RunSpell& s) { return s.id == chosen and s.level == 2; }))
        << chosen;
    EXPECT_EQ(r.player().levelUps, 0);
    EXPECT_TRUE(r.player().levelOffer.empty());
    EXPECT_EQ(r.player().shards, 2 * drops);   // free
}

TEST(arena_run, the_shop_sells_locks_and_rerolls)
{
    RunArena r;
    r.content.run.unlocks.clear();   // the weapons a run without a weapon choice sells (a mod's)
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

    ASSERT_TRUE(r.content.upgrades.count(offer.id)) << offer.id;
    ASSERT_EQ(p.boons.size(), 1u);
    EXPECT_EQ(p.boons[0], Fighter::Boon(offer.id, offer.rarity));

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

TEST(arena_run, the_shop_sells_stat_and_tag_items_priced_by_the_wave_and_rarity)
{
    RunArena r;
    r.content.run.unlocks.clear();   // the weapons a run without a weapon choice sells (a mod's)
    r.world.startRun(11);
    Fighter& p = r.player();
    ASSERT_FALSE(r.content.run.rarities.empty());

    r.world.skipToWave(9);
    r.timeUp();
    ASSERT_EQ(r.world.run.phase, RunPhase::Shop);
    r.world.collect(p, 5000);
    p.levelUps = 0;
    p.levelOffer.clear();
    p.presents = 0;
    p.presentOffer.clear();

    // What the player's spells and weapon attacks reach
    std::vector<std::string> tags;
    for (const auto& rs : p.runSpells)
        for (const auto& t : r.world.spellTags(r.world.spellAt(rs)))
            tags.push_back(t);
    for (const auto& [name, attack] : r.world.movesetOf(p)->attacks)
        for (const auto& t : r.world.attackTags(p, attack))
            tags.push_back(t);

    // Only items: stat items, and tag items for a tag the player has; priced for the wave
    const float scale = r.content.run.priceScale.x + (r.content.run.priceScale.y - r.content.run.priceScale.x) * 8.0f / static_cast<float>(r.content.run.waves - 1);
    EXPECT_FLOAT_EQ(r.world.priceScale(9), scale);
    bool sawTagItem = false, sawRare = false;
    for (int k = 0; k < 30; ++k)
    {
        r.pick(InputFrame::REROLL);
        for (const auto& o : p.shop)
        {
            ASSERT_TRUE(r.content.upgrades.count(o.id)) << o.id;
            const UpgradeDef& u = r.content.upgrades.at(o.id);
            const float rarityPrice = r.content.run.rarities[static_cast<size_t>(o.rarity)].price;
            EXPECT_NEAR(static_cast<float>(o.price), static_cast<float>(u.price) * rarityPrice * scale, 1.0f) << o.id;

            if (u.forTag())
            {
                sawTagItem = true;
                EXPECT_NE(std::find(tags.begin(), tags.end(), u.tag), tags.end()) << u.tag;
            }

            sawRare = sawRare or o.rarity > 0;
        }
    }
    EXPECT_TRUE(sawTagItem);
    EXPECT_TRUE(sawRare);
}

TEST(arena_run, prices_start_at_half_and_end_at_one_and_a_half)
{
    RunArena r;
    r.world.startRun(3);
    EXPECT_FLOAT_EQ(r.world.priceScale(1), 0.5f);
    EXPECT_FLOAT_EQ(r.world.priceScale(r.content.run.waves), 1.5f);
}

TEST(arena_run, the_interest_keeps_what_could_be_bought_affordable)
{
    RunArena r;
    r.content.run.unlocks.clear();
    r.world.startRun(5);
    Fighter& p = r.player();

    // A shop at wave 3: an item the player can just afford, not bought
    r.world.skipToWave(3);
    r.timeUp();
    p.levelUps = 0;
    p.levelOffer.clear();
    ASSERT_FALSE(p.shop.empty());
    p.shards = p.shop[0].price;
    const UpgradeDef& item = r.content.upgrades.at(p.shop[0].id);
    const int rarity = p.shop[0].rarity;

    // Ready, through the next wave earning nothing: the same item costs more now, and the interest pays for it
    r.pick(InputFrame::START);
    ASSERT_EQ(r.world.run.wave, 4);
    r.timeUp();
    const int now = static_cast<int>(std::lround(static_cast<float>(item.price) * r.content.run.rarities[static_cast<size_t>(rarity)].price * r.world.priceScale(4)));
    EXPECT_GT(p.interest, 0);
    EXPECT_GE(p.shards, now);
}

TEST(arena_run, in_co_op_every_player_gets_every_shard_and_prices_go_up_to_match)
{
    RunArena r;
    const int second = r.world.addPlayer(1);
    r.world.startRun(5);

    Fighter* other = r.world.find(second);
    ASSERT_TRUE(other);
    other->body.position = r.player().body.position + glm::vec3(12.0f, 0.0f, 0.0f);   // far out of pickup range

    const int shards = r.player().shards, otherShards = other->shards;
    const int xp = r.player().stats.xp, otherXp = other->stats.xp;

    // The first player picks a shard up: both get it, money and XP; only the one who picked it up counts it
    Pickup shard;
    shard.id = 999;
    shard.value = 4;
    shard.position = r.player().body.position + glm::vec3(0.0f, 1.0f, 1.0f);
    r.world.pickups.push_back(shard);
    r.run(40);

    other = r.world.find(second);
    ASSERT_TRUE(other);
    EXPECT_TRUE(r.world.pickups.empty());
    EXPECT_EQ(r.player().shards, shards + 4);
    EXPECT_EQ(other->shards, otherShards + 4);
    EXPECT_EQ(r.player().stats.xp, xp + 4);
    EXPECT_EQ(other->stats.xp, otherXp + 4);
    EXPECT_EQ(r.player().stats.shards, 4);
    EXPECT_EQ(other->stats.shards, 0);

    // The prices, the reroll's too: half again with a second player
    RunArena solo;
    solo.world.startRun(5);
    const float coop = 1.0f + r.content.run.coopPrice;
    EXPECT_FLOAT_EQ(r.world.priceScale(5), solo.world.priceScale(5) * coop);
    EXPECT_EQ(r.world.rerollPrice(r.player()), static_cast<int>(std::lround(static_cast<float>(solo.world.rerollPrice(solo.player())) * coop)));
}

TEST(arena_run, a_rarer_item_does_more)
{
    RunArena r;
    r.world.startRun(7);
    Fighter& p = r.player();
    const float health = p.maxHealth;
    const UpgradeDef& tough = r.content.upgrades.at("base:tough");
    const RarityDef& legendary = r.content.run.rarities.back();

    r.timeUp();
    p.shards = 1000;
    Fighter::ShopOffer offer;
    offer.id = "base:tough";
    offer.price = 1;
    offer.rarity = static_cast<int>(r.content.run.rarities.size()) - 1;
    p.shop = {offer};
    r.pick(InputFrame::BUY);
    EXPECT_NEAR(p.maxHealth, health + tough.maxHealth * legendary.power, 0.01f);
    EXPECT_GT(legendary.power, 1.0f);
}

TEST(arena_run, a_tag_item_boosts_everything_with_its_tag)
{
    RunArena r;
    r.world.startRun(7);
    Fighter& p = r.player();
    p.runSpells = {{"base:firebolt", 1}, {"base:frost_nova", 1}};

    const auto fire0 = r.world.spellOf(r.world.spellRecipe(p, 0));
    const auto ice0 = r.world.spellOf(r.world.spellRecipe(p, 1));

    // Fire damage: the firebolt, not the frost nova
    r.timeUp();
    p.levelUps = 0;
    p.levelOffer.clear();
    p.shards = 1000;
    Fighter::ShopOffer searing;
    searing.id = "base:searing";
    searing.price = 1;
    Fighter::ShopOffer split = searing;
    split.id = "base:split_shot";
    p.shop = {searing, split};
    r.pick(InputFrame::BUY);

    const auto fire1 = r.world.spellOf(r.world.spellRecipe(p, 0));
    const auto ice1 = r.world.spellOf(r.world.spellRecipe(p, 1));
    const float more = 1.0f + r.content.upgrades.at("base:searing").boost.damage;
    EXPECT_NEAR(fire1->stages[0].damage, fire0->stages[0].damage * more, 0.01f);
    EXPECT_FLOAT_EQ(ice1->stages[0].damage, ice0->stages[0].damage);

    // One more projectile: the firebolt's bolt
    r.pick(InputFrame::BUY + 1);
    EXPECT_EQ(r.world.spellOf(r.world.spellRecipe(p, 0))->stages[0].count, fire0->stages[0].count + 1);
}

TEST(arena_run, a_tag_item_reaches_a_technique)
{
    RunArena r;
    r.world.startRun(7);
    Fighter& p = r.player();
    r.world.chooseWeapon(p, "base:sword");
    p.runSkills = {{"base:whirlwind", 1}};
    p.owned.push_back("base:whirlwind");

    const float damage = r.world.attackDef(p, "@base:whirlwind")->damage;
    p.boons.push_back({"base:honed", 0});
    EXPECT_NEAR(r.world.attackDef(p, "@base:whirlwind")->damage, damage * (1.0f + r.content.upgrades.at("base:honed").boost.damage), 0.01f);

    // Its levels too, and a branch: an element and a status on its hits
    p.runSkills[0].level = r.content.run.branchLevel;
    p.runSkills[0].branch = 0;
    const AttackDef* a = r.world.attackDef(p, "@base:whirlwind");
    EXPECT_GT(a->damage, damage * (1.0f + r.content.upgrades.at("base:honed").boost.damage));
    EXPECT_EQ(a->element, Element::Fire);
    EXPECT_EQ(a->status.status, "base:burn");

    // Outside a run: as written
    r.world.stopRun();
    EXPECT_FLOAT_EQ(r.world.attackDef(r.player(), "@base:whirlwind")->damage, damage);
}

TEST(arena_run, levels_go_on_with_a_branch_then_mastery)
{
    RunArena r;
    r.world.startRun(7);
    Fighter& p = r.player();
    p.runSpells = {{"base:firebolt", r.content.spells.at("base:firebolt").maxLevel()}};

    // At the branch level: its branches are the picks for it
    auto offers = [&]() {
        p.levelUps = 1;
        r.world.rollLevel(p);
        return p.levelOffer;
    };

    bool sawBranch = false;
    for (int k = 0; k < 20 and not sawBranch; ++k)
    {
        const auto o = offers();
        sawBranch = std::find(o.begin(), o.end(), "base:firebolt#0") != o.end();
        if (sawBranch)
            EXPECT_NE(std::find(o.begin(), o.end(), "base:firebolt#1"), o.end());
        EXPECT_EQ(std::find(o.begin(), o.end(), "base:firebolt"), o.end());   // not a plain level now
    }
    ASSERT_TRUE(sawBranch);

    const auto before = r.world.spellOf(r.world.spellRecipe(p, 0));
    p.runSpells[0].level += 1;
    p.runSpells[0].branch = 0;
    const auto branched = r.world.spellOf(r.world.spellRecipe(p, 0));
    EXPECT_GT(branched->stages.size(), before->stages.size());   // Inferno: a nova as the patch ends

    // Past it: mastery, on and on
    p.runSpells[0].level += 2;
    const auto mastered = r.world.spellOf(r.world.spellRecipe(p, 0));
    EXPECT_NEAR(mastered->stages[0].damage, branched->stages[0].damage * (1.0f + 2.0f * r.content.run.mastery.damage), 0.01f);
    bool plain = false;
    for (int k = 0; k < 20 and not plain; ++k)
    {
        const auto o = offers();
        plain = std::find(o.begin(), o.end(), "base:firebolt") != o.end();
    }
    EXPECT_TRUE(plain);
}

TEST(arena_run, an_elite_may_leave_a_present_for_a_free_item)
{
    RunArena r;
    r.content.run.presentChance = 1.0f;
    r.world.startRun(9);

    // Three a wave at most
    for (int k = 0; k < 5; ++k)
    {
        Fighter& e = r.near("base:skulker", 2.0f);
        e.elite = true;
        r.killAll();
    }
    const int presents = static_cast<int>(std::count_if(r.world.pickups.begin(), r.world.pickups.end(), [](const Pickup& pk) { return pk.present; }));
    EXPECT_EQ(presents, r.content.run.presentsPerWave);

    r.past(2.0f);
    Fighter& p = r.player();   // (the enemies spawned moved the fighters about)
    EXPECT_EQ(p.presents, r.content.run.presentsPerWave);

    // At the shop, after the level ups: a free pick each
    r.timeUp();
    p.levelUps = 0;
    p.levelOffer.clear();
    r.world.rollPresent(p);
    ASSERT_EQ(p.presentOffer.size(), static_cast<size_t>(r.content.run.presentChoices));
    r.past(r.content.run.intermission);

    const int shards = p.shards;
    r.pick(InputFrame::BUY);   // the shop waits
    EXPECT_EQ(p.shards, shards);

    const std::string chosen = p.presentOffer[1].id;
    r.pick(InputFrame::PRESENT + 1);
    EXPECT_EQ(p.presents, r.content.run.presentsPerWave - 1);
    EXPECT_EQ(std::count(p.boons.begin(), p.boons.end(), chosen), 1);
    EXPECT_EQ(p.shards, shards);
}

TEST(arena_run, level_ups_give_spells_and_their_levels_before_the_shop)
{
    RunArena r;
    r.world.startRun(7);
    Fighter& p = r.player();

    // The run starts with its spells: on key 1, cast as given (no keyword to buy)
    ASSERT_EQ(p.runSpells.size(), r.content.run.startSpells.size());
    const std::string first = p.runSpells[0].id;
    EXPECT_EQ(r.world.spellRecipe(p, 0), r.content.spells.at(first).recipe);

    // A level up: the next level of a spell had adds its keywords
    p.levelUps = 3;
    r.timeUp();
    ASSERT_FALSE(p.levelOffer.empty());

    // Not the shop before the level ups are picked
    p.shards = 1000;
    r.pick(InputFrame::BUY);
    EXPECT_FALSE(p.shop[0].bought);
    r.pick(InputFrame::START);
    EXPECT_FALSE(p.ready);

    p.levelOffer = {first};
    r.pick(InputFrame::LEVEL);
    ASSERT_EQ(p.runSpells[0].level, 2);
    SpellRecipe grown = r.content.spells.at(first).recipe;
    for (const auto& k : r.content.spells.at(first).levels[0])
        grown.keywords.push_back(k);
    EXPECT_EQ(r.world.spellRecipe(p, 0), grown);
    EXPECT_TRUE(r.world.spellOf(grown)->valid());

    // A new spell: on the next key
    p.levelOffer = {"base:fireball"};
    r.pick(InputFrame::LEVEL);
    ASSERT_EQ(p.runSpells.size(), 2u);
    EXPECT_EQ(p.runSpells[1].id, "base:fireball");

    // Three keys full: no new spell is offered, only levels and skills
    p.runSpells.push_back({"base:glacier", 1});
    r.world.skipToWave(3);
    p.levelUps = 1;
    r.timeUp();
    for (const auto& id : p.levelOffer)
    {
        if (r.content.spells.count(id))
            EXPECT_TRUE(std::any_of(p.runSpells.begin(), p.runSpells.end(), [&](const Fighter::RunSpell& s) { return s.id == id; })) << id;
        else
            EXPECT_TRUE(r.content.abilities.count(id)) << id;
    }

    // The client's own recipes count for nothing in a run
    SpellRecipe mine;
    mine.element = "volt";
    mine.keywords = {"base:beam"};
    r.world.setSpells(p, {mine, mine, mine});
    EXPECT_EQ(r.world.spellRecipe(p, 0).element, r.content.spells.at(first).recipe.element);
}

TEST(arena_run, every_spells_levels_make_a_spell)
{
    RunArena r;
    for (const auto& [id, sp] : r.content.spells)
    {
        EXPECT_GE(sp.maxLevel(), 2) << id;
        for (int level = 1; level <= sp.maxLevel(); ++level)
            EXPECT_TRUE(r.world.spellOf(r.world.spellAtLevel(id, level))->valid()) << id << " level " << level;
    }
}

TEST(arena_run, nothing_is_taken_in_the_moment_after_a_wave)
{
    RunArena r;
    r.content.run.intermission = 2.0f;
    r.world.startRun(7);
    Fighter& p = r.player();

    r.timeUp();
    ASSERT_EQ(r.world.run.phase, RunPhase::Shop);
    EXPECT_GT(r.world.run.timer, 1.5f);
    p.shards = 1000;
    p.levelUps = 0;
    p.levelOffer.clear();

    // A click carried over from the fight: nothing
    r.pick(InputFrame::BUY);
    EXPECT_FALSE(p.shop[0].bought);
    r.pick(InputFrame::START);
    EXPECT_FALSE(p.ready);

    // Then the shop is open
    r.past(2.0f);
    EXPECT_EQ(r.world.run.timer, 0.0f);
    r.pick(InputFrame::BUY);
    EXPECT_TRUE(p.shop[0].bought);
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

    // A spell: only the ones given (key 1: the run's start spell; key 2: none yet)
    auto cast = [&](int slot) {
        p.mana = r.tuning.mana.max;
        std::fill(std::begin(p.abilityCooldown), std::end(p.abilityCooldown), 0.0f);
        int key = -1;
        for (int i = 0; i < InputFrame::ABILITIES; ++i)
            if (r.world.spellSlotAt(i) == slot)
                key = i;
        InputFrame c;
        c.abilityPressed[key] = true;
        c.aimDir = glm::vec3(0, 0, 1);
        r.world.step(c);
        r.run(2);
        return p.mana < r.tuning.mana.max - 1.0f;
    };

    EXPECT_TRUE(cast(0));
    EXPECT_FALSE(cast(1));

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
    r.past(1.0f / (r.content.run.spawnRate.x * r.content.run.spawnScale) + r.content.run.warn + 0.5f);

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
    r.player().boons = {Fighter::Boon("base:keen")};
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
    EXPECT_NEAR(p.health, 10.0f + r.content.upgrades.at("base:mending").regen + r.content.weapons.at(p.weapon).regen, 0.1f);   // and the weapon's

    // At its most, it is not offered again
    for (int i = 0; i < 10; ++i)
        p.boons.push_back(Fighter::Boon("base:nimble"));
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
    ASSERT_FALSE(r.content.run.arenas.empty());
    const ArenaDef& first = r.content.arenaById(r.content.run.arenas.front().arena);
    const glm::ivec3 training(r.world.grid.width(), r.world.grid.height(), r.world.grid.depth());

    r.world.startRun(9);
    const glm::ivec3 size = first.size;
    EXPECT_EQ(glm::ivec3(r.world.grid.width(), r.world.grid.height(), r.world.grid.depth()), size);
    EXPECT_NEAR(glm::length(r.player().body.position - first.playerSpawn), 0.0f, 0.5f);

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

TEST(arena_run, the_first_waves_are_slimes_and_the_first_is_short)
{
    RunArena r;
    r.world.startRun(17);
    EXPECT_FLOAT_EQ(r.world.run.timer, 45.0f);

    r.past(20.0f);
    int slimes = 0;
    for (const auto& f : r.world.fighters)
        if (f.team == Team::Enemy)
        {
            EXPECT_EQ(f.type, "base:slime");
            ++slimes;
        }
    EXPECT_GE(slimes, 2);
}

TEST(arena_run, the_slime_hops_at_the_player_and_bounces_off)
{
    RunArena r;
    ScriptBrain brain;
    ContentReport report;
    brain.load(r.content, report);
    ASSERT_TRUE(report.errors.empty()) << report.errors.front().text;
    r.world.setBrain(&brain);
    r.world.aiEnabled = true;

    const int id = r.world.spawnEnemy("base:slime", r.player().body.position + glm::vec3(0, 0, 9.0f), 180.0f);
    const float start = glm::length(r.world.find(id)->body.position - r.player().body.position);
    const float health = r.player().health;

    bool airborne = false;
    bool staggered = false;
    std::vector<std::string> attacks;
    for (int i = 0; i < 60 * 8; ++i)
    {
        r.world.step(InputFrame{});
        const Fighter* s = r.world.find(id);
        ASSERT_TRUE(s);
        EXPECT_TRUE(s->aiLastError.empty()) << s->aiLastError;
        airborne = airborne or not s->body.onGround;
        staggered = staggered or r.player().action == Action::Stagger;
        if (s->action == Action::Attack and (attacks.empty() or attacks.back() != s->attack))
            attacks.push_back(s->attack);
    }

    // It moves by hopping, it closes in, it pounces and bounces back off, and it hurts a little,
    // without ever staggering the player
    EXPECT_TRUE(airborne);
    EXPECT_FALSE(staggered);
    EXPECT_TRUE(std::find(attacks.begin(), attacks.end(), "hop") != attacks.end());
    const auto pounce = std::find(attacks.begin(), attacks.end(), "pounce");
    ASSERT_NE(pounce, attacks.end());
    ASSERT_NE(pounce + 1, attacks.end());
    EXPECT_EQ(*(pounce + 1), "bounce");
    EXPECT_LT(glm::length(r.world.find(id)->body.position - r.player().body.position), start);
    EXPECT_LT(r.player().health, health);
}

TEST(arena_run, the_slime_hops_up_a_block)
{
    RunArena r;
    ScriptBrain brain;
    ContentReport report;
    brain.load(r.content, report);
    r.world.setBrain(&brain);
    r.world.aiEnabled = true;
    r.world.godMode = true;

    // The player on a step one block high, the slime below it
    r.world.grid.fill({8, 1, 2}, {24, 1, 10}, r.content.block("base:stone"));
    r.player().body.position = glm::vec3(16, 2, 6);
    const int id = r.world.spawnEnemy("base:slime", glm::vec3(16, 1, 17), 180.0f);

    float highest = 0.0f;
    for (int i = 0; i < 60 * 8; ++i)
    {
        r.world.step(InputFrame{});
        const Fighter* s = r.world.find(id);
        ASSERT_TRUE(s);
        if (s->body.onGround)
            highest = std::max(highest, s->body.position.y);
    }

    EXPECT_GE(highest, 1.99f);   // stood on the step
    EXPECT_LT(glm::length(r.world.find(id)->body.position - r.player().body.position), 4.0f);
}

// ---- The final duel (multiplayer) -------------------------------------------------------------

namespace
{
    /** A run of two players straight to its last wave, the Warden down: the duel begins. */
    int toTheDuel(RunArena& r)
    {
        const int second = r.world.addPlayer(1);
        r.world.startRun(5);
        r.world.skipToWave(20);
        r.past(2.5f);
        r.killAll();
        return second;
    }
}

TEST(arena_run, alone_the_last_wave_is_the_victory_with_others_a_duel_follows)
{
    RunArena solo;
    solo.world.startRun(5);
    solo.world.skipToWave(20);
    solo.past(2.5f);
    solo.killAll();
    EXPECT_EQ(solo.world.run.phase, RunPhase::Victory);
    EXPECT_EQ(solo.world.run.champion, 0);

    RunArena r;
    const int second = toTheDuel(r);
    ASSERT_EQ(r.world.run.phase, RunPhase::Duel);
    EXPECT_EQ(r.enemies(), 0);
    EXPECT_GT(r.world.run.warmup, 0.0f);

    // Everyone back up, at full, across the ring from each other
    const Fighter& a = r.player();
    const Fighter* b = r.world.find(second);
    ASSERT_TRUE(b);
    EXPECT_FLOAT_EQ(a.health, a.maxHealth);
    EXPECT_FLOAT_EQ(b->health, b->maxHealth);
    EXPECT_NEAR(glm::length(a.body.position - b->body.position), 2.0f * r.content.run.duel.radius, 0.5f);
}

TEST(arena_run, in_the_duel_players_hurt_each_other_once_the_countdown_is_over)
{
    RunArena r;
    const int second = toTheDuel(r);
    Fighter& a = r.player();

    // The countdown: nobody may hurt anybody; never yourself
    EXPECT_FALSE(r.world.hostile(Team::Player, a.id, *r.world.find(second)));
    r.past(r.content.run.duel.countdown);
    EXPECT_EQ(r.world.run.warmup, 0.0f);
    EXPECT_TRUE(r.world.hostile(Team::Player, a.id, *r.world.find(second)));
    EXPECT_FALSE(r.world.hostile(Team::Player, a.id, a));

    // A real swing: the other player right in front
    Fighter* b = r.world.find(second);
    b->body.position = a.body.position + glm::vec3(0, 0, 1.3f);
    a.yaw = 0.0f;
    r.run(10);
    b = r.world.find(second);
    b->body.position = r.player().body.position + glm::vec3(0, 0, 1.3f);
    const float before = b->health;

    std::map<int, InputFrame> inputs;
    inputs[r.player().id].lightPressed = true;
    r.world.step(inputs);
    r.run(30);

    EXPECT_LT(r.world.find(second)->health, before);
}

TEST(arena_run, the_last_one_standing_wins_the_duel)
{
    RunArena r;
    const int second = toTheDuel(r);
    r.past(r.content.run.duel.countdown);

    // The second player falls: one life each, nobody gets back up
    Fighter* b = r.world.find(second);
    b->health = 0.01f;
    r.world.applyStatus(*b, "base:burn");
    r.run(3);

    EXPECT_EQ(r.world.run.phase, RunPhase::Victory);
    EXPECT_EQ(r.world.run.champion, r.player().id);
}

TEST(arena_run, when_the_duels_time_is_up_the_most_health_left_wins)
{
    RunArena r;
    const int second = toTheDuel(r);
    r.past(r.content.run.duel.countdown);

    r.player().health = r.player().maxHealth * 0.3f;
    r.world.run.timer = 0.01f;
    r.run(2);

    EXPECT_EQ(r.world.run.phase, RunPhase::Victory);
    EXPECT_EQ(r.world.run.champion, second);
}

// ---- The weapon choice, the bow, the healer ---------------------------------------------------

TEST(arena_run, a_run_starts_with_each_player_choosing_a_weapon)
{
    RunArena r;
    r.content.run.chooseWeapon = true;
    const int second = r.world.addPlayer(1);
    r.world.startRun(5);

    ASSERT_EQ(r.world.run.phase, RunPhase::Armory);
    EXPECT_EQ(r.enemies(), 0);

    const auto& order = r.content.weaponOrder;
    const int bow = static_cast<int>(std::find(order.begin(), order.end(), "base:bow") - order.begin());
    const int staff = static_cast<int>(std::find(order.begin(), order.end(), "base:staff") - order.begin());
    ASSERT_LT(bow, static_cast<int>(order.size()));
    ASSERT_LT(staff, static_cast<int>(order.size()));

    // The first one takes the bow: the wave waits for the second
    std::map<int, InputFrame> inputs;
    inputs[r.player().id].pick = InputFrame::WEAPON + bow;
    r.world.step(inputs);
    EXPECT_EQ(r.player().weapon, "base:bow");
    EXPECT_EQ(r.world.run.phase, RunPhase::Armory);

    inputs.clear();
    inputs[second].pick = InputFrame::WEAPON + staff;
    r.world.step(inputs);
    EXPECT_EQ(r.world.find(second)->weapon, "base:staff");
    EXPECT_EQ(r.world.run.phase, RunPhase::Fighting);
    EXPECT_EQ(r.world.run.wave, 1);

    // One weapon each for the run: the start kit's sword is not theirs any more
    EXPECT_TRUE(r.world.owns(r.player(), "base:bow"));
    EXPECT_FALSE(r.world.owns(r.player(), "base:sword"));
}

TEST(arena_run, level_ups_give_no_weapons_only_the_skills_of_the_one_in_hand)
{
    RunArena r;
    r.content.run.chooseWeapon = true;
    r.world.startRun(5);

    const auto& order = r.content.weaponOrder;
    const int bow = static_cast<int>(std::find(order.begin(), order.end(), "base:bow") - order.begin());
    r.pick(InputFrame::WEAPON + bow);
    ASSERT_EQ(r.world.run.phase, RunPhase::Fighting);

    // Many level ups: never a weapon, never another weapon's skill; the shop never sells one either
    r.timeUp();
    ASSERT_EQ(r.world.run.phase, RunPhase::Shop);
    Fighter& p = r.player();

    bool sawBowSkill = false;
    for (int k = 0; k < 60; ++k)
    {
        p.levelUps = 1;
        r.world.rollLevel(p);
        for (const auto& id : p.levelOffer)
        {
            EXPECT_FALSE(r.content.weapons.count(id)) << id;
            if (auto a = r.content.abilities.find(id); a != r.content.abilities.end() and not a->second.weapons.empty())
            {
                EXPECT_EQ(a->second.weapons.front(), "bow") << id;
                sawBowSkill = true;
            }
        }
    }
    EXPECT_TRUE(sawBowSkill);

    r.world.collect(p, 5000);
    p.levelUps = 0;
    p.levelOffer.clear();
    for (int k = 0; k < 20; ++k)
    {
        r.pick(InputFrame::REROLL);
        for (const auto& o : p.shop)
            EXPECT_TRUE(r.content.upgrades.count(o.id)) << o.id;
    }
}

TEST(arena_run, the_bow_shoots_where_you_aim_and_its_volley_fans_out)
{
    RunArena r;
    r.player().weapon = "base:bow";
    r.run(5);

    InputFrame in;
    in.lightPressed = true;
    in.aimDir = glm::vec3(0, 0, 1);
    r.world.step(in);
    r.run(10, InputFrame{});
    ASSERT_EQ(r.world.projectiles.size(), 1u);
    EXPECT_GT(r.world.projectiles[0].velocity.z, 30.0f);

    // The volley skill: five arrows, all going different ways
    r.world.projectiles.clear();
    const AbilityDef& volley = r.content.abilities.at("base:volley");
    EXPECT_EQ(volley.attack.projectile.count, 5);
    r.player().owned.push_back("base:volley");
    const int slot = [&]() {
        for (int i = 0; i < InputFrame::ABILITIES; ++i)
            if (const AbilityDef* a = r.world.ability(i, &r.player()); a and a->id == "base:volley")
                return i;
        return -1;
    }();
    ASSERT_GE(slot, 0);

    InputFrame skill;
    skill.abilityPressed[slot] = true;
    skill.abilityHeld[slot] = true;
    skill.aimDir = glm::vec3(0, 0, 1);
    r.world.step(skill);
    r.run(20, InputFrame{});
    ASSERT_EQ(r.world.projectiles.size(), 5u);
    EXPECT_GT(std::fabs(r.world.projectiles.front().velocity.x - r.world.projectiles.back().velocity.x), 2.0f);
}

TEST(arena_run, the_healer_heals_itself_and_allies_close_by_never_above_their_most)
{
    RunArena r;
    const int ally = r.world.addPlayer(1);
    r.run(5);

    Fighter& healer = r.player();
    healer.weapon = "base:staff";
    healer.health = 50.0f;
    Fighter* friendNear = r.world.find(ally);
    friendNear->body.position = healer.body.position + glm::vec3(2, 0, 0);
    friendNear->health = friendNear->maxHealth - 5.0f;

    InputFrame in;
    in.heavyPressed = true;
    r.world.step(in);
    r.run(40, InputFrame{});

    EXPECT_NEAR(r.player().health, 50.0f + r.content.weapons.at("base:staff").moveset.attacks.at("mending_pulse").heal.amount, 0.5f);
    EXPECT_FLOAT_EQ(r.world.find(ally)->health, r.world.find(ally)->maxHealth);   // topped up, not past it
}

TEST(arena_run, sanctuary_heals_the_players_in_it_for_a_while_and_not_the_enemies)
{
    RunArena r;
    r.run(5);
    const int enemy = r.world.spawnEnemy("base:slime", r.player().body.position + glm::vec3(1, 0, 0), 0.0f);   // a dummy mends itself
    r.world.find(enemy)->health = 10.0f;

    // (after the spawn: a new fighter can move the others in memory)
    Fighter& healer = r.player();
    healer.weapon = "base:staff";
    healer.health = 20.0f;

    healer.owned.push_back("base:sanctuary");
    int slot = -1;
    for (int i = 0; i < InputFrame::ABILITIES; ++i)
        if (const AbilityDef* a = r.world.ability(i, &healer); a and a->id == "base:sanctuary")
            slot = i;
    ASSERT_GE(slot, 0);

    InputFrame skill;
    skill.abilityPressed[slot] = true;
    skill.abilityHeld[slot] = true;
    r.world.step(skill);
    r.run(30, InputFrame{});
    ASSERT_EQ(r.world.zones.size(), 1u);
    EXPECT_GT(r.world.zones[0].heal, 0.0f);

    // A little every half second: about 3 a second for the 3 s it has been there
    r.past(3.0f);
    EXPECT_GT(r.player().health, 26.0f);
    EXPECT_FLOAT_EQ(r.world.find(enemy)->health, 10.0f);
}

TEST(arena_run, the_bows_light_is_drawn_a_tap_is_weak_a_held_shot_strong)
{
    auto shoot = [](int holdSteps) {
        RunArena r;
        r.player().weapon = "base:bow";
        r.run(5);

        InputFrame in;
        in.lightPressed = true;
        in.lightHeld = holdSteps > 0;
        in.aimDir = glm::vec3(0, 0, 1);
        r.world.step(in);

        InputFrame keep;
        keep.aimDir = in.aimDir;
        for (int i = 0; i < 90 and r.world.projectiles.empty(); ++i)
        {
            keep.lightHeld = i < holdSteps;
            r.world.step(keep);
        }

        return r.world.projectiles.empty() ? Projectile{} : r.world.projectiles.front();
    };

    const Projectile tap = shoot(0);
    const Projectile drawn = shoot(60);
    RunArena arena;
    const auto& charge = arena.content.weapons.at("base:bow").moveset.attacks.at("shot1").charge;

    ASSERT_GT(tap.def.damage, 0.0f);
    EXPECT_NEAR(drawn.def.damage, tap.def.damage * charge.damage, 0.01f);
    EXPECT_GT(glm::length(drawn.velocity), glm::length(tap.velocity) * 1.3f);
}

// ---- The arenas: one per stretch of waves, the duel's own ---------------------------------------

namespace
{
    bool standsAt(const ArenaWorld& w, const glm::vec3& p)
    {
        const int x = static_cast<int>(std::floor(p.x)), y = static_cast<int>(std::floor(p.y + 0.01f)), z = static_cast<int>(std::floor(p.z));
        const auto& t = w.content->blockTable;
        return w.grid.isSolid(t, x, y - 1, z) and not w.grid.isSolid(t, x, y, z) and not w.grid.isSolid(t, x, y + 1, z);
    }
}

TEST(arena_run, the_base_mods_arenas_load_and_the_run_names_them)
{
    RunArena r;
    EXPECT_TRUE(r.content.report.errors.empty()) << (r.content.report.errors.empty() ? "" : r.content.report.errors[0].str());

    for (const char* id : {"base:pit", "base:stockade", "base:gorge", "base:icefall", "base:quarry", "base:crucible"})
    {
        ASSERT_TRUE(r.content.arenas.count(id)) << id;
        EXPECT_FALSE(r.content.arenas.at(id).name.empty()) << id;
    }

    EXPECT_EQ(r.content.runArenaId(1, false), "base:pit");
    EXPECT_EQ(r.content.runArenaId(4, false), "base:pit");
    EXPECT_EQ(r.content.runArenaId(5, false), "base:stockade");
    EXPECT_EQ(r.content.runArenaId(12, false), "base:gorge");
    EXPECT_EQ(r.content.runArenaId(20, false), "base:quarry");
    EXPECT_EQ(r.content.runArenaId(20, true), "base:crucible");

    // An unknown id is the training arena
    EXPECT_EQ(&r.content.arenaById("base:nowhere"), &r.content.arena);
}

TEST(arena_run, a_new_arena_is_built_as_its_first_wave_starts_everyone_at_its_spawn)
{
    RunArena r;
    r.world.startRun(3);
    EXPECT_EQ(r.world.arenaId, "base:pit");

    r.world.skipToWave(5);
    const ArenaDef& stockade = r.content.arenas.at("base:stockade");
    EXPECT_EQ(r.world.arenaId, "base:stockade");
    EXPECT_EQ(glm::ivec3(r.world.grid.width(), r.world.grid.height(), r.world.grid.depth()), stockade.size);
    EXPECT_NEAR(glm::length(r.player().body.position - stockade.playerSpawn), 0.0f, 0.5f);

    // Enemies come in round its centre, on its ground
    r.past(6.0f);
    int seen = 0;
    for (const auto& f : r.world.fighters)
        if (f.team == Team::Enemy)
        {
            ++seen;
            EXPECT_LT(glm::length(glm::vec2(f.spawnPosition.x - stockade.centre.x, f.spawnPosition.z - stockade.centre.z)), stockade.spawnRadius + 0.5f);
        }
    EXPECT_GT(seen, 0);

    // Back to an earlier wave: its arena again; the run's end: the training arena
    r.world.skipToWave(2);
    EXPECT_EQ(r.world.arenaId, "base:pit");
    r.world.stopRun();
    EXPECT_EQ(r.world.arenaId, "");
    EXPECT_EQ(r.world.grid.width(), 32);
}

TEST(arena_run, every_run_arena_has_ground_to_spawn_on)
{
    RunArena r;
    r.world.startRun(11);

    for (const auto& pick : r.content.run.arenas)
    {
        r.world.skipToWave(pick.from);
        ASSERT_EQ(r.world.arenaId, pick.arena);
        const ArenaDef& a = r.content.arenas.at(pick.arena);

        EXPECT_TRUE(standsAt(r.world, a.playerSpawn)) << pick.arena << " player spawn";

        // The duel's ring and the enemies' edge: somewhere to stand
        for (int k = 0; k < 8; ++k)
        {
            const float angle = static_cast<float>(k) * 0.785f;
            const glm::vec3 ring = r.world.groundNear(a.centre + glm::vec3(std::sin(angle), 0.0f, std::cos(angle)) * r.content.run.duel.radius);
            EXPECT_TRUE(standsAt(r.world, ring)) << pick.arena << " ring " << k;
        }
    }
}

TEST(arena_run, the_final_duel_is_fought_in_its_own_arena_on_its_ground)
{
    RunArena r;
    const int second = toTheDuel(r);
    ASSERT_EQ(r.world.run.phase, RunPhase::Duel);
    EXPECT_EQ(r.world.arenaId, "base:crucible");

    const Fighter* b = r.world.find(second);
    ASSERT_TRUE(b);
    EXPECT_TRUE(standsAt(r.world, r.player().spawnPosition));
    EXPECT_TRUE(standsAt(r.world, b->spawnPosition));
}

TEST(arena_run, the_stockades_barn_comes_down_once_its_posts_are_gone)
{
    RunArena r;
    r.world.showArena("base:stockade");
    ASSERT_EQ(r.world.arenaId, "base:stockade");

    const BlockId brick = r.content.block("base:brick");
    ASSERT_EQ(r.world.grid.get(22, 9, 22), brick);

    // All posts but one: the roof still holds
    const int posts[8][2] = {{16, 16}, {22, 16}, {28, 16}, {16, 22}, {28, 22}, {16, 28}, {22, 28}, {28, 28}};
    for (int k = 0; k < 7; ++k)
        for (int y = 4; y <= 7; ++y)
            r.world.terrain.remove({posts[k][0], y, posts[k][1]});
    r.run(3);
    EXPECT_EQ(r.world.grid.get(22, 9, 22), brick);
    EXPECT_TRUE(r.world.terrain.falling.empty());

    // The last one: it falls
    for (int y = 4; y <= 7; ++y)
        r.world.terrain.remove({posts[7][0], y, posts[7][1]});
    r.run(3);
    EXPECT_EQ(r.world.grid.get(22, 9, 22), AIR);
    EXPECT_FALSE(r.world.terrain.falling.empty());
}

TEST(arena_run, the_quarrys_table_falls_when_its_prop_burns)
{
    RunArena r;
    r.world.showArena("base:quarry");
    const BlockId dark = r.content.block("base:dark_stone");
    ASSERT_EQ(r.world.grid.get(10, 8, 37), dark);

    for (int y = 4; y <= 6; ++y)
        r.world.terrain.remove({10, y, 37});
    r.run(3);
    EXPECT_EQ(r.world.grid.get(10, 8, 37), AIR);
    EXPECT_FALSE(r.world.terrain.falling.empty());
}

TEST(arena_run, leech_heals_whole_from_a_direct_hit_a_fifth_from_an_area)
{
    RunArena r;
    r.world.startRun(7);
    Fighter& p = r.player();
    p.boons.push_back({"base:leech", 0});
    r.world.sumBoons(p);
    const float per = p.boon.lifeOnHit;
    ASSERT_GT(per, 0.0f);

    Fighter& e = r.near("base:brute", 2.0f);
    const int id = e.id;
    Fighter& me = r.player();

    // A weapon hit: all of it
    me.health = 50.0f;
    AttackDef swing;
    swing.damage = 1.0f;
    r.world.applyHit(me, *r.world.find(id), swing);
    EXPECT_NEAR(me.health, 50.0f + per, 0.01f);

    // An area's (a nova, a burst, a zone, a chain): a fifth
    me.health = 50.0f;
    AttackDef bolt = swing;
    bolt.leech = AREA_LEECH;
    r.world.applyHit(me, *r.world.find(id), bolt);
    EXPECT_NEAR(me.health, 50.0f + per * 0.2f, 0.01f);
}

TEST(arena_run, melee_and_attack_items_reach_the_weapons_own_attacks)
{
    RunArena r;
    r.world.startRun(7);
    Fighter& p = r.player();
    r.world.chooseWeapon(p, "base:sword");
    const Moveset* m = r.world.movesetOf(p);
    ASSERT_TRUE(m);
    const float light = r.world.attackDef(p, m->light)->damage;

    p.boons.push_back({"base:honed", 0});
    const float honed = 1.0f + r.content.upgrades.at("base:honed").boost.damage;
    EXPECT_NEAR(r.world.attackDef(p, m->light)->damage, light * honed, 0.01f);

    p.boons.push_back({"base:whetstone", 0});
    const float whet = r.content.upgrades.at("base:whetstone").boost.damage;
    EXPECT_NEAR(r.world.attackDef(p, m->light)->damage, light * (honed + whet), 0.01f);

    // Not an enemy's, not outside a run
    r.world.stopRun();
    EXPECT_FLOAT_EQ(r.world.attackDef(r.player(), m->light)->damage, light);
}

TEST(arena_run, each_player_picks_a_first_spell_after_the_weapon)
{
    RunArena r;
    r.content.run.chooseWeapon = true;
    r.content.run.startSpells.clear();
    r.content.run.startChoices = {"base:firebolt", "base:frostbolt", "base:spark"};
    r.world.startRun(5);
    ASSERT_EQ(r.world.run.phase, RunPhase::Armory);

    const auto& order = r.content.weaponOrder;
    const int sword = static_cast<int>(std::find(order.begin(), order.end(), "base:sword") - order.begin());

    // The weapon: not ready yet, the spell is still to pick
    r.pick(InputFrame::WEAPON + sword);
    EXPECT_TRUE(r.player().armed);
    EXPECT_FALSE(r.player().ready);
    EXPECT_EQ(r.world.run.phase, RunPhase::Armory);

    // Ice: on key 1, and the wave begins
    r.pick(InputFrame::FIRST_SPELL + 1);
    ASSERT_EQ(r.world.run.phase, RunPhase::Fighting);
    ASSERT_EQ(r.player().runSpells.size(), 1u);
    EXPECT_EQ(r.player().runSpells[0].id, "base:frostbolt");
    EXPECT_EQ(r.world.spellOf(r.world.spellRecipe(r.player(), 0))->element, Element::Ice);
}

TEST(arena_run, a_melee_weapon_mends_a_little)
{
    RunArena r;
    r.world.startRun(5);
    Fighter& p = r.player();
    r.world.chooseWeapon(p, "base:sword");
    const float regen = r.content.weapons.at("base:sword").regen;
    ASSERT_GT(regen, 0.0f);
    EXPECT_FLOAT_EQ(r.world.healthRegen(p), regen);
    EXPECT_FLOAT_EQ(r.content.weapons.at("base:bow").regen, 0.0f);

    p.health = 20.0f;
    r.past(1.0f);
    EXPECT_NEAR(r.player().health, 20.0f + regen, 0.05f);
}

TEST(arena_run, attack_speed_makes_the_weapons_attacks_quicker)
{
    RunArena r;
    r.world.startRun(5);
    Fighter& p = r.player();
    r.world.chooseWeapon(p, "base:dagger");
    const Moveset* m = r.world.movesetOf(p);
    ASSERT_TRUE(m);
    const AttackDef before = *r.world.attackDef(p, m->light);

    // Two Frenzies: 20% quicker, its lunge as far
    p.boons.push_back({"base:frenzy", 0});
    p.boons.push_back({"base:frenzy", 0});
    r.world.sumBoons(p);
    const AttackDef* after = r.world.attackDef(p, m->light);
    EXPECT_NEAR(static_cast<float>(after->total()), static_cast<float>(before.total()) / 1.2f, 2.0f);
    EXPECT_LT(after->total(), before.total());
    EXPECT_FLOAT_EQ(after->lunge, before.lunge * 1.2f);
    EXPECT_FLOAT_EQ(after->damage, before.damage);

    // Outside a run: as written
    r.world.stopRun();
    EXPECT_EQ(r.world.attackDef(r.player(), m->light)->total(), before.total());
}

// ---- Difficulty levels, and the items that bring more enemies and more out of them -------------

TEST(arena_run, the_shipped_levels_start_short_and_open_one_by_one_in_length)
{
    const Content c = loadContent(VOXELARENA_MODS_DIR);
    const RunDef& run = c.run;

    ASSERT_GE(run.levels(), 4);
    EXPECT_EQ(run.wavesAt(0), 10);
    EXPECT_EQ(run.wavesAt(3), run.waves);
    for (int k = 1; k < run.levels(); ++k)
    {
        EXPECT_GE(run.wavesAt(k), run.wavesAt(k - 1));
        EXPECT_GT(run.difficulty(k).health, run.difficulty(k - 1).health);
    }

    // Past the last one: the last one
    EXPECT_EQ(run.difficulty(99).name, run.difficulty(run.levels() - 1).name);
}

TEST(arena_run, a_harder_level_is_shorter_or_longer_and_its_enemies_tougher)
{
    RunArena r;
    r.content.run.difficulties = {
        DifficultyDef{},
        DifficultyDef{"Hard", 13, 1.5f, 1.25f, 0.02f, 0.01f, 2.0f, 0.1f},
    };

    // The level picked outside a run is the next run's; it stays for the one after
    r.world.run.difficulty = 1;
    r.world.startRun(5);
    EXPECT_EQ(r.world.run.difficulty, 1);
    EXPECT_EQ(r.world.run.waves, 13);

    // Health: the level's, and it grows faster each wave; damage too
    r.world.skipToWave(5);
    const float growth = r.content.run.healthGrowth + 0.02f;
    EXPECT_FLOAT_EQ(r.world.enemyHealthScale(5), 1.5f * (1.0f + growth * 4.0f));
    EXPECT_FLOAT_EQ(r.world.enemyDamageScale(5), 1.25f * (1.0f + 0.01f * 4.0f));

    r.world.run.pending.push_back({"base:slime", r.player().body.position + glm::vec3(0, 0, 5), 0.0f, false, false});
    r.run(1);
    const Fighter* slime = nullptr;
    for (const auto& f : r.world.fighters)
        if (f.team == Team::Enemy and f.type == "base:slime")
            slime = &f;
    ASSERT_TRUE(slime);
    EXPECT_NEAR(slime->maxHealth, r.content.enemies.at("base:slime").health * r.world.enemyHealthScale(5), 0.01f);

    // Its last wave won is a victory
    r.world.skipToWave(13);
    r.timeUp();
    EXPECT_EQ(r.world.run.phase, RunPhase::Victory);

    // Back out: still the level picked
    r.world.stopRun();
    EXPECT_EQ(r.world.run.difficulty, 1);
}

TEST(arena_run, the_host_chooses_the_difficulty_among_the_open_levels_before_the_run)
{
    RunArena r;
    r.content.run.difficulties = {DifficultyDef{}, DifficultyDef{"Hard", 13}, DifficultyDef{"Nightmare", 20}};
    const int guest = r.world.addPlayer(1);

    r.world.run.difficulty = 0;
    r.world.run.levelsOpen = 2;   // Nightmare is not open yet
    r.world.chooseDifficulty(5);
    EXPECT_EQ(r.world.run.phase, RunPhase::Difficulty);
    EXPECT_EQ(r.world.run.chooser, r.player().id);   // the host's player, not the guest

    // Only the chooser's pick counts, and only an open level
    std::map<int, InputFrame> in;
    in[guest].pick = InputFrame::DIFFICULTY + 1;
    r.world.step(in);
    EXPECT_EQ(r.world.run.phase, RunPhase::Difficulty);

    r.pick(InputFrame::DIFFICULTY + 2);
    EXPECT_EQ(r.world.run.phase, RunPhase::Difficulty);

    // Back: no run
    r.pick(InputFrame::BACK);
    EXPECT_EQ(r.world.run.phase, RunPhase::Off);
    EXPECT_EQ(r.world.run.levelsOpen, 2);

    // Hard: the run begins at it
    r.world.chooseDifficulty(6);
    r.pick(InputFrame::DIFFICULTY + 1);
    EXPECT_EQ(r.world.run.phase, RunPhase::Fighting);
    EXPECT_EQ(r.world.run.difficulty, 1);
    EXPECT_EQ(r.world.run.waves, 13);

    // Play again at the end: the choice again
    r.world.skipToWave(13);
    r.timeUp();
    ASSERT_TRUE(r.world.run.phase == RunPhase::Victory or r.world.run.phase == RunPhase::Duel);
    r.world.run.phase = RunPhase::Victory;
    r.pick(InputFrame::START);
    EXPECT_EQ(r.world.run.phase, RunPhase::Difficulty);
}

TEST(arena_run, with_one_level_there_is_nothing_to_choose)
{
    RunArena r;
    r.world.chooseDifficulty(5);
    EXPECT_EQ(r.world.run.phase, RunPhase::Fighting);
}

TEST(arena_run, each_player_keeps_what_they_did_for_the_recap)
{
    RunArena r;
    r.world.startRun(9);
    EXPECT_EQ(r.player().stats.kills, 0);

    // A kill of theirs: the kill, the damage up to the health it had
    Fighter& slime = r.near("base:slime");
    slime.health = 5.0f;
    AttackDef hit;
    hit.name = "test";
    hit.damage = 40.0f;
    r.world.applyHit(r.player(), slime, hit);
    EXPECT_EQ(r.player().stats.kills, 1);
    EXPECT_FLOAT_EQ(r.player().stats.damage, 5.0f);

    // Shards picked up and the XP they gave
    r.world.collect(r.player(), 7);
    EXPECT_EQ(r.player().stats.shards, 7);
    EXPECT_EQ(r.player().stats.xp, 7);

    // An item bought
    r.timeUp();
    ASSERT_EQ(r.world.run.phase, RunPhase::Shop);
    Fighter& p = r.player();
    p.levelUps = 0;
    p.levelOffer.clear();
    p.presents = 0;
    p.shards = 1000;
    r.pick(InputFrame::BUY);
    EXPECT_EQ(r.player().stats.items, 1);

    // A new run starts them over
    r.world.startRun(10);
    EXPECT_EQ(r.player().stats.kills, 0);
    EXPECT_EQ(r.player().stats.items, 0);
}

TEST(arena_run, items_have_no_limit_until_one_more_would_do_nothing)
{
    RunArena r;
    r.world.startRun(9);
    Fighter& p = r.player();

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

    // Tough has no limit: still in the shop after ten
    for (int k = 0; k < 10; ++k)
        r.world.takeBoon(p, {"base:tough"});
    EXPECT_TRUE(offered("base:tough"));

    // Armor stops at 70%: past it, Plated no longer comes
    EXPECT_TRUE(offered("base:plated"));
    while (p.boon.armor < 0.7f)
        r.world.takeBoon(p, {"base:plated"});
    EXPECT_TRUE(r.world.itemMaxed(p, r.content.upgrades.at("base:plated")));
    EXPECT_FALSE(offered("base:plated"));

    // A limit written in the item still holds
    const int most = r.content.upgrades.at("base:nimble").most;
    ASSERT_GT(most, 0);
    for (int k = 0; k < most; ++k)
        r.world.takeBoon(p, {"base:nimble"});
    EXPECT_FALSE(offered("base:nimble"));
}

TEST(arena_run, an_item_with_tiers_only_comes_at_those_rarities)
{
    RunArena r;
    r.world.startRun(9);
    r.world.skipToWave(r.world.run.waves);   // the rarer ones are likeliest at the end
    Fighter& p = r.player();

    ASSERT_EQ(r.content.upgrades.at("base:split_shot").tiers, std::vector<std::string>({"Rare"}));
    int rare = -1;
    for (size_t k = 0; k < r.content.run.rarities.size(); ++k)
        if (r.content.run.rarities[k].name == "Rare")
            rare = static_cast<int>(k);
    ASSERT_GE(rare, 0);

    // Split shot needs something that shoots: a bow
    r.world.chooseWeapon(p, "base:bow");
    int seen = 0, others = 0;
    for (int k = 0; k < 400; ++k)
    {
        r.world.rollShop(p);
        for (const auto& o : p.shop)
        {
            if (o.id == "base:split_shot")
            {
                ++seen;
                EXPECT_EQ(o.rarity, rare);
            }
            else if (o.rarity != rare)
                ++others;
        }
    }
    EXPECT_GT(seen, 0);
    EXPECT_GT(others, 0);   // the rest still come at every rarity
}

TEST(arena_run, scholar_gives_more_xp_per_shard_and_greed_more_shards)
{
    RunArena r;
    r.world.startRun(9);
    Fighter& p = r.player();

    // Half again as much XP: the half waits for the next shard
    p.boons.push_back({"base:scholar"});
    p.boons.push_back({"base:scholar"});
    r.content.upgrades["base:scholar"].xpGain = 0.25f;
    r.world.sumBoons(p);
    EXPECT_FLOAT_EQ(p.boon.xpGain, 0.5f);

    r.world.collect(p, 1);
    EXPECT_EQ(p.xp, 1);
    r.world.collect(p, 1);
    EXPECT_EQ(p.xp, 3);
    EXPECT_EQ(p.shards, 2);   // shards are not more: only the XP

    // Greed adds up with diminishing returns: 100% counts as 67% at a falloff of 0.5
    p.boons.push_back({"base:greed"});
    r.content.upgrades["base:greed"].shardChance = 1.0f;
    r.world.sumBoons(p);
    r.content.run.shardFalloff = 0.5f;
    EXPECT_NEAR(r.world.teamShardChance(), 1.0f / 1.5f, 1e-5f);

    // Without it, greed at 100%: every shard of a kill comes with one more
    r.content.run.shardFalloff = 0.0f;
    EXPECT_FLOAT_EQ(r.world.teamShardChance(), 1.0f);

    const int drops = r.content.enemies.at("base:brute").drops;
    r.near("base:brute");
    r.killAll();
    int lying = 0;
    for (const auto& pk : r.world.pickups)
        lying += pk.value;
    EXPECT_EQ(lying, 2 * drops);
}

TEST(arena_run, horde_brings_more_enemies_for_everyone)
{
    RunArena plain, horde;
    plain.world.startRun(11);
    horde.world.startRun(11);

    horde.player().boons.push_back({"base:horde"});
    horde.player().boons.push_back({"base:horde"});
    horde.content.upgrades["base:horde"].horde = 0.5f;
    horde.world.sumBoons(horde.player());
    EXPECT_FLOAT_EQ(horde.world.teamHorde(), 1.0f);

    // Twice the spawn rate: about twice the enemies after a while (none die: no AI, nobody fights)
    plain.past(20.0f);
    horde.past(20.0f);
    const int few = plain.enemies() + static_cast<int>(plain.world.run.pending.size());
    const int many = horde.enemies() + static_cast<int>(horde.world.run.pending.size());
    EXPECT_GT(few, 0);
    EXPECT_GE(many, few * 3 / 2);
}
