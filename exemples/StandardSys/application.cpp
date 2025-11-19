#include "application.h"

#include "ECS/entitysystem.h"
#include "ECS/standardsystem.h"

#include "logger.h"

using namespace pg;

namespace
{
    static const char *const DOM = "App";
}

StandardSystemImpl* createEventNotificationSystem()
{
    return createStandardSystem("EventNotification")
        .listenToEvents({"BasicEvent"})
        .onInit([](StandardSystemHandle* sys) {
            // Initialize system
            LOG_INFO("EventNotification", "System initialized");
        })
        .onEvent([](StandardSystemHandle* sys, const StandardEvent& event) {
            if (event.name == "BasicEvent")
            {
                LOG_INFO("EventNotification", "Event received");

                // Access event data
                if (event.has("CustomValue"))
                {
                    auto value = event.getElement("CustomValue").toString();
                    LOG_INFO("EventNotification", "Custom Value: " << value);
                }
                else
                {
                    LOG_INFO("EventNotification", "No Custom Value !");
                }
            }
        })
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
    });
}

GameApp::~GameApp()
{
}

int GameApp::exec()
{
    return engine.exec();
}
