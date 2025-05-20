#include "application.h"

#include "logger.h"

#include "Systems/basicsystems.h"
#include "Systems/oneventcomponent.h"

#include "Scene/scenemanager.h"

#include "2D/simple2dobject.h"
#include "2D/animator2d.h"

#include "UI/ttftext.h"
#include "UI/progressbar.h"

#include "Helpers/tinyfiledialogs.h"

#include "2D/position.h"
#include "2D/collisionsystem.h"
#include "2D/camera2d.h"

#include "UI/sizer.h"
#include "UI/prefab.h"

#include "config.h"
#include "Aseprite_Lib/AsepriteFileAtlasLoader.h"
#include "Aseprite_Lib/AsepriteLoader.h"

#include "Characters/player.h"
#include "Characters/enemy.h"

#include "Tiled_Lib/TiledLoader.h"
#include "Tiled_Lib/TileMapAtlasLoader.h"

#include "Database/weapondatabase.h"
#include "Database/enemydatabase.h"
#include "Room/room.h"

using namespace pg;

namespace {
    static const char *const DOM = "Editor app";
}

GameApp::GameApp(const std::string &appName) : appName(appName) {
    LOG_THIS_MEMBER(DOM);
}

GameApp::~GameApp() {
    LOG_THIS_MEMBER(DOM);
}

enum class SceneName {
    Nexus,
    Customization,
    Inventory,
    Location
};

struct SceneToLoad {
    SceneToLoad(const SceneName &name) : name(name) {
    }

    SceneName name;
};

struct SceneLoader : public System<Listener<SceneToLoad>, StoragePolicy, InitSys> {
    virtual std::string getSystemName() const override { return "SceneLoader"; }

    virtual void onEvent(const SceneToLoad &event) override {
        switch (event.name) {
            case SceneName::Nexus:
                break;

            case SceneName::Customization:
                break;

            case SceneName::Inventory:
                break;

            case SceneName::Location:
                break;

            default:
                break;
        }
    }

    virtual void init() override {
        // Navigation tabs
        // auto windowEnt = ecsRef->getEntity("__MainWindow");

        // auto windowAnchor = windowEnt->get<UiAnchor>();
    }
};

constexpr float repulsionStrength = 1.f;

struct TestSystem : public System<InitSys, QueuedListener<OnMouseClick>, Listener<OnSDLScanCode> > {
    int testVar = 0;
    MapData mapData;
    AsepriteFile testAnim;

    TestSystem(const MapData &mapData, const AsepriteFile& testAnim) : mapData(mapData), testAnim(testAnim) {
    }

