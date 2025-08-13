#include "application.h"

#include "Systems/basicsystems.h"
#include "player.h"

using namespace pg;

namespace
{
    static const char *const DOM = "App";
}

GameApp::GameApp(const std::string &appName) : engine(appName)
{
    engine.setSetupFunction([this](EntitySystem& ecs, Window& window)
    {
        // ecs.createSystem<FpsSystem>();

        // auto config = engine.getConfig();
        ecs.createSystem<PaddleControlSystem>();
        ecs.createSystem<BallPhysicsSystem>();
        ecs.createSystem<EntityCollisionSystem>();
        ecs.createSystem<GameStateSystem>();
    });
}

GameApp::~GameApp()
{
}

int GameApp::exec()
{
    return engine.exec();
}
