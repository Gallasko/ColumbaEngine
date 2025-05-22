#include "stdafx.h"

#include "atlasloader.h"
#include "Files/fileparser.h"
#include "logger.h"

namespace pg
{
    namespace
    {
        static constexpr char const * DOM = "Atlas Loader";
    }

    void AtlasTexture::setMesh(unsigned int xPos, unsigned int yPos, unsigned int atlasWidth, unsigned int atlasHeight)
    {
        LOG_THIS_MEMBER(DOM);

        float xMin = xPos / (float)atlasWidth;
        float xMax = (xPos + width) / (float)atlasWidth;

        float yMin = (yPos - height + 1) / (float)atlasHeight;
        float yMax = (yPos + 1) / (float)atlasHeight;

        textureLimit.x = xMin;
        textureLimit.y = yMin;
        textureLimit.z = xMax;
        textureLimit.w = yMax;
    }

    const AtlasTexture& LoadedAtlas::getTexture(size_t id) const
    {
        LOG_THIS_MEMBER(DOM);

        if (nbTextureId == 0)
        {
            LOG_ERROR(DOM, "Atlas is empty");
            return emptyTexture;
        }

        if (id < nbTextureId)
            return textureList[id];
        else
        {
            LOG_ERROR(DOM, "Id[" << id << "] is out of range of the atlas");
            return emptyTexture;
        }
    }

    const AtlasTexture& LoadedAtlas::getTexture(const std::string& charaName) const
    {
        LOG_THIS_MEMBER(DOM);

        if (nbTextureId == 0)
        {
            LOG_ERROR(DOM, "Atlas is empty");
            return emptyTexture;
        }

        if (charaName.empty())
        {
            LOG_ERROR(DOM, "No texture requested");
            return emptyTexture;
        }

        auto it = textureDict.find(charaName);

        if (it != textureDict.end())
            return textureList[textureDict.at(charaName)];
        else
        {
            LOG_ERROR(DOM, "Looking for " << charaName << ", but didn't find it in the dict");
            return emptyTexture;
        }
    }

    void LoadedAtlas::setFile(const std::string& atlasFile, std::unordered_map<std::string, ParserCallback> callbacks)
    {
        LOG_THIS(DOM);

        TextFile file = UniversalFileAccessor::openTextFile(atlasFile);

        FileParser parser(file);
        AtlasTexture texture;

        unsigned int xPos = 0;
        unsigned int yPos = 0;

        parser.addCallback("Image Path",   [&](const std::string&) { imagePath = parser.getNextLine(); } );
        parser.addCallback("Atlas Width",  [&](const std::string&) { atlasWidth  = std::stoi(parser.getNextLine()); } );
        parser.addCallback("Atlas Height", [&](const std::string&) { atlasHeight = std::stoi(parser.getNextLine()); } );
        parser.addCallback("Row",          [&](const std::string&) { if (parser.getNextLine() == "Base Y") { parser.advance(); xPos = 1; yPos = std::stoi(parser.getNextLine()); } } );
        parser.addCallback("Charactere",   [&](const std::string&) { texture = AtlasTexture(); texture.setId(nbTextureId); texture.setName(parser.getNextLine()); } );
        parser.addCallback("Width",        [&](const std::string&) { texture.setWidth(std::stoi(parser.getNextLine())); } );
        parser.addCallback("Height",       [&](const std::string&) { texture.setHeight(std::stoi(parser.getNextLine())); } );
        parser.addCallback("Y-Offset",     [&](const std::string&) { texture.setOffset(std::stoi(parser.getNextLine())); } );

        for (auto callback : callbacks)
        {
            LOG_INFO(DOM, "Added callback for: " << callback.first);
            parser.addCallback(callback.first, [&](const std::string& string) { callback.second(parser, string); });
        }

        parser.addCallback("###########",  [&](const std::string&) {
            texture.setMesh(xPos, yPos, atlasWidth, atlasHeight);
            textureList.push_back(texture);
            textureDict[texture.getName()] = nbTextureId;

            nbTextureId++;
            xPos += texture.getWidth() + 1;
        } );

        parser.run();
    }
}