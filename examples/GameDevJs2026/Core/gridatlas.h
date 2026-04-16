#pragma once

#include "Loaders/atlasloader.h"

class GridAtlas : public pg::LoadedAtlas {
public:
    GridAtlas(const std::string& imgPath,
             unsigned int atlasW, unsigned int atlasH,
             unsigned int frameW, unsigned int frameH,
             unsigned int cols, unsigned int count);
};
