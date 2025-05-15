//
// Created by nicol on 5/15/2025.
//

#ifndef ASEPRITEANIMDATA_H
#define ASEPRITEANIMDATA_H

#include <vector>
#include <string>
#include <ostream>

struct AsepriteAnimation {
    std::string name;
    int frameStart;
    int frameEnd;
    std::string customData;
};

inline std::ostream &operator<<(std::ostream &os, const AsepriteAnimation &data) {
    os << "Animation {\n"
            << "  name: " << data.name << "\n"
            << "  frame start: " << data.frameStart << "\n"
            << "  frame end: " << data.frameEnd << "\n"
            << "  custom data: " << data.customData << "\n"
            << "}";
    return os;
}

struct AsepriteFrame {
    int topLeftCornerInSPixelsX;
    int topLeftCornerInSPixelsY;
    int widthInSPixels;
    int heightInSPixels;
    float durationInMilliseconds;
};

inline std::ostream &operator<<(std::ostream &os, const AsepriteFrame &data) {
    os << "Frame {\n"
            << "  Top left Corner X S pixels: " << data.topLeftCornerInSPixelsX << "\n"
            << "  Top left Corner Y S pixels: " << data.topLeftCornerInSPixelsY << "\n"
            << "  width in S Pixels: " << data.widthInSPixels << "\n"
            << "  height in S Pixels: " << data.heightInSPixels << "\n"
    << "  frame duration in ms: " << data.durationInMilliseconds << "\n"
            << "}";
    return os;
}


struct  AsepriteMetadata {
    std::string imagePath;
    int imageWidthInSPixels;
    int imageHeightInSPixels;
    std::vector<AsepriteAnimation> anims;
};

inline std::ostream &operator<<(std::ostream &os, const AsepriteMetadata &data) {
    os << "Metadata {\n"
            << "  Image path: " << data.imagePath << "\n"
            << "  image width in S Pixels: " << data.imageWidthInSPixels << "\n"
            << "  image height in S Pixels: " << data.imageHeightInSPixels << "\n"
            << "}";

    for (const auto &anim: data.anims) {
        os << "    " << anim << "\n";
    }
    os << "  ]\n}";

    return os;
}

struct AsepriteFile {
    /*!
     * Use it to know the coordinates on the atlas to load
     */
    std::vector<AsepriteFrame> frames;
    /*!
     * Use it to know the anims and the image to load with its size
     */
    AsepriteMetadata metadata;
};

inline std::ostream &operator<<(std::ostream &os, const AsepriteFile &data) {
    os << "Aseprite File {\n"
            << "  metadatah: " << data.metadata << "\n"
            << "}";

    for (const auto &frame: data.frames) {
        os << "    " << frame << "\n";
    }
    os << "  ]\n}";

    return os;
}

#endif //ASEPRITEANIMDATA_H
