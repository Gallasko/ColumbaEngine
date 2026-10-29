#pragma once

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

        /** What happened (joins, leaves, refusals), for the console. Drained by the caller. */
        std::vector<std::string> log;

        /** Hits on a remote player wait half its round trip for a dodge already on its way (see ArenaWorld::defenderGrace). */
        bool dodgeGrace = true;

        /** How many may join: 7 next to a host's own player, 8 on a dedicated server. */
        int maxClients = 7;

        /** Input buffered per client beyond the one used each step: more is dropped (lag), none repeats the last. */
        static constexpr size_t MAX_BUFFER = 4;

    private:
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
        };

        void onMessage(Client& c, const std::vector<uint8_t>& data);
        InputFrame nextInput(Client& c);
        void sendTerrain();
        void sendSnapshots();

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
    };
}
