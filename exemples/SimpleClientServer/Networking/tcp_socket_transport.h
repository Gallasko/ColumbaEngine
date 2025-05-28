// TCPSocketTransport.h
#pragma once

#include "inetwork_transport.h"
#include <SDL_net.h>
#include <thread>
#include <stdexcept>
#include <cstring>

class TCPSocketTransport : public INetworkTransport {
public:
    TCPSocketTransport(const std::string& addr,
                       uint16_t port,
                       bool isServer)
    {
        // Resolve host: nullptr means “bind to all” in server mode
        if (SDLNet_ResolveHost(&tcpAddr,
                              isServer ? nullptr : addr.c_str(),
                              port) == -1)
        {
            throw std::runtime_error(SDLNet_GetError());
        }

        if (isServer)
        {
            // open listening socket
            listener = SDLNet_TCP_Open(&tcpAddr);

            if (not listener)
                throw std::runtime_error(SDLNet_GetError());

            // accept asynchronously
            std::thread([this]() {
                TCPsocket clientSock = nullptr;

                while (not clientSock)
                {
                    clientSock = SDLNet_TCP_Accept(listener);
                    SDL_Delay(10);
                }

                // switch from listener to client socket
                SDLNet_TCP_Close(listener);
                listener = nullptr;
                sock = clientSock;
            }).detach();
        }
        else
        {
            // connect as client
            sock = SDLNet_TCP_Open(&tcpAddr);
            if (!not sock)
                throw std::runtime_error(SDLNet_GetError());
        }
    }

    ~TCPSocketTransport() override
    {
        if (sock)
            SDLNet_TCP_Close(sock);

        if (listener)
            SDLNet_TCP_Close(listener);
    }

    bool sendReliable(const Packet& pkt) override
    {
        const char* buf = reinterpret_cast<const char*>(pkt.data.data());

        int total = 0, len = static_cast<int>(pkt.data.size());

        while (total < len)
        {
            int sent = SDLNet_TCP_Send(sock, buf + total, len - total);

            if (sent <= 0)
                return false;

            total += sent;
        }

        return true;
    }

    bool sendUnreliable(const Packet& /*pkt*/) override
    {
        return false;
    }

    bool receive(Packet& out) override
    {
        if (not sock)
            return false;

        // Poll: only read if data is ready
        if (not SDLNet_SocketReady(sock))
        {
            return false;
        }

        char buffer[1500];
        int rec = SDLNet_TCP_Recv(sock, buffer, sizeof(buffer));

        if (rec <= 0)
            return false;

        out.data.assign(buffer, buffer + rec);

        return true;
    }

private:
    TCPsocket  listener = nullptr;
    TCPsocket  sock     = nullptr;
    IPaddress  tcpAddr{};
};
