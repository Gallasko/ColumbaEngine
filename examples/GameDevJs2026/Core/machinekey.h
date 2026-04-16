#pragma once

#include <cstdint>

// Hash used by miner / crafting / inserter systems to index machines by grid
// position. The MSB holds Y so that toggling rows does not collide with X.
inline uint32_t machineKey(int x, int y)
{
    return (static_cast<uint32_t>(y) << 16) | static_cast<uint32_t>(x);
}
