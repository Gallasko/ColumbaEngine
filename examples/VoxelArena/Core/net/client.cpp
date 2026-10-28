#include "client.h"

#include <algorithm>
#include <cmath>

namespace arena::net
{
    namespace
    {
        // What a client plays for its own player as it predicts it: the server's copies are skipped
        bool predictedKind(const std::string& kind)
        {
            return kind == "dodge" or kind == "jump" or kind == "land" or kind == "hard_land" or kind == "swing" or kind == "step";
        }

        float lerpAngle(float a, float b, float t)
        {
            const float d = std::fmod(b - a + 540.0f, 360.0f) - 180.0f;
            return a + d * t;
        }
    }

    NetClient::NetClient(ArenaWorld& w, const Content& c, const Tuning& t, Transport& tr, const std::string& n)
        : world(w), content(c), tuning(t), transport(tr), name(n)
    {
        names.build(content);

        // The same arena as the server's to start with; its real state comes with the welcome
        world.start(content, tuning, false);
        world.authority = false;
        world.fighters.clear();
    }

    double NetClient::serverTick(double nowMs) const
    {
        if (lastSnapshotTick < 0.0)
            return 0.0;

        // The newest snapshot was the server's tick some half round trip ago
        return lastSnapshotTick + (nowMs - lastSnapshotAt + rttMs * 0.5) / (STEP_SECONDS * 1000.0);
    }

    void NetClient::poll(double nowMs)
    {
        if (startedAt < 0.0)
            startedAt = nowMs;

        NetEvent e;

        while (transport.poll(e))
        {
            if (e.type == NetEvent::Type::Connected)
            {
                Hello hello;
                hello.fingerprint = content.fingerprint;
                hello.name = name;
                transport.send(0, encode(hello), true);
                state = State::Joining;
                continue;
            }

            if (e.type == NetEvent::Type::Disconnected)
            {
                if (state != State::Refused)
                    state = State::Lost;
                continue;
            }

            switch (peek(e.data))
            {
                case Msg::Welcome:
                {
                    Welcome w;
                    if (not decode(e.data, w))
                        break;

                    if (not w.accepted)
                    {
                        state = State::Refused;
                        refusal = w.reason;
                        break;
                    }

                    playerId = w.playerId;
                    world.localPlayerId = playerId;
                    lastSnapshotTick = w.tick;
                    lastSnapshotAt = nowMs;
                    state = State::Playing;
                    break;
                }

                case Msg::WorldInit:
                {
                    WorldInitMsg m;
                    if (not decode(e.data, m) or m.size != glm::ivec3(world.grid.width(), world.grid.height(), world.grid.depth()))
                        break;

                    for (int y = 0; y < m.size.y; ++y)
                        for (int z = 0; z < m.size.z; ++z)
                            for (int x = 0; x < m.size.x; ++x)
                                world.grid.set(x, y, z, m.cells[world.grid.index(x, y, z)]);

                    for (const auto& [index, ratio] : m.damage)
                        world.terrain.setDamageRatio(index, ratio);
                    break;
                }

                case Msg::TerrainDelta:
                {
                    TerrainDeltaMsg m;
                    if (not decode(e.data, m))
                        break;

                    const auto& cells = world.grid.raw();

                    for (const auto& [index, id] : m.blocks)
                    {
                        if (index >= cells.size())
                            continue;

                        const int w = world.grid.width(), d = world.grid.depth();
                        const int x = static_cast<int>(index % w);
                        const int z = static_cast<int>((index / w) % d);
                        const int y = static_cast<int>(index / (static_cast<size_t>(w) * d));
                        world.grid.set(x, y, z, id);
                    }

                    for (const auto& [index, ratio] : m.damage)
                        world.terrain.setDamageRatio(index, ratio);
                    break;
                }

                case Msg::PieceSpawn:
                {
                    PieceSpawnMsg m;
                    if (not decode(e.data, m))
                        break;

                    FallingPiece p;
                    p.id = m.id;
                    p.origin = m.origin;
                    p.drop = m.drop;
                    p.cells = m.cells;

                    for (int y = 0; y < p.cells.height(); ++y)
                        for (int z = 0; z < p.cells.depth(); ++z)
                            for (int x = 0; x < p.cells.width(); ++x)
                                if (p.cells.get(x, y, z) != AIR)
                                    p.occupied.emplace_back(x, y, z);

                    pieces[p.id] = std::move(p);
                    break;
                }

                case Msg::Snapshot:
                {
                    SnapshotMsg m;
                    if (decode(e.data, m, names))
                        onSnapshot(m, nowMs);
                    break;
                }

                default:
                    break;
            }
        }
    }

