// SystemConfig.h
#pragma once

namespace pg
{
    // Flags that control main System behavior
    struct SystemFlags {
        bool enabled               = true;    // is this system active?
        bool networked             = false;   // should this system participate in NetSystem?
        bool reliableChannel       = false;   // use reliable (TCP) vs. unreliable (UDP)
        float updateRateHz         = 60.0f;   // desired tick rate (for throttling sends)netCfg
        float pingTimer            = 1000.0f; // Time between pings in ms
        float dropPacketTimeout    = 5000.0f; // Timeout for dropped packets in ms
        size_t socketSetSize       = 16;      // size of the socket set for SDLNet (default 16)
    };

    // Per-system configuration container
    struct SystemConfig
    {
        SystemFlags systemFlags;
    };
}
