// common.h
#pragma once
#include <cstdint>
#include <map>
#include <chrono>
#include <SDL_net.h>

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


    typedef std::vector<uint8_t> NetPayload;
    typedef std::vector<NetPayload> NetFragmentedPayload;
    typedef std::map<std::tuple<uint32_t, uint32_t, uint32_t, NetMsgType>, NetFragmentedPayload> NetPacketBuffer;

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
    inline void writeU16BE(uint8_t* buf, uint16_t v) { SDLNet_Write16(v, buf); }
    inline void writeU32BE(uint8_t* buf, uint32_t v) { SDLNet_Write32(v, buf); }
    inline void writeU64BE(uint8_t* buf, uint64_t v)
    {
        // split into two 32-bit
        writeU32BE(buf,     uint32_t(v >> 32));
        writeU32BE(buf + 4, uint32_t(v & 0xFFFFFFFF));
    }

    inline uint16_t readU16BE(const uint8_t* buf)    { return SDLNet_Read16(const_cast<uint8_t*>(buf)); }
    inline uint32_t readU32BE(const uint8_t* buf)    { return SDLNet_Read32(const_cast<uint8_t*>(buf)); }
    inline uint64_t readU64BE(const uint8_t* buf)
    {
        uint64_t hi = readU32BE(buf);
        uint64_t lo = readU32BE(buf + 4);
        return (hi << 32) | lo;
    }

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

    // Write header into buffer (must be at least HEADER_SIZE bytes)
    inline void writeHeader(uint8_t* buf, const PacketHeader& h)
    {
        writeU32BE(buf +   0, h.clientId);
        writeU32BE(buf +   4, h.token);
        writeU32BE(buf +   8, h.packetNumber);
        writeU64BE(buf +  12, h.timestamp);
        buf[20] = static_cast<uint8_t>(h.type);
        writeU16BE(buf +  21, h.totalFragments);
        writeU16BE(buf +  23, h.fragmentIndex);
        writeU16BE(buf +  25, h.payloadLen);
    }

    // Read header from buffer
    inline PacketHeader readHeader(const uint8_t* buf)
    {
        PacketHeader h;

        h.clientId       = readU32BE(buf +   0);
        h.token          = readU32BE(buf +   4);
        h.packetNumber   = readU32BE(buf +   8);
        h.timestamp      = readU64BE(buf +  12);
        h.type           = static_cast<NetMsgType>(buf[20]);
        h.totalFragments = readU16BE(buf +  21);
        h.fragmentIndex  = readU16BE(buf +  23);
        h.payloadLen     = readU16BE(buf +  25);

        return h;
    }

    // Fragment a payload into HEADER_SIZE + chunks of <= MAX_PAYLOAD
    inline NetFragmentedPayload fragmentPayload(
        uint32_t clientId,
        uint32_t token,
        NetMsgType type,
        uint32_t packetNumber,
        const NetPayload& payload)
    {
        size_t chunkMax = MAX_PAYLOAD - HEADER_SIZE;

        uint16_t totalFrags = uint16_t((payload.size() + chunkMax - 1) / chunkMax);

        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch())
                    .count();

        NetFragmentedPayload out;
        out.reserve(totalFrags);

        for (uint16_t idx = 0; idx < totalFrags; idx++)
        {
            size_t offset = size_t(idx) * chunkMax;
            uint16_t thisLen = uint16_t(std::min(chunkMax, payload.size() - offset));

            PacketHeader h {
                clientId,
                token,
                packetNumber,
                uint64_t(now),
                type,
                totalFrags,
                idx,
                thisLen
            };

            NetPayload pkt(HEADER_SIZE + thisLen);

            writeHeader(pkt.data(), h);

            if (thisLen)
                memcpy(pkt.data() + HEADER_SIZE, payload.data() + offset, thisLen);

            out.push_back(std::move(pkt));
        }

        return out;
    }

        // Parsed result
    struct ParsedPacket
    {
        PacketHeader header;
        NetPayload payload;
    };

    // Reassemble fragments
    inline bool parseAndReassemble(const NetPayload& raw, NetPacketBuffer& buffer, ParsedPacket& out)
    {
        if (raw.size() < HEADER_SIZE)
            return false;

        auto hdr = readHeader(raw.data());

        auto key = std::make_tuple(hdr.clientId, hdr.token, hdr.packetNumber, hdr.type);

        auto& slots = buffer[key];

        if (slots.empty())
            slots.resize(hdr.totalFragments);

        if (hdr.fragmentIndex >= hdr.totalFragments)
            return false;

        slots[hdr.fragmentIndex] =
            NetPayload(
            raw.begin() + HEADER_SIZE,
            raw.begin() + HEADER_SIZE + hdr.payloadLen);

        // check if complete
        bool complete = true;

        for (auto& s : slots)
        {
            if (s.empty())
            {
                complete = false;
                break;
            }
        }

        if (not complete)
            return false;

        // assemble
        NetPayload full;
        for (auto& s : slots)
            full.insert(full.end(), s.begin(), s.end());

        out.header = hdr;
        out.payload = std::move(full);
        buffer.erase(key);

        return true;
    }
}

