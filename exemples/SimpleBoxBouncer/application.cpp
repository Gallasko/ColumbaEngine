#include "application.h"

#include "Systems/basicsystems.h"

using namespace pg;

namespace
{
    static const char *const DOM = "App";
}

GameApp::GameApp(const std::string &appName) : engine(appName)
{
    engine.setSetupFunction([](EntitySystem& ecs, Window& window) {
        ecs.createSystem<FpsSystem>();
    });
}

GameApp::~GameApp()
{
}

int GameApp::exec()
{
    return engine.exec();
}
