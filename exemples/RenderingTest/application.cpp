#include "application.h"

#include "2D/texture.h"

using namespace pg;

namespace
{
    static const char *const DOM = "App";
}

GameApp::GameApp(const std::string &appName) : engine(appName)
{
    engine.setSetupFunction([this](EntitySystem& ecs, Window& window)
    {
        auto entity = make2DTexture(&ecs, 32.0f, 32.0f, "NoneIcon");

        entity.get<PositionComponent>()->x = 100.0f;
        entity.get<PositionComponent>()->y = 100.0f;

        LOG_INFO("App", "-- Test -- Created entity with ID: " << entity.id);
    });
}

GameApp::~GameApp()
{
}

int GameApp::exec()
{
    return engine.exec();
}
