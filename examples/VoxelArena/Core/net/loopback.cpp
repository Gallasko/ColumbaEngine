#include "loopback.h"

#include <algorithm>

namespace arena::net
{
    Transport& LoopbackHub::server()
    {
        if (not serverEnd)
            serverEnd = std::make_unique<Endpoint>(this, 0);

        return *serverEnd;
    }

    Transport& LoopbackHub::addClient()
    {
        server();
        const int id = nextPeer++;
        clients.push_back(std::make_unique<Endpoint>(this, id));

        // Both sides learn of the connection after one trip
        double ignored = 0.0;
        NetEvent toServer;
        toServer.type = NetEvent::Type::Connected;
        toServer.peer = id;
        deliver(0, toServer, true, ignored);

        NetEvent toClient;
        toClient.type = NetEvent::Type::Connected;
        toClient.peer = 0;
        deliver(id, toClient, true, ignored);

        return *clients.back();
    }

    void LoopbackHub::disconnect(Transport& client)
    {
        for (auto& c : clients)
        {
            if (c.get() != &client or not c->connected)
                continue;

            c->connected = false;
            NetEvent e;
            e.type = NetEvent::Type::Disconnected;
            e.peer = c->self;
            double ignored = 0.0;
            deliver(0, e, true, ignored);
        }
    }

    LoopbackHub::Endpoint* LoopbackHub::endpoint(int id)
    {
        if (id == 0)
            return serverEnd.get();

        for (auto& c : clients)
            if (c->self == id)
                return c.get();

        return nullptr;
    }

    void LoopbackHub::deliver(int to, NetEvent event, bool reliable, double& lastReliable)
    {
        Endpoint* e = endpoint(to);

        if (not e)
            return;

        std::uniform_real_distribution<double> unit(0.0, 1.0);

        if (not reliable and unit(rng) < link.loss)
            return;

        double at = now + link.latencyMs + link.jitterMs * unit(rng);

        // Reliable traffic keeps its order
        if (reliable)
        {
            at = std::max(at, lastReliable);
            lastReliable = at;
        }

        // Kept sorted by arrival time
        Pending p{at, std::move(event)};
        auto it = std::upper_bound(e->inbox.begin(), e->inbox.end(), p.at, [](double t, const Pending& q) { return t < q.at; });
        e->inbox.insert(it, std::move(p));
    }

    void LoopbackHub::Endpoint::send(int peer, const std::vector<uint8_t>& data, bool reliable)
    {
        if (not connected)
            return;

        NetEvent e;
        e.type = NetEvent::Type::Message;
        e.peer = self;   // the receiver sees who sent it
        e.data = data;

        const int to = self == 0 ? peer : 0;

        // Client endpoints that left receive nothing more
        if (Endpoint* target = hub->endpoint(to); not target or not target->connected)
            return;

        hub->deliver(to, std::move(e), reliable, lastReliable);
    }

    bool LoopbackHub::Endpoint::poll(NetEvent& out)
    {
        if (inbox.empty() or inbox.front().at > hub->now)
            return false;

        out = std::move(inbox.front().event);
        inbox.pop_front();

        // A client sees everything as coming from the server
        if (self != 0)
            out.peer = 0;

        return true;
    }
}
