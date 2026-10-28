#include "stdafx.h"

#include "backend_sdl.h"
#include <SDL_net.h>
#include <cstring>

#include "logger.h"

namespace pg
{
    namespace
    {
        inline constexpr const char * const DOM = "Backend SDL";

        /** SDL_net must be started before any socket (WSAStartup on Windows, SIGPIPE ignored on Linux). */
        bool ensureSdlNet()
        {
            static bool started = SDLNet_Init() == 0;

            if (not started)
                LOG_ERROR(DOM, "SDLNet_Init failed: " << SDLNet_GetError());

            return started;
        }

        std::string numericHost(const IPaddress& a)
        {
            const uint32_t ip = SDLNet_Read32(const_cast<Uint32*>(&a.host));
            return std::to_string((ip >> 24) & 0xFF) + "." + std::to_string((ip >> 16) & 0xFF) + "." + std::to_string((ip >> 8) & 0xFF) + "." + std::to_string(ip & 0xFF);
        }
    }

    SdlNetworkBackend::SdlNetworkBackend(const NetworkConfig& cfg) : _cfg(cfg)
    {
        if (not ensureSdlNet())
            return;

        // Room for every client (the server) or the one connection (the client)
        sockSetSize = std::max<int>(2, static_cast<int>(_cfg.defaultSystemFlags.socketSetSize));
        sockSet = SDLNet_AllocSocketSet(sockSetSize);

        if (not sockSet)
        {
            LOG_ERROR(DOM, "Socket set couldn't be created: " << SDLNet_GetError());
            return;
        }

        _udpPkt = SDLNet_AllocPacket(MAX_PAYLOAD);

        if (not _udpPkt)
        {
            LOG_ERROR(DOM, "UDP packet couldn't be allocated");
            return;
        }

        if (_cfg.isServer)
        {
            IPaddress addr{};

            if (SDLNet_ResolveHost(&addr, nullptr, _cfg.tcpPort) < 0 or not (_listener = SDLNet_TCP_Open(&addr)))
            {
                LOG_ERROR(DOM, "Cannot listen on TCP port " << _cfg.tcpPort << ": " << SDLNet_GetError());
                return;
            }

            if (not (_udpSock = SDLNet_UDP_Open(_cfg.udpLocalPort)))
            {
                LOG_ERROR(DOM, "Cannot open UDP port " << _cfg.udpLocalPort << ": " << SDLNet_GetError());
                return;
            }
        }

        ready = true;
    }

    SdlNetworkBackend::~SdlNetworkBackend()
    {
        joinConnectThread();

        if (connectedSock)
            SDLNet_TCP_Close(connectedSock);

        for (auto& [sock, buffer] : rxBuffers)
        {
            if (sock != _tcpSock)
            {
                if (sockSet)
                    SDLNet_TCP_DelSocket(sockSet, sock);
                SDLNet_TCP_Close(sock);
            }
        }

        disconnectFromServer();

        if (_listener)
            SDLNet_TCP_Close(_listener);

        if (_udpSock)
            SDLNet_UDP_Close(_udpSock);

        if (sockSet)
            SDLNet_FreeSocketSet(sockSet);

        if (_udpPkt)
            SDLNet_FreePacket(_udpPkt);
    }

    // ---- Connections -----------------------------------------------------------------------------

    SocketHandle SdlNetworkBackend::acceptTcpClient()
    {
        if (not _listener)
            return nullptr;

        TCPsocket newSock = SDLNet_TCP_Accept(_listener);

        if (not newSock)
            return nullptr;

        if (socketsInSet >= sockSetSize or SDLNet_TCP_AddSocket(sockSet, newSock) == -1)
        {
            LOG_WARNING(DOM, "Too many connections: refused one");
            SDLNet_TCP_Close(newSock);
            return nullptr;
        }

        ++socketsInSet;
        rxBuffers[newSock];
        return newSock;
    }

