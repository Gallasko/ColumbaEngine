#include "application.h"

#include "ECS/entitysystem.h"
#include "ECS/entitysystem_vm_modules.h"

#include "ECS/standardsystem.h"

#include "logger.h"

#include "2D/texture.h"

#include "2D/collisionsystem.h"

#include "UI/ttftext.h"

#include "window.h"

#include "particle_system.h"
#include "particlemodule.h"

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
        .onEvent("PlayerHit", "res/asteroid/handle_player_hit.pg")
        .onEvent("RespawnPlayer", "res/asteroid/respawn_player.pg")
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
        // .onDelta([](StandardSystemHandle* sys, float deltaTime)
        // {
        //     float timer = sys->getData("spawnTimer").get<float>();
        //     timer += deltaTime;

        //     if (timer > 10.0f)
        //     {
        //         timer -= 10.0f;
        //         // for (int i = 0; i < 10; i++)
        //             sys->sendEvent("SpawnAsteroid");
        //     }

        //     sys->setData("spawnTimer", timer);
        // })
        .onEvent("GameOver", "res/asteroid/clear_asteroids.pg")
        .onDelta("res/asteroid/spawn_asteroid_timer.pg")
        .build();
}

// StandardSystemImpl* createAsteroidSystem()
// {
//     return createStandardSystem("AsteroidSystem")
//         .onInit([](StandardSystemHandle* sys)
//         {
//             LOG_MILE(DOM, "AsteroidSystem initialized");
//         })
//         .ownComponent("Asteroid")
//         .onEvent("SpawnAsteroid", "res/asteroid/spawn_single_asteroid.pg")
//         .onDelta("res/asteroid/update_asteroids.pg")
//         .build();
// }

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
        .onEvent("GameOver", "res/asteroid/clear_bullets.pg")
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

            auto fps = makeTTFText(sys->getWorld(), 500.0f, 10.0f, 14.0f, "light", "0", 0.5);

            sys->setData("fpsTextEntity", fps.entity.id);
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

                auto fpsTextEntId = sys->getData("fpsTextEntity").get<size_t>();

                auto fpsTextEnt = sys->getWorld()->getEntity(fpsTextEntId);
                auto fpsTextComp = fpsTextEnt->get<TTFText>();

                fpsTextComp->setText("FPS: " + std::to_string(res) + ", GFPS: " + std::to_string(res2));

                LOG_INFO("Standard FPS Sys", "FPS: " << res << ", GFPS: " << res2);

                sys->setData("nbRenderedFrames", currentNbOfFrames);
                sys->setData("nbGeneratedFrames", currentNbOfGFrames);
            }

            sys->setData("currentDelta", delta);
        })
        .build();
}

StandardSystemImpl* createScoreSystem()
{
    return createStandardSystem("ScoreSystem")
        .onInit("res/asteroid/init_score.pg")
        .onEvent("ScoreUpdate", "res/asteroid/update_score.pg")
        .onEvent("ScoreReset", "res/asteroid/reset_score.pg")
        .build();
}

StandardSystemImpl* createGameOverSystem()
{
    return createStandardSystem("GameOverSystem")
        .onInit("res/asteroid/init_gameover.pg")
        .onEvent("GameOver", "res/asteroid/handle_gameover.pg")
        .onEvent("OnSDLScanCode", "res/asteroid/handle_restart.pg")
        .build();
}

GameApp::GameApp(const std::string &appName) : engine(appName)
{
    EngineConfig config;

    // config.autoStartECS = false;

    engine.setConfig(config);

    engine.setSetupFunction([this](EntitySystem& ecs, Window& window)
    {
        // ecs.setVMOptimizationLevel(VmOptimizationLevel::O0);

        ecs.createSystem<CollisionSystem>();

        ecs.createSystem<CollisionHandlerSystem>();

        ecs.succeed<CollisionHandlerSystem, CollisionSystem>();

        // Setup TTF text system for UI
        auto ttfSys = ecs.createSystem<TTFTextSystem>(window.masterRenderer);
        ttfSys->registerFont("res/font/Inter/static/Inter_28pt-Light.ttf", "light");
        ttfSys->registerFont("res/font/Inter/static/Inter_28pt-Bold.ttf", "bold");
        ttfSys->registerFont("res/font/Inter/static/Inter_28pt-Italic.ttf", "italic");

        ecs.succeed<MasterRenderer, TTFTextSystem>();

        // Register custom VM modules for scripts
        ecs.registerCustomVmModule("particle", ParticleModule{&ecs});

        VM vm;
        ecs.setupVm(vm);

        auto result = vm.interpretFromFile("res/init.pg");

        ecs.registerSystem(createPlayerSystem());

        ecs.registerSystem(createAsteroidSpawnTimerSystem());
        // ecs.registerSystem(createAsteroidSystem());

        ecs.registerSystem(createBulletSystem());

        ecs.registerSystem(createScoreSystem());

        ecs.registerSystem(createGameOverSystem());

        ecs.registerSystem(createFPSSystem());

        // Create particle system
        ecs.createSystem<ParticleSystem>();

        // Register collision handler for Bullet-Asteroid collisions
        makeCollisionHandleScript(&ecs, "res/asteroid/bullet_asteroid_collision.pg",
            [](Entity* ent) { return ent->has("Bullet"); },
            [](Entity* ent) { return ent->has("Asteroid"); });

        makeCollisionHandleScript(&ecs, "res/asteroid/player_asteroid_collision.pg",
            [](Entity* ent) { return ent->has("Player"); },
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
