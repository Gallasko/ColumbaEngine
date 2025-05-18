//
// Created by nicol on 5/15/2025.
//

#include "AsepriteLoader.h"
#include "json.hpp"

#include <fstream>
#include <filesystem>
namespace fs = std::filesystem;

using json = nlohmann::json;

AsepriteFile AsepriteLoader::loadAnim(const std::string &path) {
    AsepriteFile result;

    std::ifstream in(path);
    if (!in.is_open()) {
        throw std::runtime_error("Failed to open: " + path);
    }

    fs::path jsonPath = fs::absolute(path);                  // full path to JSON
    fs::path jsonDir = jsonPath.parent_path();               // directory of JSON

    json j;
    in >> j;

    // Parse metadata
    const auto& meta = j["meta"];
    std::string filenameWithoutExtension = std::filesystem::path(meta["image"]).stem().string();

    result.filename = filenameWithoutExtension;

    int i = 0;

    // Parse frames
    for (const auto& frameEntry : j["frames"]) {
        const auto& frame = frameEntry["frame"];
        float duration = frameEntry["duration"];

        AsepriteFrame f;
        f.topLeftCornerInSPixelsX = frame["x"];
        f.topLeftCornerInSPixelsY = frame["y"];
        f.widthInSPixels = frame["w"];
        f.heightInSPixels = frame["h"];
        f.durationInMilliseconds = duration;

        f.textureName = filenameWithoutExtension + "." + std::to_string(i);

        result.frames.push_back(f);

        i++;
    }



    fs::path imagePath = meta["image"];       // relative to JSON
    fs::path resolvedPath = fs::absolute(jsonDir / imagePath);  // full path

    // Now you can make it relative to current working dir if needed:
    fs::path relativeToCWD = fs::relative(resolvedPath, fs::current_path());

    result.metadata.imagePath = relativeToCWD.string();
    result.metadata.imageWidthInSPixels = meta["size"]["w"];
    result.metadata.imageHeightInSPixels = meta["size"]["h"];

    for (const auto& tag : meta["frameTags"]) {
        AsepriteAnimation anim;
        anim.name = tag["name"];
        anim.frameStart = tag["from"];
        anim.frameEnd = tag["to"];
        if (tag.contains("data")) {
            anim.customData = tag["data"];
        }
        result.metadata.anims.push_back(anim);
    }

    for (const auto& anim : result.metadata.anims) {
        std::vector<AsepriteFrame> frames;
        for (int i = anim.frameStart; i <= anim.frameEnd; ++i) {
                frames.push_back(result.frames[i]);
        }
        result.animations[anim.name] = frames;
    }

    return result;
}
