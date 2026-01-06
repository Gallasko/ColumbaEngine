#include "application.h"

#include "ECS/entitysystem.h"

#include "ECS/standardsystem.h"

#include "logger.h"

#include "2D/texture.h"

#include "2D/collisionsystem.h"

using namespace pg;

namespace
{
    static const char *const DOM = "App";
}

StandardSystemImpl* createPlayerSystem()
{
    return createStandardSystem("PlayerSystem")
        .onInit("res/asteroid/init_player.pg")
        .ownComponent("Player")
        .onEvent("OnSDLScanCode", "res/asteroid/move_player.pg")
        .onEvent("OnSDLScanCodeReleased", "res/asteroid/release_player.pg")
        .onDelta("res/asteroid/update_player.pg")  // Update physics every frame
        .build();
}

StandardSystemImpl* createAsteroidSpawnTimerSystem()
{
    return createStandardSystem("AsteroidSpawnTimer")
        .onInit([](StandardSystemHandle* sys)
        {
            LOG_MILE(DOM, "AsteroidSpawnTimer initialized");
            sys->setData("spawnTimer", 0.0f);
        })
        .onDelta([](StandardSystemHandle* sys, float deltaTime)
        {
            float timer = sys->getData("spawnTimer").get<float>();
            timer += deltaTime;

            if (timer > 10.0f)
            {
                timer -= 10.0f;
                // for (int i = 0; i < 10; i++)
                    sys->sendEvent("SpawnAsteroid");
            }

            sys->setData("spawnTimer", timer);
        })
        .build();
}

StandardSystemImpl* createAsteroidSystem()
{
    return createStandardSystem("AsteroidSystem")
        .onInit([](StandardSystemHandle* sys)
        {
            LOG_MILE(DOM, "AsteroidSystem initialized");
        })
        .ownComponent("Asteroid")
        .onEvent("SpawnAsteroid", "res/asteroid/spawn_single_asteroid.pg")
        .onDelta("res/asteroid/update_asteroids.pg")
        .build();
}

StandardSystemImpl* createBulletSystem()
{
    return createStandardSystem("BulletSystem")
        .onInit([](StandardSystemHandle* sys)
        {
            LOG_MILE(DOM, "BulletSystem initialized");

            sys->setData("bulletCount", 0);
        })
        .ownComponent("Bullet")
        .onEvent("SpawnBullet", "res/asteroid/spawn_bullet.pg")
        .onDelta("res/asteroid/update_bullets.pg")
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
    EngineConfig config;

    // config.autoStartECS = false;

    engine.setConfig(config);

    engine.setSetupFunction([this](EntitySystem& ecs, Window& window)
    {
        ecs.setVMOptimizationLevel(VmOptimizationLevel::O0);

        ecs.createSystem<CollisionSystem>();

        ecs.createSystem<CollisionHandlerSystem>();

        ecs.succeed<CollisionHandlerSystem, CollisionSystem>();

        ecs.registerSystem(createPlayerSystem());

        ecs.registerSystem(createAsteroidSpawnTimerSystem());
        ecs.registerSystem(createAsteroidSystem());

        ecs.registerSystem(createBulletSystem());

        ecs.registerSystem(createFPSSystem());

        // Register collision handler for Bullet-Asteroid collisions
        makeCollisionHandleScript(&ecs, "res/asteroid/bullet_asteroid_collision.pg",
            [](Entity* ent) { return ent->has("Bullet"); },
            [](Entity* ent) { return ent->has("Asteroid"); });
    });

    // Post-init: manually control ECS execution
//     engine.setPostInitFunction([](pg::EntitySystem& ecs, pg::Window& window) {
//         printf("ECS initialized but NOT started - manual execution mode enabled\n");

//         // Start a background thread that executes ECS once per second
//         std::thread debugThread([&ecs]() {
//             auto lastExecution = std::chrono::steady_clock::now();

//             while (true) {
//                 auto now = std::chrono::steady_clock::now();
//                 auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastExecution);

//                 // Execute once per second
//                 if (elapsed.count() >= 1000) {
//                     printf("--- Executing ECS frame (debug mode) ---\n");

//                     // Execute one ECS update cycle
//                     ecs.executeOnce();  // Simulate 60 FPS delta time

//                     lastExecution = now;
//                 }

//                 // Sleep briefly to avoid busy-waiting
//                 std::this_thread::sleep_for(std::chrono::milliseconds(10));
//             }
//         });

//         debugThread.detach();
//     });
}

GameApp::~GameApp()
{
}

int GameApp::exec()
{
    return engine.exec();
}
