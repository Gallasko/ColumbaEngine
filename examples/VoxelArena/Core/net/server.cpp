#include "server.h"

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
                    break;

                case NetEvent::Type::Disconnected:
                {
                    auto it = clients.find(e.peer);

                    if (it != clients.end())
                    {
                        if (it->second.joined)
                        {
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
                    Client& c = clients[e.peer];
                    c.peer = e.peer;
                    onMessage(c, e.data);
                    break;
                }
            }
        }
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

                Welcome welcome;
                welcome.peer = c.peer;
                welcome.tick = static_cast<uint32_t>(world.steps);

                if (hello.version != PROTOCOL_VERSION)
                    welcome.reason = "protocol " + std::to_string(hello.version) + ", the server runs " + std::to_string(PROTOCOL_VERSION);
                else if (hello.fingerprint != content.fingerprint)
                    welcome.reason = "different mods or content than the server (load the same mods)";
                else if (clientCount() >= maxClients)
                    welcome.reason = "the server is full";

                if (not welcome.reason.empty())
                {
                    transport.send(c.peer, encode(welcome), true);
                    log.push_back(hello.name + " was refused: " + welcome.reason);
                    return;
                }

                c.joined = true;
                c.name = hello.name.empty() ? "player " + std::to_string(c.peer) : hello.name.substr(0, 32);
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
                c.rttMs = msg.rttMs;

                // Several inputs per message (repeated against loss): keep the ones not played yet
                for (const auto& s : msg.inputs)
                    if (s.seq > c.lastSeq)
                        c.inputs[s.seq] = s.input;
                break;
            }

            case Msg::Bye:
            {
                if (c.joined)
                {
                    log.push_back(c.name + " left");
                    world.removeFighter(c.playerId);
                }

                c.joined = false;
                break;
            }

            default:
                break;
        }
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
            return c.last;
        }

        // Nothing arrived in time: hold what was held, press nothing
        InputFrame held = c.last;
        held.jumpPressed = held.dodgePressed = held.lightPressed = held.heavyPressed = false;
        held.equip = 0;
        for (bool& p : held.abilityPressed)
            p = false;
        return held;
    }

    void NetServer::step(const InputFrame* localInput)
    {
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
            world.defenderGrace[peer] = dodgeGrace ? std::clamp(static_cast<int>(std::lround(c.rttMs * 0.5 / (STEP_SECONDS * 1000.0))), 0, 8) : 0;
        }

        const size_t eventsBefore = world.events.size();
        world.step(inputs);

        events.insert(events.end(), world.events.begin() + static_cast<std::ptrdiff_t>(std::min(eventsBefore, world.events.size())), world.events.end());

        // Nobody plays the events of a dedicated server
        if (headless)
            world.events.clear();

        sendTerrain();

        if (++stepsSinceSnapshot >= SNAPSHOT_EVERY)
        {
            stepsSinceSnapshot = 0;
            sendSnapshots();
            events.clear();
        }
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
        base.tick = static_cast<uint32_t>(world.steps);
        base.fighters = world.fighters;
        base.events = events;

        for (const auto& p : world.projectiles)
            base.projectiles.push_back({p.id, p.position, p.velocity, p.def.model, p.def.modelScale});

        for (const auto& piece : world.terrain.falling)
            base.pieces.push_back({piece.id, piece.drop});

        for (const auto& [peer, c] : clients)
        {
            if (not c.joined)
                continue;

            SnapshotMsg m = base;
            m.ackSeq = c.lastSeq;
            m.echoTimeMs = c.lastClientTimeMs;
            m.playerId = c.playerId;
            transport.send(peer, encode(m, names), false);
        }
    }
}
