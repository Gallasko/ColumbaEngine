// NetworkConfig.h
#pragma once

#include <cstdint>
#include <string>
#include "network_sys_config.h"

namespace pg
{
    // Holds all networking-related settings for backend & systems
    struct NetworkConfig {
        // --- Global mode & addressing ---
        bool        isServer       = false;         // bind (server) vs connect (client)
        std::string peerAddress    = "127.0.0.1";   // remote host to connect/send to

        // --- UDP settings ---
        uint16_t    udpLocalPort   = 0;             // 0 = ephemeral (client); non-zero for server bind
        uint16_t    udpPeerPort    = 0;             // where to send UDP packets (server port)

        // --- TCP fallback (reliable) ---
        bool        tcpEnabled     = true;
        uint16_t    tcpPort        = 0;             // port to bind/connect TCP

        // --- Default flags for all net‐systems ---
        SystemFlags defaultSystemFlags;

        // (Optional) Hook you can set to dispatch deserialized state:
        // std::function<void(const Packet&, Entity)> onPacketReceived;
    };

}

