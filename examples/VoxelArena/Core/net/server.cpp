#include "server.h"

#include "client.h"
#include "modfiles.h"

#include <algorithm>
#include <cmath>

namespace arena::net
{
    NetServer::NetServer(ArenaWorld& w, const Content& c, Transport& t, bool h) : world(w), content(c), transport(t), headless(h)
    {
        names.build(content);
    }

    size_t NetServer::clientCount() const
    {
        size_t n = 0;
        for (const auto& [peer, c] : clients)
            n += c.joined;
        return n;
    }

    void NetServer::poll()
    {
        NetEvent e;

        while (transport.poll(e))
        {
            switch (e.type)
            {
                case NetEvent::Type::Connected:
                    clients[e.peer].peer = e.peer;
                    clients[e.peer].since = ticks;
                    break;

                case NetEvent::Type::Disconnected:
                {
                    auto it = clients.find(e.peer);

                    if (it != clients.end())
                    {
                        if (it->second.joined)
                        {
                            lobbyDirty = true;
                            log.push_back(it->second.name + " left");
                            world.removeFighter(it->second.playerId);
                            world.defenderGrace.erase(e.peer);
                        }

                        clients.erase(it);
                    }
                    break;
                }

                case NetEvent::Type::Message:
                {
                    auto it = clients.find(e.peer);

                    // From a connection never announced, or one already refused: nothing to say to it
                    if (it == clients.end())
                    {
                        Client& c = clients[e.peer];
                        c.peer = e.peer;
                        c.since = ticks;
                        it = clients.find(e.peer);
                    }

                    if (it->second.closeAt == 0)
                        onMessage(it->second, e.data);
                    break;
                }
            }
        }
    }

    void NetServer::offerMods()
    {
        ModBundle bundle;
        bundle.fingerprint = content.fingerprint;

        // Under its id, not its folder's name: ids are unique among the loaded mods, folders across two places are not
        for (const auto& mod : content.mods)
            for (auto& f : readModFiles(mod.dir))
                bundle.files.push_back({mod.id + "/" + f.path, std::move(f.data)});

        size_t total = 0;
        for (const auto& f : bundle.files)
            total += f.data.size();

        // More than a client takes: better a clear refusal than a long download for nothing
        if (bundle.files.empty() or bundle.files.size() > MAX_MOD_FILES or total > MAX_MOD_BYTES)
        {
            modBundle.clear();
            log.push_back("the mods are too big to send to players");
            return;
        }

        modBundle = encode(bundle);
    }

