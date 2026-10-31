#pragma once

#include <deque>
#include <map>
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

        /**
         * Events to play: the server's, except the ones this client already played for its own player.
         * Those about this client's player (it was hit, it hit something) come at once, with the
         * player that is drawn in the present; the rest come when the others, drawn INTERP_TICKS in
         * the past, reach the moment they happened, so a spark goes off as the blow is seen landing.
         */
        std::vector<WorldEvent> takeEvents();

        void leave();

        /** The name sent when it joins (before it connects). */
        void setName(const std::string& n) { name = n; }

        /** The recipes on this player's spell keys: sent to the server (now, or once joined). */
        void setSpells(const std::vector<SpellRecipe>& recipes);

        /** Ready (or not) to start the run, in the lobby: sent to the server (now, or once joined). */
        void setReady(bool ready);
        bool ready = false;

        /** Who is in the game, as the server last said. */
        LobbyMsg lobby;

        /** A player's name from the lobby, or "" when not known. */
        std::string nameOf(int playerId) const;

        enum class State { Connecting, Joining, ModsArrived, Playing, Refused, Lost };
        State state = State::Connecting;
        std::string refusal;

        /** The server's mods, in state ModsArrived. Nothing of them is checked yet: see writeModFiles. */
        ModBundle mods;

        /** Says hello on a connection already open: a new client, made once the server's mods are loaded. */
        void rejoin();

        /** Gives up joining, for a reason found outside (the server's mods would not load). */
        void refuse(const std::string& reason);

        int playerId = 0;
        float rttMs = 0.0f;

        /**
         * The action frame of everyone but the predicted player, as a fraction, at the moment they are
         * drawn (see updateDisplay). Snapshots come every other step: drawn from their whole frames,
         * attacks and rolls would move in steps of two.
         */
        std::map<int, float> displayFrame;

        /** Where the prediction was minus where the server's correction put it: drawn on top, decays. */
        glm::vec3 correction = glm::vec3(0.0f);

        static constexpr int INTERP_TICKS = 6;        // 100 ms behind the newest snapshot
        static constexpr size_t SEND_REDUNDANCY = 5;  // inputs repeated in each message, against loss

        /** Estimate of the server's tick now. */
        double serverTick(double nowMs) const;

        size_t pendingInputs() const { return pending.size(); }

    private:
        void sayHello();
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
        std::deque<WorldEvent> held;   // the others' events, until the render tick reaches them (oldest first)
        std::map<int, FallingPiece> pieces;

        // What only looks: worked out here from what the server sends, never sent
        std::map<int, double> flashUntil;   // fighter -> server tick its hit flash ends (from hit events)

        struct StepSmoothing
        {
            float y = 0.0f;        // the height it was drawn at last
            float offset = 0.0f;   // drawn this much lower, easing back to 0 (a step up does not pop)
            double atMs = -1.0;
        };
        std::map<int, StepSmoothing> steps;

        std::vector<SpellRecipe> spells;
    };
}
