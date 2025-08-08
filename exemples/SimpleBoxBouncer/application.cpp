#include "application.h"

#include "Systems/basicsystems.h"
#include "boxbouncersystem.h"

using namespace pg;

namespace
{
    static const char *const DOM = "App";
}

GameApp::GameApp(const std::string &appName) : engine(appName)
{
    engine.setSetupFunction([this](EntitySystem& ecs, Window& window)
    {
        ecs.createSystem<FpsSystem>();

        auto config = engine.getConfig();
        ecs.createSystem<BoxBouncerSystem>(config.width, config.height);
    });
}

GameApp::~GameApp()
{
}

int GameApp::exec()
{
    return engine.exec();
}