    bool SdlNetworkBackend::openClientUdp()
    {
        if (_udpSock)
            return true;

        _udpSock = SDLNet_UDP_Open(_cfg.udpLocalPort);

        if (not _udpSock)
            LOG_ERROR(DOM, "Cannot open the client UDP socket: " << SDLNet_GetError());

        return _udpSock != nullptr;
    }

    bool SdlNetworkBackend::connectToServer()
    {
        if (_isConnectedToServer)
            return true;

        IPaddress serv{};

        if (SDLNet_ResolveHost(&serv, _cfg.peerAddress.c_str(), _cfg.tcpPort) < 0)
            return false;

        TCPsocket sock = SDLNet_TCP_Open(&serv);

        if (not sock)
            return false;

        if (not openClientUdp() or SDLNet_TCP_AddSocket(sockSet, sock) == -1)
        {
            SDLNet_TCP_Close(sock);
            return false;
        }

        _tcpSock = sock;
        rxBuffers[_tcpSock];
        _isConnectedToServer = true;
        return true;
    }

    void SdlNetworkBackend::joinConnectThread()
    {
        if (connectThread.joinable())
            connectThread.join();
    }

    void SdlNetworkBackend::startConnect()
    {
        if (_isConnectedToServer or connectState == 0)
            return;

        joinConnectThread();
        connectState = 0;

        // Resolving and connecting block (seconds for a host that does not answer): not on the game's thread
        const std::string host = _cfg.peerAddress;
        const uint16_t port = _cfg.tcpPort;

        connectThread = std::thread([this, host, port]() {
            IPaddress serv{};
            TCPsocket sock = nullptr;

            if (SDLNet_ResolveHost(&serv, host.c_str(), port) == 0)
                sock = SDLNet_TCP_Open(&serv);

            connectedSock = sock;
            connectState = sock ? 1 : -1;
        });
    }

    int SdlNetworkBackend::connectStatus()
    {
        if (_isConnectedToServer)
            return 1;

        const int state = connectState;

        if (state != 1)
            return state;

        // Done on the other thread: finish here, on the caller's
        joinConnectThread();
        TCPsocket sock = connectedSock;
        connectedSock = nullptr;
        connectState = -1;

        if (not openClientUdp() or SDLNet_TCP_AddSocket(sockSet, sock) == -1)
        {
            SDLNet_TCP_Close(sock);
            return -1;
        }

        _tcpSock = sock;
        rxBuffers[_tcpSock];
        _isConnectedToServer = true;
        return 1;
    }

    void SdlNetworkBackend::disconnectFromServer()
    {
        if (_tcpSock)
        {
            SDLNet_TCP_DelSocket(sockSet, _tcpSock);
            SDLNet_TCP_Close(_tcpSock);
            rxBuffers.erase(_tcpSock);
            _tcpSock = nullptr;
        }

        // The client's UDP socket goes with the connection; the server's stays bound
        if (_udpSock and not _cfg.isServer)
        {
            SDLNet_UDP_Close(_udpSock);
            _udpSock = nullptr;
        }

        _isConnectedToServer = false;
    }

    void SdlNetworkBackend::closeTcp(SocketHandle handle)
    {
        auto sock = static_cast<TCPsocket>(handle);

        if (not sock)
            return;

        if (rxBuffers.erase(sock))
        {
            SDLNet_TCP_DelSocket(sockSet, sock);
            --socketsInSet;
        }

        SDLNet_TCP_Close(sock);
    }

    // ---- Sending ---------------------------------------------------------------------------------

    bool SdlNetworkBackend::sendTcp(SocketHandle handle, const NetPayload& data)
    {
        if (not handle or data.size() > MAX_FRAME)
            return false;

        // Length then bytes, in one write
        NetPayload frame(4 + data.size());
        SDLNet_Write32(static_cast<Uint32>(data.size()), frame.data());

        if (not data.empty())
            std::memcpy(frame.data() + 4, data.data(), data.size());

        const int sent = SDLNet_TCP_Send(static_cast<TCPsocket>(handle), frame.data(), static_cast<int>(frame.size()));
        return sent == static_cast<int>(frame.size());
    }