    void NetClient::onSnapshot(SnapshotMsg& m, double nowMs)
    {
        // Out of order: an older one than what is already known is of no use
        if (not snapshots.empty() and m.tick <= snapshots.back().tick)
            return;

        if (m.echoTimeMs > 0)
        {
            const float sample = static_cast<float>(nowMs - startedAt) - static_cast<float>(m.echoTimeMs);

            if (sample >= 0.0f and sample < 5000.0f)
                rttMs = rttMs <= 0.0f ? sample : rttMs + (sample - rttMs) * 0.1f;
        }

        lastSnapshotTick = m.tick;
        lastSnapshotAt = nowMs;

        // The server's events, without the ones already played locally for the predicted player
        for (auto& e : m.events)
        {
            // Coloured by this machine's own blocks
            if (e.block != AIR and e.block < content.blockTable.size())
                e.color = content.blockTable[e.block].color;

            // A hit makes its target flash, from the tick it happened
            if (e.fighter != 0 and (e.kind == "hit" or e.kind == "stagger" or e.kind == "kill"))
                flashUntil[e.fighter] = static_cast<double>(e.tick) + tuning.feel.hitFlashFrames;

            if (e.fighter == playerId and predictedKind(e.kind))
                continue;

            // About this player: now. About the others: when they are drawn there
            if (e.fighter == playerId or e.source == playerId)
                events.push_back(e);
            else
                held.push_back(e);
        }

        for (const auto& f : m.fighters)
            if (f.id == playerId)
                reconcile(f, m.ackSeq);

        snapshots.push_back(std::move(m));

        while (snapshots.size() > 30)
            snapshots.pop_front();
    }

    void NetClient::reconcile(const Fighter& server, uint32_t ackSeq)
    {
        Fighter* me = world.find(playerId);

        if (not me)
        {
            world.fighters.push_back(server);
            me = &world.fighters.back();
        }

        const glm::vec3 before = me->body.position;

        // The server's state, then the inputs it had not played yet, on top
        const Fighter keep = *me;
        *me = server;
        me->hitThisSwing = keep.hitThisSwing;
        me->aimDir = keep.aimDir;
        me->stepOffset = keep.stepOffset;   // only looks: the server does not send it

        while (not pending.empty() and pending.front().seq <= ackSeq)
            pending.pop_front();

        const size_t eventsBefore = world.events.size();

        for (const auto& s : pending)
            world.step(std::map<int, InputFrame>{{playerId, s.input}});

        // Replays make no sound: those events were played when the input was first predicted
        world.events.resize(std::min(world.events.size(), eventsBefore));

        const glm::vec3 error = before - world.find(playerId)->body.position;
        correction = glm::length(error) > 2.0f ? glm::vec3(0.0f) : correction + error;
    }

    void NetClient::step(InputFrame input, double nowMs)
    {
        if (state != State::Playing)
            return;

        input.stamp = static_cast<int>(std::lround(serverTick(nowMs)));
        input.viewTick = static_cast<int>(std::lround(serverTick(nowMs) - INTERP_TICKS));

        pending.push_back({nextSeq++, input});

        // Too far ahead of the server (it stopped answering): forget the oldest
        while (pending.size() > 120)
            pending.pop_front();

        InputMsg msg;
        msg.clientTimeMs = static_cast<uint32_t>(std::max(1.0, nowMs - startedAt));
        msg.rttMs = static_cast<uint16_t>(std::clamp(rttMs, 0.0f, 65535.0f));

        const size_t from = pending.size() > SEND_REDUNDANCY ? pending.size() - SEND_REDUNDANCY : 0;
        msg.inputs.assign(pending.begin() + static_cast<std::ptrdiff_t>(from), pending.end());
        transport.send(0, encode(msg), false);

        // Predict
        if (world.find(playerId))
        {
            const size_t eventsBefore = world.events.size();
            world.step(std::map<int, InputFrame>{{playerId, input}});

            // Own predicted events are played now; the rest of the world's events come from the server
            for (size_t i = eventsBefore; i < world.events.size(); ++i)
                if (world.events[i].fighter == playerId and predictedKind(world.events[i].kind))
                    events.push_back(world.events[i]);

            world.events.clear();
        }

        // The correction fades over a few frames
        correction *= std::exp(-12.0f * STEP_SECONDS);
    }

