#include "atlaspickersystem.h"

#include "2D/texture.h"
#include "2D/position.h"
#include "UI/ttftext.h"

#include <cmath>
#include <string>

#ifdef __EMSCRIPTEN__
    #include <SDL2/SDL.h>
#else
    #ifdef __linux__
        #include <SDL2/SDL.h>
    #elif _WIN32
        #include <SDL.h>
    #endif
#endif

using namespace pg;

AtlasPickerSystem::AtlasPickerSystem(MasterRenderer* renderer,
                                     const std::string& atlasName,
                                     const std::string& fontPath,
                                     unsigned int tileW, unsigned int tileH,
                                     unsigned int cols, unsigned int totalCount,
                                     float scale,
                                     float screenW, float screenH)
    : renderer(renderer)
    , atlasName(atlasName)
    , fontPath(fontPath)
    , tileW(tileW), tileH(tileH)
    , cols(cols), totalCount(totalCount)
    , scale(scale)
    , screenW(screenW), screenH(screenH)
{
    displayTileW = tileW * scale;
    displayTileH = tileH * scale;
}

void AtlasPickerSystem::init()
{
    // Create a tile entity for each frame in the atlas
    for (unsigned int i = 0; i < totalCount; ++i)
    {
        unsigned int col = i % cols;
        unsigned int row = i / cols;

        std::string texName = atlasName + "." + std::to_string(i);

        auto tile = make2DTexture(ecsRef, displayTileW, displayTileH, texName);

        auto pos = tile.get<PositionComponent>();
        pos->setX(col * displayTileW);
        pos->setY(row * displayTileH);
        pos->setZ(0.5f);

        tileEntityIds.push_back(tile.entity->id);
    }

    // Create the ID label (rendered in UI viewport so it stays on top)
    auto label = makeTTFText(ecsRef,
        10.0f, screenH - 40.0f, 0.99f,
        fontPath, "Click a tile to see its ID",
        0.5f,
        {255.0f, 255.0f, 0.0f, 255.0f});

    label.get<TTFText>()->setViewport(2);
    labelEntityId = label.entity->id;
}

void AtlasPickerSystem::onEvent(const OnMouseClick& event)
{
    if (event.button != SDL_BUTTON_LEFT)
        return;

    float mx = event.pos.x;
    float my = event.pos.y;

    int col = static_cast<int>(std::floor(mx / displayTileW));
    int row = static_cast<int>(std::floor(my / displayTileH));

    if (col < 0 or row < 0 or static_cast<unsigned int>(col) >= cols)
        return;

    unsigned int id = row * cols + col;

    if (id >= totalCount)
        return;

    // Update the label text
    auto labelEntity = ecsRef->getEntity(labelEntityId);
    if (labelEntity)
    {
        auto ttf = labelEntity->get<TTFText>();
        ttf->setText("ID: " + std::to_string(id));
    }
}