    bool SdlNetworkBackend::sendTcp(const NetPayload& data)
    {
        return _isConnectedToServer and sendTcp(_tcpSock, data);
    }

    bool SdlNetworkBackend::sendUdp(const IpEndpoint& dest, const NetPayload& data)
    {
        if (not _udpPkt or not _udpSock or data.size() > static_cast<size_t>(_udpPkt->maxlen))
            return false;

        // Resolved once per destination: a lookup per datagram would stall the sender
        const std::string key = dest.host + ":" + std::to_string(dest.port);
        auto it = resolved.find(key);

        if (it == resolved.end())
        {
            IPaddress addr{};

            if (SDLNet_ResolveHost(&addr, dest.host.c_str(), dest.port) < 0)
                return false;

            it = resolved.emplace(key, addr).first;
        }

        _udpPkt->address = it->second;
        _udpPkt->len = static_cast<int>(data.size());
        std::memcpy(_udpPkt->data, data.data(), data.size());

        return SDLNet_UDP_Send(_udpSock, -1, _udpPkt) == 1;
    }

    // ---- Receiving -------------------------------------------------------------------------------

    bool SdlNetworkBackend::readFrame(TCPsocket sock, NetPayload& out, bool& closed)
    {
        auto& rx = rxBuffers[sock];

        auto extract = [&]() {
            if (rx.size() < 4)
                return false;

            const uint32_t len = SDLNet_Read32(rx.data());

            if (len > MAX_FRAME)
            {
                LOG_ERROR(DOM, "A peer sent a " << len << " byte message: connection closed");
                closed = true;
                return false;
            }

            if (rx.size() < 4 + static_cast<size_t>(len))
                return false;

            out.assign(rx.begin() + 4, rx.begin() + 4 + len);
            rx.erase(rx.begin(), rx.begin() + 4 + len);
            return true;
        };

        // A whole frame may already be waiting from an earlier read
        if (extract() or closed)
            return not closed;

        SDLNet_CheckSockets(sockSet, 0);

        if (not SDLNet_SocketReady(sock))
            return false;

        uint8_t buf[MAX_PAYLOAD];
        const int rec = SDLNet_TCP_Recv(sock, buf, sizeof(buf));

        if (rec <= 0)
        {
            closed = true;
            return false;
        }

        rx.insert(rx.end(), buf, buf + rec);
        return extract();
    }

    bool SdlNetworkBackend::receiveUdp(IpEndpoint& srcUdp, NetPayload& out)
    {
        if (not _udpSock or not _udpPkt)
            return false;

        if (SDLNet_UDP_Recv(_udpSock, _udpPkt) <= 0)
            return false;

        out.assign(_udpPkt->data, _udpPkt->data + _udpPkt->len);

        // Numbers, not a name: a reverse lookup per datagram can take seconds
        srcUdp.host = numericHost(_udpPkt->address);
        srcUdp.port = SDLNet_Read16(&_udpPkt->address.port);
        return true;
    }

    bool SdlNetworkBackend::receive(SocketHandle& tcpSock, IpEndpoint& srcUdp, NetPayload& out)
    {
        if (receiveUdp(srcUdp, out))
        {
            tcpSock = nullptr;
            return true;
        }

        bool closed = false;

        if (receiveTcp(out, closed))
        {
            tcpSock = _tcpSock;
            return true;
        }

        return false;
    }

    bool SdlNetworkBackend::receiveTcp(SocketHandle& tcpHandle, NetPayload& out, bool& socketClosed)
    {
        auto sock = static_cast<TCPsocket>(tcpHandle);

        if (not sock or not rxBuffers.count(sock))
            return false;

        return readFrame(sock, out, socketClosed);
    }

    bool SdlNetworkBackend::receiveTcp(NetPayload& out, bool& socketClosed)
    {
        if (not _tcpSock)
            return false;

        const bool got = readFrame(_tcpSock, out, socketClosed);

        if (socketClosed)
        {
            LOG_WARNING(DOM, "Connection to the server closed");
            disconnectFromServer();
        }

        return got;
    }
}
