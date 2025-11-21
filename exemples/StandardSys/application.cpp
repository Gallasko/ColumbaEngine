#include "application.h"

#include "ECS/entitysystem.h"
#include "ECS/standardsystem.h"

#include "logger.h"

#include <chrono>

// Todo remove this when we can close a window with an event
#include "window.h"

using namespace pg;

namespace
{
    static const char *const DOM = "App";
}

int test = 0;

StandardSystemImpl* createEventNotificationSystem()
{
    return createStandardSystem("EventNotification")
        .onInit([](StandardSystemHandle* sys) {
            // Initialize system
            LOG_INFO("EventNotification", "System initialized");
        })
        .onEvent("BasicEvent", [](StandardSystemHandle* sys, const StandardEvent& event) {
            bool verbose = true;

            test++;

            if (event.has("verbose"))
                verbose = event.get<bool>("verbose");

            if (verbose)
            {
                LOG_INFO("EventNotification", "Event received");
            }

            // Access event data
            if (event.has("CustomValue"))
            {
                auto value = event.getElement("CustomValue").toString();

                if (verbose)
                {
                    LOG_INFO("EventNotification", "Custom Value: " << value);
                }
            }
            else
            {
                if (verbose)
                {
                    LOG_INFO("EventNotification", "No Custom Value !");
                }
            }
        })
        .useStoragePolicy() // Only react to events, no execute() needed
        .build();
}

StandardSystemImpl* createScriptEventNotificationSystem()
{
    return createStandardSystem("ScriptEventNotification")
        .onInit([](StandardSystemHandle* sys) {
            // Initialize system
            LOG_INFO("ScriptEventNotification", "System initialized");
        })
        .onEvent("BasicScriptEvent", "scripthandler.pg")
        .useStoragePolicy() // Only react to events, no execute() needed
        .build();
}

StandardSystemImpl* createSimplePositionSystem()
{
    return createStandardSystem("SimplePosition")
        .onInit([](StandardSystemHandle* sys) {
            // Initialize system
            LOG_INFO("SimplePosition", "System initialized");
        })
        .ownComponent("SimplePosition")
        .ownComponent("SimpleObj", "name", "obj", "value", 5)
        // .onEvent("BasicScriptEvent", "scripthandler.pg")
        .useStoragePolicy() // Only react to events, no execute() needed
        .build();
}

StandardSystemImpl* createSimpleExecSystem()
{
    return createStandardSystem("SimpleExec")
        .onInit([](StandardSystemHandle* sys) {
            // Initialize system
            LOG_INFO("SimpleExec", "System initialized");
        })
        .onExecute("simpleExec.pg")
        .build();
}

StandardSystemImpl* createCompExecSystem()
{
    return createStandardSystem("CompExec")
        .onInit([](StandardSystemHandle* sys) {
            // Initialize system
            LOG_INFO("CompExec", "System initialized");
        })
        .ownComponent("ExecComp", "name", "exec", "value", 0)
        .onExecute("simpleExecComp.pg")
        .build();
}

StandardSystemImpl* createCompExecReactorSystem()
{
    return createStandardSystem("CompExecReactor")
        .onInit([](StandardSystemHandle* sys) {
            // Initialize system
            LOG_INFO("CompExec", "System initialized");
        })
        .onEvent("ChangedExecComp", "simpleExecReact.pg")
        .build();
}

GameApp::GameApp(const std::string &appName) : engine(appName)
{
    engine.setSetupFunction([this](EntitySystem& ecs, Window& window)
    {
        ecs.registerSystem(createEventNotificationSystem());

        // Basic Event without a custom value
        ecs.sendEvent(StandardEvent{"BasicEvent"});

        // Basic Event with a custom value
        ecs.sendEvent(StandardEvent{"BasicEvent", "CustomValue", "Hello World!"});

        auto start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < 10000; i++)
        {
            // Basic Event with a custom value
            ecs.sendEvent(StandardEvent{"BasicEvent", "CustomValue", "Hello World!", "verbose", false});
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        LOG_INFO(DOM, "i: " << test);

        LOG_INFO(DOM, "Event loop took " << duration.count() << " microseconds ("
                 << duration.count() / 1000.0 << " ms, "
                 << duration.count() / 1000000.0 << " s)");

        ecs.registerSystem(createScriptEventNotificationSystem());

        ecs.sendEvent(StandardEvent{"BasicScriptEvent"});
        ecs.sendEvent(StandardEvent{"BasicScriptEvent", "verbose", true, "test1", 15});
        ecs.sendEvent(StandardEvent{"BasicScriptEvent", "verbose", false});

        start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < 10000; i++)
        {
            // Basic Event with a custom value
            ecs.sendEvent(StandardEvent{"BasicScriptEvent", "CustomValue", "Hello World!", "verbose", false});
        }

        end = std::chrono::high_resolution_clock::now();
        duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        LOG_INFO(DOM, "Script Event loop took " << duration.count() << " microseconds ("
                 << duration.count() / 1000.0 << " ms, "
                 << duration.count() / 1000000.0 << " s)");

        ecs.registerSystem(createSimplePositionSystem());

        {
            auto ent = ecs.createEntity();

            // auto simplePos = ent.attach("SimplePosition");
            auto simplePos = ecs.attach(ent, "SimplePosition", "x", 5, "y", 10);

            LOG_INFO("SimplePos", simplePos->typeName);
            LOG_INFO("SimplePos", simplePos->get<int>("x"));
        }

        {
            auto ent = ecs.createEntity();

            // auto simpleObj = ent.attach<StandardComponent>("SimpleObj");
            auto simpleObj = ecs.attach(ent, "SimpleObj");

            LOG_INFO("SimpleObj", simpleObj->typeName);
            LOG_INFO("SimpleObj", simpleObj->get<std::string>("name"));
        }

        auto simpleSys = ecs.registerSystem(createSimpleExecSystem());

        ecs.executeOnce();

        ecs.registerSystem(createCompExecReactorSystem());

        auto compSys = ecs.registerSystem(createCompExecSystem());

        {
            auto ent = ecs.createEntity();

            auto execComp = ecs.attach(ent, "ExecComp");
        }

        ecs.executeOnce();

        window.receivedQuitRequest();

    });
}

GameApp::~GameApp()
{
}

int GameApp::exec()
{
    return engine.exec();
}
