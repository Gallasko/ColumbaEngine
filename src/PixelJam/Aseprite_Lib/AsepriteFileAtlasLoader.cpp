//
// Created by nicol on 5/17/2025.
//

#include "AsepriteFileAtlasLoader.h"

pg::AsepriteFileAtlasLoader::AsepriteFileAtlasLoader(const AsepriteFile &aseprite) {
    this->imagePath = aseprite.metadata.imagePath;
    this->atlasWidth = aseprite.metadata.imageWidthInSPixels;
    this->atlasHeight = aseprite.metadata.imageHeightInSPixels;

    int i = 0;

    for (const auto &frame: aseprite.frames) {

        const auto name = std::to_string(i);

        AtlasTexture atlasTex;
        atlasTex.setHeight(frame.heightInSPixels);
        atlasTex.setWidth(frame.widthInSPixels);
        atlasTex.setId(i);
        atlasTex.setName(name);
        atlasTex.setMesh(frame.topLeftCornerInSPixelsX, frame.topLeftCornerInSPixelsY, aseprite.metadata.imageWidthInSPixels, aseprite.metadata.imageHeightInSPixels);


        this->textureList.push_back(atlasTex);
        this->textureDict[name] = this->nbTextureId++;

        i++;
    }
}