    virtual void init() override {
        testVar = 0;

        // makeCollisionHandle(ecsRef, [](Entity*, Entity*) { LOG_INFO(DOM, "Collision with a wall! "); },
        //     [](Entity* ent) { return ent->has<WallFlag>(); });
        // makeCollisionHandle(ecsRef, [](Entity *, Entity *) { LOG_INFO(DOM, "Collision ! "); });

        // Todo possibility to add multiple colliders on an entity : for example player feet only for the walls and the whole body for the bullets

        makeCollisionHandlePair(ecsRef, [](PlayerFlag *, CollectibleFlag *) {
            LOG_INFO(DOM, "Collectible collected! ");
        });

        makeCollisionHandlePair(ecsRef, [&](PlayerFlag*, RoomTriggerFlag* room) {
            LOG_INFO(DOM, "Player hit a room trigger! ");

            ecsRef->sendEvent(EnterRoomEvent{room->roomIndex});
        });

        makeCollisionHandlePair(ecsRef, [&](AllyBulletFlag *bullet, WallFlag *) {
            LOG_INFO(DOM, "Bullet hit a wall! ");

            ecsRef->removeEntity(bullet->entityId);
        });

        makeCollisionHandlePair(ecsRef, [&](EnemyBulletFlag *bullet, WallFlag *) {
            LOG_INFO(DOM, "Bullet hit a wall! ");

            ecsRef->removeEntity(bullet->entityId);
        });

        makeCollisionHandlePair(ecsRef, [&](AllyBulletFlag *bullet, EnemyFlag *enemy) {
            LOG_INFO(DOM, "Bullet hit an enemy! ");

            if (enemy->invicibilityTimeLeft > 0)
            {
                ecsRef->removeEntity(bullet->entityId);
                return;
            }

            enemy->health -= bullet->damage;
            enemy->invicibilityTimeLeft = ENEMYINVICIBILITYTIMEMS;

            if (enemy->health <= 0)
            {
                auto weapon = ecsRef->getComponent<WeaponComponent>(enemy->entityId);
                auto pos = ecsRef->getComponent<PositionComponent>(enemy->entityId);

                if (weapon and pos and (not (weapon->weapon.ammo == 0)))
                {
                    auto collectibleEnt = makeUiSimple2DShape(ecsRef, Shape2D::Square, 25.f, 25.f, {125.f, 0.f, 125.f, 255.f});

                    collectibleEnt.get<Simple2DObject>()->setViewport(1);

                    collectibleEnt.get<PositionComponent>()->setX(pos->x + pos->width / 2.f - 12.5f);
                    collectibleEnt.get<PositionComponent>()->setY(pos->y + pos->height / 2.f - 12.5f);
                    collectibleEnt.get<PositionComponent>()->setZ(10.f);

                    ecsRef->attach<CollisionComponent>(collectibleEnt.entity, 3);
                    ecsRef->attach<CollectibleFlag>(collectibleEnt.entity, weapon->weapon);
                }

                ecsRef->sendEvent(EnemyDeathEvent{enemy->entityId});
                ecsRef->removeEntity(enemy->entityId);
            }

            ecsRef->removeEntity(bullet->entityId);
        });

        // Todo make a macro for LOG_INFO and LOG_ERROR with a single argument that use a default DOM

        // Todo we need this because sweep move is bugged
        makeCollisionHandlePair(ecsRef, [&](PlayerFlag* player, WallFlag* wall){
            // get both entities’ positions
            auto wallEnt  = wall->ecsRef->getEntity(wall->entityId);
            auto playerEnt = player->ecsRef->getEntity(player->entityId);
            auto wpos     = wallEnt->get<PositionComponent>();
            auto epos     = playerEnt->get<PositionComponent>();

            // compute normalized vector from wall→enemy

            float x = epos->x;
            float y = epos->y;

            float wx = wpos->x;
            float wy = wpos->y;

            float dx = x - wx;
            float dy = y - wy;
            float len = std::sqrt(dx * dx + dy * dy);

            if (len > 1e-5f)
            {
                dx /= len;
                dy /= len;
                // shove enemy out
                epos->setX(x + dx * repulsionStrength);
                epos->setY(y + dy * repulsionStrength);
            }
        });

        makeCollisionHandlePair(ecsRef, [&](PlayerFlag* player, HoleFlag* hole){
            // get both entities’ positions
            auto wallEnt  = hole->ecsRef->getEntity(hole->entityId);
            auto playerEnt = player->ecsRef->getEntity(player->entityId);
            auto wpos     = wallEnt->get<PositionComponent>();
            auto epos     = playerEnt->get<PositionComponent>();

            // compute normalized vector from wall→enemy

            float x = epos->x;
            float y = epos->y;

            float wx = wpos->x;
            float wy = wpos->y;

            float dx = x - wx;
            float dy = y - wy;
            float len = std::sqrt(dx * dx + dy * dy);

            if (len > 1e-5f)
            {
                dx /= len;
                dy /= len;
                // shove enemy out
                epos->setX(x + dx * repulsionStrength);
                epos->setY(y + dy * repulsionStrength);
            }
        });

        makeCollisionHandlePair(ecsRef, [&](PlayerFlag*, TestGridFlag* wall) {
            // get both entities’ positions
            auto wallEnt = wall->ecsRef->getEntity(wall->entityId);

            wallEnt->get<Simple2DObject>()->setColors({255.f, 0.f, 0.f, 255.f});
        });

        makeCollisionHandlePair(ecsRef, [&](AllyBulletFlag*, TestGridFlag* wall) {
            // get both entities’ positions
            auto wallEnt = wall->ecsRef->getEntity(wall->entityId);

            wallEnt->get<Simple2DObject>()->setColors({0.f, 0.f, 125.f, 255.f});
        });

        makeCollisionHandlePair(ecsRef, [&](PlayerFlag* player, EnemyBulletFlag* bullet) {
            if (player->inDodge)
                return;
            
            ecsRef->sendEvent(PlayerHitEvent{bullet->damage});

            ecsRef->removeEntity(bullet->entityId);
        });

        // Enemy <-> Wall: push enemy out of the wall
        makeCollisionHandlePair(ecsRef, [&](EnemyFlag* enemy, WallFlag* wall){
            // get both entities’ positions
            auto wallEnt  = wall->ecsRef->getEntity(wall->entityId);
            auto enemyEnt = enemy->ecsRef->getEntity(enemy->entityId);
            auto wpos     = wallEnt->get<PositionComponent>();
            auto epos     = enemyEnt->get<PositionComponent>();

            // compute normalized vector from wall→enemy
            float dx = epos->x - wpos->x;
            float dy = epos->y - wpos->y;
            float len = std::sqrt(dx*dx + dy*dy);
            if (len > 0.f) {
                dx /= len;
                dy /= len;
                // shove enemy out
                epos->setX(epos->x + dx * repulsionStrength);
                epos->setY(epos->y + dy * repulsionStrength);
            }
        });

        makeCollisionHandlePair(ecsRef, [&](EnemyFlag* enemy, HoleFlag* hole){
            // get both entities’ positions
            auto wallEnt  = hole->ecsRef->getEntity(hole->entityId);
            auto enemyEnt = enemy->ecsRef->getEntity(enemy->entityId);
            auto wpos     = wallEnt->get<PositionComponent>();
            auto epos     = enemyEnt->get<PositionComponent>();

            // compute normalized vector from wall→enemy
            float dx = epos->x - wpos->x;
            float dy = epos->y - wpos->y;
            float len = std::sqrt(dx*dx + dy*dy);
            if (len > 0.f) {
                dx /= len;
                dy /= len;
                // shove enemy out
                epos->setX(epos->x + dx * repulsionStrength);
                epos->setY(epos->y + dy * repulsionStrength);
            }
        });

        // Enemy <-> Enemy: mutual separation
        makeCollisionHandlePair(ecsRef, [&](EnemyFlag* a, EnemyFlag* b) {
            // ignore self‐collision
            if (a->entityId == b->entityId) return;

            auto entA = a->ecsRef->getEntity(a->entityId);
            auto entB = b->ecsRef->getEntity(b->entityId);

            if (not entA or not entB) return;
            if (not entA->has<PositionComponent>() or not entB->has<PositionComponent>()) return;

            auto posA = entA->get<PositionComponent>();
            auto posB = entB->get<PositionComponent>();

            // vector from B→A
            float dx = posA->x - posB->x;
            float dy = posA->y - posB->y;

            float len = std::sqrt(dx*dx + dy*dy);

            if (len > 0.f)
            {
                dx /= len;
                dy /= len;
                // push each about half the strength
                posA->setX(posA->x + dx * (repulsionStrength * 0.5f));
                posA->setY(posA->y + dy * (repulsionStrength * 0.5f));
                posB->setX(posB->x - dx * (repulsionStrength * 0.5f));
                posB->setY(posB->y - dy * (repulsionStrength * 0.5f));
            }

            if (not entA->has<AIStateComponent>() or not entB->has<AIStateComponent>()) return;

            entA->get<AIStateComponent>()->orbitDirection *= -1.0f;
            entB->get<AIStateComponent>()->orbitDirection *= -1.0f;
        });


        printf("---------- Load Level ---------\n");

        int z = 0;

        size_t scaledTileWidth = mapData.tileWidthInSPixels;
        size_t scaledTileHeight = mapData.tileHeightInSPixels;

        int holeCount = 0;

        for (const auto &layer: mapData.layers)
        {
            for (const auto &tile : layer.tiles)
            {
                auto tex = makeUiTexture(ecsRef, scaledTileWidth, scaledTileHeight, tile.textureName);
                auto texComp = tex.get<Texture2DComponent>();
                texComp->setViewport(1);

                auto posComp = tex.get<PositionComponent>();
                posComp->setX(tile.x * scaledTileWidth);
                posComp->setY(tile.y * scaledTileHeight);
                posComp->setZ(z);

                if (tile.isWall)
                {
                    //LOG_INFO("TILED", std::to_string(count++));
                    ecsRef->attach<CollisionComponent>(tex.entity, 0);
                    ecsRef->attach<WallFlag>(tex.entity);
                }

                if (tile.isHole)
                {
                    LOG_INFO("TILED", "Is Hole " << std::to_string(++holeCount));
                    ecsRef->attach<CollisionComponent>(tex.entity, 0);
                    ecsRef->attach<HoleFlag>(tex.entity);
                }
            }

            z++;
        }

        int count = 0;
        for (const auto& frame : testAnim.frames) {
            auto tex = makeUiTexture(ecsRef, scaledTileWidth, scaledTileHeight, frame.textureName);
            auto texComp = tex.get<Texture2DComponent>();
            texComp->setViewport(1);

            auto posComp = tex.get<PositionComponent>();
            posComp->setX(mapData.playerSpawn.positionSPixels.x + frame.topLeftCornerInSPixelsX);
            posComp->setY(mapData.playerSpawn.positionSPixels.y + frame.topLeftCornerInSPixelsY);
            posComp->setZ(z+5);

            count++;
        }


        // drawDebugGrid(ecsRef, 2500, 5000);

        printf("Loaded Map\n");
    }

