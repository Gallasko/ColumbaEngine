#pragma once

#include <deque>
#include <memory>
#include <random>
#include <vector>

#include "transport.h"

namespace arena::net
{
    /**
     * An in-memory network for tests: one server and its clients on a simulated link with latency,
     * jitter and packet loss, on a clock the test advances. Reliable messages keep their order and
     * are never lost; unreliable ones can be dropped, and jitter can reorder them.
     */
    class LoopbackHub
    {
    public:
        struct Link
        {
            double latencyMs = 0.0;   // one way
            double jitterMs = 0.0;
            double loss = 0.0;        // 0..1, unreliable messages only
        };

        Link link;

        Transport& server();
        /** A new client endpoint, already connecting (the server sees it on its next poll). */
        Transport& addClient();
        void disconnect(Transport& client);

        void advance(double ms) { now += ms; }
        double time() const { return now; }

    private:
        struct Pending
        {
            double at;
            NetEvent event;
        };

        class Endpoint : public Transport
        {
        public:
            Endpoint(LoopbackHub* hub, int self) : hub(hub), self(self) {}

            void send(int peer, const std::vector<uint8_t>& data, bool reliable) override;
            bool poll(NetEvent& out) override;
            float rttMs(int) const override { return static_cast<float>(hub->link.latencyMs * 2.0); }

            LoopbackHub* hub;
            int self;                  // 0 = server, else the client's peer id
            std::deque<Pending> inbox;
            double lastReliable = 0.0;
            bool connected = true;
        };

        void deliver(int to, NetEvent event, bool reliable, double& lastReliable);
        Endpoint* endpoint(int id);

        std::unique_ptr<Endpoint> serverEnd;
        std::vector<std::unique_ptr<Endpoint>> clients;
        double now = 0.0;
        int nextPeer = 1;
        std::mt19937 rng{42};
    };
}
