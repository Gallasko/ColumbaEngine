// INetworkBackend.h
#pragma once

#include <cstdint>
#include <vector>
#include <functional>
#include <SDL_net.h>
#include "common.h"   // Packet, UdpHeader, writeHeader, readHeader


namespace pg
{
    // Pure-virtual backend that your ECS system will call
    struct INetworkBackend
    {
        virtual ~INetworkBackend() {}

        // Server-side: accept one incoming TCP connection; return its socket or nullptr
        virtual TCPsocket acceptTcpClient() = 0;

        // Client-side: try to connect; return true on success
        virtual bool connectToServer() = 0;

        bool _isConnectedToServer = false;
        inline bool isConnectedToServer() const { return _isConnectedToServer; }

        // Send raw packet (header+payload)
        virtual bool sendUdp(const IPaddress& dest, const std::vector<uint8_t>& data) = 0;
        virtual bool sendTcp(TCPsocket sock, const std::vector<uint8_t>& data) = 0;
        virtual bool sendTcp(const std::vector<uint8_t>& data) = 0;

        // Poll for incoming packets
        //   If tcpSock!=nullptr, it reads exactly one TCP packet (or returns false)
        //   If tcpSock==nullptr, it reads one UDP packet (keeping sourceAddr).
        virtual bool receive(TCPsocket& tcpSock, IPaddress& srcUdp, std::vector<uint8_t>& out) = 0;

        virtual bool receiveUdp(IPaddress& srcUdp, std::vector<uint8_t>& out) = 0;
        virtual bool receiveTcp(TCPsocket& tcpSock, std::vector<uint8_t>& out, bool& socketClosed) = 0;
        virtual bool receiveTcp(std::vector<uint8_t>& out, bool& socketClosed) = 0;

        // Helpers to pull out your UDP header types
        // virtual bool recvUdpHeader(UdpHeader& hdr, IPaddress& src) = 0;
    };

}
