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
        AsepriteFile loadAnim(const std::string &path);

        AsepriteFile& getLoadedFile(const std::string &path)
        {
            if (loadedAnims.find(path) == loadedAnims.end())
            {
                loadedAnims[path] = loadAnim(path);
            }

            return loadedAnims[path];
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

    static std::vector<AsepriteFrame> __emptyAnimation;

    private:
        std::unordered_map<std::string, AsepriteFile> loadedAnims;
    };
}