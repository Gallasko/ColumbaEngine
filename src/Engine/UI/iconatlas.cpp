#include "stdafx.h"

#include "iconatlas.h"

namespace pg
{
    namespace
    {
        // Gap between packed coverages, matching TTFTextSystem::registerFont.
        constexpr int PADDING = 2;

        std::string makeKey(const std::string& name, int size)
        {
            return name + "@" + std::to_string(size);
        }
    }

    IconAtlasBuilder::IconAtlasBuilder(int atlasWidth, int atlasHeight) :
        atlasWidth(atlasWidth),
        atlasHeight(atlasHeight),
        atlasBuffer(static_cast<size_t>(atlasWidth) * atlasHeight, 0)
    {
    }

    bool IconAtlasBuilder::add(const std::string& name, int size, const SvgCoverage& coverage)
    {
        const int w = coverage.width;
        const int h = coverage.height;

        int placeX = currentX;
        int placeY = currentY;
        int newRowHeight = rowHeight;

        // Move to the next row if the coverage would overflow the current one.
        if (placeX + w + PADDING > atlasWidth)
        {
            placeX = 0;
            placeY = currentY + rowHeight + PADDING;
            newRowHeight = 0;
        }

        // Reject if it does not fit (too wide for the atlas, or past the bottom).
        if (placeX + w > atlasWidth or placeY + h > atlasHeight)
        {
            return false;
        }

        // Copy the coverage into the atlas at (placeX, placeY).
        for (int y = 0; y < h; ++y)
        {
            for (int x = 0; x < w; ++x)
            {
                atlasBuffer[(placeY + y) * atlasWidth + (placeX + x)] = coverage.alpha[y * w + x];
            }
        }

        IconEntry entry;
        entry.name = name;
        entry.size = size;
        entry.sizePx = {w, h};
        entry.uvTopLeft = {static_cast<float>(placeX) / atlasWidth, static_cast<float>(placeY) / atlasHeight};
        entry.uvBottomRight = {static_cast<float>(placeX + w) / atlasWidth, static_cast<float>(placeY + h) / atlasHeight};

        index[makeKey(name, size)] = entryList.size();
        entryList.push_back(entry);

        // Commit the packer state now that the coverage is placed.
        currentX = placeX + w + PADDING;
        currentY = placeY;
        rowHeight = std::max(newRowHeight, h);

        return true;
    }

    const IconEntry* IconAtlasBuilder::find(const std::string& name, int size) const
    {
        auto it = index.find(makeKey(name, size));

        if (it == index.end())
            return nullptr;

        return &entryList[it->second];
    }
}
