#include "stdafx.h"

#include <gtest/gtest.h>

#include <memory>

#include "Core/content.h"
#include "Core/net/client.h"
#include "Core/net/loopback.h"
#include "Core/net/protocol.h"
#include "Core/net/server.h"
#include "Core/tuning.h"
#include "Core/world.h"

using namespace arena;
using namespace arena::net;

namespace
{
    constexpr double STEP_MS = 1000.0 / 60.0;

    /** A server and its clients on a simulated link, stepped together at 60 per second. */
    struct Session
    {
        Content content;
        Tuning tuning;
        LoopbackHub hub;
        ArenaWorld serverWorld;
        std::unique_ptr<NetServer> server;

        struct Peer
        {
            ArenaWorld world;
            std::unique_ptr<NetClient> client;
            Transport* transport = nullptr;
            InputFrame input;
        };
        std::vector<std::unique_ptr<Peer>> peers;

        explicit Session(double latencyMs = 0.0, double jitterMs = 0.0, double loss = 0.0)
        {
            content = loadContent(VOXELARENA_MODS_DIR);
            ContentReport r;
            tuning = readTuning(content.tuning, content.tuningFile, r);

            // A bare floor, no enemies: tests add what they need
            content.arena.size = glm::ivec3(32, 16, 32);
            content.arena.shapes = parseJson(R"([ { "block": "stone", "from": [0, 0, 0], "to": [31, 0, 31] } ])").value;
            content.arena.spawns.clear();
            content.arena.playerSpawn = glm::vec3(16, 1, 8);

            hub.link.latencyMs = latencyMs;
            hub.link.jitterMs = jitterMs;
            hub.link.loss = loss;

            serverWorld.start(content, tuning, false);
            server = std::make_unique<NetServer>(serverWorld, content, hub.server(), true);
        }

        Peer& join(const std::string& name = "p")
        {
            auto peer = std::make_unique<Peer>();
            peer->transport = &hub.addClient();
            peer->client = std::make_unique<NetClient>(peer->world, content, tuning, *peer->transport, name);
            peers.push_back(std::move(peer));
            return *peers.back();
        }

        void tick()
        {
            const double now = hub.time();

            for (auto& p : peers)
            {
                p->client->poll(now);
                p->client->step(p->input, now);
                // Presses last one step
                p->input.jumpPressed = p->input.dodgePressed = p->input.lightPressed = p->input.heavyPressed = false;
            }

            server->poll();
            server->step(nullptr);
            hub.advance(STEP_MS);

            for (auto& p : peers)
                p->client->updateDisplay(hub.time());
        }

        void run(int ticks)
        {
            for (int i = 0; i < ticks; ++i)
                tick();
        }

        Fighter* serverPlayer(Peer& p) { return serverWorld.find(p.client->playerId); }
        Fighter* clientPlayer(Peer& p) { return p.world.find(p.client->playerId); }
    };
}

// ---- Messages ---------------------------------------------------------------------------------