    virtual void onProcessEvent(const OnMouseClick &event) override
    {
        if (event.button == SDL_BUTTON_RIGHT)
        {
            if (testVar == 0)
            {
                auto wallEnt = makeUiSimple2DShape(ecsRef, Shape2D::Square, 50.f, 50.f, {0.f, 0.f, 255.f, 255.f});

                wallEnt.get<Simple2DObject>()->setViewport(1);

                wallEnt.get<PositionComponent>()->setX(event.pos.x - 25.f);
                wallEnt.get<PositionComponent>()->setY(event.pos.y - 25.f);
                wallEnt.get<PositionComponent>()->setZ(10);

                ecsRef->attach<CollisionComponent>(wallEnt.entity, 0);
                ecsRef->attach<WallFlag>(wallEnt.entity);
            }
            else if (testVar == 1)
            {
                auto collectibleEnt = makeUiSimple2DShape(ecsRef, Shape2D::Square, 25.f, 25.f,
                                                          {125.f, 0.f, 125.f, 255.f});

                collectibleEnt.get<PositionComponent>()->setX(event.pos.x - 12.5f);
                collectibleEnt.get<PositionComponent>()->setY(event.pos.y - 12.5f);

                ecsRef->attach<CollisionComponent>(collectibleEnt.entity, 3);
                ecsRef->attach<CollectibleFlag>(collectibleEnt.entity);
            }
            else if (testVar == 2)
            {
                auto enemyEnt = makeSimple2DShape(ecsRef, Shape2D::Square, 50.f, 50.f, {255.f, 0.f, 0.f, 255.f});

                enemyEnt.get<PositionComponent>()->setX(event.pos.x - 25.f);
                enemyEnt.get<PositionComponent>()->setY(event.pos.y - 25.f);
                enemyEnt.get<PositionComponent>()->setZ(10.f);

                ecsRef->attach<CollisionComponent>(enemyEnt.entity, 4);
                ecsRef->attach<EnemyFlag>(enemyEnt.entity);
            }
        }
    }

