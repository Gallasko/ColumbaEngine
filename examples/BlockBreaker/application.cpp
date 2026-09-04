#include "application.h"

#include "logger.h"

#include "ECS/entitysystem.h"

#include "Compiler/vm.h"

#include "2D/collisionsystem.h"

#include "window.h"

using namespace pg;

namespace
{
    static const char *const DOM = "BlockBreaker";
}

GameApp::GameApp(const std::string& appName, const std::string& scriptPath) : engine(appName)
{
    engine.setSetupFunction([scriptPath](EntitySystem& ecs, Window&)
    {
        LOG_INFO(DOM, "Running game script: " << scriptPath);

        // Collision infrastructure: entities that attach a "Collision"
        // component are checked by the CollisionSystem, and the handlers
        // below run a script (with ent1/ent2 globals) on each contact.
        ecs.enableCollision();

        makeCollisionHandleScript(&ecs, "examples/BlockBreaker/scripts/ball_paddle_collision.pg",
            [](Entity* ent) { return ent->has("Ball"); },
            [](Entity* ent) { return ent->has("Paddle"); });

        makeCollisionHandleScript(&ecs, "examples/BlockBreaker/scripts/ball_brick_collision.pg",
            [](Entity* ent) { return ent->has("Ball"); },
            [](Entity* ent) { return ent->has("Brick"); });

        // The script does everything else: it registers the game systems and
        // their per-frame / event hooks through the "ecs" module (createSystem).
        VM vm;
        ecs.setupVm(vm);

        vm.interpretFromFile(scriptPath);
    });
}

GameApp::~GameApp()
{
}

int GameApp::exec()
{
    return engine.exec();
}
