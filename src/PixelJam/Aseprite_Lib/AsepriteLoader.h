//
// Created by nicol on 5/15/2025.
//

#ifndef ASEPRITELOADER_H
#define ASEPRITELOADER_H
#include <string>

#include "AsepriteFile.h"


class AsepriteLoader {
public:
    AsepriteFile loadAnim(const std::string &path);
};

#endif //ASEPRITELOADER_H
