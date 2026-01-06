#include "application.h"

#include "ECS/entitysystem.h"
#include "ECS/standardsystem.h"

#include "logger.h"

#include <chrono>

// Todo remove this when we can close a window with an event
#include "window.h"

#include "Renderer/renderer.h"

#include "Helpers/fsm.h"

using namespace pg;

#ifdef PG_AUTO_CONVERT_EVENTS_TO_STANDARD
       #pragma message("Auto-conversion enabled")
   #endif

namespace
{
    static const char *const DOM = "App";
}

int test = 0;

StandardSystemImpl* createEventNotificationSystem()
{
    return createStandardSystem("EventNotification")
        .onInit([](StandardSystemHandle*) {
            // Initialize system
            LOG_INFO("EventNotification", "System initialized");
        })
        .onEvent("BasicEvent", [](StandardSystemHandle*, const StandardEvent& event) {
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
        .onInit([](StandardSystemHandle*) {
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
        .onInit([](StandardSystemHandle*) {
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
        .onInit([](StandardSystemHandle*) {
            // Initialize system
            LOG_INFO("SimpleExec", "System initialized");
        })
        .onExecute("simpleExec.pg")
        .build();
}

StandardSystemImpl* createCompExecSystem()
{
    return createStandardSystem("CompExec")
        .onInit([](StandardSystemHandle*) {
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
        .onInit([](StandardSystemHandle*) {
            // Initialize system
            LOG_INFO("CompExec", "System initialized");
        })
        .onEvent("ChangedExecComp", "simpleExecReact.pg")
        .build();
}

StandardSystemImpl* createMouseClickHandlerSystem()
{
    return createStandardSystem("MouseClickHandler")
        .onInit([](StandardSystemHandle*) {
            // Initialize system
            LOG_INFO("CompExec", "System initialized");
        })
        .onEvent("OnMouseClick", "onMouseClick.pg")
        .build();
}

StandardSystemImpl* createKeyHandlerSystem()
{
    return createStandardSystem("MouseClickHandler")
        .onInit([](StandardSystemHandle*) {
            // Initialize system
            LOG_INFO("CompExec", "System initialized");
        })
        .onEvent("OnSDLScanCode", "test_key.pg")
        .build();
}

StandardSystemImpl* createDeltaSystem()
{
    return createStandardSystem("Delta")
        .onInit([](StandardSystemHandle* sys) {
            // Initialize system
            sys->setData("currentDelta", 0);
        })
        .onDelta("test_delta.pg")
        .build();
}

StandardSystemImpl* createFPSSystem()
{
    return createStandardSystem("FPS")
        .onInit([](StandardSystemHandle* sys) {
            // Initialize system
            sys->setData("currentDelta", 0.0f);
            sys->setData("nbRenderedFrames", 0);
            sys->setData("nbGeneratedFrames", 0);
        })
        .onDelta([](StandardSystemHandle* sys, float deltaTime) {
            float delta = sys->getData("currentDelta").get<float>();

            delta += deltaTime;

            if (delta > 1.0f)
            {
                delta -= 1.0f;

                auto rendererSys = sys->getWorld()->getSystem<MasterRenderer>();

                auto currentNbOfFrames = rendererSys->getNbRenderedFrames();
                auto currentNbOfGFrames = rendererSys->getNbGeneratedFrames();

                auto lastNbOfFrames = sys->getData("nbRenderedFrames").get<size_t>();
                auto lastNbOfGFrames = sys->getData("nbGeneratedFrames").get<size_t>();

                if (currentNbOfFrames < lastNbOfFrames or currentNbOfGFrames < lastNbOfGFrames)
                {
                    sys->setData("nbRenderedFrames", currentNbOfFrames);
                    sys->setData("nbGeneratedFrames", currentNbOfGFrames);
                    sys->setData("currentDelta", delta);

                    return;
                }

                auto res = currentNbOfFrames - lastNbOfFrames;
                auto res2 = currentNbOfGFrames - lastNbOfGFrames;

                LOG_INFO("Standard FPS Sys", "FPS: " << res << ", GFPS: " << res2);

                sys->setData("nbRenderedFrames", currentNbOfFrames);
                sys->setData("nbGeneratedFrames", currentNbOfGFrames);
            }

            sys->setData("currentDelta", delta);
        })
        .build();
}

GameApp::GameApp(const std::string &appName) : engine(appName)
{
    engine.setSetupFunction([this](EntitySystem& ecs, Window& window)
    {
        // Todo O3 optimization breaks the event system for some reason
        ecs.setVMOptimizationLevel(VmOptimizationLevel::O0);

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

        ecs.deleteSystem(simpleSys->_id);

        auto reactorSys = ecs.registerSystem(createCompExecReactorSystem());

        auto compSys = ecs.registerSystem(createCompExecSystem());

        {
            auto ent = ecs.createEntity();

            auto execComp = ecs.attach(ent, "ExecComp");
        }

        {
            auto& reactorSysData = compSys->getSystemData();

            auto it = reactorSysData.find("i");

            if (it == reactorSysData.end())
            {
                LOG_INFO("ReactorSys", "Correctly not found i in properties");
            }
        }

        ecs.executeOnce();

        LOG_INFO("React", "Hello");

        {
            auto& reactorSysData = compSys->getSystemData();

            for (const auto& elem : reactorSysData)
            {
                LOG_INFO("Sss", elem.first);
            }

            auto it = reactorSysData.find("i");

            if (it != reactorSysData.end())
            {
                LOG_INFO("ReactorSys", "Correctly found i in properties: " << it->second);
            }

            reactorSysData["stop"] = true;
        }

        ecs.executeOnce();

        ecs.deleteSystem(compSys->_id);
        reactorSys->getSystemData()["stop"] = true;

        ecs.registerSystem(createMouseClickHandlerSystem());

        ecs.registerSystem(createKeyHandlerSystem());

        ecs.registerSystem(createDeltaSystem());

        ecs.registerSystem(createFPSSystem());

        ecs.createSystem<FSMSystem>();

        auto ent = ecs.createEntity();

        auto fsm = ent.attach<FiniteStateMachine>();

        FSMState idleState("idle");

        idleState.setEnterCallback([](FSMState&) {
            LOG_INFO("FSM", "Entering idle state");
        });

        idleState.setExitCallback([](FSMState&) {
            LOG_INFO("FSM", "Exiting idle state");
        });

        idleState.setEventCallback("OnSDLScanCode", [](const StandardEvent& event, FSMState& state) {
            auto key = event.getElement("key").toString();

            LOG_INFO("FSM", "Idle State received scan code: " << key);

            if (key == "A")
            {
                state.getFSM()->setState("base", "base");
            }
        });

        FSMState baseState("base");

        baseState.setEnterCallback([](FSMState&) {
            LOG_INFO("FSM", "Entering base state");
        });

        baseState.setExitCallback([](FSMState&) {
            LOG_INFO("FSM", "Exiting base state");
        });

        baseState.setEventCallback("OnSDLScanCode", [](const StandardEvent& event, FSMState& state) {
            auto key = event.getElement("key").toString();

            LOG_INFO("FSM", "Idle State received scan code: " << key);

            if (key == "A" or key == "D")
            {
                state.getFSM()->setState("base", "idle");
            }
        });

        fsm->registerState(idleState);
        fsm->registerState(baseState);

        fsm->setState("base", "idle");

        // fsm->setState("base", "base");

        // window.receivedQuitRequest();

    });
}

GameApp::~GameApp()
{
}

int GameApp::exec()
{
    return engine.exec();
}
