//
// Created by nicol on 5/12/2025.
//

#include "TileMapAtlasLoader.h"

namespace pg {
    TileMapAtlasLoader::TileMapAtlasLoader(const TileSet &tileset) {
        this->imagePath = tileset.imagePath;
        this->atlasWidth = tileset.width;
        this->atlasHeight = tileset.height;

        for (int i = 0; i < tileset.tileCount; ++i) {
            const int id = i + 1;

            // Position in tileset
            const int localId = id - 1;

            const int col = localId % tileset.columns;
            const int row = localId / tileset.columns;

            const int pixelX = col * tileset.tileWidthInTPixels;
            const int pixelY = (row + 1) * (tileset.tileHeightInTPixels) - 1;

            const auto name = std::to_string(id);

            AtlasTexture atlasTex;
            atlasTex.setHeight(tileset.tileHeightInTPixels);
            atlasTex.setWidth(tileset.tileWidthInTPixels);
            atlasTex.setId(id);
            atlasTex.setName(name);
            atlasTex.setMesh(pixelX, pixelY, tileset.width, tileset.height);

            this->textureList.push_back(atlasTex);
            this->textureDict[name] = this->nbTextureId++;
        }
    }
} // pg