TEST(arena_net, messages_read_back_what_was_written)
{
    Session s;
    Names names;
    names.build(s.content);

    Fighter f;
    f.id = 7;
    f.team = Team::Player;
    f.type = "player";
    f.weapon = "base:sword";
    f.attack = "light2";
    f.action = Action::Attack;
    f.frame = 12;
    f.body.position = glm::vec3(1.5f, 2.25f, -3.0f);
    f.stamina = 42.0f;
    f.queuedAge = 0.05f;
    f.abilityCooldown[1] = 0.3f;

    SnapshotMsg snap;
    snap.tick = 1234;
    snap.ackSeq = 99;
    snap.playerId = 7;
    snap.fighters.push_back(f);
    Fighter other = f;
    other.id = 8;
    other.stamina = 1.0f;   // a view of someone else: stamina is not sent
    snap.fighters.push_back(other);
    snap.events.push_back({"hit", glm::vec3(1, 2, 3), glm::vec3(0.5f), 10.0f, 8});

    SnapshotMsg back;
    ASSERT_TRUE(decode(encode(snap, names), back, names));
    EXPECT_EQ(back.tick, 1234u);
    EXPECT_EQ(back.ackSeq, 99u);
    ASSERT_EQ(back.fighters.size(), 2u);
    EXPECT_EQ(back.fighters[0].attack, "light2");
    EXPECT_EQ(back.fighters[0].weapon, "base:sword");
    EXPECT_EQ(back.fighters[0].action, Action::Attack);
    EXPECT_EQ(back.fighters[0].frame, 12);
    EXPECT_FLOAT_EQ(back.fighters[0].stamina, 42.0f);
    EXPECT_FLOAT_EQ(back.fighters[0].abilityCooldown[1], 0.3f);
    EXPECT_EQ(back.fighters[0].body.position, f.body.position);
    EXPECT_FLOAT_EQ(back.fighters[1].stamina, 100.0f);   // the default: not in a view
    ASSERT_EQ(back.events.size(), 1u);
    EXPECT_EQ(back.events[0].kind, "hit");

    InputMsg in;
    StampedInput si;
    si.seq = 5;
    si.input.move = glm::vec2(0.5f, -0.8f);
    si.input.lightPressed = true;
    si.input.abilityHeld[2] = true;
    si.input.cameraYaw = 33.0f;
    si.input.aimDir = glm::normalize(glm::vec3(0.2f, -0.5f, 1.0f));
    si.input.stamp = 1000;
    in.inputs.push_back(si);
    InputMsg inBack;
    ASSERT_TRUE(decode(encode(in), inBack));
    ASSERT_EQ(inBack.inputs.size(), 1u);
    EXPECT_EQ(inBack.inputs[0].seq, 5u);
    EXPECT_TRUE(inBack.inputs[0].input.lightPressed);
    EXPECT_FALSE(inBack.inputs[0].input.heavyPressed);
    EXPECT_TRUE(inBack.inputs[0].input.abilityHeld[2]);
    EXPECT_NEAR(inBack.inputs[0].input.move.x, 0.5f, 0.01f);
    EXPECT_NEAR(inBack.inputs[0].input.aimDir.y, si.input.aimDir.y, 1e-3f);
    EXPECT_EQ(inBack.inputs[0].input.stamp, 1000);

    // A truncated message is refused, not half read
    auto bytes = encode(snap, names);
    bytes.resize(bytes.size() / 2);
    SnapshotMsg broken;
    EXPECT_FALSE(decode(bytes, broken, names));

    WorldInitMsg init;
    init.size = glm::ivec3(4, 2, 3);
    init.cells.assign(24, 1);
    init.cells[5] = 3;
    init.damage = {{5, 0.5f}};
    WorldInitMsg initBack;
    ASSERT_TRUE(decode(encode(init), initBack));
    EXPECT_EQ(initBack.cells, init.cells);
    EXPECT_NEAR(initBack.damage[0].second, 0.5f, 0.01f);

    // A snapshot of a busy arena still fits a datagram (MTU about 1200 bytes)
    SnapshotMsg busy;
    for (int i = 0; i < 10; ++i)
    {
        Fighter g = other;
        g.id = 100 + i;
        busy.fighters.push_back(g);
    }
    busy.fighters[0].id = busy.playerId = 100;
    EXPECT_LT(encode(busy, names).size(), 1200u);
}

// ---- Sessions ---------------------------------------------------------------------------------

TEST(arena_net, a_client_joins_and_gets_its_player)
{
    Session s(30.0);
    auto& a = s.join("alice");
    s.run(30);

    EXPECT_EQ(a.client->state, NetClient::State::Playing);
    EXPECT_EQ(s.server->clientCount(), 1u);
    ASSERT_TRUE(s.serverPlayer(a));
    ASSERT_TRUE(s.clientPlayer(a));
    EXPECT_NEAR(glm::length(s.clientPlayer(a)->body.position - s.serverPlayer(a)->body.position), 0.0f, 0.05f);
}

TEST(arena_net, a_client_with_different_content_is_refused)
{
    Session s(20.0);

    Content other = s.content;
    other.fingerprint ^= 0xdeadbeef;

    ArenaWorld w;
    Transport& t = s.hub.addClient();
    NetClient c(w, other, s.tuning, t, "mallory");

    for (int i = 0; i < 20; ++i)
    {
        c.poll(s.hub.time());
        s.server->poll();
        s.server->step(nullptr);
        s.hub.advance(STEP_MS);
    }

    EXPECT_EQ(c.state, NetClient::State::Refused);
    EXPECT_NE(c.refusal.find("mods"), std::string::npos);
    EXPECT_EQ(s.server->clientCount(), 0u);
}

