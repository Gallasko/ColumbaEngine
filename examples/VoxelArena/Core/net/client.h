#pragma once

#include <deque>
#include <string>
#include <vector>

#include "protocol.h"
#include "transport.h"

namespace arena::net
{
    /**
     * The client side. The game's world becomes a view of the server's:
     *   - your own player is predicted: stepped at once from your input, then, when a snapshot says
     *     where the server had it, put there and stepped again through the inputs it has not seen yet
     *     (a small mismatch is smoothed out by `correction`, a big one snaps);
     *   - everyone else is drawn INTERP_TICKS behind the newest snapshot, between two snapshots;
     *   - terrain changes and falling pieces are applied as the server sends them.
     * The world runs with authority off: no AI, hits, projectiles or terrain of its own.
     */
    class NetClient
    {
    public:
        NetClient(ArenaWorld& world, const Content& content, const Tuning& tuning, Transport& transport, const std::string& name);

        /** Reads the network. nowMs: a steady clock. */
        void poll(double nowMs);

        /** One local step: the input is predicted at once and sent. */
        void step(InputFrame input, double nowMs);

        /** Places the other fighters, projectiles and pieces for drawing at this moment. */
        void updateDisplay(double nowMs);

        /** Events to play: the server's, except the ones this client already played for its own player. */
        std::vector<WorldEvent> takeEvents();

        void leave();

        enum class State { Connecting, Joining, Playing, Refused, Lost };
        State state = State::Connecting;
        std::string refusal;

        int playerId = 0;
        float rttMs = 0.0f;

        /** Where the prediction was minus where the server's correction put it: drawn on top, decays. */
        glm::vec3 correction = glm::vec3(0.0f);

        static constexpr int INTERP_TICKS = 6;        // 100 ms behind the newest snapshot
        static constexpr size_t SEND_REDUNDANCY = 5;  // inputs repeated in each message, against loss

        /** Estimate of the server's tick now. */
        double serverTick(double nowMs) const;

        size_t pendingInputs() const { return pending.size(); }

    private:
        void onSnapshot(SnapshotMsg& m, double nowMs);
        void reconcile(const Fighter& server, uint32_t ackSeq);

        ArenaWorld& world;
        const Content& content;
        Tuning tuning;
        Transport& transport;
        std::string name;
        Names names;

        uint32_t nextSeq = 1;
        std::deque<StampedInput> pending;

        std::deque<SnapshotMsg> snapshots;   // newest last, for interpolation
        double lastSnapshotTick = -1.0;
        double lastSnapshotAt = 0.0;
        double startedAt = -1.0;

        std::vector<WorldEvent> events;
        std::map<int, FallingPiece> pieces;
    };
}
