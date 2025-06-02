// INetworkBackend.h
#pragma once

#include <cstdint>
#include <vector>
#include <functional>
#include "common.h"   // Packet, UdpHeader, writeHeader, readHeader


namespace pg
{
    // Pure-virtual backend that your ECS system will call
    struct INetworkBackend
    {
        virtual ~INetworkBackend() {}

        // Server-side: accept one incoming TCP connection; return its socket or nullptr
        virtual SocketHandle acceptTcpClient() = 0;

        // Client-side: try to connect; return true on success
        virtual bool connectToServer() = 0;

        bool _isConnectedToServer = false;
        inline bool isConnectedToServer() const { return _isConnectedToServer; }

        // Send raw packet (header+payload)
        virtual bool sendUdp(const IpEndpoint& dest, const NetPayload& data) = 0;
        virtual bool sendTcp(SocketHandle sock, const NetPayload& data) = 0;
        virtual bool sendTcp(const NetPayload& data) = 0;

        // Poll for incoming packets
        //   If tcpSock!=nullptr, it reads exactly one TCP packet (or returns false)
        //   If tcpSock==nullptr, it reads one UDP packet (keeping sourceAddr).
        virtual bool receive(SocketHandle& tcpSock, IpEndpoint& srcUdp, NetPayload& out) = 0;

        virtual bool receiveUdp(IpEndpoint& srcUdp, NetPayload& out) = 0;
        virtual bool receiveTcp(SocketHandle& tcpSock, NetPayload& out, bool& socketClosed) = 0;
        virtual bool receiveTcp(NetPayload& out, bool& socketClosed) = 0;

        virtual void writeU16BE(uint8_t* buf, uint16_t v) = 0;
        virtual void writeU32BE(uint8_t* buf, uint32_t v) = 0;
        virtual void writeU64BE(uint8_t* buf, uint64_t v) = 0;

        virtual uint16_t readU16BE(const uint8_t* buf) = 0;
        virtual uint32_t readU32BE(const uint8_t* buf) = 0;
        virtual uint64_t readU64BE(const uint8_t* buf) = 0;

        // Helpers to pull out your UDP header types
        // virtual bool recvUdpHeader(UdpHeader& hdr, IPaddress& src) = 0;
    };

}