    void NetServer::onMessage(Client& c, const std::vector<uint8_t>& data)
    {
        switch (peek(data))
        {
            case Msg::Hello:
            {
                Hello hello;

                if (not decode(data, hello) or c.joined)
                    return;

                const std::string name = cleanName(hello.name);

                Welcome welcome;
                welcome.peer = c.peer;
                welcome.tick = static_cast<uint32_t>(world.steps);

                if (hello.version != PROTOCOL_VERSION)
                    welcome.reason = "protocol " + std::to_string(hello.version) + ", the server runs " + std::to_string(PROTOCOL_VERSION);
                else if (clientCount() >= static_cast<size_t>(std::max(0, maxClients)))
                    welcome.reason = "the server is full";
                else if (hello.fingerprint != content.fingerprint)
                {
                    // Same game, other content: it gets the server's mods and says hello again
                    if (not modBundle.empty() and not c.modsSent)
                    {
                        c.modsSent = true;
                        c.since = ticks;
                        transport.send(c.peer, modBundle, true);
                        log.push_back((name.empty() ? "a player" : name) + " is getting the server's mods");
                        return;
                    }

                    welcome.reason = "different mods or content than the server (load the same mods)";
                }

                if (not welcome.reason.empty())
                {
                    transport.send(c.peer, encode(welcome), true);
                    refuse(c, name, welcome.reason);
                    return;
                }

                c.joined = true;
                c.ready = false;
                c.joinOrder = ++joins;
                lobbyDirty = true;
                c.name = name.empty() ? "player " + std::to_string(c.peer) : name;
                c.playerId = world.addPlayer(c.peer);
                welcome.accepted = true;
                welcome.playerId = c.playerId;

                transport.send(c.peer, encode(welcome), true);

                // The arena as it is now, then what is falling
                WorldInitMsg init;
                init.size = glm::ivec3(world.grid.width(), world.grid.height(), world.grid.depth());
                init.cells = world.grid.raw();
                init.damage = world.terrain.damagedCells();
                transport.send(c.peer, encode(init), true);

                for (const auto& piece : world.terrain.falling)
                {
                    PieceSpawnMsg spawn;
                    spawn.id = piece.id;
                    spawn.origin = piece.origin;
                    spawn.drop = piece.drop;
                    spawn.cells = piece.cells;
                    transport.send(c.peer, encode(spawn), true);
                }

                log.push_back(c.name + " joined");
                break;
            }

            case Msg::Input:
            {
                InputMsg msg;

                if (not c.joined or not decode(data, msg))
                    return;

                c.lastClientTimeMs = std::max(c.lastClientTimeMs, msg.clientTimeMs);
                c.snapshotAck = std::max(c.snapshotAck, std::min(msg.snapshotAck, snapshotSeq));
                c.rttMs = msg.rttMs;   // only used when the transport cannot measure it (see rttOf)

                // Several inputs per message (repeated against loss): keep the ones not played yet
                for (const auto& s : msg.inputs)
                    if (s.seq > c.lastSeq)
                        c.inputs[s.seq] = s.input;
                break;
            }

            case Msg::Spells:
            {
                // The recipes on the player's spell keys: a recipe that makes no spell just does nothing
                SpellsMsg msg;

                if (not c.joined or not decode(data, msg, names))
                    return;

                if (Fighter* f = world.find(c.playerId))
                    world.setSpells(*f, msg.spells);
                break;
            }

            case Msg::Ready:
            {
                ReadyMsg msg;

                if (not c.joined or not decode(data, msg))
                    return;

                if (c.ready != msg.ready)
                {
                    c.ready = msg.ready;
                    lobbyDirty = true;
                }
                break;
            }

            case Msg::Name:
            {
                NameMsg msg;

                if (not c.joined or not decode(data, msg))
                    return;

                const std::string name = cleanName(msg.name);

                if (name.empty() or name == c.name)
                    return;

                log.push_back(c.name + " is now " + name);
                c.name = name;
                lobbyDirty = true;
                break;
            }

            case Msg::Bye:
            {
                if (c.joined)
                {
                    lobbyDirty = true;
                    log.push_back(c.name + " left");
                    world.removeFighter(c.playerId);
                }

                c.joined = false;
                c.since = ticks;   // it may say hello again, in time
                break;
            }

            default:
                break;
        }
    }

    void NetServer::refuse(Client& c, const std::string& name, const std::string& reason)
    {
        log.push_back((name.empty() ? "a player" : name) + " was refused: " + reason);
        c.closeAt = ticks + REFUSED_LINGER;
    }

    void NetServer::closeIdle()
    {
        for (auto& [peer, c] : clients)
        {
            if (c.joined)
                continue;

            // Refused, its refusal sent: closed. Connected and never joined: closed after a while
            const bool refused = c.closeAt != 0 and ticks >= c.closeAt;
            const bool idle = c.closeAt == 0 and ticks - c.since > (c.modsSent ? MODS_TIMEOUT : JOIN_TIMEOUT);

            if (refused or idle)
            {
                if (idle)
                    log.push_back("closed a connection that never joined");

                transport.disconnect(peer);
                c.closeAt = ticks + MODS_TIMEOUT;   // asked once; the transport says when it is gone
            }
        }
    }

    float NetServer::rttOf(const Client& c) const
    {
        const float measured = transport.rttMs(c.peer);
        return std::clamp(measured > 0.0f ? measured : static_cast<float>(c.rttMs), 0.0f, MAX_COMPENSATED_RTT_MS);
    }

    void NetServer::clampStamps(const Client& c, InputFrame& in) const
    {
        // What a client sees is INTERP_TICKS behind its estimate of the server, and its input then waits
        // half a round trip and the buffer: older than that is a client claiming to see the past
        const int rttSteps = static_cast<int>(std::ceil(rttOf(c) / stepRealMs(world.tuning)));
        const int now = static_cast<int>(world.steps);
        const int oldest = now - (NetClient::INTERP_TICKS + rttSteps + static_cast<int>(MAX_BUFFER) + SNAPSHOT_EVERY + 4);

        if (in.viewTick >= 0)
            in.viewTick = std::clamp(in.viewTick, std::max(0, oldest), now);

        if (in.stamp >= 0)
            in.stamp = std::clamp(in.stamp, std::max(0, oldest), now);
    }

