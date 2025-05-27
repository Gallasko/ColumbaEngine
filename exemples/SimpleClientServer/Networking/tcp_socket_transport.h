// TCPSocketTransport.h
#pragma once

#include "INetworkTransport.h"
#include <SDL2/SDL_net.h>
#include <thread>
#include <stdexcept>

class TCPSocketTransport : public INetworkTransport
{
public:
    TCPSocketTransport(const std::string& addr, uint16_t port, bool isServer)
    {
        // ensure SDL_net is initialized
        if (SDLNet_Init() == -1)
        {
            throw std::runtime_error("SDLNet_Init failed");
        }

        if (isServer)
        {
            // open a listening socket on port
            SDLNet_ResolveHost(&tcpAddr, nullptr, port);
            listener = SDLNet_TCP_Open(&tcpAddr);
            if (!listener) {
                throw std::runtime_error("SDLNet_TCP_Open (server) failed");
            }
            // accept asynchronously
            std::thread([this]() {
                TCPsocket clientSock = nullptr;
                while (!clientSock) {
                    clientSock = SDLNet_TCP_Accept(listener);
                    SDL_Delay(10);
                }
                // close listener, use clientSock from now on
                SDLNet_TCP_Close(listener);
                listener = nullptr;
                sock     = clientSock;
            }).detach();
        }
        else
        {
            // client: connect to remote
            SDLNet_ResolveHost(&tcpAddr, addr.c_str(), port);
            sock = SDLNet_TCP_Open(&tcpAddr);

            if (not sock)
            {
                throw std::runtime_error("SDLNet_TCP_Open (client) failed");
            }
        }

        // make non-blocking
        if (sock) SDLNet_TCP_SetNonBlocking(sock);
    }

    ~TCPSocketTransport() override
    {
        if (sock)     SDLNet_TCP_Close(sock);
        if (listener) SDLNet_TCP_Close(listener);

        SDLNet_Quit();
    }

    bool sendReliable(const Packet& pkt) override {
        // prepend length if you need framing; here we assume raw payload
        int total = 0, len = static_cast<int>(pkt.data.size());
        const char* buffer = reinterpret_cast<const char*>(pkt.data.data());
        while (total < len) {
            int sent = SDLNet_TCP_Send(sock, buffer + total, len - total);
            if (sent <= 0) return false;
            total += sent;
        }
        return true;
    }

    bool sendUnreliable(const Packet& /*pkt*/) override {
        // no UDP semantics on TCP
        return false;
    }

    bool receive(Packet& out) override {
        // read up to buffer size
        char buffer[1500];
        int rec = SDLNet_TCP_Recv(sock, buffer, sizeof(buffer));
        if (rec <= 0) return false;
        out.data.assign(buffer, buffer + rec);
        return true;
    }

private:
    TCPsocket listener = nullptr;
    TCPsocket sock     = nullptr;
    IPaddress tcpAddr{};
};
