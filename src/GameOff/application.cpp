#include "application.h"

#include "logger.h"

#include "Systems/basicsystems.h"
#include "Systems/oneventcomponent.h"

#include "Scene/scenemanager.h"

#include "UI/ttftext.h"

#include "fightscene.h"
#include "characustomizationscene.h"
#include "inventory.h"

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
    Customization,
    Inventory
};

struct SceneToLoad
{
    SceneToLoad(const SceneName& name) : name(name) {}

    SceneName name;
};

struct SceneLoader : public System<Listener<SceneToLoad>, StoragePolicy, InitSys>
{
    virtual void onEvent(const SceneToLoad& event) override
    {
        switch (event.name)
        {
        case SceneName::Customization:
            ecsRef->getSystem<SceneElementSystem>()->loadSystemScene<PlayerCustomizationScene>();
            break;

        case SceneName::Inventory:
            ecsRef->getSystem<SceneElementSystem>()->loadSystemScene<InventoryScene>();
            break;

        default:
            break;
        }
    }

    virtual void init() override
    {
        // Navigation tabs
        auto titleTTF = makeTTFText(ecsRef, 50, 0, "res/font/Inter/static/Inter_28pt-Light.ttf", "Customization", 0.4);
        titleTTF.get<UiComponent>()->setZ(1);
        ecsRef->attach<MouseLeftClickComponent>(titleTTF.entity, makeCallable<SceneToLoad>(SceneName::Customization));

        auto titleTTF2 = makeTTFText(ecsRef, 225, 0, "res/font/Inter/static/Inter_28pt-Light.ttf", "Inventory", 0.4);
        ecsRef->attach<MouseLeftClickComponent>(titleTTF2.entity, makeCallable<SceneToLoad>(SceneName::Inventory));
    }
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

    mainWindow->initEngine();

    printf("Engine initialized ...\n");

    mainWindow->ecs.createSystem<FpsSystem>();

    mainWindow->ecs.createSystem<InventorySystem>();

    mainWindow->ecs.createSystem<MoveToSystem>();

    // mainWindow->ecs.createSystem<ContextMenu>();
    // mainWindow->ecs.createSystem<InspectorSystem>();
    auto ttfSys = mainWindow->ecs.createSystem<TTFTextSystem>(mainWindow->masterRenderer);

    ttfSys->registerFont("res/font/Inter/static/Inter_28pt-Light.ttf");
    ttfSys->registerFont("res/font/Inter/static/Inter_28pt-Bold.ttf");
    ttfSys->registerFont("res/font/Inter/static/Inter_28pt-Italic.ttf");

    mainWindow->masterRenderer->processTextureRegister();

    auto fightSys = mainWindow->ecs.createSystem<FightSystem>();

    Character p1{"Player 1", CharacterType::Player, 100};

    p1.stat.speed = 120;
    p1.spells.push_back(Spell{"Fireball", 20});

    Character p2{"Player 2", CharacterType::Player, 100};
    p2.stat.speed = 145;

    Spell heal{"Self Heal", -20};
    heal.selfOnly = true;

    p2.spells.push_back(heal);
    p2.spells.push_back(Spell{"Taunt", 20});
    p2.spells.push_back(Spell{"Big ball", 80, 0, 3});

    Character p3{"Player 3", CharacterType::Player, 100};
    p3.stat.speed = 105;
    p3.spells.push_back(Spell{"Stab", 20});

    auto healthBoost = makeSimplePlayerBoostPassive(PlayerBoostType::Health, 50, 3);
    p3.addPassive(healthBoost);

    Character p4{"Player 4", CharacterType::Player, 100};
    p4.stat.speed = 100;
    
    Spell multishot{"Multishot", 10};
    multishot.nbTargets = 3;

    p4.spells.push_back(Spell{"Scan", 20});
    p4.spells.push_back(multishot);

    fightSys->addCharacter(p1);
    fightSys->addCharacter(p2);
    fightSys->addCharacter(p3);
    fightSys->addCharacter(p4);

    Spell basicAttack{"Multishot", 10};

    Character e1{"Enemy 1", CharacterType::Enemy, 100};
    e1.spells.push_back(basicAttack);

    Character e2{"Boss 1",  CharacterType::Enemy, 100};
    e2.spells.push_back(basicAttack);

    Character e3{"Enemy 2", CharacterType::Enemy, 100};
    e3.spells.push_back(basicAttack);

    fightSys->addCharacter(e1);
    fightSys->addCharacter(e2);
    fightSys->addCharacter(e3);

    mainWindow->ecs.succeed<MasterRenderer, TTFTextSystem>();

    mainWindow->ecs.createSystem<PlayerHandlingSystem>();

    mainWindow->ecs.createSystem<SceneLoader>();

    mainWindow->ecs.start();

    mainWindow->render();


    // mainWindow->ecs.getSystem<SceneElementSystem>()->loadSystemScene<FightScene>();
    // mainWindow->ecs.getSystem<SceneElementSystem>()->loadSystemScene<PlayerCustomizationScene>();
    // mainWindow->ecs.getSystem<SceneElementSystem>()->loadSystemScene<InventoryScene>();

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
    EM_ASM(
        FS.mkdir('/save');
        FS.mount(IDBFS, {autoPersist: true}, '/save');
        FS.syncfs(true, function (err) {
            if (err) {
                console.error("Initial sync error:", err);
            }
        });
    );

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
