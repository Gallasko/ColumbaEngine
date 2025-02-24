#pragma once

#include <stdint.h>
#include <cstddef>

namespace pg
{
    // ElementLess (Neutral) = 0, Water, Earth, Air, Fire, Light, Dark
    inline constexpr size_t NbElements = 7;

    enum class Element : uint8_t
    {
        ElementLess = 0,
        Water,
        Earth,
        Air,
        Fire,
        Light,
        Dark
    };

    enum class DamageType : uint8_t
    {
        Physical = 0,
        Magical
    };
}