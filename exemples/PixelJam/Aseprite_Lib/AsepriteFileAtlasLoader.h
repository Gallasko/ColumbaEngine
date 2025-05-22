//
// Created by nicol on 5/17/2025.
//

#ifndef ASEPRITEFILEATLASLOADER_H
#define ASEPRITEFILEATLASLOADER_H

#include "AsepriteFile.h"
#include "Loaders/atlasloader.h"

namespace pg {
    class AsepriteFileAtlasLoader : public LoadedAtlas {
    public:
        AsepriteFileAtlasLoader(const AsepriteFile &aseprite);
    };
}



#endif //ASEPRITEFILEATLASLOADER_H
