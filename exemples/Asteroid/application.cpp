#include "application.h"

#include "ECS/entitysystem.h"

#include "ECS/standardsystem.h"

#include "logger.h"

using namespace pg;

namespace
{
    static const char *const DOM = "App";
}

StandardSystemImpl* createPlayerSystem()
{
    return createStandardSystem("PositionSystem")
        .onInit("init_player.pg")
        .ownComponent("Player")
        .onEvent("OnSDLScanCode", "move_player.pg")
        .useStoragePolicy() // Only react to events, no execute() needed
        .build();
}

GameApp::GameApp(const std::string &appName) : engine(appName)
{
    engine.setSetupFunction([this](EntitySystem& ecs, Window& window)
    {
        ecs.registerSystem(createPlayerSystem());
    });
}

GameApp::~GameApp()
{
}

int GameApp::exec()
{
    return engine.exec();
}
