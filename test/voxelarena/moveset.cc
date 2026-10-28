#include "stdafx.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>

#include "Core/content.h"
#include "Core/tuning.h"
#include "Core/world.h"

using namespace arena;

// The moves beyond the basic set (air tech, dash and air attacks) and the spells.

namespace
{
    /** The shipped content on a bare floor, as in world.cc. */
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

            content.arena.size = glm::ivec3(32, 16, 32);
            content.arena.shapes = parseJson(R"([ { "block": "stone", "from": [0, 0, 0], "to": [31, 0, 31] } ])").value;
            content.arena.spawns.clear();
            content.arena.playerSpawn = glm::vec3(16, 1, 8);

            world.start(content, tuning);
            world.aiEnabled = false;
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

        /** Puts the player in the air, still. */
        void lift(float y)
        {
            player().body.position.y = y;
            player().body.velocity = glm::vec3(0.0f);
            player().body.onGround = false;
        }

        int count(const std::string& kind) const
        {
            return static_cast<int>(std::count_if(world.events.begin(), world.events.end(), [&](const WorldEvent& e) { return e.kind == kind; }));
        }

        /** The slot of an ability in the player's list. */
        int slot(const std::string& id) const
        {
            for (size_t i = 0; i < tuning.abilities.size(); ++i)
                if (std::find(tuning.abilities[i].begin(), tuning.abilities[i].end(), id) != tuning.abilities[i].end())
                    return static_cast<int>(i);
            return -1;
        }

        /** Casts the ability in `slot`, aimed from behind the player's head at `target`. */
        void cast(const std::string& id, const glm::vec3& target)
        {
            const int s = slot(id);
            ASSERT_GE(s, 0) << id;

            InputFrame in;
            in.abilityPressed[s] = true;
            in.aimOrigin = player().body.position + glm::vec3(0, 1.6f, -2.0f);
            in.aimDir = glm::normalize(target - in.aimOrigin);
            world.step(in);
        }

        Fighter* brute(const glm::vec3& at)
        {
            Fighter* f = world.find(world.spawnEnemy("base:brute", at, 180.0f));
            settle(3);
            return f;
        }

        bool has(const Fighter& f, const std::string& status) const
        {
            return std::any_of(f.statuses.begin(), f.statuses.end(), [&](const StatusState& s) { return s.id == status; });
        }

        int stacks(const Fighter& f, const std::string& status) const
        {
            for (const auto& s : f.statuses)
                if (s.id == status)
                    return s.stacks;
            return 0;
        }
    };

    InputFrame jumpPress()
    {
        InputFrame in;
        in.jumpPressed = true;
        in.jumpHeld = true;
        return in;
    }
}

// ---- Air tech ---------------------------------------------------------------------------------

TEST(arena_moves, one_more_jump_in_the_air_then_none_until_landing)
{
    Arena a;
    InputFrame held;
    held.jumpHeld = true;

    a.world.step(jumpPress());
    a.run(15, held);
    ASSERT_FALSE(a.player().body.onGround);

    a.world.events.clear();
    a.world.step(jumpPress());
    EXPECT_GT(a.player().body.velocity.y, 5.0f);   // up again
    a.run(10, held);
    a.world.step(jumpPress());                     // a third: nothing
    EXPECT_EQ(a.count("air_jump"), 1);

    // Landed: it is back
    a.run(120);
    ASSERT_TRUE(a.player().body.onGround);
    a.world.step(jumpPress());
    a.run(15, held);
    a.world.events.clear();
    a.world.step(jumpPress());
    EXPECT_EQ(a.count("air_jump"), 1);
}

