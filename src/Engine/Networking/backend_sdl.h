// SdlNetworkBackend.h
#pragma once

#include "ibackend.h"
#include "network_config.h"
#include <SDL_net.h>

#include <atomic>
#include <map>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace pg
{
    /**
     * TCP and UDP over SDL_net.
     *
     * TCP is a byte stream: every sendTcp is written as a 4-byte big-endian length then the bytes,
     * and each socket keeps a receive buffer, so receiveTcp hands back whole messages however the
     * stream was cut. UDP sources are read as numeric addresses (no reverse lookup), and resolved
     * destinations are kept. The client connects on a thread of its own (connecting can take seconds).
     */
    class SdlNetworkBackend : public INetworkBackend
    {
    public:
        explicit SdlNetworkBackend(const NetworkConfig& cfg);
        virtual ~SdlNetworkBackend() override;

        bool isReady() const override { return ready; }

        // Server-side: accept one new TCP client (or return nullptr)
        SocketHandle acceptTcpClient() override;

        // Client-side: connect to cfg.peerAddress/cfg.tcpPort (blocking)
        bool connectToServer() override;
        void startConnect() override;
        int connectStatus() override;
        virtual void disconnectFromServer() override;

        virtual void closeTcp(SocketHandle sock) override;

        bool sendUdp(const IpEndpoint& dest, const NetPayload& data) override;
        bool sendTcp(SocketHandle sock, const NetPayload& data) override;
        bool sendTcp(const NetPayload& data) override;

        // One message, UDP first then TCP (client side)
        bool receive(SocketHandle& tcpSock, IpEndpoint& srcUdp, NetPayload& out) override;

        virtual bool receiveUdp(IpEndpoint& srcUdp, NetPayload& out) override;
        virtual bool receiveTcp(SocketHandle& tcpSock, NetPayload& out, bool& socketClosed) override;
        virtual bool receiveTcp(NetPayload& out, bool& socketClosed) override;

        virtual void writeU16BE(uint8_t* buf, uint16_t v) override { SDLNet_Write16(v, buf); }
        virtual void writeU32BE(uint8_t* buf, uint32_t v) override { SDLNet_Write32(v, buf); }
        virtual void writeU64BE(uint8_t* buf, uint64_t v) override
        {
            writeU32BE(buf,     uint32_t(v >> 32));
            writeU32BE(buf + 4, uint32_t(v & 0xFFFFFFFF));
        }

        virtual uint16_t readU16BE(const uint8_t* buf) override { return SDLNet_Read16(const_cast<uint8_t*>(buf)); }
        virtual uint32_t readU32BE(const uint8_t* buf) override { return SDLNet_Read32(const_cast<uint8_t*>(buf)); }
        virtual uint64_t readU64BE(const uint8_t* buf) override
        {
            uint64_t hi = readU32BE(buf);
            uint64_t lo = readU32BE(buf + 4);
            return (hi << 32) | lo;
        }

        /** The largest TCP message accepted: a bigger length means a broken or hostile peer, and the socket is closed. */
        static constexpr uint32_t MAX_FRAME = 16 * 1024 * 1024;

    private:
        /** Reads what a socket has and returns one whole frame if there is one; closed when the peer left or broke the framing. */
        bool readFrame(TCPsocket sock, NetPayload& out, bool& closed);
        bool openClientUdp();
        void joinConnectThread();

        NetworkConfig   _cfg;
        bool            ready = false;

        // Server sockets
        TCPsocket       _listener = nullptr;
        UDPsocket       _udpSock  = nullptr;

        // Client socket
        TCPsocket       _tcpSock  = nullptr;

        UDPpacket*      _udpPkt   = nullptr;

        // Every TCP socket in one set (the listener is not in it)
        SDLNet_SocketSet sockSet  = nullptr;
        int              sockSetSize = 0;
        int              socketsInSet = 0;

        std::unordered_map<TCPsocket, NetPayload> rxBuffers;
        std::map<std::string, IPaddress> resolved;

        // Background connect
        std::thread       connectThread;
        std::atomic<int>  connectState{-1};   // 1 done, 0 trying, -1 idle or failed
        TCPsocket         connectedSock = nullptr;
    };
}
