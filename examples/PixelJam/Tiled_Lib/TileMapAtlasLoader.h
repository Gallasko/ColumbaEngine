//
// Created by nicol on 5/12/2025.
//

#ifndef TILEMAPATLASLOADER_H
#define TILEMAPATLASLOADER_H

#include "Loaders/atlasloader.h"
#include "MapData.h"

namespace pg {
    class TileMapAtlasLoader : public LoadedAtlas {
    public:
        TileMapAtlasLoader(const TileSet &tileset);
    };
} // pg

#endif //TILEMAPATLASLOADER_H
