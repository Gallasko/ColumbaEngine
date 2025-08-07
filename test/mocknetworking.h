// MockBackend.h
#pragma once

#include "Networking/ibackend.h"    // your INetworkBackend definition
#include <deque>
#include <functional>
#include <utility>
#include <vector>

namespace pg
{

/// A lightweight struct to represent one “incoming TCP” event for receive().
///   - sock:         the fake SocketHandle that “sent” this frame
///   - payload:      the NetPayload we want receive(...) to hand back
///   - socketClosed: if true, simulates “the peer closed the socket”
///
/// Once receive() or receiveTcp(...) sees one of these, it pops it and returns true.
struct TcpReceiveEvent
{
    SocketHandle sock;
    NetPayload   payload;
    bool         socketClosed;

    TcpReceiveEvent() = default;
    TcpReceiveEvent(SocketHandle s, NetPayload p, bool closed)
        : sock(s), payload(std::move(p)), socketClosed(closed) {}
};

/// A lightweight struct to represent one “incoming UDP” event for receiveUdp().
struct UdpReceiveEvent
{
    IpEndpoint src;
    NetPayload payload;

    UdpReceiveEvent() = default;
    UdpReceiveEvent(const IpEndpoint& s, NetPayload p)
        : src(s), payload(std::move(p)) {}
};

///
/// MockBackend
///
///   - Implements every pure‐virtual of INetworkBackend by storing calls
///     into small vectors/queues; you can inspect `sentTcpPackets` /
///     `sentUdpPackets` afterward in your test to see exactly what was “sent.”
///   - You can also push “fake receives” into `tcpRecvQueue` or
///     `udpRecvQueue` so that receive()/receiveTcp()/receiveUdp() returns them
///     in FIFO order.
///   - For connectToServer() and the various sendTcp/sendUdp overloads, you can
///     override the default “always succeed” by setting the corresponding
///     std::function<bool(...)> (e.g. `connectToServerFn`) from your test.
///   - readUxxBE / writeUxxBE are implemented in‐place (big‐endian) so that
///     NetworkSystem’s packet‐parsing code can run normally.
///
class MockBackend : public INetworkBackend
{
public:
    MockBackend()
        : connectToServerFn([]() { return true; })
        , sendUdpFn([](const IpEndpoint&, const NetPayload&) { return true; })
        , sendTcpSockFn([](SocketHandle, const NetPayload&) { return true; })
        , sendTcpNoSockFn([](const NetPayload&) { return true; })
        , receiveTcpFn([](SocketHandle&, NetPayload&, bool&) { return false; })
        , receiveTcpNoSockFn([](NetPayload&, bool&) { return false; })
    {}

    ~MockBackend() override = default;

    //──────────────────────────────────────────────────────────────────────────
    // 1) acceptTcpClient()
    //
    //    By default returns `nullptr`. In your test you can do:
    //       backend.acceptTcpClientFn = []() { return reinterpret_cast<SocketHandle>(0x1234); };
    //
    std::function<SocketHandle()> acceptTcpClientFn = []() { return nullptr; };
    SocketHandle acceptTcpClient() override
    {
        return acceptTcpClientFn();
    }

    //──────────────────────────────────────────────────────────────────────────
    // 2) connectToServer()
    //
    //    By default returns `true`. In your test you can do:
    //       backend.connectToServerFn = []() { return false; };
    //
    std::function<bool()> connectToServerFn;
    bool connectToServer() override
    {
        bool result = connectToServerFn();
        _isConnectedToServer = result;
        return result;
    }


    //──────────────────────────────────────────────────────────────────────────
    // 3) sendUdp(dest, data)
    //
    //    - Always stamps each call into sentUdpPackets.
    //    - Then invokes sendUdpFn(dest, data) to decide “true/false.”
    //
    std::function<bool(const IpEndpoint&, const NetPayload&)> sendUdpFn;
    std::vector<std::pair<IpEndpoint, NetPayload>> sentUdpPackets;
    bool sendUdp(const IpEndpoint& dest, const NetPayload& data) override
    {
        sentUdpPackets.emplace_back(dest, data);
        return sendUdpFn(dest, data);
    }


