#include "application.h"
#include "window.h"

#include "Systems/basicsystems.h"
#include "gamesystem.h"

using namespace pg;

GameApp::GameApp(const std::string &appName) : engine(appName)
{
    engine.setSetupFunction([this](EntitySystem& ecs, Window& window)
    {
        auto config = engine.getConfig();

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
