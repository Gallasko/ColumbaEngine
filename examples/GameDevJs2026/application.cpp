#include "application.h"
#include "window.h"

#include "Systems/basicsystems.h"
#include "Loaders/Aseprite/asepriteloader.h"
#include "Loaders/Aseprite/asepritefileatlasloader.h"
#include "gamesystem.h"

using namespace pg;

GameApp::GameApp(const std::string &appName) : engine(appName)
{
    engine.setSetupFunction([this](EntitySystem& ecs, Window& window)
    {
        auto config = engine.getConfig();

        // Aseprite loader for sprite atlas metadata
        auto* asepriteLoader = ecs.createSystem<AsepriteLoader>();

        // Load conveyor belt sprite atlas
        auto anim = asepriteLoader->loadAnim(
            "res/ext/Automation Components/Conveyor_Belt.json", "Conveyor_Belt");

        window.masterRenderer->registerAtlasTexture(
            anim.filename,
            anim.metadata.imagePath.c_str(),
            "",
            std::make_unique<AsepriteFileAtlasLoader>(anim));

        // Camera must be created first so it exists before grid renders
        auto* cameraSystem = ecs.createSystem<CameraSystem>(
            window.masterRenderer,
            static_cast<float>(config.width),
            static_cast<float>(config.height));

        auto* gridSystem = ecs.createSystem<GridSystem>();

        ecs.createSystem<GameSystem>(gridSystem, cameraSystem);
    });
}

GameApp::~GameApp()
{
}

int GameApp::exec()
{
    return engine.exec();
}
