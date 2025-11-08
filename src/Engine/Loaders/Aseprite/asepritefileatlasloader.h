//
// Created by nicol on 5/17/2025.
//

#pragma once

#include "asepritefile.h"
#include "Loaders/atlasloader.h"

namespace pg
{
    class AsepriteFileAtlasLoader : public LoadedAtlas
    {
    public:
        AsepriteFileAtlasLoader(const AsepriteFile &aseprite);
    };
}
