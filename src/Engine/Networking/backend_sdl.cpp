// SdlNetworkBackend.cpp
#include "backend_sdl.h"
#include <SDL_net.h>
#include <cstring>

#include "logger.h"

namespace pg
{
    namespace
    {
        inline constexpr const char * const DOM = "Backend SDL";
    }

    SdlNetworkBackend::SdlNetworkBackend(const NetworkConfig& cfg) : _cfg(cfg)
    {
        sockSet = SDLNet_AllocSocketSet(2);
        if (not sockSet)
        {
            LOG_ERROR(DOM, "Socket set couldn't be created !");
        }

        // Only set up server listener sockets here; client will connect in connectToServer()
        if (_cfg.isServer)
        {
            // TCP listener
            IPaddress addr{};
            SDLNet_ResolveHost(&addr, nullptr, _cfg.tcpPort);
            _listener = SDLNet_TCP_Open(&addr);

            if (sockSet)
                SDLNet_TCP_AddSocket(sockSet, _listener);

            // UDP socket
            _udpSock = SDLNet_UDP_Open(_cfg.udpLocalPort);
        }

        // Allocate a reusable UDP packet buffer
        _udpPkt = SDLNet_AllocPacket(MAX_PAYLOAD + sizeof(UdpHeader));
    }

    SdlNetworkBackend::~SdlNetworkBackend()
    {
        if (sockSet)
            SDLNet_FreeSocketSet(sockSet);

        if (_listener)
            SDLNet_TCP_Close(_listener);

        if (_tcpSock)
            SDLNet_TCP_Close(_tcpSock);

        if (_udpSock)
            SDLNet_UDP_Close(_udpSock);

        if (_udpPkt)
            SDLNet_FreePacket(_udpPkt);
    }

    TCPsocket SdlNetworkBackend::acceptTcpClient()
    {
        if (not _listener)
            return nullptr;

        return SDLNet_TCP_Accept(_listener);
    }

    bool SdlNetworkBackend::connectToServer()
    {
        if (_tcpSock)
            return true;  // already connected

        // Resolve and open TCP
        IPaddress serv {};
        if (SDLNet_ResolveHost(&serv, _cfg.peerAddress.c_str(), _cfg.tcpPort) < 0)
            return false;

        _tcpSock = SDLNet_TCP_Open(&serv);
        if (not _tcpSock)
            return false;

        // Open UDP on local port (0 = ephemeral)
        _udpSock = SDLNet_UDP_Open(_cfg.udpLocalPort);
        if (not _udpSock)
            return false;

        _isConnectedToServer = SDLNet_TCP_AddSocket(sockSet, _tcpSock) != -1;

        return _isConnectedToServer;
    }

    bool SdlNetworkBackend::sendTcp(TCPsocket sock, const std::vector<uint8_t>& data)
    {
        if (not sock)
            return false;

        const uint8_t* buf = data.data();
        int remaining = (int)data.size();

        while (remaining > 0)
        {
            int sent = SDLNet_TCP_Send(sock, buf, remaining);

            if (sent <= 0)
                return false;

            buf       += sent;
            remaining -= sent;
        }

        return true;
    }

    bool SdlNetworkBackend::sendTcp(const std::vector<uint8_t>& data)
    {
        if (_isConnectedToServer)
            return sendTcp(_tcpSock, data);
        else
            return false;
    }

    bool SdlNetworkBackend::sendUdp(const IPaddress& dest, const std::vector<uint8_t>& data)
    {
        if (not _udpPkt)
            return false;

        // prepare packet
        _udpPkt->address = dest;
        _udpPkt->len     = SDL_min((int)data.size(), _udpPkt->maxlen);

        std::memcpy(_udpPkt->data, data.data(), _udpPkt->len);

        return SDLNet_UDP_Send(_udpSock, -1, _udpPkt) == 1;
    }