    InputFrame NetServer::nextInput(Client& c)
    {
        // A client running a little fast fills the buffer: drop the oldest to keep the lag down
        while (c.inputs.size() > MAX_BUFFER)
        {
            c.last = c.inputs.begin()->second;
            c.lastSeq = c.inputs.begin()->first;
            c.inputs.erase(c.inputs.begin());
        }

        if (not c.inputs.empty())
        {
            // The next one, or the first after a gap (lost beyond the redundancy)
            auto it = c.inputs.begin();
            c.lastSeq = it->first;
            c.last = it->second;
            c.inputs.erase(it);
            clampStamps(c, c.last);
            return c.last;
        }

        // Nothing arrived in time: hold what was held, press nothing
        InputFrame held = c.last;
        clampStamps(c, held);
        held.jumpPressed = held.dodgePressed = held.lightPressed = held.heavyPressed = false;
        held.equip = 0;
        held.pick = 0;
        for (bool& p : held.abilityPressed)
            p = false;
        return held;
    }

    void NetServer::step(const InputFrame* localInput)
    {
        ++ticks;
        closeIdle();

        // The arena may have been rebuilt (a reset): keep listening to the current one
        world.grid.changeLog = &gridLog;
        world.terrain.damageLog = &damageLog;

        std::map<int, InputFrame> inputs;

        if (localInput)
            if (const Fighter* p = world.player())
                inputs[p->id] = *localInput;

        for (auto& [peer, c] : clients)
        {
            if (not c.joined)
                continue;

            inputs[c.playerId] = nextInput(c);

            // How long a hit on this player waits for a dodge already on its way: half the round trip
            world.defenderGrace[peer] = dodgeGrace ? std::clamp(static_cast<int>(std::lround(rttOf(c) * 0.5 / stepRealMs(world.tuning))), 0, 8) : 0;
        }

        const size_t eventsBefore = world.events.size();
        world.step(inputs);

        events.insert(events.end(), world.events.begin() + static_cast<std::ptrdiff_t>(std::min(eventsBefore, world.events.size())), world.events.end());

        // Nobody plays the events of a dedicated server
        if (headless)
            world.events.clear();

        // The arena built anew (a run, its next wave): sent whole to everyone, its old changes dropped
        if (sentArenaBuilds < 0)
            sentArenaBuilds = world.arenaBuilds;

        if (world.arenaBuilds != sentArenaBuilds)
        {
            sentArenaBuilds = world.arenaBuilds;
            gridLog.clear();
            damageLog.clear();
            knownPieces.clear();

            WorldInitMsg init;
            init.size = glm::ivec3(world.grid.width(), world.grid.height(), world.grid.depth());
            init.cells = world.grid.raw();
            init.damage = world.terrain.damagedCells();

            for (auto& [peer, c] : clients)
                if (c.joined)
                    transport.send(peer, encode(init), true);
        }

        sendTerrain();
        stepLobby();

        if (++stepsSinceSnapshot >= SNAPSHOT_EVERY)
        {
            stepsSinceSnapshot = 0;
            sendSnapshots();
            events.clear();
        }
    }

    LobbyMsg NetServer::lobby() const
    {
        LobbyMsg m;
        m.dedicated = headless;

        if (not headless)
            if (const Fighter* p = world.player())
                m.players.push_back({p->id, hostName, true, true, 0});

        std::vector<const Client*> joined;
        for (const auto& [peer, c] : clients)
            if (c.joined)
                joined.push_back(&c);
        std::sort(joined.begin(), joined.end(), [](const Client* a, const Client* b) { return a->joinOrder < b->joinOrder; });

        for (const Client* c : joined)
        {
            const float measured = transport.rttMs(c->peer);
            m.players.push_back({c->playerId, c->name, false, c->ready, static_cast<uint16_t>(std::clamp(measured > 0.0f ? measured : static_cast<float>(c->rttMs), 0.0f, 65535.0f))});
        }

        return m;
    }