TEST(arena_net, your_own_player_moves_at_once_and_agrees_with_the_server)
{
    // 120 ms round trip, some jitter and loss
    Session s(60.0, 10.0, 0.05);
    auto& a = s.join();
    s.run(40);
    ASSERT_TRUE(s.clientPlayer(a));

    const glm::vec3 start = s.clientPlayer(a)->body.position;

    // The first step with input already moves the predicted player: no waiting for the server
    a.input.move = glm::vec2(0, 1);
    s.tick();
    EXPECT_GT(s.clientPlayer(a)->body.position.z, start.z);

    // Walk, jump, roll, stop: once the inputs are played on both sides, they agree
    s.run(30);
    a.input.jumpPressed = true;
    a.input.jumpHeld = true;
    s.run(30);
    a.input.jumpHeld = false;
    a.input.move = glm::vec2(1, 0);
    a.input.dodgePressed = true;
    s.run(30);
    a.input.move = glm::vec2(0.0f);
    s.run(60);

    const glm::vec3 predicted = s.clientPlayer(a)->body.position;
    const glm::vec3 truth = s.serverPlayer(a)->body.position;
    EXPECT_GT(glm::length(predicted - start), 3.0f);
    EXPECT_NEAR(glm::length(predicted - truth), 0.0f, 0.05f);
    EXPECT_EQ(s.clientPlayer(a)->action, s.serverPlayer(a)->action);
}

TEST(arena_net, players_see_each_other_and_a_bit_in_the_past)
{
    Session s(40.0, 5.0);
    auto& a = s.join("a");
    auto& b = s.join("b");
    s.run(40);

    ASSERT_EQ(s.serverWorld.fighters.size(), 2u);
    ASSERT_EQ(a.world.fighters.size(), 2u);
    ASSERT_EQ(b.world.fighters.size(), 2u);

    // b walks; a sees b follow, about INTERP_TICKS plus the latency behind
    b.input.move = glm::vec2(0, 1);
    s.run(60);

    const int bId = b.client->playerId;
    const Fighter* bOnServer = s.serverWorld.find(bId);
    const Fighter* bForA = a.world.find(bId);
    ASSERT_TRUE(bOnServer and bForA);

    const float speed = s.tuning.player.walkSpeed;
    const float behind = bOnServer->body.position.z - bForA->body.position.z;
    EXPECT_GT(behind, 0.0f);
    EXPECT_LT(behind, speed * 0.35f);   // under about 350 ms of walking behind

    // Stopped: a sees b where the server has it
    b.input.move = glm::vec2(0.0f);
    s.run(60);
    EXPECT_NEAR(glm::length(a.world.find(bId)->body.position - s.serverWorld.find(bId)->body.position), 0.0f, 0.02f);
}

TEST(arena_net, terrain_changes_reach_every_client)
{
    Session s(30.0);
    auto& a = s.join();
    s.run(20);

    const BlockId stone = s.content.block("base:stone");
    s.serverWorld.terrain.damage({3, 0, 3}, 50.0f);     // a crack
    s.serverWorld.grid.fill({5, 1, 5}, {7, 3, 5}, stone); // a wall
    s.serverWorld.terrain.remove({10, 0, 10});           // a hole
    s.run(10);

    EXPECT_EQ(a.world.grid.get(6, 2, 5), stone);
    EXPECT_EQ(a.world.grid.get(10, 0, 10), AIR);
    EXPECT_NEAR(a.world.terrain.damageRatio(3, 0, 3), s.serverWorld.terrain.damageRatio(3, 0, 3), 0.01f);
    EXPECT_EQ(a.world.grid.raw(), s.serverWorld.grid.raw());

    // A late joiner gets the arena as it is now
    auto& b = s.join();
    s.run(15);
    EXPECT_EQ(b.world.grid.raw(), s.serverWorld.grid.raw());
}