TEST(arena_moves, a_jump_off_a_wall_pushes_away_and_up)
{
    Arena a;
    a.world.grid.fill({18, 1, 0}, {18, 8, 31}, a.content.block("base:stone"));   // a wall at x = 18
    a.player().body.position = glm::vec3(17.6f, 1.0f, 16.0f);
    a.settle(5);

    a.world.step(jumpPress());
    a.run(12);
    ASSERT_FALSE(a.player().body.onGround);

    a.world.events.clear();
    a.world.step(jumpPress());
    EXPECT_EQ(a.count("wall_jump"), 1);
    EXPECT_EQ(a.count("air_jump"), 0);                                  // the wall first: the air jump is kept
    EXPECT_LT(a.player().body.velocity.x, -a.tuning.air.wallJumpPush * 0.9f);   // away from the wall
    EXPECT_GT(a.player().body.velocity.y, 5.0f);

    // Pushing back towards the wall is not steered for a moment
    InputFrame back;
    back.move = glm::vec2(1, 0);   // camera yaw 0: right is +x, into the wall
    a.run(4, back);
    EXPECT_LT(a.player().body.velocity.x, -a.tuning.air.wallJumpPush * 0.5f);
}

TEST(arena_moves, an_air_dash_bursts_straight_once_per_time_in_the_air)
{
    Arena a;
    a.lift(9.0f);

    InputFrame dash;
    dash.dodgePressed = true;
    dash.move = glm::vec2(0, 1);
    a.world.step(dash);
    ASSERT_EQ(a.player().action, Action::AirDash);

    InputFrame hold;
    hold.move = glm::vec2(0, 1);
    a.run(4, hold);
    EXPECT_NEAR(a.player().body.velocity.z, a.tuning.air.dashSpeed, 0.01f);
    EXPECT_NEAR(a.player().body.velocity.y, 0.0f, 1e-4f);   // no fall during it
    a.run(a.tuning.air.dashFrames, hold);
    EXPECT_EQ(a.player().action, Action::Free);

    // A second one: no (one per time in the air)
    a.world.step(dash);
    a.run(2, hold);
    EXPECT_NE(a.player().action, Action::AirDash);
}

TEST(arena_moves, crouch_held_in_the_air_falls_faster)
{
    auto heightAfter = [](bool crouch) {
        Arena a;
        a.lift(12.0f);
        InputFrame in;
        in.crouchHeld = crouch;
        a.run(20, in);
        return a.player().body.position.y;
    };

    EXPECT_LT(heightAfter(true), heightAfter(false) - 1.0f);
}

// ---- Attacks ----------------------------------------------------------------------------------

TEST(arena_moves, a_light_press_while_sprinting_is_the_dash_attack)
{
    auto attackAfter = [](bool sprint) {
        Arena a;
        InputFrame run;
        run.move = glm::vec2(0, 1);
        run.sprintHeld = sprint;
        a.run(30, run);

        InputFrame light = run;
        light.lightPressed = true;
        a.world.step(light);
        return a.player().attack;
    };

    EXPECT_EQ(attackAfter(true), "dash_thrust");
    EXPECT_EQ(attackAfter(false), "light1");
}

TEST(arena_moves, a_light_in_the_air_is_the_air_slash_and_hangs)
{
    Arena a;
    a.lift(8.0f);
    a.run(10);
    const float falling = a.player().body.velocity.y;

    InputFrame light;
    light.lightPressed = true;
    a.world.step(light);
    EXPECT_EQ(a.player().attack, "air_slash");

    a.run(6);
    EXPECT_GT(a.player().body.velocity.y, falling);   // caught, falling slower than before
}

TEST(arena_moves, a_heavy_in_the_air_plunges_and_strikes_where_it_lands)
{
    Arena a;
    Fighter* target = a.brute(a.player().body.position + glm::vec3(0, 0, 1.2f));
    ASSERT_TRUE(target);
    const int targetId = target->id;
    a.lift(7.0f);

    InputFrame heavy;
    heavy.heavyPressed = true;
    a.world.step(heavy);
    ASSERT_EQ(a.player().attack, "plunge");
    const AttackDef* plunge = a.content.weapons.at("base:sword").moveset.find("plunge");
    ASSERT_TRUE(plunge);

    // The startup hangs, then it dives holding its first active frame
    a.run(plunge->startup + 2);
    EXPECT_NEAR(a.player().body.velocity.y, -plunge->plungeSpeed, 0.01f);
    EXPECT_EQ(a.player().frame, plunge->startup);
    EXPECT_FLOAT_EQ(a.world.find(targetId)->health, a.world.find(targetId)->maxHealth);   // nothing hit in the air

    int steps = 0;
    while (not a.player().body.onGround and steps++ < 60)
        a.world.step(InputFrame{});
    ASSERT_TRUE(a.player().body.onGround);

    a.run(plunge->active + 2);
    EXPECT_LT(a.world.find(targetId)->health, a.world.find(targetId)->maxHealth);   // the landing strikes
}

