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

}

