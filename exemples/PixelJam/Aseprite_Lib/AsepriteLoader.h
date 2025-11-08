//
// Created by nicol on 5/15/2025.
//

#pragma once

#include <string>

#include "AsepriteFile.h"

namespace pg
{
    class AsepriteLoader
    {
    public:
        AsepriteFile loadAnim(const std::string &path);
    };
}