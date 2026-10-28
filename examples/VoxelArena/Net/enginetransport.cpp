#include "stdafx.h"

#include "enginetransport.h"

#include "Networking/network_system.h"

namespace arena::net
{
    EngineTransport::EngineTransport(pg::NetworkSystem* s) : system(s)
    {
    }

    void EngineTransport::send(int peer, const std::vector<uint8_t>& data, bool reliable)
    {
        if (system->isServer())
            system->send(static_cast<uint32_t>(peer), data, reliable);
        else
            system->sendToServer(data, reliable);
    }

    bool EngineTransport::poll(NetEvent& out)
    {
        pg::NetIncoming in;

        if (not system->poll(in))
            return false;

        switch (in.kind)
        {
            case pg::NetIncoming::Kind::Connected: out.type = NetEvent::Type::Connected; break;
            case pg::NetIncoming::Kind::Disconnected: out.type = NetEvent::Type::Disconnected; break;
            case pg::NetIncoming::Kind::Message: out.type = NetEvent::Type::Message; break;
        }

        out.peer = system->isServer() ? static_cast<int>(in.clientId) : 0;
        out.data = std::move(in.data);
        return true;
    }

    float EngineTransport::rttMs(int peer) const
    {
        return system->isServer() ? system->rttMs(static_cast<uint32_t>(peer)) : 0.0f;
    }
}
