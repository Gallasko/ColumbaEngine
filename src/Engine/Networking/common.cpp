#include "stdafx.h"

#include "common.h"

#include "ibackend.h"

#include <memory>
#include <algorithm>
#include <cstring>

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
        const NetPayload& payload,
        size_t maxPacket)
    {
        size_t chunkMax = maxPacket - HEADER_SIZE;

        auto pSize = payload.size();

        // An empty message is still one (empty) fragment: it has to arrive
        uint16_t totalFrags = uint16_t(std::max<size_t>(1, (pSize + chunkMax - 1) / chunkMax));

        const auto now = steadyMs();

        NetFragmentedPayload out;
        out.reserve(totalFrags);

        for (uint16_t idx = 0; idx < totalFrags; idx++)
        {
            size_t offset = size_t(idx) * chunkMax;
            uint16_t thisLen = uint16_t(pSize > offset ? std::min(chunkMax, pSize - offset) : 0);

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

    uint64_t steadyMs()
    {
        return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
    }

    // Reassemble fragments
    bool parseAndReassemble(
        const NetPayload& raw,
        NetPacketBuffer& buffer,
        NetPacketTimers& rTimers,
        INetworkBackend* backend,
        ParsedPacket& out)
    {
        if (raw.size() < HEADER_SIZE)
            return false;

        const auto hdr = readHeader(raw.data(), backend);

        // Checked before the fragment takes any room
        if (hdr.totalFragments == 0 or hdr.fragmentIndex >= hdr.totalFragments)
            return false;

        if (HEADER_SIZE + hdr.payloadLen > raw.size())
            return false;

        if (static_cast<size_t>(hdr.totalFragments) * (MAX_PAYLOAD - HEADER_SIZE) > MAX_MESSAGE + MAX_PAYLOAD)
            return false;

        NetPayload fragment(raw.begin() + HEADER_SIZE, raw.begin() + HEADER_SIZE + hdr.payloadLen);

        // The common case: a message in one piece
        if (hdr.totalFragments == 1)
        {
            out.header = hdr;
            out.payload = std::move(fragment);
            return true;
        }

        const auto key = std::make_tuple(hdr.clientId, hdr.token, hdr.packetNumber, hdr.type);
        auto& slots = buffer[key];

        if (slots.empty())
        {
            slots.resize(hdr.totalFragments);
            rTimers[key] = steadyMs();
        }

        // A fragment that disagrees with the first one about the message's size: drop the message
        if (slots.size() != hdr.totalFragments)
        {
            buffer.erase(key);
            rTimers.erase(key);
            return false;
        }

        // An empty fragment marks a slot as filled with nothing: keep one byte of bookkeeping apart
        slots[hdr.fragmentIndex] = std::move(fragment);
        slots[hdr.fragmentIndex].insert(slots[hdr.fragmentIndex].begin(), 1);

        for (const auto& s : slots)
            if (s.empty())
                return false;

        NetPayload full;
        for (const auto& s : slots)
            full.insert(full.end(), s.begin() + 1, s.end());

        out.header = hdr;
        out.payload = std::move(full);
        buffer.erase(key);
        rTimers.erase(key);

        return true;
    }

    size_t dropStaleFragments(NetPacketBuffer& buffer, NetPacketTimers& timers, uint64_t now, uint64_t timeoutMs)
    {
        size_t dropped = 0;

        for (auto it = buffer.begin(); it != buffer.end();)
        {
            auto t = timers.find(it->first);
            const bool stale = t == timers.end() or now - t->second > timeoutMs;

            if (stale)
            {
                if (t != timers.end())
                    timers.erase(t);

                it = buffer.erase(it);
                ++dropped;
            }
            else
            {
                ++it;
            }
        }

        // Timers left without a buffer entry
        for (auto it = timers.begin(); it != timers.end();)
            it = buffer.count(it->first) ? std::next(it) : timers.erase(it);

        return dropped;
    }
}
