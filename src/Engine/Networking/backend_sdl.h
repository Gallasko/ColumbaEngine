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
        TCPsocket acceptTcpClient() override;

        // Client‐side: connect to server using cfg.peerAddress/cfg.tcpPort
        bool connectToServer() override;

        // Send raw data
        bool sendUdp(const IPaddress& dest, const std::vector<uint8_t>& data) override;
        bool sendTcp(TCPsocket sock, const std::vector<uint8_t>& data) override;
        bool sendTcp(const std::vector<uint8_t>& data) override;

        // Receive exactly one packet: either UDP or TCP (non-blocking)
        //  - If you get a UDP packet, tcpSock=nullptr and srcUdp is set.
        //  - If you get a TCP packet, tcpSock is the socket and srcUdp is untouched.
        bool receive(TCPsocket& tcpSock, IPaddress& srcUdp, std::vector<uint8_t>& out) override;

        // Helper: just read one UDP header into hdr and src
        bool recvUdpHeader(UdpHeader& hdr, IPaddress& src) override;

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

        // Internal helpers
        UDPpacket* allocUdpPacket();
    };
}