// ---- Spells -----------------------------------------------------------------------------------

TEST(arena_spells, a_fireball_bursts_burns_and_costs_mana)
{
    Arena a;
    Fighter* target = a.brute(a.player().body.position + glm::vec3(0, 0, 7.0f));
    ASSERT_TRUE(target);
    const int id = target->id;
    const AbilityDef& fireball = a.content.abilities.at("base:fireball");

    a.cast("base:fireball", a.world.find(id)->body.position + glm::vec3(0, 1.0f, 0));
    EXPECT_NEAR(a.player().mana, a.tuning.mana.max - fireball.manaCost, 0.01f);
    ASSERT_EQ(a.world.projectiles.size(), 1u);

    int steps = 0;
    while (not a.world.projectiles.empty() and steps++ < 60)
        a.world.step(InputFrame{});

    Fighter& hit = *a.world.find(id);
    EXPECT_TRUE(a.has(hit, "base:burn"));
    const float afterHit = hit.health;
    EXPECT_LT(afterHit, hit.maxHealth);

    a.run(60);   // a second of burning
    EXPECT_NEAR(afterHit - a.world.find(id)->health, a.content.statuses.at("base:burn").damagePerSecond, 0.5f);

    // Not enough mana: nothing
    a.run(60);
    a.player().mana = fireball.manaCost - 1.0f;
    a.cast("base:fireball", a.world.find(id)->body.position + glm::vec3(0, 1.0f, 0));
    EXPECT_TRUE(a.world.projectiles.empty());
}

TEST(arena_spells, fire_sets_planks_alight_and_it_spreads_and_burns_away)
{
    Arena a;
    const BlockId planks = a.content.block("base:planks");
    a.world.grid.fill({14, 1, 15}, {18, 1, 15}, planks);   // a row of planks, five long (one high: nothing falls as it burns)
    a.settle(2);

    a.cast("base:fireball", glm::vec3(16.5f, 1.5f, 15.0f));
    int steps = 0;
    while (not a.world.projectiles.empty() and steps++ < 60)
        a.world.step(InputFrame{});

    EXPECT_TRUE(a.world.terrain.isBurning({16, 1, 15}));
    EXPECT_FALSE(a.world.terrain.isBurning({14, 1, 15}));   // not yet: it has to spread there

    const auto& E = a.tuning.elements;
    a.run(static_cast<int>((E.burnSeconds + E.spreadSeconds * 3.0f + 0.5f) * 60.0f));

    for (int x = 14; x <= 18; ++x)
        EXPECT_EQ(a.world.grid.get(x, 1, 15), AIR) << x;   // all of it, spread and gone
    EXPECT_EQ(a.world.terrain.burningCount(), 0u);
}

TEST(arena_spells, ice_chills_then_freezes_solid)
{
    Arena a;
    Fighter* target = a.brute(a.player().body.position + glm::vec3(0, 0, 4.0f));
    ASSERT_TRUE(target);
    const int id = target->id;

    auto castAndWait = [&]() {
        a.cast("base:ice_shards", a.world.find(id)->body.position + glm::vec3(0, 1.0f, 0));
        int steps = 0;
        while (not a.world.projectiles.empty() and steps++ < 60)
            a.world.step(InputFrame{});
    };

    castAndWait();
    EXPECT_EQ(a.stacks(*a.world.find(id), "base:chill"), 3);   // the three shards of the fan

    a.run(40);   // the cooldown
    castAndWait();
    Fighter& frozen = *a.world.find(id);
    EXPECT_TRUE(a.has(frozen, "base:frozen"));
    EXPECT_FALSE(a.has(frozen, "base:chill"));
    EXPECT_EQ(frozen.action, Action::Stagger);
    EXPECT_TRUE(frozen.heavyStagger);
}