    virtual void onEvent(const OnSDLScanCode &event) override {
        if (event.key == SDL_SCANCODE_1) {
            LOG_INFO(DOM, "TestSystem: 1 pressed");
            testVar = 0;
        } else if (event.key == SDL_SCANCODE_2) {
            LOG_INFO(DOM, "TestSystem: 2 pressed");
            testVar = 1;
        } else if (event.key == SDL_SCANCODE_3) {
            LOG_INFO(DOM, "TestSystem: 3 pressed");
            testVar = 2;
        } else if (event.key == SDL_SCANCODE_4) {
            LOG_INFO(DOM, "TestSystem: 4 pressed");
            ecsRef->sendEvent(StartSpawnWaveEvent{});
        }
    }
};

struct FlagSystem : public System<StoragePolicy, Own<WallFlag>, Own<PlayerFlag>, Own<AllyBulletFlag>, Own<CollectibleFlag>,
    Own<EnemyFlag>, Own<EnemyBulletFlag>, Own<WeaponComponent>, Own<TestGridFlag>, Own<HoleFlag>>
{
};

std::thread *initThread;
pg::Window *mainWindow = nullptr;
std::atomic<bool> initialized = {false};
bool init = false;
bool running = true;

void initWindow(const std::string &appName) {
#ifdef __EMSCRIPTEN__
    mainWindow = new pg::Window(appName, "/save/savedData.sz");
#else
    mainWindow = new pg::Window(appName);
#endif

    LOG_INFO(DOM, "Window init...");

    initialized = true;
}

