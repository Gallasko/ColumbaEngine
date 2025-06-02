// SdlNetworkBackend.h
#pragma once

#include "ibackend.h"
#include "network_config.h"
#include <SDL_net.h>
#include <vector>

namespace pg
{

    class SdlNetworkBackend : public INetworkBackend
    {
    public:
        explicit SdlNetworkBackend(const NetworkConfig& cfg);
        virtual ~SdlNetworkBackend() override;

        // Server‐side: accept one new TCP client (or return nullptr)
        SocketHandle acceptTcpClient() override;

        // Client‐side: connect to server using cfg.peerAddress/cfg.tcpPort
        bool connectToServer() override;

        // Send raw data
        bool sendUdp(const IpEndpoint& dest, const NetPayload& data) override;
        bool sendTcp(SocketHandle sock, const NetPayload& data) override;
        bool sendTcp(const NetPayload& data) override;

        // Receive exactly one packet: either UDP or TCP (non-blocking)
        //  - If you get a UDP packet, tcpSock=nullptr and srcUdp is set.
        //  - If you get a TCP packet, tcpSock is the socket and srcUdp is untouched.
        bool receive(SocketHandle& tcpSock, IpEndpoint& srcUdp, NetPayload& out) override;

        virtual bool receiveUdp(IpEndpoint& srcUdp, NetPayload& out) override;
        virtual bool receiveTcp(SocketHandle& tcpSock, NetPayload& out, bool& socketClosed) override;
        virtual bool receiveTcp(NetPayload& out, bool& socketClosed) override;

        inline virtual void writeU16BE(uint8_t* buf, uint16_t v) override { SDLNet_Write16(v, buf); }
        inline virtual void writeU32BE(uint8_t* buf, uint32_t v) override { SDLNet_Write32(v, buf); }
        inline virtual void writeU64BE(uint8_t* buf, uint64_t v) override
        {
            // split into two 32-bit
            writeU32BE(buf,     uint32_t(v >> 32));
            writeU32BE(buf + 4, uint32_t(v & 0xFFFFFFFF));
        }

        inline virtual uint16_t readU16BE(const uint8_t* buf) override { return SDLNet_Read16(const_cast<uint8_t*>(buf)); }
        inline virtual uint32_t readU32BE(const uint8_t* buf) override { return SDLNet_Read32(const_cast<uint8_t*>(buf)); }
        inline virtual uint64_t readU64BE(const uint8_t* buf) override
        {
            uint64_t hi = readU32BE(buf);
            uint64_t lo = readU32BE(buf + 4);
            return (hi << 32) | lo;
        }

        // Helper: just read one UDP header into hdr and src
        // bool recvUdpHeader(UdpHeader& hdr, IPaddress& src) override;

    private:
        NetworkConfig   _cfg;

        // Server sockets
        TCPsocket       _listener = nullptr;
        UDPsocket       _udpSock  = nullptr;

        // Client sockets
        TCPsocket       _tcpSock  = nullptr;

        // Shared receive buffer
        UDPpacket*      _udpPkt   = nullptr;

        SDLNet_SocketSet sockSet  = nullptr;

        std::unordered_map<TCPsocket, SDLNet_SocketSet> sockSetsMap;
        std::vector<SDLNet_SocketSet> sockSets;

        // Internal helpers
        UDPpacket* allocUdpPacket();
    };
}

