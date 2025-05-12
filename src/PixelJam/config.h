#pragma once

#include "Input/keyconfig.h"

namespace pg
{
    enum class GameKeyConfig : uint8_t
    {
        MoveLeft,
        MoveRight,
        MoveUp,
        MoveDown,
        Interact,
        Pause,
    };

    extern std::map<GameKeyConfig, DefaultScancode> scancodeMap;

} // namespace pg
