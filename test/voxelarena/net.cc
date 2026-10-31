#include <filesystem>
#include <fstream>
#include "stdafx.h"

#include <gtest/gtest.h>

#include <memory>
#include <set>

#include "Core/content.h"
#include "Core/net/modfiles.h"
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
            content.run.chooseWeapon = false;   // straight to the first wave

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
    snap.events.back().source = 7;
    snap.events.back().tick = 1233;

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
    EXPECT_LT(glm::length(back.fighters[0].body.position - f.body.position), 1e-6f);   // positions go across exactly
    EXPECT_FLOAT_EQ(back.fighters[1].stamina, 100.0f);   // the default: not in a view
    ASSERT_EQ(back.events.size(), 1u);
    EXPECT_EQ(back.events[0].kind, "hit");
    EXPECT_EQ(back.events[0].source, 7);
    EXPECT_EQ(back.events[0].tick, 1233);

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

TEST(arena_net, a_client_with_different_content_gets_the_servers_mods_and_joins)
{
    Session s(20.0);
    s.server->offerMods(VOXELARENA_MODS_DIR);

    Content other = s.content;
    other.fingerprint ^= 0xdeadbeef;

    auto run = [&](NetClient& c, int steps) {
        for (int i = 0; i < steps; ++i)
        {
            c.poll(s.hub.time());
            s.server->poll();
            s.server->step(nullptr);
            s.hub.advance(STEP_MS);
        }
    };

    ArenaWorld w;
    Transport& t = s.hub.addClient();
    NetClient c(w, other, s.tuning, t, "bob");

    run(c, 20);
    ASSERT_EQ(c.state, NetClient::State::ModsArrived);
    EXPECT_EQ(c.mods.fingerprint, s.content.fingerprint);
    EXPECT_EQ(c.mods.files.size(), readModFiles(VOXELARENA_MODS_DIR).size());
    EXPECT_EQ(s.server->clientCount(), 0u);

    // Saved and loaded, they give the content the server runs
    const std::string dir = (std::filesystem::temp_directory_path() / "voxelarena-test-mods").string();
    std::string error;
    ASSERT_TRUE(writeModFiles(c.mods.files, dir, error)) << error;
    EXPECT_EQ(loadContent(dir).fingerprint, loadContent(VOXELARENA_MODS_DIR).fingerprint);
    std::filesystem::remove_all(dir);

    // A new client on the same connection, now with the same content
    NetClient joined(w, s.content, s.tuning, t, "bob");
    joined.rejoin();
    run(joined, 20);
    EXPECT_EQ(joined.state, NetClient::State::Playing);
    EXPECT_EQ(s.server->clientCount(), 1u);
}

TEST(arena_net, a_client_still_different_after_the_mods_is_refused)
{
    Session s(20.0);
    s.server->offerMods(VOXELARENA_MODS_DIR);

    Content other = s.content;
    other.fingerprint ^= 0xdeadbeef;

    auto run = [&](NetClient& c) {
        for (int i = 0; i < 20; ++i)
        {
            c.poll(s.hub.time());
            s.server->poll();
            s.server->step(nullptr);
            s.hub.advance(STEP_MS);
        }
    };

    ArenaWorld w;
    Transport& t = s.hub.addClient();
    NetClient c(w, other, s.tuning, t, "bob");

    run(c);
    ASSERT_EQ(c.state, NetClient::State::ModsArrived);

    c.rejoin();
    run(c);
    EXPECT_EQ(c.state, NetClient::State::Refused);
    EXPECT_EQ(s.server->clientCount(), 0u);
}

TEST(arena_net, mods_read_back_and_a_cut_bundle_does_not)
{
    ModBundle m;
    m.fingerprint = 1234;
    m.files = {{"base/mod.json", "{}"}, {"base/big.json", std::string(100000, 'x')}};

    ModBundle back;
    auto bytes = encode(m);
    ASSERT_TRUE(decode(bytes, back));
    EXPECT_EQ(back.fingerprint, 1234u);
    ASSERT_EQ(back.files.size(), 2u);
    EXPECT_EQ(back.files[1].data, m.files[1].data);

    bytes.resize(bytes.size() / 2);
    ModBundle cut;
    EXPECT_FALSE(decode(bytes, cut));
}