    //──────────────────────────────────────────────────────────────────────────
    // 4) sendTcp(sock, data)
    //
    //    - Stashes (sock, data) into sentTcpPackets.
    //    - Then returns sendTcpSockFn(sock, data).
    //
    std::function<bool(SocketHandle, const NetPayload&)> sendTcpSockFn;
    std::vector<std::pair<SocketHandle, NetPayload>> sentTcpPackets;
    bool sendTcp(SocketHandle sock, const NetPayload& data) override
    {
        sentTcpPackets.emplace_back(sock, data);
        return sendTcpSockFn(sock, data);
    }


    //──────────────────────────────────────────────────────────────────────────
    // 5) sendTcp(data)  (the overload that has no SocketHandle)
    //
    //    - Stashes (NetPayload) into sentTcpNoSockPackets.
    //    - Then returns sendTcpNoSockFn(data).
    //
    std::function<bool(const NetPayload&)> sendTcpNoSockFn;
    std::vector<NetPayload> sentTcpNoSockPackets;
    bool sendTcp(const NetPayload& data) override
    {
        sentTcpNoSockPackets.push_back(data);
        return sendTcpNoSockFn(data);
    }


    //──────────────────────────────────────────────────────────────────────────
    // 6) receive(SocketHandle& tcpSock, IpEndpoint& srcUdp, NetPayload& out)
    //
    //    - If tcpRecvQueue is non‐empty, it pops one TcpReceiveEvent, fills
    //      tcpSock, `out`, and sets `socketClosedFlag` accordingly, then returns true.
    //    - Otherwise, if udpRecvQueue is non‐empty, it pops one UdpReceiveEvent,
    //      sets `tcpSock = nullptr`, `srcUdp` = the event’s source, `out` = payload,
    //      then returns true.
    //    - Otherwise returns false.
    //
    std::deque<TcpReceiveEvent> tcpRecvQueue;
    std::deque<UdpReceiveEvent> udpRecvQueue;
    bool socketClosedFlag = false;

    bool receive(SocketHandle& tcpSock,
                 IpEndpoint&   srcUdp,
                 NetPayload&   out) override
    {
        if (!tcpRecvQueue.empty())
        {
            TcpReceiveEvent ev = tcpRecvQueue.front();
            tcpRecvQueue.pop_front();

            tcpSock        = ev.sock;
            out            = std::move(ev.payload);
            socketClosedFlag = ev.socketClosed;
            return true;
        }
        else if (!udpRecvQueue.empty())
        {
            UdpReceiveEvent ev = udpRecvQueue.front();
            udpRecvQueue.pop_front();

            tcpSock = nullptr; // indicate “this was UDP, not TCP”
            srcUdp  = ev.src;
            out     = std::move(ev.payload);
            return true;
        }
        return false;
    }


    //──────────────────────────────────────────────────────────────────────────
    // 7) receiveUdp(srcUdp, out)
    //
    //    - If udpRecvQueue is non‐empty, pops one entry and returns true.
    //    - Otherwise returns false.
    //
    bool receiveUdp(IpEndpoint& srcUdp, NetPayload& out) override
    {
        if (udpRecvQueue.empty())
            return false;

        UdpReceiveEvent ev = udpRecvQueue.front();
        udpRecvQueue.pop_front();
        srcUdp = ev.src;
        out   = std::move(ev.payload);
        return true;
    }


    //──────────────────────────────────────────────────────────────────────────
    // 8) receiveTcp(SocketHandle& tcpSock, NetPayload& out, bool& socketClosed)
    //
    //    - If tcpRecvQueue is non‐empty, pops one TcpReceiveEvent, fills
    //      tcpSock, out, and socketClosed, returns true.
    //    - Otherwise returns false.
    //
    std::function<bool(SocketHandle&, NetPayload&, bool&)> receiveTcpFn;
    bool receiveTcp(SocketHandle& tcpSock, NetPayload& out, bool& socketClosed) override
    {
        if (!tcpRecvQueue.empty())
        {
            TcpReceiveEvent ev = tcpRecvQueue.front();
            tcpRecvQueue.pop_front();

            tcpSock       = ev.sock;
            out           = std::move(ev.payload);
            socketClosed  = ev.socketClosed;
            return true;
        }
        // If no queued TCP event, fallback to receiveTcpFn (default returns false)
        return receiveTcpFn(tcpSock, out, socketClosed);
    }


