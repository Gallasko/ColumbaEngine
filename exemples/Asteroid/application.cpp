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

            if (timer > 2.0f)
            {
                timer -= 2.0f;
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
    engine.setSetupFunction([this](EntitySystem& ecs, Window& window)
    {
        ecs.registerSystem(createPlayerSystem());

        ecs.registerSystem(createAsteroidSpawnTimerSystem());
        ecs.registerSystem(createAsteroidSystem());

        ecs.registerSystem(createBulletSystem());

        ecs.registerSystem(createFPSSystem());

        // Register collision handler for Bullet-Asteroid collisions
        makeCollisionHandleScript(&ecs, "res/asteroid/bullet_asteroid_collision.pg",
            [](Entity* ent) { return ent->has<StandardComponent>() && ent->get<StandardComponent>()->typeName == "Bullet"; },
            [](Entity* ent) { return ent->has<StandardComponent>() && ent->get<StandardComponent>()->typeName == "Asteroid"; });
    });
}

GameApp::~GameApp()
{
}

int GameApp::exec()
{
    return engine.exec();
}