void initGame() {
    printf("Initializing engine ...\n");


#ifdef __EMSCRIPTEN__
        EM_ASM(
            console.error("Syncing... !");
            FS.mkdir('/save');
            console.error("Syncing... !");
            FS.mount(IDBFS, {autoPersist: true}, '/save');
            console.error("Syncing... !");
            FS.syncfs(true, function (err) {
                console.error("Synced !");
                if (err) {
                    console.error("Initial sync error:", err);
                }
            });
            console.error("Syncing... !");
        );
#endif

    mainWindow->initEngine();

    printf("Engine initialized ...\n");

    mainWindow->ecs.createSystem<Texture2DAnimatorSystem>();

    mainWindow->ecs.createSystem<FollowCamera2DSystem>(mainWindow->masterRenderer);
    mainWindow->ecs.createSystem<CameraShakeSystem>();

    mainWindow->ecs.succeed<FollowCamera2DSystem, CameraShakeSystem>();
    mainWindow->ecs.succeed<FollowCamera2DSystem, PositionComponent>();
    mainWindow->ecs.succeed<MasterRenderer, FollowCamera2DSystem>();

    mainWindow->ecs.createSystem<FlagSystem>();

    mainWindow->ecs.createSystem<FpsSystem>();

    mainWindow->ecs.createSystem<MoveToSystem>();

    mainWindow->ecs.createSystem<MoveDirSystem>();

    mainWindow->ecs.createSystem<ConfiguredKeySystem<GameKeyConfig> >(scancodeMap);

    mainWindow->ecs.createSystem<CollisionSystem>();

    mainWindow->ecs.createSystem<CollisionHandlerSystem>();

    mainWindow->ecs.succeed<CollisionHandlerSystem, CollisionSystem>();

    mainWindow->ecs.succeed<MoveToSystem, CollisionSystem>();

    // mainWindow->ecs.succeed<CollisionSystem, PositionComponent>();
    mainWindow->ecs.succeed<PositionComponent, CollisionSystem>();
    mainWindow->ecs.succeed<MasterRenderer, CollisionSystem>();

    // mainWindow->ecs.createSystem<ContextMenu>();
    // mainWindow->ecs.createSystem<InspectorSystem>();
    auto ttfSys = mainWindow->ecs.createSystem<TTFTextSystem>(mainWindow->masterRenderer);

    ttfSys->registerFont("res/font/Inter/static/Inter_28pt-Light.ttf");
    ttfSys->registerFont("res/font/Inter/static/Inter_28pt-Bold.ttf");
    ttfSys->registerFont("res/font/Inter/static/Inter_28pt-Italic.ttf");

    mainWindow->masterRenderer->processTextureRegister();

    mainWindow->ecs.succeed<MasterRenderer, TTFTextSystem>();

    mainWindow->ecs.createSystem<SceneLoader>();

    AsepriteLoader aseprite_loader;
    const AsepriteFile anim = aseprite_loader.loadAnim("res/sprites/main-char.json");

    mainWindow->masterRenderer->registerAtlasTexture(anim.filename, anim.metadata.imagePath.c_str(), "", std::make_unique<AsepriteFileAtlasLoader>(anim));

    std::cout << "Anim " << anim << std::endl;

    mainWindow->ecs.createSystem<PlayerSystem>(anim);

    mainWindow->ecs.createSystem<EnemyAISystem>();
    mainWindow->ecs.createSystem<EnemySpawnSystem>();

    // auto worldFacts = mainWindow->ecs.createSystem<WorldFacts>();

    // worldFacts->setDefaultFact("startTuto", true);

    // auto achievementSys = mainWindow->ecs.createSystem<AchievementSys>();

    // Achievement slimeSlayed;

    // slimeSlayed.name = "SlimeSlayed";
    // slimeSlayed.prerequisiteFacts = { FactChecker{"Slime_defeated", 10, FactCheckEquality::GreaterEqual} };

    // achievementSys->setDefaultAchievement(slimeSlayed);


    // mainWindow->ecs.succeed<AchievementSys, WorldFacts>();

    mainWindow->ecs.dumbTaskflow();

    // mainWindow->interpreter->interpretFromFile("main.pg");

    auto weaponDb = mainWindow->ecs.createSystem<WeaponDatabase>();
    auto enemyDb = mainWindow->ecs.createSystem<EnemyDatabase>();

    auto roomSystem = mainWindow->ecs.createSystem<RoomSystem>(weaponDb, enemyDb);

    //MapData map;
    TiledLoader loader;
    int factor = 2;
    const MapData map = loader.loadMap("res/tiled/LEVELS/Level_0001.json", factor);

    for (const auto &tileset: map.tilesets)
    {
        mainWindow->masterRenderer->registerAtlasTexture(tileset.name, tileset.imagePath.c_str(), "", std::make_unique<TileMapAtlasLoader>(tileset));
    }

    for (const auto &w: map.weaponDatas)
    {
        weaponDb->addWeapon(w);
    }

    for (const auto &e: map.enemyTemplates)
    {
        enemyDb->addEnemy(e);
    }

    roomSystem->addPlayerSpawn(map.playerSpawn);

    for (const auto &r: map.roomDatas)
    {
        roomSystem->addRoom(r);
    }

    for (const auto &trigger : map.roomTriggers)
    {
        roomSystem->addRoomTrigger(trigger);
    }

    for (const auto &door : map.doors)
    {
        roomSystem->addDoor(door);
    }

    std::cout << "---PRINT SPIKES---" << std::endl;
    for (const auto &spike : map.spikes) {
        std::cout << "Spike: " << spike << std::endl;
    }
    std::cout << "---PRINT SPIKES--- END" << std::endl;

    std::cout << "---PRINT SPIKES IMAGES---" << std::endl;
    for (const auto &spike : map.spike_images) {
        std::cout << "Spike Image: " << spike << std::endl;
    }
    std::cout << "---PRINT SPIKES IMAGES--- END" << std::endl;

    std::cout << "---PRINT GOLDS---" << std::endl;
    for (const auto &g : map.golds) {
        std::cout << "gold: " << g << std::endl;
    }
    std::cout << "---PRINT GOLDS--- END" << std::endl;

    for (const auto &spawner : map.spawners)
    {
        roomSystem->addSpawner(spawner);
    }

    roomSystem->checkRoomsIntegrity();

    roomSystem->startLevel();

    mainWindow->ecs.createSystem<TestSystem>(map, anim);

    mainWindow->ecs.start();

    mainWindow->render();

    mainWindow->resize(820, 640);

    printf("Engine initialized\n");
}

