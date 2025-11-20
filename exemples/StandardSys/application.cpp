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
        ecs.sendEvent(StandardEvent{"BasicScriptEvent", "verbose", true});
        ecs.sendEvent(StandardEvent{"BasicScriptEvent", "verbose", false});

        start = std::chrono::high_resolution_clock::now();

        // for (int i = 0; i < 10000; i++)
        // {
        //     // Basic Event with a custom value
        //     ecs.sendEvent(StandardEvent{"BasicScriptEvent", "CustomValue", "Hello World!", "verbose", false});
        // }

        end = std::chrono::high_resolution_clock::now();
        duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        LOG_INFO(DOM, "Script Event loop took " << duration.count() << " microseconds ("
                 << duration.count() / 1000.0 << " ms, "
                 << duration.count() / 1000000.0 << " s)");

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
