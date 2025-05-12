#include "config.h"

namespace pg
{
    std::map<GameKeyConfig, DefaultScancode> scancodeMap = {
        {GameKeyConfig::MoveLeft,    {"MoveLeft", SDL_SCANCODE_A}},
        {GameKeyConfig::MoveRight,   {"MoveRight", SDL_SCANCODE_D}},
        {GameKeyConfig::MoveUp,      {"MoveUp", SDL_SCANCODE_W}},
        {GameKeyConfig::MoveDown,    {"MoveDown", SDL_SCANCODE_S}},
        {GameKeyConfig::Interact,    {"Interact", SDL_SCANCODE_E}},
        {GameKeyConfig::Pause,       {"Pause", SDL_SCANCODE_ESCAPE}},
        };


} // namespace pg