// New function for syncing manually when needed
void syncFilesystem() {
#ifdef __EMSCRIPTEN__
    EM_ASM(
        FS.syncfs(false, function (err) {
            if (err) {
                console.error("Sync error:", err);
            } else {
                console.log("Filesystem synced.");
            }
        });
    );
#endif
}

void mainloop(void *arg) {
    if (not initialized.load())
        return;

    if (not init) {
        if (initThread) {
            printf("Joining thread...\n");

            initThread->join();

            delete initThread;

            printf("Thread joined...\n");
        }

        init = true;

        mainWindow->init(820, 640, false, static_cast<SDL_Window *>(arg));

        printf("Window init done !\n");

        initGame();
    }

    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        mainWindow->processEvents(event);
    }

    mainWindow->render();

#ifdef __EMSCRIPTEN__
    // Sync file system at a specific point instead of every frame
    if (event.type == SDL_QUIT)
    {
        syncFilesystem();
    }
#endif

    if (mainWindow->requestQuit()) {
        LOG_ERROR("Window", "RequestQuit");
        std::terminate();
    }
}

int GameApp::exec() {
#ifdef __EMSCRIPTEN__
    printf("Start init thread...\n");
    initThread = new std::thread(initWindow, appName);
    printf("Detach init thread...\n");

    SDL_Window *pWindow =
    SDL_CreateWindow("Hello Triangle Minimal",
                        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                        820, 640,
                        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN);

    emscripten_set_main_loop_arg(mainloop, pWindow, 0, 1);

#else
    LOG_THIS_MEMBER(DOM);

    initWindow(appName);

    mainWindow->init(820, 640, false);

    LOG_INFO(DOM, "Window init done !");

    initGame();

    while (running) {
        SDL_Event event;

        while (SDL_PollEvent(&event)) {
            mainWindow->processEvents(event);
        }

        mainWindow->render();

        if (mainWindow->requestQuit())
            break;
    }

    delete mainWindow;
#endif

    return 0;
}