TEST(arena_spells, chill_slows_by_its_stacks)
{
    auto speedWith = [](int stacks) {
        Arena a;
        if (stacks > 0)
            a.world.applyStatus(a.player(), "base:chill", stacks);
        InputFrame walk;
        walk.move = glm::vec2(0, 1);
        a.run(30, walk);
        return a.player().body.velocity.z;
    };

    const float slow = Arena().content.statuses.at("base:chill").slow;
    EXPECT_NEAR(speedWith(2), speedWith(0) * (1.0f - 2.0f * slow), 0.05f);
}

TEST(arena_spells, ice_freezes_a_dug_pit_over_until_before_it_fills)
{
    Arena a;
    const BlockId stone = a.content.block("base:stone"), ice = a.content.block("base:ice");
    a.world.grid.fill({0, 1, 0}, {31, 2, 31}, stone);   // a thicker floor to dig in
    a.player().body.position.y = 3.0f;
    a.settle(10);

    // A 3 x 3 pit, two deep, ahead
    for (int x = 15; x <= 17; ++x)
        for (int z = 13; z <= 15; ++z)
            for (int y = 1; y <= 2; ++y)
                a.world.terrain.digFor({x, y, z}, 10.0f);
    a.settle(2);

    // On the ground at the pit's edge (from chest height a shot cannot reach down into it): the
    // freeze reaches over the pit
    a.cast("base:ice_shards", glm::vec3(16.5f, 3.0f, 12.6f));
    int steps = 0;
    while (not a.world.projectiles.empty() and steps++ < 60)
        a.world.step(InputFrame{});

    // Ice in the top cell of the pit's columns: a floor at the ground's level
    EXPECT_EQ(a.world.grid.get(16, 2, 13), ice);
    EXPECT_EQ(a.world.grid.get(16, 2, 14), ice);
    EXPECT_EQ(a.world.grid.get(16, 1, 13), AIR);

    // It melts before the pit fills back in, and the pit fills as it should
    a.run(11 * 60);
    EXPECT_EQ(a.world.grid.get(16, 2, 13), stone);
    EXPECT_EQ(a.world.grid.get(16, 1, 13), stone);
}

TEST(arena_spells, volt_chains_through_enemies_and_shocks_them)
{
    Arena a;
    const glm::vec3 p = a.player().body.position;
    std::vector<int> ids;
    for (float z : {6.0f, 10.0f, 14.0f})
        ids.push_back(a.brute(p + glm::vec3(0, 0, z))->id);
    Fighter* far = a.brute(p + glm::vec3(10, 0, 14));   // too far from the others to be chained
    const int farId = far->id;

    a.cast("base:volt", a.world.find(ids[0])->body.position + glm::vec3(0, 1.0f, 0));

    for (int id : ids)
    {
        const Fighter& f = *a.world.find(id);
        EXPECT_LT(f.health, f.maxHealth) << id;
        EXPECT_EQ(f.action, Action::Stagger) << id;   // shocked: interrupted
    }

    EXPECT_FLOAT_EQ(a.world.find(farId)->health, a.world.find(farId)->maxHealth);
    EXPECT_EQ(a.count("volt_arc"), 3);
}

TEST(arena_spells, volt_breaks_stone_and_shrugs_off_armor)
{
    Arena a;
    const BlockId stone = a.content.block("base:stone");
    a.world.grid.set(16, 1, 14, stone);
    a.world.grid.set(16, 2, 14, stone);
    a.settle(2);

    a.cast("base:volt", glm::vec3(16.5f, 1.5f, 14.0f));
    a.settle(2);
    EXPECT_EQ(a.world.grid.get(16, 1, 14), AIR);   // 40 x 4 against a hardness of 120

    // A shock is light: an attack with armor goes through it
    Fighter* brute = a.brute(a.player().body.position + glm::vec3(0, 0, 5.0f));
    brute->action = Action::Attack;
    brute->attack = "slam";
    brute->frame = 5;
    ASSERT_GT(a.world.currentAttack(*brute)->armor, 0.0f);
    a.world.applyStatus(*brute, "base:shock");
    EXPECT_EQ(brute->action, Action::Attack);
}


// ---- The dagger --------------------------------------------------------------------------------