    void NetServer::stepLobby()
    {
        // A run begins: everyone is not ready again, for the next time in the lobby
        const RunPhase phase = world.run.phase;
        if (phase != lastPhase)
        {
            const bool started = phase == RunPhase::Difficulty or phase == RunPhase::Armory or phase == RunPhase::Fighting;
            if (started and (lastPhase == RunPhase::Off or lastPhase == RunPhase::Victory or lastPhase == RunPhase::Defeat))
            {
                for (auto& [peer, c] : clients)
                    c.ready = false;
                lobbyDirty = true;
            }
            lastPhase = phase;
        }

        // A dedicated server has no host to start the run: it starts when everyone is ready
        if (headless and phase == RunPhase::Off and clientCount() > 0)
        {
            bool all = true;
            for (const auto& [peer, c] : clients)
                if (c.joined)
                    all = all and c.ready;

            if (all)
            {
                log.push_back("everyone is ready: the run begins");
                world.chooseDifficulty(static_cast<uint32_t>(world.steps) * 2654435761u + 7u);
            }
        }

        if (not lobbyDirty and ++stepsSinceLobby < LOBBY_EVERY)
            return;

        lobbyDirty = false;
        stepsSinceLobby = 0;
        const auto bytes = encode(lobby());

        for (const auto& [peer, c] : clients)
            if (c.joined)
                transport.send(peer, bytes, true);
    }

    void NetServer::sendTerrain()
    {
        // Pieces that broke off: their cells, once
        for (const auto& piece : world.terrain.falling)
        {
            if (knownPieces.count(piece.id))
                continue;

            knownPieces.insert(piece.id);

            PieceSpawnMsg spawn;
            spawn.id = piece.id;
            spawn.origin = piece.origin;
            spawn.drop = piece.drop;
            spawn.cells = piece.cells;
            const auto bytes = encode(spawn);

            for (const auto& [peer, c] : clients)
                if (c.joined)
                    transport.send(peer, bytes, true);
        }

        for (auto it = knownPieces.begin(); it != knownPieces.end();)
        {
            const bool falling = std::any_of(world.terrain.falling.begin(), world.terrain.falling.end(), [&](const FallingPiece& p) { return p.id == *it; });
            it = falling ? std::next(it) : knownPieces.erase(it);
        }

        if (gridLog.empty() and damageLog.empty())
            return;

        TerrainDeltaMsg delta;
        delta.blocks = gridLog;

        std::sort(damageLog.begin(), damageLog.end());
        damageLog.erase(std::unique(damageLog.begin(), damageLog.end()), damageLog.end());

        for (uint32_t index : damageLog)
            delta.damage.emplace_back(index, world.terrain.damageRatioAt(index));

        gridLog.clear();
        damageLog.clear();

        const auto bytes = encode(delta);

        for (const auto& [peer, c] : clients)
            if (c.joined)
                transport.send(peer, bytes, true);
    }

    void NetServer::sendSnapshots()
    {
        SnapshotMsg base;
        base.seq = ++snapshotSeq;
        base.tick = static_cast<uint32_t>(world.steps);
        base.fighters = world.fighters;
        base.events = events;

        for (const auto& p : world.projectiles)
            base.projectiles.push_back({p.id, p.position, p.velocity, p.source, p.attack});

        for (const auto& piece : world.terrain.falling)
            base.pieces.push_back({piece.id, piece.drop});

        for (const auto& z : world.zones)
            base.zones.push_back({z.id, z.element, z.position, z.radius, z.remaining, z.duration, z.heal, z.blasting(), z.team == Team::Enemy,
                                  z.blasting() ? z.owner : 0});

        base.run = world.run;
        base.run.pending.clear();
        base.pickups = world.pickups;

        // Everyone's items and run stats: every tenth snapshot, and all the while at a run's end (the recap)
        base.extras = (world.steps / SNAPSHOT_EVERY) % 10 == 0 or world.run.phase == RunPhase::Victory or world.run.phase == RunPhase::Defeat;

        for (auto& [peer, c] : clients)
        {
            if (not c.joined)
                continue;

            SnapshotMsg m = base;
            m.ackSeq = c.lastSeq;
            m.echoTimeMs = c.lastClientTimeMs;
            m.playerId = c.playerId;

            // As the changes since the newest one it read, while that one is still kept (else whole). Older
            // ones are never built on again
            while (not c.sent.empty() and c.sent.front().seq < c.snapshotAck)
                c.sent.pop_front();

            const SnapshotRecords* read = not c.sent.empty() and c.sent.front().seq == c.snapshotAck ? &c.sent.front() : nullptr;
            SnapshotRecords records;
            transport.send(peer, encode(m, names, read, &records), false);

            c.sent.push_back(std::move(records));
            while (c.sent.size() > SNAPSHOTS_KEPT)
                c.sent.pop_front();
        }
    }
}
