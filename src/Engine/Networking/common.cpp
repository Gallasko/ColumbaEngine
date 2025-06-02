#include "stdafx.h"

#include "common.h"

#include "ibackend.h"

#include <memory>
#include <algorithm>

namespace pg
{
    void writeHeader(uint8_t* buf, INetworkBackend* backend, const PacketHeader& h)
    {
        backend->writeU32BE(buf +   0, h.clientId);
        backend->writeU32BE(buf +   4, h.token);
        backend->writeU32BE(buf +   8, h.packetNumber);
        backend->writeU64BE(buf +  12, h.timestamp);
        buf[20] = static_cast<uint8_t>(h.type);
        backend->writeU16BE(buf +  21, h.totalFragments);
        backend->writeU16BE(buf +  23, h.fragmentIndex);
        backend->writeU16BE(buf +  25, h.payloadLen);
    }

    // Read header from buffer
    PacketHeader readHeader(const uint8_t* buf, INetworkBackend* backend)
    {
        PacketHeader h;

        h.clientId       = backend->readU32BE(buf +   0);
        h.token          = backend->readU32BE(buf +   4);
        h.packetNumber   = backend->readU32BE(buf +   8);
        h.timestamp      = backend->readU64BE(buf +  12);
        h.type           = static_cast<NetMsgType>(buf[20]);
        h.totalFragments = backend->readU16BE(buf +  21);
        h.fragmentIndex  = backend->readU16BE(buf +  23);
        h.payloadLen     = backend->readU16BE(buf +  25);

        return h;
    }

    // Fragment a payload into HEADER_SIZE + chunks of <= MAX_PAYLOAD
    NetFragmentedPayload fragmentPayload(
        uint32_t clientId,
        uint32_t token,
        NetMsgType type,
        uint32_t packetNumber,
        INetworkBackend* backend,
        const NetPayload& payload)
    {
        size_t chunkMax = MAX_PAYLOAD - HEADER_SIZE;

        auto pSize = payload.size();

        uint16_t totalFrags = uint16_t((pSize + chunkMax - 1) / chunkMax);

        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch())
                    .count();

        NetFragmentedPayload out;
        out.reserve(totalFrags);

        for (uint16_t idx = 0; idx < totalFrags; idx++)
        {
            size_t offset = size_t(idx) * chunkMax;
            uint16_t thisLen = uint16_t(std::min(chunkMax, pSize - offset));

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

            writeHeader(pkt.data(), backend, h);

            if (thisLen)
                memcpy(pkt.data() + HEADER_SIZE, payload.data() + offset, thisLen);

            out.push_back(std::move(pkt));
        }

        return out;
    }

    // Reassemble fragments
    bool parseAndReassemble(
        const NetPayload& raw,
        NetPacketBuffer& buffer,
        std::map<uint32_t, uint64_t>& rTimers,
        INetworkBackend* backend,
        ParsedPacket& out)
    {
        if (raw.size() < HEADER_SIZE)
            return false;

        auto hdr = readHeader(raw.data(), backend);

        auto key = std::make_tuple(hdr.clientId, hdr.token, hdr.packetNumber, hdr.type);

        auto& slots = buffer[key];

        if (slots.empty())
        {
            slots.resize(hdr.totalFragments);

            auto now =  std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();

            rTimers[hdr.packetNumber] = now;
        }

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