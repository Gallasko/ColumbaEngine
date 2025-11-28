//
// Created by nicol on 5/15/2025.
//

#pragma once

#include <vector>
#include <unordered_map>
#include <string>
#include <ostream>
#include <iostream>
#include <map>

#include "logger.h"

namespace pg
{
    struct AsepriteAnimation
    {
        std::string name;
        int frameStart;
        int frameEnd;
        bool reversed;
    };

    struct AsepriteFrame
    {
        int topLeftCornerInSPixelsX;
        int topLeftCornerInSPixelsY;
        int widthInSPixels;
        int heightInSPixels;
        float durationInMilliseconds;
        std::string textureName;
    };

    struct AsepriteMetadata
    {
        std::string imagePath;
        int imageWidthInSPixels;
        int imageHeightInSPixels;
        std::vector<AsepriteAnimation> anims;
    };

    struct AsepriteFile
    {
        /** Use it to know the coordinates on the atlas to load */
        std::vector<AsepriteFrame> frames;

        /** Use it to know the anims and the image to load with its size */
        AsepriteMetadata metadata;

        /** Used for texture atlas */
        std::string filename;

        std::map<std::string, std::string> customData;

        std::unordered_map<std::string, std::vector<AsepriteFrame>> animations;

        std::vector<AsepriteFrame>& operator[](const std::string &name)
        {
            if (animations.find(name) == animations.end())
            {
                LOG_WARNING("AsepriteFile", "Animation not found: " << name);
            }

            return animations[name];
        }

        const std::vector<AsepriteFrame>& operator[](const std::string &name) const
        {
            auto it = animations.find(name);

            if (it == animations.end())
            {
                LOG_WARNING("AsepriteFile", "Animation not found: " << name);
                return __emptyAnimation;
            }

            return it->second;
        }

        static std::vector<AsepriteFrame> __emptyAnimation;
    };
}

inline std::ostream &operator<<(std::ostream &os, const pg::AsepriteAnimation& data)
{
    os << "Animation {\n"
        << "  name: " << data.name << "\n"
        << "  frame start: " << data.frameStart << "\n"
        << "  frame end: " << data.frameEnd << "\n"
        << " reversed : " << data.reversed << "\n"
        << "}";

    return os;
}

inline std::ostream &operator<<(std::ostream &os, const pg::AsepriteFrame& data)
{
    os << "Frame {\n"
        << "  Top left Corner X S pixels: " << data.topLeftCornerInSPixelsX << "\n"
        << "  Top left Corner Y S pixels: " << data.topLeftCornerInSPixelsY << "\n"
        << "  width in S Pixels: " << data.widthInSPixels << "\n"
        << "  height in S Pixels: " << data.heightInSPixels << "\n"
        << "  frame duration in ms: " << data.durationInMilliseconds << "\n"
        << "}";
    return os;
}

inline std::ostream &operator<<(std::ostream &os, const pg::AsepriteMetadata& data)
{
    os << "Metadata {\n"
            << "  Image path: " << data.imagePath << "\n"
            << "  image width in S Pixels: " << data.imageWidthInSPixels << "\n"
            << "  image height in S Pixels: " << data.imageHeightInSPixels << "\n"
            << "}";

    for (const auto &anim: data.anims)
    {
        os << "    " << anim << "\n";
    }

    os << "  ]\n}";

    return os;
}

inline std::ostream &operator<<(std::ostream &os, const pg::AsepriteFile& data)
{
    os << "Aseprite File {\n"
            << "  metadata: " << data.metadata << "\n"
            << "}";

    /*for (const auto &frame: data.frames) {
        os << "    " << frame << "\n";
    }
    os << "  ]\n}";*/

    for (const auto &[key, val] : data.animations) {
        os << key  << "\n";
        for (const auto &frame: val) {
            os << "    " << frame << "\n";
        }
    }

    os << "  CUSTOM DATA \n";

    for (const auto &[key, val] : data.customData) {
        os << key  << " : " << val << "\n";
    }

    return os;
}