namespace
{
    /** Presses `sequence` ('L' light, 'H' heavy) one after the other, each once the attack before can
     *  chain, and returns the attacks that ran, in order. */
    std::vector<std::string> combo(Arena& a, const std::string& sequence, std::vector<float>* zs = nullptr, bool* wasInvulnerable = nullptr)
    {
        std::vector<std::string> ran;
        size_t next = 0;

        for (int step = 0; step < 240; ++step)
        {
            InputFrame in;
            const Fighter& p = a.player();
            const AttackDef* now = a.world.currentAttack(p);

            // The next press as soon as the current attack remembers it
            if (next < sequence.size() and (not now or p.frame >= now->bufferFrom + 1))
            {
                const bool fresh = ran.empty() or (now and ran.back() == now->name and ran.size() == next);
                if (fresh)
                {
                    (sequence[next] == 'L' ? in.lightPressed : in.heavyPressed) = true;
                    ++next;
                }
            }

            a.world.step(in);

            if (const AttackDef* after = a.world.currentAttack(a.player()); after and (ran.empty() or ran.back() != after->name))
                ran.push_back(after->name);
            if (zs)
                zs->push_back(a.player().body.position.z);
            if (wasInvulnerable and a.player().invulnerable(a.tuning))
                *wasInvulnerable = true;

            if (next == sequence.size() and not a.world.currentAttack(a.player()) and a.player().body.onGround)
                break;
        }

        return ran;
    }
}

TEST(arena_dagger, two_combos_that_end_in_a_dash_or_a_backflip)
{
    auto run = [](const std::string& presses) {
        Arena a;
        a.player().weapon = "base:dagger";
        return combo(a, presses);
    };

    EXPECT_EQ(run("LLLL"), (std::vector<std::string>{"jab1", "jab2", "jab3", "dash_finisher"}));
    EXPECT_EQ(run("LLLH"), (std::vector<std::string>{"jab1", "jab2", "jab3", "backflip_finisher"}));
    EXPECT_EQ(run("HHH"), (std::vector<std::string>{"spin_cut", "rising_cut", "backflip_finisher"}));
    EXPECT_EQ(run("HHL"), (std::vector<std::string>{"spin_cut", "rising_cut", "dash_finisher"}));
}

TEST(arena_dagger, the_dash_finisher_goes_through_an_enemy)
{
    Arena a;
    a.player().weapon = "base:dagger";
    const int dummy = a.world.spawnEnemy("base:dummy", a.player().body.position + glm::vec3(0, 0, 2.5f), 180.0f);
    a.settle(3);

    bool invulnerable = false;
    combo(a, "LLLL", nullptr, &invulnerable);

    EXPECT_TRUE(invulnerable);
    EXPECT_GT(a.player().body.position.z, a.world.find(dummy)->body.position.z);   // out the other side
    EXPECT_LT(a.world.find(dummy)->health, a.world.find(dummy)->maxHealth);
}

TEST(arena_dagger, the_backflip_leaps_back_and_up)
{
    Arena a;
    a.player().weapon = "base:dagger";
    std::vector<float> zs;
    const float start = a.player().body.position.z;

    float highest = 0.0f;
    combo(a, "LLLH", &zs);
    const float flipStart = *std::max_element(zs.begin(), zs.end());   // the jabs went forward first

    // Leapt: back behind where the flip began, and off the ground on the way
    Arena b;
    b.player().weapon = "base:dagger";
    b.player().action = Action::Attack;
    b.player().attack = "backflip_finisher";
    b.player().frame = 0;
    for (int i = 0; i < 40; ++i)
    {
        b.world.step(InputFrame{});
        highest = std::max(highest, b.player().body.position.y);
    }

    EXPECT_GT(highest, b.content.arena.playerSpawn.y + 1.0f);
    EXPECT_LT(a.player().body.position.z, flipStart - 2.0f);
    EXPECT_GT(a.player().body.position.z, start - 10.0f);   // a flip, not a flight
    EXPECT_TRUE(a.player().body.onGround);                   // and down again
}

