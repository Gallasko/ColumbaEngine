#pragma once

#include <cstdint>
#include <vector>

namespace arena::net
{
    /** What a transport hands up: a peer came, left, or sent a message. */
    struct NetEvent
    {
        enum class Type { Connected, Disconnected, Message };

        Type type = Type::Message;
        int peer = 0;                 // server side: the client; client side: always 0 (the server)
        std::vector<uint8_t> data;
    };

    /**
     * How messages travel. Reliable messages arrive, in order; unreliable ones may be lost or
     * arrive out of order, and are for what the next message replaces (inputs, snapshots).
     * The game uses the engine's NetworkSystem; the tests an in-memory network with simulated lag.
     */
    class Transport
    {
    public:
        virtual ~Transport() = default;

        virtual void send(int peer, const std::vector<uint8_t>& data, bool reliable) = 0;
        virtual bool poll(NetEvent& out) = 0;

        /** Round trip to a peer in ms, when the transport knows it (else 0). */
        virtual float rttMs(int peer) const { (void)peer; return 0.0f; }
    };
}
