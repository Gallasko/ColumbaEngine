#pragma once

#include <string>

#include "ECS/entitysystem.h"

namespace chronicle
{
    // Ascender of a style's "H" (a property of the font atlas), cached per font alias:
    // the atlases never change after registration, so the first measure is the truth.
    float ascenderOf(pg::EntitySystem*, const std::string& style);

    // asc(from) - asc(to): the number a head row adds to a smaller label's top to sit
    // it on a larger style's baseline.
    float baselineShift(pg::EntitySystem*, const std::string& fromStyle, const std::string& toStyle);
}
