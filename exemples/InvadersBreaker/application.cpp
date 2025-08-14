#include "application.h"

#include "window.h"

#include "Systems/basicsystems.h"
#include "player.h"
#include "enemy.h"
#include "gamestate.h"
#include "hud.h"

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
        auto ttfSys = ecs.createSystem<TTFTextSystem>(window.masterRenderer);

        #ifdef __EMSCRIPTEN__
            // Need to fix this
            ttfSys->registerFont("/res/font/Inter/static/Inter_28pt-Light.ttf", "light");
            ttfSys->registerFont("/res/font/Inter/static/Inter_28pt-Bold.ttf", "bold");
            ttfSys->registerFont("/res/font/Inter/static/Inter_28pt-Italic.ttf", "italic");
        #else
            ttfSys->registerFont("res/font/Inter/static/Inter_28pt-Light.ttf", "light");
            ttfSys->registerFont("res/font/Inter/static/Inter_28pt-Bold.ttf", "bold");
            ttfSys->registerFont("res/font/Inter/static/Inter_28pt-Italic.ttf", "italic");
        #endif

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

        ecs.createSystem<HUDSystem>();
    });
}

GameApp::~GameApp()
{
}

int GameApp::exec()
{
    return engine.exec();
}
