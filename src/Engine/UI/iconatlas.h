#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

#include "Loaders/svgloader.h"

namespace pg
{
    struct IconEntry
    {
        std::string name;        // "time"
        int size = 0;            // requested px, e.g. 24
        glm::ivec2 sizePx{0, 0}; // actual raster size (== size, size)
        glm::vec2 uvTopLeft{0.0f, 0.0f};
        glm::vec2 uvBottomRight{0.0f, 0.0f};
    };

    /// Packs alpha coverages into one single-channel atlas. Same row packer as TTFTextSystem::registerFont, 2 px padding.
    class IconAtlasBuilder
    {
    public:
        IconAtlasBuilder(int atlasWidth = 1024, int atlasHeight = 1024);

        /// Returns false if the coverage does not fit; nothing is written in that case.
        bool add(const std::string& name, int size, const SvgCoverage& coverage);

        const std::vector<unsigned char>& buffer() const { return atlasBuffer; }
        int width() const { return atlasWidth; }
        int height() const { return atlasHeight; }

        const IconEntry* find(const std::string& name, int size) const;
        const std::vector<IconEntry>& entries() const { return entryList; }

    private:
        int atlasWidth;
        int atlasHeight;
        int currentX = 0;
        int currentY = 0;
        int rowHeight = 0;
        std::vector<unsigned char> atlasBuffer;
        std::vector<IconEntry> entryList;
        std::unordered_map<std::string, size_t> index; // key = name + "@" + size
    };
}
