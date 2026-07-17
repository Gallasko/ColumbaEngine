#pragma once

#include "Loaders/atlasloader.h"

namespace pg
{
    /// Builds an atlas by slicing a sprite sheet into a uniform grid of frames.
    class GridAtlasLoader : public LoadedAtlas
    {
    public:
        GridAtlasLoader(const std::string& imgPath,
                 unsigned int atlasW, unsigned int atlasH,
                 unsigned int frameW, unsigned int frameH,
                 unsigned int cols, unsigned int count,
                 unsigned int startX = 0, unsigned int startY = 0);
    };
}
