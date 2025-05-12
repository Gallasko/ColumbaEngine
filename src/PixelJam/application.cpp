#include "application.h"

#include "logger.h"

#include "Systems/basicsystems.h"
#include "Systems/oneventcomponent.h"

#include "Scene/scenemanager.h"

#include "2D/simple2dobject.h"

#include "UI/ttftext.h"
#include "UI/progressbar.h"

#include "Helpers/tinyfiledialogs.h"

#include "2D/position.h"
#include "2D/collisionsystem.h"

#include "UI/sizer.h"
#include "UI/prefab.h"

#include "config.h"

#include "Characters/player.h"

#include "Tiled_Lib/TiledLoader.h"
#include "Tiled_Lib/TileMapAtlasLoader.h"

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
        auto windowEnt = ecsRef->getEntity("__MainWindow");

        auto windowAnchor = windowEnt->get<UiAnchor>();
    }
};

struct TestSystem : public System<InitSys, QueuedListener<OnMouseClick>, Listener<OnSDLScanCode> > {
    int testVar = 0;
    MapData mapData;

    TestSystem(const MapData& mapData) : mapData(mapData) {}

    virtual void init() override {
        testVar = 0;

        // makeCollisionHandle(ecsRef, [](Entity*, Entity*) { LOG_INFO(DOM, "Collision with a wall! "); },
        //     [](Entity* ent) { return ent->has<WallFlag>(); });
        makeCollisionHandle(ecsRef, [](Entity *, Entity *) { LOG_INFO(DOM, "Collision ! "); });

        makeCollisionHandlePair(ecsRef, [](PlayerFlag *, CollectibleFlag *) {
            LOG_INFO(DOM, "Collectible collected! ");
        });

        makeCollisionHandlePair(ecsRef, [&](AllyBulletFlag *bullet, WallFlag *) {
            LOG_INFO(DOM, "Bullet hit a wall! ");

            ecsRef->removeEntity(bullet->entityId);
        });

        makeCollisionHandlePair(ecsRef, [&](AllyBulletFlag *bullet, EnemyFlag *enemy) {
            LOG_INFO(DOM, "Bullet hit an enemy! ");

            enemy->health -= bullet->damage;

            if (enemy->health <= 0) {
                ecsRef->removeEntity(enemy->entityId);
            }

            ecsRef->removeEntity(bullet->entityId);
        });


        printf("---------- Load Level ---------\n");

        int z = 0;
        int factor = 3;

        size_t scaledTileWidth = factor * mapData.tileWidth;
        size_t scaledTileHeight = factor * mapData.tileHeight;

        int count = 0;

        for (const auto &layer: mapData.layers) {

            for (const auto &tile : layer.tiles) {
                auto tex = makeUiTexture(ecsRef, scaledTileWidth, scaledTileHeight, tile.textureName);
                auto posComp = tex.get<PositionComponent>();
                posComp->setX(tile.x * scaledTileWidth);
                posComp->setY(tile.y * scaledTileHeight);
                posComp->setZ(z);

                if (tile.isWall) {
                    LOG_INFO("TILED", std::to_string(count++));
                    ecsRef->attach<CollisionComponent>(tex.entity, 0);
                    ecsRef->attach<WallFlag>(tex.entity);
                }
            }

            z++;
        }


        printf("Loaded Map\n");
    }

    virtual void onProcessEvent(const OnMouseClick &event) override {
        if (event.button == SDL_BUTTON_RIGHT) {
            if (testVar == 0) {
                auto wallEnt = makeUiSimple2DShape(ecsRef, Shape2D::Square, 50.f, 50.f, {0.f, 0.f, 255.f, 255.f});

                wallEnt.get<PositionComponent>()->setX(event.pos.x - 25.f);
                wallEnt.get<PositionComponent>()->setY(event.pos.y - 25.f);

                ecsRef->attach<CollisionComponent>(wallEnt.entity, 0);
                ecsRef->attach<WallFlag>(wallEnt.entity);
            } else if (testVar == 1) {
                auto collectibleEnt = makeUiSimple2DShape(ecsRef, Shape2D::Square, 25.f, 25.f,
                                                          {125.f, 0.f, 125.f, 255.f});

                collectibleEnt.get<PositionComponent>()->setX(event.pos.x - 12.5f);
                collectibleEnt.get<PositionComponent>()->setY(event.pos.y - 12.5f);

                ecsRef->attach<CollisionComponent>(collectibleEnt.entity, 3);
                ecsRef->attach<CollectibleFlag>(collectibleEnt.entity);
            } else if (testVar == 2) {
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
        }
    }
};

struct FlagSystem : public System<StoragePolicy, Own<WallFlag>, Own<PlayerFlag>, Own<AllyBulletFlag>, Own<
            CollectibleFlag>, Own<EnemyFlag> > {
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

    TiledLoader loader;
    const MapData map = loader.loadMap("res/tiled/LEVELS/Level_DEV_0001.json");

    for (const auto &tileset: map.tilesets) {
        LOG_INFO("TILED", "B" << tileset.imagePath);

        mainWindow->masterRenderer->registerAtlasTexture(tileset.name, tileset.imagePath.c_str(), "",
                                                         std::make_unique<TileMapAtlasLoader>(tileset));
    }


    mainWindow->ecs.createSystem<FlagSystem>();

    mainWindow->ecs.createSystem<FpsSystem>();

    mainWindow->ecs.createSystem<MoveToSystem>();

    mainWindow->ecs.createSystem<ConfiguredKeySystem<GameKeyConfig> >(scancodeMap);

    mainWindow->ecs.createSystem<CollisionSystem>();

    mainWindow->ecs.createSystem<CollisionHandlerSystem>();

    mainWindow->ecs.succeed<MoveToSystem, CollisionSystem>();

    mainWindow->ecs.createSystem<PlayerSystem>();

    mainWindow->ecs.createSystem<TestSystem>(map);

    // mainWindow->ecs.createSystem<ContextMenu>();
    // mainWindow->ecs.createSystem<InspectorSystem>();
    auto ttfSys = mainWindow->ecs.createSystem<TTFTextSystem>(mainWindow->masterRenderer);

    ttfSys->registerFont("res/font/Inter/static/Inter_28pt-Light.ttf");
    ttfSys->registerFont("res/font/Inter/static/Inter_28pt-Bold.ttf");
    ttfSys->registerFont("res/font/Inter/static/Inter_28pt-Italic.ttf");

    mainWindow->masterRenderer->processTextureRegister();

    mainWindow->ecs.succeed<MasterRenderer, TTFTextSystem>();

    mainWindow->ecs.createSystem<SceneLoader>();


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
