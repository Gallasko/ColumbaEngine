#pragma once

#include <deque>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "protocol.h"
#include "transport.h"

namespace arena::net
{
    /**
     * The authoritative side: owns the real simulation and tells the clients about it.
     *
     * Listen server: the game's own world, with the host's player (owner 0) stepped from local
     * input. Dedicated server: a world with no local player, and nobody draining its events.
     *
     * Per step: every client's next input (buffered, a step each), one world step, terrain changes
     * and falling pieces (reliable), and every SNAPSHOT_EVERY steps a snapshot per client.
     */
    class NetServer
    {
    public:
        NetServer(ArenaWorld& world, const Content& content, Transport& transport, bool headless);

        /** Reads the network: joins, leaves, inputs. Call often (every frame). */
        void poll();

        /** One simulation step. localInput: the host's player (listen server), nullptr for none. */
        void step(const InputFrame* localInput);

        size_t clientCount() const;

        /** Every connection, joined or not (yet): someone refused or getting the mods is one too. */
        size_t peerCount() const { return clients.size(); }

        /** What happened (joins, leaves, refusals), for the console. Drained by the caller. */
        std::vector<std::string> log;

        /** Hits on a remote player wait half its round trip for a dodge already on its way (see ArenaWorld::defenderGrace). */
        bool dodgeGrace = true;

        /** How many may join: 7 next to a host's own player, 8 on a dedicated server. */
        int maxClients = 7;

        /**
         * The mods this server offers to a client whose content differs, instead of refusing it: the
         * files of every mod its content loaded, each under its id (whichever folder it came from;
         * the ones turned off are not sent). Not called (or too much to send): such a client is refused.
         */
        void offerMods();

        /** The host's own name, for the lobby (a listen server). */
        std::string hostName = "host";

        /** Who is in the game, the host first: what the lobby shows (and the clients are sent). */
        LobbyMsg lobby() const;

        /** The lobby is sent when it changes, and this often for the pings. */
        static constexpr int LOBBY_EVERY = 120;

        /** Input buffered per client beyond the one used each step: more is dropped (lag), none repeats the last. */
        static constexpr size_t MAX_BUFFER = 4;

        /** A connection that has not joined after this many steps is closed (it holds a slot for nothing). */
        static constexpr uint64_t JOIN_TIMEOUT = 30 * 60;

        /** ... and after this many when it is getting the server's mods (a download, then loading them). */
        static constexpr uint64_t MODS_TIMEOUT = 120 * 60;

        /** A refused client is closed this many steps after its refusal (time for the refusal to reach it). */
        static constexpr uint64_t REFUSED_LINGER = 60;

        /** The round trip lag compensation and the dodge grace go by, at most (a slower link gets less). */
        static constexpr float MAX_COMPENSATED_RTT_MS = 300.0f;

    private:
        std::vector<uint8_t> modBundle;    // encoded once, ready to send

        struct Client
        {
            int peer = 0;
            bool joined = false;
            std::string name;
            int playerId = 0;
            std::map<uint32_t, InputFrame> inputs;
            uint32_t lastSeq = 0;
            InputFrame last;
            uint32_t lastClientTimeMs = 0;
            uint16_t rttMs = 0;
            bool ready = false;      // in the lobby: ready to start the run
            std::set<std::string> locked;   // what its player has not unlocked (Msg::Locks): put back on its fighter each step
            uint64_t joinOrder = 0;
            bool modsSent = false;   // once only: a client still different after them is refused
            uint64_t since = 0;      // the step it connected (or got the mods) at
            uint64_t closeAt = 0;    // refused: closed at this step (0: not refused)
            uint32_t snapshotAck = 0;               // the newest snapshot it read: the next ones are the changes since
            std::deque<SnapshotRecords> sent;       // what it was sent from that one on
        };

        /** The round trip of a client: the transport's measure when it has one, else what the client says. */
        float rttOf(const Client& c) const;

        /** An input's network stamps, kept within what the client's round trip makes possible. */
        void clampStamps(const Client& c, InputFrame& in) const;

        void refuse(Client& c, const std::string& name, const std::string& reason);
        void closeIdle();

        void onMessage(Client& c, const std::vector<uint8_t>& data);
        InputFrame nextInput(Client& c);
        void sendTerrain();
        void sendSnapshots();
        void stepLobby();

        ArenaWorld& world;
        const Content& content;
        Transport& transport;
        bool headless;
        Names names;

        std::map<int, Client> clients;
        std::vector<std::pair<uint32_t, BlockId>> gridLog;
        std::vector<uint32_t> damageLog;
        int sentArenaBuilds = -1;   // the arena's builds already sent whole
        std::vector<WorldEvent> events;   // since the last snapshot
        std::set<int> knownPieces;
        int stepsSinceSnapshot = 0;
        uint32_t snapshotSeq = 0;

        bool lobbyDirty = true;
        int stepsSinceLobby = 0;
        uint64_t joins = 0;
        RunPhase lastPhase = RunPhase::Off;
        uint64_t ticks = 0;   // steps of this server, for its timeouts
    };
}
