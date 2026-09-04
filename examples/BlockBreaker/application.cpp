#include "application.h"

#include "logger.h"

#include "ECS/entitysystem.h"

#include "window.h"

#include "Interpreter/pginterpreter.h"

using namespace pg;

namespace
{
    static const char *const DOM = "BlockBreaker";
}

GameApp::GameApp(const std::string& appName, const std::string& scriptPath) : engine(appName)
{
    engine.setSetupFunction([scriptPath](EntitySystem&, Window& window)
    {
        LOG_INFO(DOM, "Running game script: " << scriptPath);

        window.interpreter->interpretFromFile(scriptPath);
    });
}

GameApp::~GameApp()
{
}

int GameApp::exec()
{
    return engine.exec();
}
