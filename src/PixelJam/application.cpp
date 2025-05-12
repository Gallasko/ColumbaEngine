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

using namespace pg;

namespace
{
    static const char* const DOM = "Editor app";
}

GameApp::GameApp(const std::string& appName) : appName(appName)
{
    LOG_THIS_MEMBER(DOM);
}

GameApp::~GameApp()
{
    LOG_THIS_MEMBER(DOM);
}

enum class SceneName
{
    Nexus,
    Customization,
    Inventory,
    Location
};

struct SceneToLoad
{
    SceneToLoad(const SceneName& name) : name(name) {}

    SceneName name;
};

struct SceneLoader : public System<Listener<SceneToLoad>, StoragePolicy, InitSys>
{
    virtual std::string getSystemName() const override { return "SceneLoader"; }

    virtual void onEvent(const SceneToLoad& event) override
    {
        switch (event.name)
        {
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

    virtual void init() override
    {
        // Navigation tabs
        auto windowEnt = ecsRef->getEntity("__MainWindow");

        auto windowAnchor = windowEnt->get<UiAnchor>();
    }
};

struct CollisionHandleBase
{
    virtual ~CollisionHandleBase() = default;
    virtual void tryInvoke(EntitySystem* ecs, _unique_id id1, _unique_id id2) const = 0;
    virtual std::unique_ptr<CollisionHandleBase> clone() const = 0;
};

template <typename Comp1, typename Comp2>
struct CollisionHandlePair : public CollisionHandleBase
{
    CollisionHandlePair(std::function<void(Comp1*, Comp2*)> f) : fn(std::move(f)) {}

    CollisionHandlePair(const CollisionHandlePair& other) : fn(other.fn) {}

    CollisionHandlePair& operator=(const CollisionHandlePair& other)
    {
        fn = other.fn;

        return *this;
    }

    virtual void tryInvoke(EntitySystem* ecs, _unique_id id1, _unique_id id2) const override
    {
        auto* ca = ecs->getComponent<Comp1>(id1);
        auto* cb = ecs->getComponent<Comp2>(id2);

        if (ca and cb)
        {
            fn(ca, cb);
        }

        // and swap:
        if constexpr (not std::is_same_v<Comp1, Comp2>)
        {
            ca = ecs->getComponent<Comp1>(id2);
            cb = ecs->getComponent<Comp2>(id1);

            if (ca and cb)
            {
                fn(ca, cb);
            }
        }
    }

    std::unique_ptr<CollisionHandleBase> clone() const override
    {
        return std::make_unique<CollisionHandlePair<Comp1, Comp2>>(*this);
    }

    std::function<void(Comp1*, Comp2*)> fn;
};

struct CollisionHandleExpert : public CollisionHandleBase
{
    CollisionHandleExpert(
        std::function<void(Entity*, Entity*)> f,
        std::function<bool(Entity*)> filterEnt1 = [](Entity*) { return true; },
        std::function<bool(Entity*)> filterEnt2 = [](Entity*) { return true; }) :
        fn(std::move(f)), filterEnt1(std::move(filterEnt1)), filterEnt2(std::move(filterEnt2))
        {}

    CollisionHandleExpert(const CollisionHandleExpert& other) : fn(other.fn), filterEnt1(other.filterEnt1), filterEnt2(other.filterEnt2) {}

    CollisionHandleExpert& operator=(const CollisionHandleExpert& other)
    {
        if (this != &other)
        {
            fn = other.fn;
            filterEnt1 = other.filterEnt1;
            filterEnt2 = other.filterEnt2;
        }

        return *this;
    }

    virtual void tryInvoke(EntitySystem* ecs, _unique_id id1, _unique_id id2) const override
    {
        auto* ent1 = ecs->getEntity(id1);
        auto* ent2 = ecs->getEntity(id2);

        if (filterEnt1(ent1) and filterEnt2(ent2))
        {
            fn(ent1, ent2);
        }
        // and swap:
        else if (filterEnt1(ent2) and filterEnt2(ent1))
        {
            fn(ent2, ent1);
        }
    }

    std::unique_ptr<CollisionHandleBase> clone() const override
    {
        // make a brand‐new MyCollisionHandler by copying *this
        return std::make_unique<CollisionHandleExpert>(*this);
    }

    std::function<void(Entity*, Entity*)> fn;
    std::function<bool(Entity*)> filterEnt1;
    std::function<bool(Entity*)> filterEnt2;
};

struct CollisionHandleComponent
{
    CollisionHandleComponent() = default;
    CollisionHandleComponent(std::unique_ptr<CollisionHandleBase> handler) : handler(std::move(handler)) {}
    CollisionHandleComponent(const CollisionHandleComponent& other)
    {
        handler = other.handler ? other.handler->clone() : nullptr;
    }

    CollisionHandleComponent& operator=(const CollisionHandleComponent& other)
    {
        if (this != &other)
        {
            handler = other.handler ? other.handler->clone() : nullptr;
        }

        return *this;
    }

    std::unique_ptr<CollisionHandleBase> handler;
};

struct CollisionHandlerSystem : public System<Listener<CollisionEvent>, Own<CollisionHandleComponent>, StoragePolicy>
{
    virtual void onEvent(const CollisionEvent& event) override
    {
        for (const auto& comp : view<CollisionHandleComponent>())
        {
            comp->handler->tryInvoke(ecsRef, event.id1, event.id2);
        }
    }
};

template <typename Comp1, typename Comp2, typename Type>
EntityRef makeCollisionHandlePair(Type* ecsRef, std::function<void(Comp1*, Comp2*)> fn)
{
    auto ent = ecsRef->createEntity();

    auto handle = std::make_unique<CollisionHandlePair<Comp1, Comp2>>(fn);

    ecsRef->template attach<CollisionHandleComponent>(ent, std::move(handle));

    return ent;
}

template <typename Type>
EntityRef makeCollisionHandle(Type* ecsRef,
    std::function<void(Entity*, Entity*)> fn,
    std::function<bool(Entity*)> filterEnt1 = [](Entity*) { return true; },
    std::function<bool(Entity*)> filterEnt2 = [](Entity*) { return true; })
{
    auto ent = ecsRef->createEntity();

    auto handle = std::make_unique<CollisionHandleExpert>(fn, filterEnt1, filterEnt2);

    ecsRef->template attach<CollisionHandleComponent>(ent, std::move(handle));

    return ent;
}

struct TestSystem : public System<InitSys, QueuedListener<OnMouseClick>, Listener<OnSDLScanCode>, Listener<CollisionEvent>>
{
    int testVar = 0;

    virtual void init() override
    {
        testVar = 0;

        makeCollisionHandle(ecsRef, [](Entity*, Entity*) { LOG_INFO(DOM, "Collision with a wall! "); },
            [](Entity* ent) { return ent->has<WallFlag>(); });

        makeCollisionHandlePair<PlayerFlag, CollectibleFlag>(ecsRef, [](PlayerFlag*, CollectibleFlag*) { LOG_INFO(DOM, "Collectible collected! "); });
    }

    virtual void onProcessEvent(const OnMouseClick& event) override
    {
        if (event.button == SDL_BUTTON_RIGHT)
        {
            if (testVar == 0)
            {
                auto wallEnt = makeUiSimple2DShape(ecsRef, Shape2D::Square, 50.f, 50.f, {0.f, 0.f, 255.f, 255.f});

                wallEnt.get<PositionComponent>()->setX(event.pos.x - 25.f);
                wallEnt.get<PositionComponent>()->setY(event.pos.y - 25.f);

                ecsRef->attach<CollisionComponent>(wallEnt.entity, 0);
                ecsRef->attach<WallFlag>(wallEnt.entity);
            }
            else if (testVar == 1)
            {
                auto collectibleEnt = makeUiSimple2DShape(ecsRef, Shape2D::Square, 25.f, 25.f, {125.f, 0.f, 125.f, 255.f});

                collectibleEnt.get<PositionComponent>()->setX(event.pos.x - 12.5f);
                collectibleEnt.get<PositionComponent>()->setY(event.pos.y - 12.5f);

                ecsRef->attach<CollisionComponent>(collectibleEnt.entity, 3);
                ecsRef->attach<CollectibleFlag>(collectibleEnt.entity);
            }

        }
    }

    virtual void onEvent(const OnSDLScanCode& event) override
    {
        if (event.key == SDL_SCANCODE_1)
        {
            LOG_INFO(DOM, "TestSystem: 1 pressed");
            testVar = 0;
        }
        else if (event.key == SDL_SCANCODE_2)
        {
            LOG_INFO(DOM, "TestSystem: 2 pressed");
            testVar = 1;
        }
    }

    virtual void onEvent(const CollisionEvent& event) override
    {
        LOG_INFO(DOM, "Collision detected " << event.id1 << " with " << event.id2);

        auto entity1 = ecsRef->getEntity(event.id1);
        auto entity2 = ecsRef->getEntity(event.id2);

        if (entity1 == nullptr or entity2 == nullptr)
            return;
    }
};

struct FlagSystem : public System<StoragePolicy, Own<WallFlag>, Own<PlayerFlag>, Own<AllyBulletFlag>, Own<CollectibleFlag>>
{

};

std::thread *initThread;
pg::Window *mainWindow = nullptr;
std::atomic<bool> initialized = {false};
bool init = false;
bool running = true;

void initWindow(const std::string& appName)
{
#ifdef __EMSCRIPTEN__
    mainWindow = new pg::Window(appName, "/save/savedData.sz");
#else
    mainWindow = new pg::Window(appName);
#endif

    LOG_INFO(DOM, "Window init...");

    initialized = true;
}

void initGame()
{
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

    mainWindow->ecs.createSystem<FlagSystem>();

    mainWindow->ecs.createSystem<FpsSystem>();

    mainWindow->ecs.createSystem<MoveToSystem>();

    mainWindow->ecs.createSystem<ConfiguredKeySystem<GameKeyConfig>>(scancodeMap);

    mainWindow->ecs.createSystem<CollisionSystem>();

    mainWindow->ecs.createSystem<CollisionHandlerSystem>();

    mainWindow->ecs.succeed<MoveToSystem, CollisionSystem>();

    mainWindow->ecs.createSystem<PlayerSystem>();

    mainWindow->ecs.createSystem<TestSystem>();

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
void syncFilesystem()
{
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

void mainloop(void* arg)
{
    if (not initialized.load())
        return;

    if (not init)
    {
        if (initThread)
        {
            printf("Joining thread...\n");

            initThread->join();

            delete initThread;

            printf("Thread joined...\n");
        }

        init = true;

        mainWindow->init(820, 640, false, static_cast<SDL_Window*>(arg));

        printf("Window init done !\n");

        initGame();
    }

    SDL_Event event;

    while (SDL_PollEvent(&event))
    {
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

    if (mainWindow->requestQuit())
    {
        LOG_ERROR("Window", "RequestQuit");
        std::terminate();
    }
}

int GameApp::exec()
{
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

    while (running)
    {
        SDL_Event event;

        while (SDL_PollEvent(&event))
        {
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