TEST(arena_net, a_hit_lands_on_the_enemy_where_you_saw_it)
{
    // A skulker running across the player's front at 9 m/s; the client sees it some 230 ms late,
    // about 2 m behind where it really is: more than the sword's swing covers
    auto attempt = [](bool lagCompensation) {
        Session s(65.0);
        auto& a = s.join();
        s.run(40);

        s.serverWorld.aiEnabled = false;
        const int id = s.serverWorld.spawnEnemy("base:skulker", glm::vec3(10.0f, 1.0f, 9.6f), 0.0f);
        s.run(30);

        int hits = 0;

        for (int i = 0; i < 160 and hits == 0; ++i)
        {
            // It runs along +X at its speed, past the player at x = 16
            Fighter* e = s.serverWorld.find(id);
            e->body.position.z = 9.6f;
            e->body.velocity = glm::vec3(9.0f, 0.0f, 0.0f);
            e->push = glm::vec3(0.0f);

            // The client swings as soon as it sees it right in front (where its screen shows it)
            const Fighter* seen = a.world.find(id);
            // First person, looking straight ahead: no aim assist to turn the swing towards the real enemy
            a.input.faceCamera = true;
            a.input.cameraYaw = 0.0f;

            if (seen and std::fabs(seen->body.position.x - 16.0f) < 0.3f and s.clientPlayer(a)->action == Action::Free)
                a.input.lightPressed = true;

            s.server->poll();
            const double now = s.hub.time();
            a.client->poll(now);
            InputFrame in = a.input;
            a.client->step(in, now);
            a.input.lightPressed = false;

            // Lag compensation off: the server forgets what the client saw
            if (not lagCompensation)
                s.serverWorld.history.clear();

            s.server->step(nullptr);

            for (const auto& h : s.serverWorld.hits)
                hits += h.target == id;
            s.serverWorld.hits.clear();

            s.hub.advance(STEP_MS);
            a.client->updateDisplay(s.hub.time());
        }

        return hits;
    };

    EXPECT_EQ(attempt(true), 1);
    EXPECT_EQ(attempt(false), 0);
}

TEST(arena_net, a_roll_pressed_in_time_by_your_clock_saves_you)
{
    // The brute slams a remote player; the player rolls a little before the blow by its own clock,
    // which reaches the server after the blow (one way 80 ms = about 5 steps)
    auto attempt = [](bool grace) {
        Session s(80.0);
        s.server->dodgeGrace = grace;
        auto& a = s.join();
        s.run(60);
        s.serverWorld.aiEnabled = false;
        s.serverWorld.godMode = false;

        const Fighter* p = s.serverPlayer(a);
        const int bruteId = s.serverWorld.spawnEnemy("base:brute", p->body.position + glm::vec3(0, 0, 1.5f), 180.0f);
        s.run(20);

        const AttackDef* slam = s.content.enemies.at("base:brute").moveset.find("slam");
        Fighter* brute = s.serverWorld.find(bruteId);
        brute->action = Action::Attack;
        brute->attack = "slam";
        brute->frame = 0;
        // The first step its hitbox can land (right in front of it: the first active frame)
        const uint64_t blow = s.serverWorld.steps + static_cast<uint64_t>(slam->startup) + 1;

        const float before = s.serverPlayer(a)->health;
        bool pressed = false;

        for (int i = 0; i < slam->total() + 40; ++i)
        {
            // Roll when, by the client's estimate of the server clock, the blow is 2 steps away: the roll
            // then reaches the server about 3 steps after the blow
            if (not pressed and a.client->serverTick(s.hub.time()) >= static_cast<double>(blow) - 2.0)
            {
                a.input.dodgePressed = true;
                a.input.move = glm::vec2(1, 0);
                pressed = true;
            }

            s.tick();
        }

        return before - s.serverPlayer(a)->health;
    };

    EXPECT_FLOAT_EQ(attempt(true), 0.0f);   // the server waited for the roll it knew was coming
    EXPECT_GT(attempt(false), 0.0f);        // without the grace the roll arrives too late
}

TEST(arena_net, a_player_who_leaves_is_removed)
{
    Session s(20.0);
    auto& a = s.join();
    auto& b = s.join();
    s.run(30);
    ASSERT_EQ(s.serverWorld.fighters.size(), 2u);

    s.hub.disconnect(*b.transport);
    s.run(20);

    EXPECT_EQ(s.serverWorld.fighters.size(), 1u);
    EXPECT_EQ(a.world.fighters.size(), 1u);
    EXPECT_FALSE(s.server->log.empty());
}
