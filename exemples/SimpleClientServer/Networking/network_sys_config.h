// SystemConfig.h
#pragma once

// Flags that control main System behavior
struct SystemFlags {
    bool enabled               = true;   // is this system active?
    bool networked             = false;  // should this system participate in NetSystem?
    bool reliableChannel       = false;  // use reliable (TCP) vs. unreliable (UDP)
    float updateRateHz         = 60.0f;  // desired tick rate (for throttling sends)
};

// Per-system configuration container
struct SystemConfig {
    SystemFlags systemFlags;
};