TEST(arena_net, a_server_cannot_write_outside_the_mods_folder)
{
    EXPECT_TRUE(safeModPath("base/weapons/sword.json"));
    EXPECT_TRUE(safeModPath("base/scripts/slime.pg"));

    for (const char* bad : {"", "../evil.json", "base/../../evil.json", "/etc/evil.json", "C:/evil.json", "base\\evil.json",
                            "base/evil.exe", "base/evil.dll", "base/evil.json.exe", "base/.hidden.json", "base//evil.json",
                            "base/con.json", "base/COM1.pg", "base/evil .json", "base/evil."})
        EXPECT_FALSE(safeModPath(bad)) << bad;

    // One bad path and nothing is touched
    const auto dir = std::filesystem::temp_directory_path() / "voxelarena-test-refused";
    std::filesystem::create_directories(dir);
    std::ofstream(dir / "kept.json") << "{}";

    std::string error;
    EXPECT_FALSE(writeModFiles({{"base/mod.json", "{}"}, {"../evil.json", "{}"}}, dir.string(), error));
    EXPECT_TRUE(std::filesystem::exists(dir / "kept.json"));
    EXPECT_FALSE(std::filesystem::exists(dir / "base"));
    std::filesystem::remove_all(dir);

    error.clear();
    EXPECT_FALSE(writeModFiles({{"base/big.json", std::string(MAX_MOD_BYTES + 1, 'x')}}, dir.string(), error));
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

TEST(arena_net, the_others_actions_play_smoothly_between_snapshots)
{
    Session s(40.0);
    auto& a = s.join("a");
    auto& b = s.join("b");
    s.run(40);

    // b rolls; a sees the roll's frame go up a frame a step, and in between when drawn between steps
    // (snapshots come every other step: their whole frames alone would go up two at a time)
    b.input.move = glm::vec2(0, 1);
    b.input.dodgePressed = true;
    const int bId = b.client->playerId;

    std::vector<float> perStep;

    for (int i = 0; i < 40; ++i)
    {
        s.tick();

        const Fighter* seen = a.world.find(bId);
        ASSERT_TRUE(seen);

        if (seen->action != Action::Dodge or not a.client->displayFrame.count(bId))
            continue;

        perStep.push_back(a.client->displayFrame.at(bId));

        // A quarter step later, a quarter frame further
        const float now = a.client->displayFrame.at(bId);
        a.client->updateDisplay(s.hub.time() + STEP_MS * 0.25);
        EXPECT_NEAR(a.client->displayFrame.at(bId) - now, 0.25f, 0.05f);
        a.client->updateDisplay(s.hub.time());
    }

    ASSERT_GT(perStep.size(), 15u);

    for (size_t i = 1; i < perStep.size(); ++i)
        EXPECT_NEAR(perStep[i] - perStep[i - 1], 1.0f, 0.05f) << "step " << i;
}

TEST(arena_net, each_client_draws_with_its_own_looks)
{
    std::unique_ptr<Content> looks;   // outlives the session, which holds the client using it
    Session s(30.0);
    auto& b = s.join("b");

    // A client with its own looks: another stone colour, another arrow model. Same gameplay, so it joins
    looks = std::make_unique<Content>(s.content);
    const BlockId stone = looks->block("base:stone");
    looks->blockTable[stone].color = glm::vec3(1.0f, 0.0f, 0.0f);
    looks->enemies.at("base:archer").moveset.attacks.at("shot").projectile.model = "my_arrow";

    auto peer = std::make_unique<Session::Peer>();
    peer->transport = &s.hub.addClient();
    peer->client = std::make_unique<NetClient>(peer->world, *looks, s.tuning, *peer->transport, "a");
    s.peers.push_back(std::move(peer));
    auto& a = *s.peers.back();
    s.run(40);
    ASSERT_EQ(a.client->state, NetClient::State::Playing);

    // An arrow from an archer, flying at b
    const Fighter* bOnServer = s.serverPlayer(b);
    ASSERT_TRUE(bOnServer);
    Projectile arrow;
    arrow.id = 1;
    arrow.team = Team::Enemy;
    arrow.source = "base:archer";
    arrow.attack = "shot";
    arrow.def = s.content.enemies.at("base:archer").moveset.attacks.at("shot").projectile;
    arrow.def.gravity = 0.0f;
    arrow.position = bOnServer->body.position + glm::vec3(0, 1.2f, -6.0f);
    arrow.velocity = glm::vec3(0, 0, arrow.def.speed);
    s.serverWorld.projectiles.push_back(arrow);

    // The block it would chip, and the arrow, in this client's looks
    s.serverWorld.terrain.damage({3, 0, 3}, 10000.0f);

    bool sawArrow = false, sawFlash = false, sawStone = false;

    for (int i = 0; i < 40; ++i)
    {
        s.tick();

        for (const auto& p : a.world.projectiles)
            sawArrow = sawArrow or p.def.model == "my_arrow";

        if (const Fighter* bForA = a.world.find(b.client->playerId))
            sawFlash = sawFlash or bForA->flash > 0;

        for (const auto& e : a.client->takeEvents())
            if (e.kind == "break" and e.block == stone)
                sawStone = sawStone or e.color == glm::vec3(1.0f, 0.0f, 0.0f);
    }

    EXPECT_TRUE(sawArrow);   // its model from this client's content
    EXPECT_TRUE(sawStone);   // the broken block coloured by this client's blocks
    EXPECT_TRUE(sawFlash);   // b flashes when hit, from the hit event (the flash is not sent)
    EXPECT_LT(s.serverPlayer(b)->health, s.serverPlayer(b)->maxHealth);

    // And the flash fades
    s.run(30);
    EXPECT_EQ(a.world.find(b.client->playerId)->flash, 0);
}

TEST(arena_net, the_others_events_play_when_they_are_seen_your_own_at_once)
{
    // A short latency, so arriving and waiting for the drawn moment are far apart; no dodge grace,
    // which holds a hit on a remote player on purpose
    Session s(10.0);
    s.server->dodgeGrace = false;
    auto& a = s.join("a");
    s.run(40);

    // Ticks from when something happens on the server to when this client plays it
    auto ticksUntil = [&](const std::set<std::string>& kinds, int fighter) {
        for (int i = 1; i <= 60; ++i)
        {
            s.tick();
            for (const auto& e : a.client->takeEvents())
                if (kinds.count(e.kind) and e.fighter == fighter)
                    return i;
        }
        return 999;
    };
    const int latency = static_cast<int>(std::ceil(10.0 / STEP_MS));

    // A block breaking (nobody's): when the others, drawn INTERP_TICKS behind the server's present,
    // reach it. It arrived well before (after the latency), and waited
    s.serverWorld.terrain.damage({3, 0, 3}, 10000.0f);
    const int breakTicks = ticksUntil({"break"}, 0);
    EXPECT_GT(breakTicks, latency + 1);
    EXPECT_GE(breakTicks, NetClient::INTERP_TICKS - 1);
    EXPECT_LE(breakTicks, NetClient::INTERP_TICKS + 2);

    // An arrow hitting this client's own player: at once, as it arrives
    const Fighter* me = s.serverPlayer(a);
    ASSERT_TRUE(me);
    Projectile arrow;
    arrow.id = 1;
    arrow.team = Team::Enemy;
    arrow.source = "base:archer";
    arrow.attack = "shot";
    arrow.def = s.content.enemies.at("base:archer").moveset.attacks.at("shot").projectile;
    arrow.def.gravity = 0.0f;
    arrow.position = me->body.position + glm::vec3(0, 1.2f, -1.0f);
    arrow.velocity = glm::vec3(0, 0, arrow.def.speed);
    s.serverWorld.projectiles.push_back(arrow);

    // (a hit: an arrow does no poise damage)
    const int hitTicks = ticksUntil({"hit", "stagger", "kill"}, a.client->playerId);
    EXPECT_LE(hitTicks, latency + 3);   // the arrow's flight, the latency, the next snapshot
    EXPECT_LT(hitTicks, breakTicks);
}

TEST(arena_net, statuses_and_mana_reach_the_clients)
{
    Session s(30.0);
    auto& a = s.join("a");
    auto& b = s.join("b");
    s.run(40);

    // b chilled on the server: a sees it on b (it slows and shows), b has it on itself too
    s.serverWorld.applyStatus(*s.serverPlayer(b), "base:chill", 2);
    s.serverPlayer(a)->mana = 40.0f;
    s.run(15);

    auto stacks = [](const Fighter* f) {
        for (const auto& st : f->statuses)
            if (st.id == "base:chill")
                return st.stacks;
        return 0;
    };

    EXPECT_EQ(stacks(a.world.find(b.client->playerId)), 2);
    EXPECT_EQ(stacks(s.clientPlayer(b)), 2);
    EXPECT_NEAR(s.clientPlayer(a)->mana, s.serverPlayer(a)->mana, 0.5f);   // the player's own mana, for its HUD
}

TEST(arena_net, a_clients_own_spells_reach_the_server_and_its_zones_come_back)
{
    Session s(30.0);
    auto& a = s.join("a");
    s.run(40);

    SpellRecipe firebolt;
    firebolt.element = "fire";
    firebolt.keywords = {"base:bolt", "base:zone"};
    a.client->setSpells({firebolt, {}, {}});
    s.run(10);

    ASSERT_TRUE(s.serverPlayer(a));
    ASSERT_FALSE(s.serverPlayer(a)->spells.empty());
    EXPECT_EQ(s.serverPlayer(a)->spells[0], firebolt);

    // Cast at the ground ahead: the server makes the zone, the client draws it
    int key = -1;
    for (int i = 0; i < InputFrame::ABILITIES; ++i)
        if (s.serverWorld.spellSlotAt(i) == 0)
            key = i;
    ASSERT_GE(key, 0);

    const glm::vec3 me = s.serverPlayer(a)->body.position;
    a.input.aimOrigin = me + glm::vec3(0, 1.6f, -2.0f);
    a.input.aimDir = glm::normalize(me + glm::vec3(0, 0.05f, 7.0f) - a.input.aimOrigin);
    a.input.abilityPressed[key] = true;
    s.tick();
    a.input.abilityPressed[key] = false;
    s.run(60);

    ASSERT_EQ(s.serverWorld.zones.size(), 1u);
    ASSERT_EQ(a.world.zones.size(), 1u);
    EXPECT_EQ(a.world.zones[0].element, Element::Fire);
    EXPECT_LT(glm::length(a.world.zones[0].position - s.serverWorld.zones[0].position), 0.01f);
}

TEST(arena_net, the_run_plays_for_every_client_and_each_has_their_own_shop)
{
    Session s(30.0);
    auto& a = s.join("a");
    auto& b = s.join("b");
    s.run(40);

    s.content.run.pools = {{"base:skulker", 1, 1000, 1.0f}};
    s.serverWorld.startRun(5);
    s.run(10);

    // Both see the run
    EXPECT_EQ(a.world.run.phase, RunPhase::Fighting);
    EXPECT_EQ(b.world.run.wave, 1);
    EXPECT_NEAR(a.world.run.timer, s.serverWorld.run.timer, 0.5f);

    // A kill: its shards, where the server has them
    const int id = s.serverWorld.spawnEnemy("base:brute", s.serverPlayer(a)->body.position + glm::vec3(0, 0, 12.0f), 180.0f);
    s.run(5);
    Fighter* brute = s.serverWorld.find(id);
    brute->health = 0.01f;
    s.serverWorld.applyStatus(*brute, "base:burn");
    s.run(10);
    ASSERT_FALSE(s.serverWorld.pickups.empty());
    ASSERT_EQ(a.world.pickups.size(), s.serverWorld.pickups.size());
    EXPECT_LT(glm::length(a.world.pickups[0].position - s.serverWorld.pickups[0].position), 0.5f);

    // The time is up: each their own shop
    s.serverPlayer(a)->shards = 100;
    s.serverWorld.run.timer = 0.0f;
    s.run(10);
    ASSERT_EQ(a.world.run.phase, RunPhase::Shop);
    ASSERT_EQ(s.clientPlayer(a)->shop.size(), static_cast<size_t>(s.content.run.shopSlots));
    EXPECT_GE(s.clientPlayer(a)->shards, 100);
    EXPECT_EQ(s.clientPlayer(b)->shop.size(), static_cast<size_t>(s.content.run.shopSlots));

    // a buys, and is ready; the wave waits for b
    const auto offer = s.clientPlayer(a)->shop[0];
    const int shards = s.clientPlayer(a)->shards;
    a.input.pick = InputFrame::BUY;
    s.tick();
    a.input.pick = 0;
    s.run(10);
    EXPECT_TRUE(s.serverPlayer(a)->shop[0].bought);
    EXPECT_TRUE(s.clientPlayer(a)->shop[0].bought);
    EXPECT_EQ(s.clientPlayer(a)->shards, shards - offer.price);
    EXPECT_FALSE(s.clientPlayer(b)->shop[0].bought);

    a.input.pick = InputFrame::START;
    s.tick();
    a.input.pick = 0;
    s.run(10);
    EXPECT_TRUE(s.clientPlayer(a)->ready);
    EXPECT_EQ(s.serverWorld.run.phase, RunPhase::Shop);

    b.input.pick = InputFrame::START;
    s.tick();
    b.input.pick = 0;
    s.run(10);
    EXPECT_EQ(s.serverWorld.run.phase, RunPhase::Fighting);
    EXPECT_EQ(b.world.run.wave, 2);
}

TEST(arena_net, a_client_gets_the_runs_arena_and_each_reset)
{
    Session s(30.0);
    auto& a = s.join("a");
    s.run(40);

    s.serverWorld.startRun(5);
    s.run(15);

    // The run's own arena, its size and all
    const glm::ivec3 size = s.content.run.arena.size;
    EXPECT_EQ(glm::ivec3(a.world.grid.width(), a.world.grid.height(), a.world.grid.depth()), size);
    EXPECT_EQ(a.world.grid.raw(), s.serverWorld.grid.raw());

    // A pillar broken, then the next wave: whole again for the client too
    const glm::ivec3 pillar(9, 5, 13);
    const BlockId stone = s.serverWorld.grid.get(pillar.x, pillar.y, pillar.z);
    s.serverWorld.terrain.damage(pillar, 1e6f);
    s.run(10);
    EXPECT_EQ(a.world.grid.get(pillar.x, pillar.y, pillar.z), AIR);

    s.serverWorld.skipToWave(2);
    s.run(10);
    EXPECT_EQ(a.world.grid.get(pillar.x, pillar.y, pillar.z), stone);
    EXPECT_EQ(a.world.grid.raw(), s.serverWorld.grid.raw());
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

// ---- The lobby --------------------------------------------------------------------------------

TEST(arena_net, lobby_messages_read_back)
{
    LobbyMsg m;
    m.dedicated = true;
    m.players = {{3, "ann", true, true, 0}, {7, "bob", false, false, 85}};

    LobbyMsg back;
    ASSERT_TRUE(decode(encode(m), back));
    EXPECT_TRUE(back.dedicated);
    ASSERT_EQ(back.players.size(), 2u);
    EXPECT_EQ(back.players[1].name, "bob");
    EXPECT_EQ(back.players[1].pingMs, 85);
    EXPECT_TRUE(back.players[0].host);

    ReadyMsg r{true}, rb;
    ASSERT_TRUE(decode(encode(r), rb));
    EXPECT_TRUE(rb.ready);
}

TEST(arena_net, every_client_sees_who_is_in_by_name_and_who_is_ready)
{
    Session s(20.0);
    auto& a = s.join("ann");
    auto& b = s.join("bob");
    s.run(30);

    // In the order they came, by name, nobody ready yet
    ASSERT_EQ(a.client->lobby.players.size(), 2u);
    EXPECT_EQ(a.client->lobby.players[0].name, "ann");
    EXPECT_EQ(a.client->lobby.players[1].name, "bob");
    EXPECT_TRUE(a.client->lobby.dedicated);
    EXPECT_FALSE(a.client->lobby.players[1].ready);
    EXPECT_EQ(a.client->nameOf(b.client->playerId), "bob");

    b.client->setReady(true);
    s.run(10);
    EXPECT_TRUE(a.client->lobby.players[1].ready);

    // Someone leaves: gone from everyone's list
    s.hub.disconnect(*b.transport);
    s.run(20);
    ASSERT_EQ(a.client->lobby.players.size(), 1u);
    EXPECT_EQ(a.client->lobby.players[0].name, "ann");
}

TEST(arena_net, a_dedicated_server_starts_the_run_when_everyone_is_ready)
{
    Session s(20.0);
    auto& a = s.join("ann");
    auto& b = s.join("bob");
    s.run(30);

    a.client->setReady(true);
    s.run(20);
    EXPECT_EQ(s.serverWorld.run.phase, RunPhase::Off);   // bob is not

    b.client->setReady(true);
    s.run(20);
    EXPECT_EQ(s.serverWorld.run.phase, RunPhase::Fighting);

    // Not ready any more, for the next time in the lobby
    s.run(10);
    for (const auto& p : s.server->lobby().players)
        EXPECT_FALSE(p.ready);
}

TEST(arena_net, a_ready_set_while_joining_is_sent_once_in)
{
    Session s(40.0);
    auto& a = s.join("ann");
    a.client->setReady(true);   // before the welcome
    s.run(30);

    ASSERT_EQ(s.server->lobby().players.size(), 1u);
    EXPECT_EQ(s.serverWorld.run.phase, RunPhase::Fighting);   // alone and ready: it began
}

TEST(arena_net, a_listen_servers_lobby_has_its_host_first)
{
    Content content = loadContent(VOXELARENA_MODS_DIR);
    ContentReport r;
    Tuning tuning = readTuning(content.tuning, content.tuningFile, r);

    LoopbackHub hub;
    ArenaWorld host;
    host.start(content, tuning, true);
    NetServer server(host, content, hub.server(), false);
    server.hostName = "hana";

    ArenaWorld world;
    NetClient client(world, content, tuning, hub.addClient(), "ann");

    for (int i = 0; i < 30; ++i)
    {
        client.poll(hub.time());
        client.step(InputFrame{}, hub.time());
        server.poll();
        server.step(nullptr);
        hub.advance(STEP_MS);
    }

    ASSERT_EQ(client.lobby.players.size(), 2u);
    EXPECT_EQ(client.lobby.players[0].name, "hana");
    EXPECT_TRUE(client.lobby.players[0].host);
    EXPECT_EQ(client.lobby.players[1].name, "ann");
    EXPECT_FALSE(client.lobby.dedicated);

    // A host has to start it: everyone ready is not enough
    client.setReady(true);
    for (int i = 0; i < 20; ++i)
    {
        client.poll(hub.time());
        server.poll();
        server.step(nullptr);
        hub.advance(STEP_MS);
    }
    EXPECT_EQ(host.run.phase, RunPhase::Off);
}
