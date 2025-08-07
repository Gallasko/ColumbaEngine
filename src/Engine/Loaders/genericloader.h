#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include "constant.h"

namespace pg
{

    // Base class for an individual resource entry
    struct ResourceEntry
    {
        size_t id = 0;
        std::string name;

        inline size_t getId() const { return id; }
        inline const std::string& getName() const { return name; }

        inline void setId(size_t newId) { id = newId; }
        inline void setName(const std::string& newName) { name = newName; }
    };

    // Generic loader for resources composed of Entry types
    template<typename Entry>
    class ResourceLoader
    {
    public:
        ResourceLoader() = default;

        // Directly add a preconfigured entry
        void pushEntry(Entry&& entry)
        {
            entry.setId(entries_.size());
            entries_.emplace_back(std::move(entry));
            nameMap_[entries_.back().getName()] = entries_.back().getId();
        }

        // Accessors
        const Entry& getById(size_t id) const
        {
            if (id < entries_.size())
                return entries_[id];

            throw std::out_of_range("ResourceLoader: invalid id");
        }

        const Entry& getByName(const std::string& name) const
        {
            auto it = nameMap_.find(name);

            if (it != nameMap_.end())
                return getById(it->second);

                throw std::out_of_range("ResourceLoader: name not found");
        }

        size_t size() const { return entries_.size(); }
        bool empty() const { return entries_.empty(); }

    protected:
        void clear()
        {
            entries_.clear();
            nameMap_.clear();
        }

        std::vector<Entry> entries_;
        std::unordered_map<std::string, size_t> nameMap_;
    };

    // Specialized atlas texture entry
    struct AtlasTexture : public ResourceEntry
    {
        unsigned int width = 0;
        unsigned int height = 0;
        unsigned int yOffset = 0;
        constant::Vector4D textureLimit;

        inline unsigned int getWidth() const { return width; }
        inline unsigned int getHeight() const { return height; }
        inline unsigned int getOffset() const { return yOffset; }
        inline const constant::Vector4D& getTextureLimit() const { return textureLimit; }

        void setWidth(unsigned int w) { width = w; }
        void setHeight(unsigned int h) { height = h; }
        void setOffset(unsigned int o) { yOffset = o; }

        // Compute UV limits once dimensions and offsets are set
        void setMesh(unsigned int xPos, unsigned int yPos,
                     unsigned int atlasWidth, unsigned int atlasHeight)
{
            float u0 = static_cast<float>(xPos) / atlasWidth;
            float v0 = static_cast<float>(yPos) / atlasHeight;
            float u1 = static_cast<float>(xPos + width)  / atlasWidth;
            float v1 = static_cast<float>(yPos + height) / atlasHeight;
            textureLimit = { u0, v0, u1, v1 };
        }
    };

    // Generic atlas loader that holds atlas metadata and entries
    class AtlasLoader : public ResourceLoader<AtlasTexture>
    {
    public:
        AtlasLoader() = default;

        // Set atlas metadata
        AtlasLoader& setImagePath(const std::string& path)
        {
            imagePath = path;

            return *this;
        }

        AtlasLoader& setAtlasSize(unsigned int width, unsigned int height)
        {
            atlasWidth = width;
            atlasHeight = height;

            return *this;
        }

        const std::string& getImagePath() const { return imagePath; }
        unsigned int getAtlasWidth() const { return atlasWidth; }
        unsigned int getAtlasHeight() const { return atlasHeight; }

        // Convenience for external generators (e.g. Aseprite loader)
        AtlasLoader& addTexture(const std::string& name,
                        unsigned int w, unsigned int h,
                        unsigned int xPos, unsigned int yPos,
                        unsigned int yOff = 0)
        {
            if (w == 0 or h == 0)
            {
                LOG_ERROR("AtlasLoader", "AtlasTexture must have non-zero dimensions");
            }

            if (atlasWidth == 0 or atlasHeight == 0)
            {
                LOG_ERROR("AtlasLoader", "Atlas must have non-zero dimensions");
            }

            AtlasTexture tex;

            tex.setName(name);
            tex.setWidth(w);
            tex.setHeight(h);
            tex.setOffset(yOff);
            tex.setMesh(xPos, yPos, atlasWidth, atlasHeight);

            pushEntry(std::move(tex));

            return *this;
        }

    private:
        std::string imagePath;
        unsigned int atlasWidth = 0;
        unsigned int atlasHeight = 0;
    };

} // namespace pg
