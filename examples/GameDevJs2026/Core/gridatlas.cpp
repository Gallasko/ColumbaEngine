#include "gridatlas.h"

GridAtlas::GridAtlas(const std::string& imgPath,
                     unsigned int atlasW, unsigned int atlasH,
                     unsigned int frameW, unsigned int frameH,
                     unsigned int cols, unsigned int count,
                     unsigned int startX, unsigned int startY)
{
    this->imagePath = imgPath;
    this->atlasWidth = atlasW;
    this->atlasHeight = atlasH;

    for (unsigned int i = 0; i < count; ++i)
    {
        unsigned int xPos = startX + (i % cols) * frameW;
        unsigned int yPos = startY + (i / cols) * frameH;

        pg::AtlasTexture tex;
        tex.setId(nbTextureId);
        tex.setName(std::to_string(i));
        tex.setWidth(frameW);
        tex.setHeight(frameH);
        tex.setMesh(xPos, yPos + frameH - 1, atlasW, atlasH);

        textureList.push_back(tex);
        textureDict[tex.getName()] = nbTextureId++;
    }
}
