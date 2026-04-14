#include "application.h"
#include "window.h"

#include "Systems/basicsystems.h"
#include "Loaders/Aseprite/asepriteloader.h"
#include "Loaders/Aseprite/asepritefileatlasloader.h"
#include "gamesystem.h"

using namespace pg;

GameApp::GameApp(const std::string &appName) : engine(appName)
{
    registry = createDefaultRegistry();

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

        float screenW = static_cast<float>(config.width);
        float screenH = static_cast<float>(config.height);

        // Camera must be created first so it exists before grid renders
        auto* cameraSystem = ecs.createSystem<CameraSystem>(
            window.masterRenderer, screenW, screenH);

        auto* gridSystem = ecs.createSystem<GridSystem>(&registry);

        auto* toolbarSystem = ecs.createSystem<ToolbarSystem>(
            &registry, screenW, screenH);

        ecs.createSystem<GameSystem>(gridSystem, cameraSystem, toolbarSystem, &registry);
    });
}

GameApp::~GameApp()
{
}

int GameApp::exec()
{
    return engine.exec();
}
