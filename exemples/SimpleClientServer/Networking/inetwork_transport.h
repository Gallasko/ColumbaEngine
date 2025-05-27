// INetworkTransport.h
#pragma once

#include <cstdint>
#include <vector>

// Simple packet type used by all transports
struct Packet {
    uint32_t    entityId;
    uint16_t    compType;
    std::vector<uint8_t> data;
};

// Abstract base for UDP/TCP socket wrappers
class INetworkTransport {
public:
    virtual ~INetworkTransport() = default;

    // Send a “reliable” packet (e.g. over TCP)
    virtual bool sendReliable(const Packet& pkt) = 0;

    // Send an “unreliable” packet (e.g. over UDP)
    virtual bool sendUnreliable(const Packet& pkt) = 0;

    // Receive next packet (blocking=false) into out; return true if one was filled
    virtual bool receive(Packet& out) = 0;
};
