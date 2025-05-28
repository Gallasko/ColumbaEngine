// UDPSocketTransport.h
#pragma once

#include "inetwork_transport.h"
#include <SDL_net.h>
#include <vector>
#include <stdexcept>

class UDPSocketTransport : public INetworkTransport {
public:
    UDPSocketTransport(uint16_t localPort,
                       const std::string& peerAddr,
                       uint16_t peerPort)
    {
        // open a UDP socket (0 = ephemeral, or specific port)
        sock = SDLNet_UDP_Open(localPort);

        if (not sock)
        {
            throw std::runtime_error("SDLNet_UDP_Open failed");
        }

        // resolve peer host
        if (SDLNet_ResolveHost(&peer, peerAddr.c_str(), peerPort) == -1)
        {
            throw std::runtime_error("SDLNet_ResolveHost failed");
        }

        // allocate a packet buffer up to max size
        packet = SDLNet_AllocPacket(1500);

        if (not packet)
        {
            throw std::runtime_error("SDLNet_AllocPacket failed");
        }
    }

    ~UDPSocketTransport() override
    {
        if (packet)
            SDLNet_FreePacket(packet);

        if (sock)
            SDLNet_UDP_Close(sock);
    }

    bool sendUnreliable(const Packet& pkt) override
    {
        // copy payload into SDLNet packet
        packet->address = peer;
        packet->len     = static_cast<int>(pkt.data.size());

        if (packet->len > packet->maxlen)
            return false;

        memcpy(packet->data, pkt.data.data(), packet->len);

        return SDLNet_UDP_Send(sock, -1, packet) == 1;
    }

    bool sendReliable(const Packet& /*pkt*/) override
    {
        // no reliable on UDP
        return false;
    }

    bool receive(Packet& out) override
    {
        // non-blocking receive
        int num = SDLNet_UDP_Recv(sock, packet);

        if (num <= 0)
            return false;

        out.data.resize(packet->len);

        memcpy(out.data.data(), packet->data, packet->len);

        // NOTE: if you embed entityId/compType in the packet you must parse them here
        return true;
    }

private:
    UDPsocket sock   = nullptr;
    IPaddress peer{};
    UDPpacket* packet = nullptr;
};
