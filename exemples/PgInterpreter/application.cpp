#include "application.h"

#include "logger.h"

#include "Systems/coremodule.h"
#include "Systems/logmodule.h"
#include "Systems/timemodule.h"
#include "Interpreter/systemfunction.h"

using namespace pg;

namespace {
    static const char *const DOM = "App";
}

InterpreterApp::InterpreterApp(const std::string &fileName) : fileName(fileName) {
    LOG_THIS_MEMBER(DOM);
}

InterpreterApp::~InterpreterApp() {
    LOG_THIS_MEMBER(DOM);
}

int InterpreterApp::exec()
{
    LOG_THIS_MEMBER(DOM);

    EntitySystem ecs;

    PgInterpreter *interpreter = ecs.createSystem<PgInterpreter>();

    auto terminalSink = std::shared_ptr<pg::Logger::LogSink>(pg::Logger::registerSink<pg::TerminalSink>());

    terminalSink->addFilter("log", new pg::Logger::LogSink::FilterLogLevel(pg::Logger::InfoLevel::log));
    terminalSink->addFilter("info", new pg::Logger::LogSink::FilterLogLevel(pg::Logger::InfoLevel::info));
    terminalSink->addFilter("mile", new pg::Logger::LogSink::FilterLogLevel(pg::Logger::InfoLevel::mile));
    terminalSink->addFilter("test", new pg::Logger::LogSink::FilterLogLevel(pg::Logger::InfoLevel::test));
    terminalSink->addFilter("warn", new pg::Logger::LogSink::FilterLogLevel(pg::Logger::InfoLevel::warning));
    // terminalSink->addFilter("error", new pg::Logger::LogSink::FilterLogLevel(pg::Logger::InfoLevel::error));

    interpreter->addSystemFunction<TestPrint>("print");
    interpreter->addSystemFunction<DebugPrint>("debugPrint");
    interpreter->addSystemFunction<ToString>("toString");

    interpreter->addSystemModule("log", LogModule{terminalSink});
    interpreter->addSystemModule("time", TimeModule{&ecs});
    interpreter->addSystemModule("ecs", EcsModule{&ecs});
    interpreter->addSystemModule("core", CoreModule{&ecs});

    // interpreter->addSystemModule("ui", UiModule{ecs});
    // interpreter->addSystemModule("2Dshapes", Shape2DModule{ecs});
    // interpreter->addSystemModule("2Dtexture", Texture2DModule{ecs});
    // interpreter->addSystemModule("input", InputModule{ecs});
    // interpreter->addSystemModule("uitext", SentenceModule{ecs});
    // interpreter->addSystemModule("scene", SceneModule{ecs});
    // interpreter->addSystemModule("audio", AudioModule{ecs});

    interpreter->interpretFromFile(fileName);

    return 0;
}
