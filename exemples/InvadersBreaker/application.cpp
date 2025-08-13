#include "application.h"

#include "Systems/basicsystems.h"
#include "player.h"
#include "enemy.h"
#include "gamestate.h"

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

        // Physics
        ecs.createSystem<BallPhysicsSystem>();
        ecs.createSystem<BulletPhysicsSystem>();

        // AI
        ecs.createSystem<AlienFormationSystem>();
        ecs.createSystem<AlienShootingSystem>();

        ecs.createSystem<EntityCollisionSystem>();
        ecs.createSystem<AlienCollisionSystem>(); // Everything else

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
