#include "application.h"

#include "Systems/basicsystems.h"
#include "player.h"
#include "enemy.h"
#include "gamestate.h"

#include "bgscroller.h"

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

        ecs.createSystem<BackgroundScrollerSystem>();

        // auto config = engine.getConfig();
        ecs.createSystem<PaddleControlSystem>();

        // Physics
        ecs.createSystem<BallPhysicsSystem>();
        ecs.createSystem<BulletPhysicsSystem>();

        // AI
        ecs.createSystem<AlienFormationSystem>();
        ecs.createSystem<AlienShootingSystem>();

        // Collision
        ecs.createSystem<EntityCollisionSystem>();
        ecs.createSystem<AlienCollisionSystem>();

        ecs.createSystem<DangerZoneVisualSystem>();

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
