//
// Created by nicol on 5/15/2025.
//

#pragma once

#include <string>

#include "ECS/system.h"

#include "asepritefile.h"

namespace pg
{
    class AsepriteLoader : public System<StoragePolicy>
    {
    public:
        AsepriteFile loadAnim(const std::string &path, const std::string &shortname);

        AsepriteFile& getLoadedFile(const std::string &shortname)
        {
            if (loadedAnims.find(shortname) == loadedAnims.end())
            {
                LOG_ERROR("AsepriteLoader", "Aseprite file not loaded: " << shortname << ". Loading now.");
                loadedAnims[shortname] = loadAnim(shortname, shortname);
            }

            return loadedAnims[shortname];
        }

        std::vector<AsepriteFrame>& getAnimationFrames(const std::string &path, const std::string &animName)
        {
            AsepriteFile &asepriteFile = getLoadedFile(path);

            if (asepriteFile.animations.find(animName) == asepriteFile.animations.end())
            {
                LOG_ERROR("AsepriteLoader", "Animation not found: " << animName << " in file: " << path);
                return __emptyAnimation;
            }

            return asepriteFile.animations[animName];
        }

        std::string getFirstFrame(const std::string &path)
        {
            AsepriteFile &asepriteFile = getLoadedFile(path);

            if (asepriteFile.frames.empty())
            {
                LOG_ERROR("AsepriteLoader", "No frames found in file: " << path);
                return "";
            }

            return asepriteFile.frames[0].textureName;
        }

    static std::vector<AsepriteFrame> __emptyAnimation;

    private:
        std::unordered_map<std::string, AsepriteFile> loadedAnims;
    };
}