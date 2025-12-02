#include "application.h"

#include "ECS/entitysystem.h"

#include "ECS/standardsystem.h"

#include "logger.h"

#include "2D/texture.h"

using namespace pg;

namespace
{
    static const char *const DOM = "App";
}

StandardSystemImpl* createPlayerSystem()
{
    return createStandardSystem("PositionSystem")
        .onInit("res/asteroid/init_player.pg")
        .ownComponent("Player")
        .onEvent("OnSDLScanCode", "res/asteroid/move_player.pg")
        .useStoragePolicy() // Only react to events, no execute() needed
        .build();
}

StandardSystemImpl* createEnemySpawnSystem()
{
    return createStandardSystem("EnemySpawnSystem")
        .onInit([](StandardSystemHandle* sys)
        {
            LOG_MILE(DOM, "EnemySpawnSystem initialized");

            sys->setData("spawnTimer", 0.0f);
            sys->setData("x", 0);
        })
        .ownComponent("Enemy")
        .onDelta("res/asteroid/spawn_enemies.pg")
        .build();
}

GameApp::GameApp(const std::string &appName) : engine(appName)
{
    engine.setSetupFunction([this](EntitySystem& ecs, Window& window)
    {
        ecs.registerSystem(createPlayerSystem());

        ecs.registerSystem(createEnemySpawnSystem());
    });
}

GameApp::~GameApp()
{
}

int GameApp::exec()
{
    return engine.exec();
}