    //──────────────────────────────────────────────────────────────────────────
    // 9) receiveTcp(NetPayload& out, bool& socketClosed)
    //
    //    - If tcpRecvQueue is non‐empty, pops one TcpReceiveEvent, but ignores sock,
    //      fills out and socketClosed, returns true.
    //    - Otherwise returns false.
    //
    std::function<bool(NetPayload&, bool&)> receiveTcpNoSockFn;
    bool receiveTcp(NetPayload& out, bool& socketClosed) override
    {
        if (!tcpRecvQueue.empty())
        {
            TcpReceiveEvent ev = tcpRecvQueue.front();
            tcpRecvQueue.pop_front();

            out          = std::move(ev.payload);
            socketClosed = ev.socketClosed;
            return true;
        }
        // If no queued TCP event, fallback to receiveTcpNoSockFn (default returns false)
        return receiveTcpNoSockFn(out, socketClosed);
    }


    //──────────────────────────────────────────────────────────────────────────
    // 10) Big‐endian read/write helpers
    //
    //    These allow your NetworkSystem’s packet‐parsing code to run “for real.”
    //
    void writeU16BE(uint8_t* buf, uint16_t v) override
    {
        buf[0] = static_cast<uint8_t>((v >> 8) & 0xFF);
        buf[1] = static_cast<uint8_t>( v        & 0xFF);
    }

    void writeU32BE(uint8_t* buf, uint32_t v) override
    {
        buf[0] = static_cast<uint8_t>((v >> 24) & 0xFF);
        buf[1] = static_cast<uint8_t>((v >> 16) & 0xFF);
        buf[2] = static_cast<uint8_t>((v >>  8) & 0xFF);
        buf[3] = static_cast<uint8_t>((v >>  0) & 0xFF);
    }

    void writeU64BE(uint8_t* buf, uint64_t v) override
    {
        buf[0] = static_cast<uint8_t>((v >> 56) & 0xFF);
        buf[1] = static_cast<uint8_t>((v >> 48) & 0xFF);
        buf[2] = static_cast<uint8_t>((v >> 40) & 0xFF);
        buf[3] = static_cast<uint8_t>((v >> 32) & 0xFF);
        buf[4] = static_cast<uint8_t>((v >> 24) & 0xFF);
        buf[5] = static_cast<uint8_t>((v >> 16) & 0xFF);
        buf[6] = static_cast<uint8_t>((v >>  8) & 0xFF);
        buf[7] = static_cast<uint8_t>((v >>  0) & 0xFF);
    }

    uint16_t readU16BE(const uint8_t* buf) override
    {
        return static_cast<uint16_t>((static_cast<uint16_t>(buf[0]) << 8) |
                                     (static_cast<uint16_t>(buf[1]) << 0));
    }

    uint32_t readU32BE(const uint8_t* buf) override
    {
        return (static_cast<uint32_t>(buf[0]) << 24) |
               (static_cast<uint32_t>(buf[1]) << 16) |
               (static_cast<uint32_t>(buf[2]) <<  8) |
               (static_cast<uint32_t>(buf[3]) <<  0);
    }

    uint64_t readU64BE(const uint8_t* buf) override
    {
        return (static_cast<uint64_t>(buf[0]) << 56) |
               (static_cast<uint64_t>(buf[1]) << 48) |
               (static_cast<uint64_t>(buf[2]) << 40) |
               (static_cast<uint64_t>(buf[3]) << 32) |
               (static_cast<uint64_t>(buf[4]) << 24) |
               (static_cast<uint64_t>(buf[5]) << 16) |
               (static_cast<uint64_t>(buf[6]) <<  8) |
               (static_cast<uint64_t>(buf[7]) <<  0);
    }
};

} // namespace pg
