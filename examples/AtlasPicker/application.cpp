#include "application.h"
#include "window.h"

#include "Renderer/camera.h"
#include "UI/ttftext.h"
#include "gridatlas.h"
#include "atlaspickersystem.h"

using namespace pg;

namespace
{
    // Atlas configuration — change these to pick a different atlas
    const char* ATLAS_NAME       = "PixelwoodIcons";
    const char* ATLAS_IMAGE_PATH = "res/ext/Pixelwood Valley Icon Pack 1.0/1.0/Items 16x16.png";
    const char* ATLAS_IMAGE_FILE = "Items 16x16.png";
    const unsigned int ATLAS_W   = 336;
    const unsigned int ATLAS_H   = 240;
    const unsigned int TILE_W    = 16;
    const unsigned int TILE_H    = 16;
    const unsigned int COLS      = 21;
    const unsigned int COUNT     = 315;
    const float TILE_SCALE       = 3.0f;

    const char* FONT_PATH = "res/font/Inter/static/Inter_28pt-Light.ttf";
}

GameApp::GameApp(const std::string &appName) : engine(appName)
{
    auto cfg = engine.getConfig();
    cfg.width = static_cast<int>(COLS * TILE_W * TILE_SCALE);
    cfg.height = static_cast<int>((COUNT / COLS + 1) * TILE_H * TILE_SCALE);
    engine.setConfig(cfg);

    engine.setSetupFunction([this](EntitySystem& ecs, Window& window)
    {
        auto config = engine.getConfig();
        float screenW = static_cast<float>(config.width);
        float screenH = static_cast<float>(config.height);

        // Register the atlas
        window.masterRenderer->registerAtlasTexture(
            ATLAS_NAME,
            ATLAS_IMAGE_PATH,
            "",
            std::make_unique<GridAtlas>(ATLAS_IMAGE_FILE, ATLAS_W, ATLAS_H,
                                        TILE_W, TILE_H, COLS, COUNT));

        // TTF text system
        auto* ttfSys = ecs.createSystem<TTFTextSystem>(window.masterRenderer);
        ttfSys->registerFont(FONT_PATH);

        // Camera for viewport 0 (world — the tile grid)
        auto worldCam = ecs.createEntity();
        {
            auto cam = ecs._attach<BaseCamera2D>(worldCam);
            cam->setWidth(screenW);
            cam->setHeight(screenH);
            window.masterRenderer->queueRegisterCamera(worldCam->id);
        }

        // Camera for viewport 2 (UI overlay — the ID label)
        auto uiCam = ecs.createEntity();
        {
            auto cam = ecs._attach<BaseCamera2D>(uiCam);
            cam->setWidth(screenW);
            cam->setHeight(screenH);
            window.masterRenderer->queueRegisterCamera(uiCam->id);
        }

        // The picker system
        ecs.createSystem<AtlasPickerSystem>(
            window.masterRenderer,
            std::string(ATLAS_NAME),
            std::string(FONT_PATH),
            TILE_W, TILE_H, COLS, COUNT,
            TILE_SCALE,
            screenW, screenH);
    });
}

GameApp::~GameApp()
{
}

int GameApp::exec()
{
    return engine.exec();
}