    bool SdlNetworkBackend::receive(TCPsocket& tcpSock,
                                    IPaddress& srcUdp,
                                    std::vector<uint8_t>& out)
    {

        if (sockSet)
        {
            auto nbSocketReady = SDLNet_CheckSockets(sockSet, 0);

            if (nbSocketReady <= 0)
            {
                // LOG_INFO(DOM, "No data to read");
            }
        }

        // 1) Try UDP (non-blocking)
        if (_udpSock)
        {
            UDPpacket* pkt = _udpPkt;

            if (SDLNet_UDP_Recv(_udpSock, pkt) > 0)
            {
                srcUdp = pkt->address;
                out.assign(pkt->data, pkt->data + pkt->len);
                tcpSock = nullptr;

                return true;
            }
        }

        // 2) Try TCP (non-blocking)
        if (_tcpSock and SDLNet_SocketReady(_tcpSock))
        {
            // peek a chunk
            char buf[4096];
            int rec = SDLNet_TCP_Recv(_tcpSock, buf, sizeof(buf));
            if (rec > 0)
            {
                out.assign((uint8_t*)buf, (uint8_t*)buf + rec);
                tcpSock = _tcpSock;
                return true;
            }
            else
            {
                SDLNet_TCP_DelSocket(sockSet, _tcpSock);
                SDLNet_TCP_Close(_tcpSock);
                _tcpSock = nullptr;
                _isConnectedToServer = false;
                LOG_ERROR(DOM, "TCP receive failed, disconnected from server !");
            }
        }

        return false;
    }

    bool SdlNetworkBackend::receiveUdp(IPaddress& srcUdp, std::vector<uint8_t>& out)
    {
        if (_udpSock)
        {
            UDPpacket* pkt = _udpPkt;

            if (SDLNet_UDP_Recv(_udpSock, pkt) > 0)
            {
                srcUdp = pkt->address;
                out.assign(pkt->data, pkt->data + pkt->len);

                return true;
            }
        }

        return false;
    }

    bool SdlNetworkBackend::receiveTcp(TCPsocket& tcpSock, const SDLNet_SocketSet& socketSet, std::vector<uint8_t>& out, bool& socketClosed)
    {
        if (tcpSock and SDLNet_SocketReady(tcpSock))
        {
            // peek a chunk
            char buf[4096];
            int rec = SDLNet_TCP_Recv(tcpSock, buf, sizeof(buf));

            if (rec > 0)
            {
                out.assign((uint8_t*)buf, (uint8_t*)buf + rec);

                return true;
            }
            else
            {
                SDLNet_TCP_DelSocket(socketSet, tcpSock);
                SDLNet_TCP_Close(tcpSock);

                tcpSock = nullptr;
                socketClosed = false;
                LOG_ERROR(DOM, "TCP receive failed, disconnected from server !");
            }
        }

        return false;
    }

    bool SdlNetworkBackend::receiveTcp(std::vector<uint8_t>& out, bool& socketClosed)
    {
        if (_tcpSock and SDLNet_SocketReady(_tcpSock))
        {
            // peek a chunk
            char buf[4096];
            int rec = SDLNet_TCP_Recv(_tcpSock, buf, sizeof(buf));
            if (rec > 0)
            {
                out.assign((uint8_t*)buf, (uint8_t*)buf + rec);

                return true;
            }
            else
            {
                SDLNet_TCP_DelSocket(sockSet, _tcpSock);
                SDLNet_TCP_Close(_tcpSock);

                _tcpSock = nullptr;
                _isConnectedToServer = false;

                LOG_ERROR(DOM, "TCP receive failed, disconnected from server !");
            }
        }

        return false;
    }

    bool SdlNetworkBackend::recvUdpHeader(UdpHeader& hdr, IPaddress& src)
    {
        TCPsocket dummySock;
        std::vector<uint8_t> raw;

        if (not receive(dummySock, src, raw) or dummySock != nullptr)
        {
            return false;
        }

        if ((int)raw.size() < (int)sizeof(UdpHeader))
            return false;

        hdr = readHeader(raw.data());
        return true;
    }

}
