#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <functional>

#include "pgconstant.h"

namespace pg {
    // Type forwarding
    class OpenGLVertexArrayObject;
    class OpenGLBuffer;
    class FileParser;

    typedef std::function<void(FileParser &, const std::string &)> ParserCallback;

    struct AtlasTexture {
        friend class LoadedAtlas;

    public:
        inline int getId() const { return id; }
        inline std::string getName() const { return name; }

        inline unsigned int getWidth() const { return width; }
        inline unsigned int getHeight() const { return height; }
        inline unsigned int getOffset() const { return yOffset; }

        inline const constant::Vector4D &getTextureLimit() const { return textureLimit; }

        inline void setId(size_t id) { this->id = id; }
        inline void setWidth(unsigned int w) { this->width = w; }
        inline void setHeight(unsigned int h) { this->height = h; }
        inline void setOffset(unsigned int offset) { this->yOffset = offset; }
        inline void setName(const std::string &name) { this->name = name; }

        void setMesh(unsigned int xPos, unsigned int yPos, unsigned int atlasWidth, unsigned int atlasHeight);

    private:
        size_t id = 0;
        std::string name = "";
        unsigned int width = 0;
        unsigned int height = 0;
        unsigned int yOffset = 0;

        constant::Vector4D textureLimit;
    };

    class LoadedAtlas {
    public:


    public:
        LoadedAtlas() {
        }

        LoadedAtlas(const std::string &atlasFile,
                    std::unordered_map<std::string, ParserCallback> callbacks = std::unordered_map<std::string,
                        ParserCallback>()) {
            setFile(atlasFile, callbacks);

            LOG_INFO("Atlas", "Loaded atlas [" << atlasFile << "] with textures: " << nbTextureId);
        }

        LoadedAtlas(const LoadedAtlas &other) : version(other.version), imagePath(other.imagePath),
                                                atlasWidth(other.atlasWidth), atlasHeight(other.atlasHeight),
                                                nbTextureId(other.nbTextureId),
                                                textureList(other.textureList), textureDict(other.textureDict),
                                                emptyTexture(other.emptyTexture) {
            LOG_ERROR("Atlas", "Copying a loaded atlas should never happen");
        }

        void setFile(const std::string &atlasFile,
                     std::unordered_map<std::string, ParserCallback> callbacks = std::unordered_map<std::string,
                         ParserCallback>());

        inline const std::string &getImagePath() const { return imagePath; }

        inline unsigned int getAtlasWidth() const { return atlasWidth; }
        inline unsigned int getAtlasHeight() const { return atlasHeight; }

        const AtlasTexture &getTexture(size_t id) const;

        const AtlasTexture &getTexture(const std::string &charaName) const;

        size_t getNbTexture() { return nbTextureId; }

        bool isEmpty() const { return nbTextureId == 0; }

    protected:
        std::string version;
        std::string imagePath;
        unsigned int atlasWidth = 0;
        unsigned int atlasHeight = 0;
        size_t nbTextureId = 0;

        std::vector<AtlasTexture> textureList;
        std::unordered_map<std::string, int> textureDict;

        /** Empty texture returned in case of an error */
        AtlasTexture emptyTexture;
    };
}
