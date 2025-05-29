// common.h
#pragma once
#include <cstdint>
#include <SDL_net.h>

namespace pg
{
    // Maximum UDP payload
    constexpr int MAX_PAYLOAD = 1500;

    // Packet header carried on every UDP message
    struct UdpHeader
    {
        uint32_t clientId;
        uint32_t token;
        uint16_t payloadLen;  // host‐order
    };

    // Serialize header into buffer
    // Serialize header into buffer (big-endian on the wire)
    inline void writeHeader(uint8_t* buf, const UdpHeader& h)
    {
        SDLNet_Write32(h.clientId,  buf + 0); // writes 4 bytes BE
        SDLNet_Write32(h.token,     buf + 4);
        SDLNet_Write16(h.payloadLen, buf + 8); // writes 2 bytes BE
    }

    // Read header from buffer (convert from BE to host order)
    inline UdpHeader readHeader(const uint8_t* buf)
    {
        UdpHeader h;

        h.clientId   = SDLNet_Read32(const_cast<uint8_t*>(buf + 0));
        h.token      = SDLNet_Read32(const_cast<uint8_t*>(buf + 4));
        h.payloadLen = SDLNet_Read16(const_cast<uint8_t*>(buf + 8));

        return h;
    }
    enum class NetMsgType : uint8_t
    {
        Heartbeat   = 0x01,
        Connect     = 0x02,
        Disconnect  = 0x03,
        Handshake   = 0x04,
        EntityData  = 0x05,
        Custom      = 0xFE,
    };

    // Write helpers using SDL_net
    inline void writeU32BE(uint8_t* buf, uint32_t v) { SDLNet_Write32(v, buf); }
    inline uint32_t readU32BE(const uint8_t* buf)    { return SDLNet_Read32(const_cast<uint8_t*>(buf)); }
    inline void writeU16BE(uint8_t* buf, uint16_t v) { SDLNet_Write16(v, buf); }
    inline uint16_t readU16BE(const uint8_t* buf)    { return SDLNet_Read16(const_cast<uint8_t*>(buf)); }

    // Build a full packet
    inline std::vector<uint8_t> makePacket(
        uint32_t clientId,
        uint32_t token,
        NetMsgType type,
        const std::vector<uint8_t>& payload)
    {
        size_t total = 4 + 4 + 1 + 2 + payload.size();

        std::vector<uint8_t> pkt;
        pkt.resize(total);

        uint8_t* p = pkt.data();

        // clientId
        writeU32BE(p, clientId);
        p += 4;

        // token
        writeU32BE(p, token);
        p += 4;

        // type
        *p++ = static_cast<uint8_t>(type);

        // length
        writeU16BE(p, (uint16_t)payload.size());
        p += 2;

        // payload
        if (not payload.empty())
        {
            memcpy(p, payload.data(), payload.size());
        }

        return pkt;
    }

    // Parsed result
    struct ParsedPacket
    {
        uint32_t    clientId;
        uint32_t    token;
        NetMsgType  type;
        std::vector<uint8_t> payload;
    };

    // Parse a full packet
    inline bool parsePacket(const std::vector<uint8_t>& pkt, ParsedPacket& out)
    {
        if (pkt.size() < 11)
            return false;

        const uint8_t* p = pkt.data();

        out.clientId = readU32BE(p); p += 4;
        out.token    = readU32BE(p); p += 4;
        out.type     = static_cast<NetMsgType>(*p++);
        uint16_t len = readU16BE(p); p += 2;

        if (pkt.size() < 11 + len)
            return false;

        out.payload.assign(p, p + len);
        return true;
    }
}

