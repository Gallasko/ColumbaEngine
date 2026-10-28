#pragma once

#include <deque>

#include "Core/net/transport.h"

namespace pg
{
    struct NetworkSystem;
}

namespace arena::net
{
    /**
     * The game's transport over the engine's NetworkSystem (TCP for reliable, UDP for the rest).
     * Server side, a peer is the engine's client id; client side, the server is peer 0.
     * Only queues are touched here: the sockets stay with NetworkSystem::execute.
     */
    class EngineTransport : public Transport
    {
    public:
        explicit EngineTransport(pg::NetworkSystem* system);

        void send(int peer, const std::vector<uint8_t>& data, bool reliable) override;
        bool poll(NetEvent& out) override;
        float rttMs(int peer) const override;

        pg::NetworkSystem* system;
    };
}
