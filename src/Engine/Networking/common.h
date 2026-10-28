// common.h
#pragma once
#include <cstdint>
#include <map>
#include <chrono>
#include <string>
#include <tuple>
#include <vector>

namespace pg
{
    // Maximum UDP payload
    constexpr int MAX_PAYLOAD = 4096;

    enum class NetMsgType : uint8_t
    {
        Heartbeat   = 0x01,  // periodic keepalive
        Ping        = 0x02, // client → server with timestamp
        Pong        = 0x03, // server → client echo timestamp
        Disconnect  = 0x04, // either side clean‐teardown
        Connect     = 0x05,
        Handshake   = 0x06,
        EntityData  = 0x07,
        Custom      = 0xFE,
    };

    // Opaque “socket” handle: the backend will decide what it actually holds.
    using SocketHandle = void*;

    // A simple IPv4 endpoint (address+port). You could expand this to IPv6 if needed.
    struct IpEndpoint
    {
        std::string host;   // “127.0.0.1” or “example.com”
        uint16_t    port;   // e.g. 7777

        IpEndpoint() = default;
        IpEndpoint(std::string h, uint16_t p) : host(std::move(h)), port(p) {}
    };

    using NetPayload = std::vector<uint8_t>;
    using NetFragmentedPayload = std::vector<NetPayload>;
    using NetPacketKey = std::tuple<uint32_t, uint32_t, uint32_t, NetMsgType>;   // clientId, token, packetNumber, type
    using NetPacketBuffer = std::map<NetPacketKey, NetFragmentedPayload>;
    using NetPacketTimers = std::map<NetPacketKey, uint64_t>;                       // first fragment seen (steady ms)

    /** Largest message a peer may send (after reassembly): anything bigger is refused. */
    constexpr size_t MAX_MESSAGE = 16 * 1024 * 1024;

    /** Milliseconds on a steady clock (not the wall clock: it never jumps). */
    uint64_t steadyMs();

    // Total header size:
    constexpr size_t HEADER_SIZE =
        4  // clientId
        + 4  // token
        + 4  // packetNumber
        + 8  // timestamp
        + 1  // type
        + 2  // totalFragments
        + 2  // fragmentIndex
        + 2; // payloadLen

    // Helpers to write/read big-endian

    // The full header:
    struct PacketHeader
    {
        uint32_t clientId;
        uint32_t token;
        uint32_t packetNumber;
        uint64_t timestamp;
        NetMsgType type;
        uint16_t totalFragments;
        uint16_t fragmentIndex;
        uint16_t payloadLen;  // host-order
    };

    // Forward declaration of the backend interface
    struct INetworkBackend;

    // Todo move those 4 functions in the backend

    // Write header into buffer (must be at least HEADER_SIZE bytes)
    void writeHeader(uint8_t* buf, INetworkBackend* backend, const PacketHeader& h);

    // Read header from buffer
    PacketHeader readHeader(const uint8_t* buf, INetworkBackend* backend);

    // Fragment a payload into HEADER_SIZE + chunks of <= MAX_PAYLOAD
    NetFragmentedPayload fragmentPayload(
        uint32_t clientId,
        uint32_t token,
        NetMsgType type,
        uint32_t packetNumber,
        INetworkBackend* backend,
        const NetPayload& payload);

        // Parsed result
    struct ParsedPacket
    {
        PacketHeader header;
        NetPayload payload;
    };

    // Reassemble fragments
    /**
     * Adds one fragment; true (and out filled) when it completes its message. A fragment that does
     * not read cleanly is refused before it takes any room in the buffer.
     */
    bool parseAndReassemble(
        const NetPayload& raw,
        NetPacketBuffer& buffer,
        NetPacketTimers& rTimers,
        INetworkBackend* backend,
        ParsedPacket& out);

    /** Drops the messages whose fragments have been waiting longer than timeoutMs. Returns how many. */
    size_t dropStaleFragments(NetPacketBuffer& buffer, NetPacketTimers& timers, uint64_t now, uint64_t timeoutMs);
}