TEST(arena_moves, infinite_stamina_and_mana_for_testing)
{
    Arena a;
    a.world.infiniteStamina = true;
    a.world.infiniteMana = true;

    // Rolls back to back, a sprint, and spells: nothing spent
    InputFrame roll;
    roll.dodgePressed = true;
    roll.move = glm::vec2(0, 1);
    for (int i = 0; i < 8; ++i)
    {
        a.world.step(roll);
        a.run(30);
    }

    InputFrame sprint;
    sprint.sprintHeld = true;
    sprint.move = glm::vec2(0, -1);
    a.run(120, sprint);
    EXPECT_FLOAT_EQ(a.player().stamina, a.tuning.stamina.max);

    for (int i = 0; i < 4; ++i)
    {
        a.cast("base:volt", a.player().body.position + glm::vec3(0, 1, 10));
        a.run(70);
    }
    EXPECT_FLOAT_EQ(a.player().mana, a.tuning.mana.max);

    // Off again: a roll costs
    a.world.infiniteStamina = false;
    a.world.step(roll);
    EXPECT_LT(a.player().stamina, a.tuning.stamina.max);
}

TEST(arena_moves, a_roll_jump_lands_back_to_run_speed_quickly)
{
    Arena a;
    InputFrame run;
    run.move = glm::vec2(0, 1);

    // A roll, a jump out of it at once, held forward
    InputFrame roll = run;
    roll.dodgePressed = true;
    a.world.step(roll);
    a.run(2, run);
    InputFrame jump = run;
    jump.jumpPressed = true;
    jump.jumpHeld = true;
    a.world.step(jump);
    EXPECT_GT(a.player().body.velocity.z, a.tuning.player.sprintSpeed);   // the speed tech: faster than a sprint

    int steps = 0;
    while (not a.player().body.onGround and steps++ < 120)
        a.world.step(run);
    ASSERT_TRUE(a.player().body.onGround);

    // The grace (for chaining another jump), then back to the run speed within a few tenths of a second
    a.run(static_cast<int>((a.tuning.player.landingGrace + 0.3f) * 60.0f), run);
    EXPECT_NEAR(a.player().body.velocity.z, a.tuning.player.walkSpeed, 0.1f);
}

// ---- Weapon skills -----------------------------------------------------------------------------

namespace
{
    /** Presses the ability in `slot` and keeps it held `hold` steps, then lets the move play out. */
    void useSkill(Arena& a, int slot, int hold = 0, int after = 60)
    {
        InputFrame in;
        in.abilityPressed[slot] = true;
        in.abilityHeld[slot] = true;
        a.world.step(in);

        InputFrame held;
        held.abilityHeld[slot] = true;
        a.run(hold, held);
        a.run(after);
    }
}

TEST(arena_skills, the_weapon_in_hand_decides_the_skills)
{
    Arena a;
    const int r = a.slot("base:shadow_dash"), four = a.slot("base:backflip");
    ASSERT_GE(r, 0);
    ASSERT_EQ(r, a.slot("base:whirlwind"));   // the same key, one per weapon
    ASSERT_EQ(four, a.slot("base:leap_slam"));

    a.player().weapon = "base:dagger";
    EXPECT_EQ(a.world.ability(r, &a.player())->id, "base:shadow_dash");
    EXPECT_EQ(a.world.ability(four, &a.player())->id, "base:backflip");

    a.player().weapon = "base:sword";
    EXPECT_EQ(a.world.ability(r, &a.player())->id, "base:whirlwind");
    EXPECT_EQ(a.world.ability(four, &a.player())->id, "base:leap_slam");

    a.player().weapon = "testmod:spear";   // no skill for a spear in those slots
    EXPECT_EQ(a.world.ability(r, &a.player()), nullptr);
    EXPECT_NE(a.world.ability(a.slot("base:fireball"), &a.player()), nullptr);   // spells go with any weapon
}

