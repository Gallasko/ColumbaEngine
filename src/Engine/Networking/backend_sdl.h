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
     * stream was cut. Sending never blocks: what the system does not take at once waits in the
     * socket's send queue (flush sends more of it), and a peer that lets more than MAX_SEND_QUEUE
     * pile up is cut off, as one that stops reading would otherwise stall everyone.
     * UDP sources are read as numeric addresses (no reverse lookup), and resolved destinations are
     * kept. The client connects on a thread of its own (connecting can take seconds).
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
        std::string tcpPeerHost(SocketHandle sock) override;
        void flush() override;

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

        /** Bytes waiting to go to one peer, at most: more and the peer is not reading, and is cut off. */
        static constexpr size_t MAX_SEND_QUEUE = 2 * MAX_FRAME + 1024 * 1024;

        /** Destinations kept resolved, at most (forgotten all at once beyond: each costs a lookup again). */
        static constexpr size_t MAX_RESOLVED = 256;

    private:
        /** Reads what a socket has and returns one whole frame if there is one; closed when the peer left or broke the framing. */
        bool readFrame(TCPsocket sock, NetPayload& out, bool& closed);
        bool openClientUdp();
        void joinConnectThread();

        /** Sends what the socket's queue holds, as far as the system takes it now. False: the connection broke. */
        bool flushSocket(TCPsocket sock);
        void forget(TCPsocket sock);

        struct SendQueue
        {
            NetPayload data;
            size_t sent = 0;     // bytes of data already gone (dropped from the front now and then)
            bool broken = false; // a send failed, or the peer let too much pile up: reads say it closed
        };

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
        std::unordered_map<TCPsocket, SendQueue> txQueues;
        std::map<std::string, IPaddress> resolved;

        // Background connect
        std::thread       connectThread;
        std::atomic<int>  connectState{-1};   // 1 done, 0 trying, -1 idle or failed
        TCPsocket         connectedSock = nullptr;
    };
}