    void NetClient::updateDisplay(double nowMs)
    {
        if (snapshots.empty())
            return;

        const double renderTick = serverTick(nowMs) - INTERP_TICKS;

        // The others' events whose moment is now drawn
        while (not held.empty() and held.front().tick <= renderTick)
        {
            events.push_back(held.front());
            held.pop_front();
        }

        // The two snapshots around the render tick (or the newest, when the stream runs late)
        const SnapshotMsg* a = &snapshots.back();
        const SnapshotMsg* b = nullptr;

        for (size_t i = 0; i + 1 < snapshots.size(); ++i)
        {
            if (snapshots[i].tick <= renderTick and snapshots[i + 1].tick > renderTick)
            {
                a = &snapshots[i];
                b = &snapshots[i + 1];
                break;
            }
        }

        const float t = b ? static_cast<float>((renderTick - a->tick) / static_cast<double>(b->tick - a->tick)) : 0.0f;

        // Everyone but the predicted player, as of the render tick
        std::vector<Fighter> shown;
        displayFrame.clear();

        // The hit flash, in frames left at a server tick
        auto flashAt = [&](int id, double tick) {
            const auto it = flashUntil.find(id);
            return it == flashUntil.end() ? 0 : static_cast<int>(std::ceil(std::max(0.0, it->second - tick)));
        };

        if (const Fighter* me = world.find(playerId))
        {
            shown.push_back(*me);
            shown.back().flash = flashAt(playerId, serverTick(nowMs));
        }

        for (const auto& fa : a->fighters)
        {
            if (fa.id == playerId)
                continue;

            Fighter f = fa;

            if (b)
            {
                for (const auto& fb : b->fighters)
                {
                    if (fb.id != fa.id)
                        continue;

                    f.body.position = glm::mix(fa.body.position, fb.body.position, t);
                    f.yaw = lerpAngle(fa.yaw, fb.yaw, t);
                    break;
                }
            }

            // The action's frame at the render tick. The same action in both snapshots: between their
            // frames (a hitstop shows as no change). Otherwise the action of `a` runs on, a frame a step,
            // until the newer snapshot takes over
            float frame = static_cast<float>(fa.frame);
            const Fighter* fb = nullptr;

            if (b)
                for (const auto& other : b->fighters)
                    if (other.id == fa.id)
                        fb = &other;

            if (fb and fb->action == fa.action and fb->attack == fa.attack and fb->frame >= fa.frame)
                frame += static_cast<float>(fb->frame - fa.frame) * t;
            else
                frame += static_cast<float>(std::clamp(renderTick - a->tick - fa.hitstop, 0.0, static_cast<double>(INTERP_TICKS)));

            displayFrame[fa.id] = frame;
            f.flash = flashAt(f.id, renderTick);

            // Stepping up a block: the height jumps, drawn as an ease instead (like the server's own
            // step smoothing, which only looks and is not sent). A jump is in the air: not a step
            {
                StepSmoothing& s = steps[f.id];
                const bool grounded = fa.body.onGround and (not fb or fb->body.onGround);
                const float rise = f.body.position.y - s.y;

                if (s.atMs >= 0.0 and grounded and rise > 0.05f and rise < 1.5f)
                    s.offset -= rise;

                if (s.atMs >= 0.0)
                    s.offset *= std::exp(-tuning.player.stepSmoothing * static_cast<float>(std::max(0.0, nowMs - s.atMs) / 1000.0));

                s.y = f.body.position.y;
                s.atMs = nowMs;
                f.stepOffset = s.offset;
            }

            shown.push_back(std::move(f));
        }

        world.fighters = std::move(shown);

        // Forget the looks of who is gone, and flashes long over
        for (auto it = steps.begin(); it != steps.end();)
            it = world.find(it->first) ? std::next(it) : steps.erase(it);

        for (auto it = flashUntil.begin(); it != flashUntil.end();)
            it = it->second < renderTick - 1.0 and it->first != playerId ? flashUntil.erase(it) : std::next(it);

        // Projectiles fly straight between snapshots: from the newest, moved on by their speed
        const SnapshotMsg& newest = snapshots.back();
        const float since = static_cast<float>(nowMs - lastSnapshotAt) / 1000.0f;
        world.projectiles.clear();

        for (const auto& p : newest.projectiles)
        {
            Projectile q;
            q.id = p.id;
            q.position = p.position + p.velocity * since;
            q.velocity = p.velocity;
            q.source = p.source;
            q.attack = p.attack;

            // Its look comes from this machine's content
            if (const AttackDef* a = content.attackOf(p.source, p.attack))
                q.def = a->projectile;
            world.projectiles.push_back(q);
        }

        // Falling pieces: the cells from their spawn message, the drop from the newest snapshot
        world.terrain.falling.clear();

        for (const auto& ps : newest.pieces)
        {
            auto it = pieces.find(ps.id);

            if (it == pieces.end())
                continue;

            it->second.drop = ps.drop;
            world.terrain.falling.push_back(it->second);
        }

        for (auto it = pieces.begin(); it != pieces.end();)
        {
            const bool live = std::any_of(newest.pieces.begin(), newest.pieces.end(), [&](const PieceState& p) { return p.id == it->first; });
            it = live ? std::next(it) : pieces.erase(it);
        }
    }

    std::vector<WorldEvent> NetClient::takeEvents()
    {
        std::vector<WorldEvent> out;
        out.swap(events);
        return out;
    }

    void NetClient::leave()
    {
        transport.send(0, encodeBye(), true);
    }
}