TEST(arena_skills, a_charged_dash_goes_further_and_hits_harder)
{
    auto dash = [](int hold, float* damage) {
        Arena a;
        a.player().weapon = "base:dagger";
        const int dummy = a.world.spawnEnemy("base:dummy", a.player().body.position + glm::vec3(0, 0, 3.0f), 180.0f);
        a.settle(3);
        const float start = a.player().body.position.z;
        useSkill(a, a.slot("base:shadow_dash"), hold, 40);
        *damage = a.world.find(dummy)->maxHealth - a.world.find(dummy)->health;
        return a.player().body.position.z - start;
    };

    float tapDamage = 0.0f, fullDamage = 0.0f;
    const float tap = dash(0, &tapDamage);
    const float full = dash(60, &fullDamage);   // held past the full charge

    EXPECT_GT(full, tap * 1.5f);
    EXPECT_GT(tapDamage, 0.0f);
    EXPECT_NEAR(fullDamage, tapDamage * Arena().content.abilities.at("base:shadow_dash").attack.charge.damage, 0.5f);
}

TEST(arena_skills, skills_cost_stamina_and_a_cooldown_not_mana)
{
    Arena a;
    a.player().weapon = "base:dagger";
    const int four = a.slot("base:backflip");
    const AbilityDef& backflip = a.content.abilities.at("base:backflip");

    InputFrame in;
    in.abilityPressed[four] = true;
    a.world.step(in);
    EXPECT_EQ(a.player().attack, "@base:backflip");
    EXPECT_NEAR(a.player().stamina, a.tuning.stamina.max - backflip.staminaCost, 0.01f);
    EXPECT_FLOAT_EQ(a.player().mana, a.tuning.mana.max);
    EXPECT_GT(a.player().abilityCooldown[four], 0.0f);

    // It leaps back and up
    a.run(8);
    EXPECT_GT(a.player().body.velocity.y, 3.0f);
    EXPECT_LT(a.player().body.velocity.z, -5.0f);

    // Down and free again, but the cooldown is not over: nothing
    a.run(50);
    ASSERT_EQ(a.player().action, Action::Free);
    ASSERT_GT(a.player().abilityCooldown[four], 0.0f);
    a.world.step(in);
    EXPECT_EQ(a.player().action, Action::Free);   // nothing started
}

TEST(arena_skills, the_whirlwind_hits_all_around)
{
    Arena a;
    a.player().weapon = "base:sword";
    const glm::vec3 p = a.player().body.position;
    const int front = a.world.spawnEnemy("base:dummy", p + glm::vec3(0, 0, 1.4f), 180.0f);
    const int back = a.world.spawnEnemy("base:dummy", p + glm::vec3(0, 0, -1.4f), 0.0f);
    a.settle(3);

    useSkill(a, a.slot("base:whirlwind"), 0, 50);
    EXPECT_LT(a.world.find(front)->health, a.world.find(front)->maxHealth);
    EXPECT_LT(a.world.find(back)->health, a.world.find(back)->maxHealth);
}

TEST(arena_skills, the_leap_slam_flies_forward_and_strikes_where_it_lands)
{
    Arena a;
    a.player().weapon = "base:sword";
    const float start = a.player().body.position.z;
    const int target = a.world.spawnEnemy("base:dummy", a.player().body.position + glm::vec3(0, 0, 6.5f), 180.0f);
    a.settle(3);

    InputFrame in;
    in.abilityPressed[a.slot("base:leap_slam")] = true;
    a.world.step(in);
    a.run(8);
    EXPECT_FALSE(a.player().body.onGround);   // in the air

    a.run(70);
    EXPECT_TRUE(a.player().body.onGround);
    EXPECT_GT(a.player().body.position.z, start + 4.5f);
    EXPECT_LT(a.world.find(target)->health, a.world.find(target)->maxHealth);
}

TEST(arena_skills, a_skill_cancels_the_recovery_of_a_basic_attack)
{
    auto skillAt = [](int frame) {
        Arena a;
        a.player().weapon = "base:sword";
        InputFrame light;
        light.lightPressed = true;
        a.world.step(light);
        a.run(frame - 1);

        InputFrame skill;
        skill.abilityPressed[a.slot("base:whirlwind")] = true;
        a.world.step(skill);
        return a.player().attack;
    };

    const Arena ref;   // kept: the attack is read from its content
    const AttackDef& light1 = *ref.content.weapons.at("base:sword").moveset.find("light1");
    EXPECT_EQ(skillAt(light1.startup), "light1");                                   // still swinging: no
    EXPECT_EQ(skillAt(light1.startup + light1.active + 2), "@base:whirlwind");      // in its recovery: yes
}
