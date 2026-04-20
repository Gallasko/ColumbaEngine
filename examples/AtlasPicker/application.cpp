#include "application.h"
#include "window.h"

#include "Renderer/camera.h"
#include "UI/ttftext.h"
#include "gridatlas.h"
#include "atlaspickersystem.h"

#include "Helpers/tinyfiledialogs.h"
#include "Loaders/stb_image.h"

#include <cstdlib>
#include <string>

using namespace pg;

namespace
{
    const float TILE_SCALE = 3.0f;
    const char* FONT_PATH  = "res/font/Inter/static/Inter_28pt-Light.ttf";

    // Extract the filename (without directory) from a full path
    std::string extractFilename(const std::string& path)
    {
        auto pos = path.find_last_of("/\\");
        return (pos == std::string::npos) ? path : path.substr(pos + 1);
    }

    // Extract the filename stem (without directory or extension)
    std::string extractStem(const std::string& path)
    {
        auto name = extractFilename(path);
        auto dot  = name.find_last_of('.');
        return (dot == std::string::npos) ? name : name.substr(0, dot);
    }
}

GameApp::GameApp(const std::string &appName) : engine(appName)
{
    // --- 1. File dialog: choose the atlas image ---
    char const* imageFilters[3] = { "*.png", "*.jpg", "*.bmp" };
    char* selectedPath = tinyfd_openFileDialog(
        "Open a grid atlas image",
        "",
        3,
        imageFilters,
        "Image files (png, jpg, bmp)",
        0);

    if (!selectedPath)
        std::exit(0);

    std::string atlasImagePath = selectedPath;
    std::string atlasImageFile = extractFilename(atlasImagePath);
    std::string atlasName      = extractStem(atlasImagePath);

    // --- 2. Infer atlas dimensions from the image ---
    int atlasW = 0, atlasH = 0, comp = 0;
    if (!stbi_info(atlasImagePath.c_str(), &atlasW, &atlasH, &comp))
    {
        tinyfd_messageBox("Error", "Could not read image dimensions.", "ok", "error", 1);
        std::exit(1);
    }

    // --- 3. Input dialogs: tile width and height ---
    char const* tileWStr = tinyfd_inputBox("Tile width", "Enter the tile width in pixels:", "16");
    if (!tileWStr)
        std::exit(0);
    unsigned int tileW = static_cast<unsigned int>(std::stoi(tileWStr));

    char const* tileHStr = tinyfd_inputBox("Tile height", "Enter the tile height in pixels:", "16");
    if (!tileHStr)
        std::exit(0);
    unsigned int tileH = static_cast<unsigned int>(std::stoi(tileHStr));

    if (tileW == 0 || tileH == 0)
    {
        tinyfd_messageBox("Error", "Tile dimensions must be greater than zero.", "ok", "error", 1);
        std::exit(1);
    }

    // --- 4. Compute derived values ---
    unsigned int cols  = static_cast<unsigned int>(atlasW) / tileW;
    unsigned int rows  = static_cast<unsigned int>(atlasH) / tileH;
    unsigned int count = cols * rows;

    // --- 5. Configure window size ---
    auto cfg = engine.getConfig();
    cfg.width  = static_cast<int>(cols * tileW * TILE_SCALE);
    cfg.height = static_cast<int>((count / cols + 1) * tileH * TILE_SCALE);
    engine.setConfig(cfg);

    // --- 6. Engine setup (captures the dialog results) ---
    engine.setSetupFunction([this, atlasName, atlasImagePath, atlasImageFile,
                             atlasW, atlasH, tileW, tileH, cols, count]
                            (EntitySystem& ecs, Window& window)
    {
        auto config = engine.getConfig();
        float screenW = static_cast<float>(config.width);
        float screenH = static_cast<float>(config.height);

        // Register the atlas
        window.masterRenderer->registerAtlasTexture(
            atlasName.c_str(),
            atlasImagePath.c_str(),
            "",
            std::make_unique<GridAtlas>(atlasImageFile,
                                        static_cast<unsigned int>(atlasW),
                                        static_cast<unsigned int>(atlasH),
                                        tileW, tileH, cols, count));

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
            atlasName,
            std::string(FONT_PATH),
            tileW, tileH, cols, count,
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
