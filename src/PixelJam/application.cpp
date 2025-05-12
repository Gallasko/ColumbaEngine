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

    mainWindow->ecs.createSystem<FpsSystem>();

    mainWindow->ecs.createSystem<MoveToSystem>();

    mainWindow->ecs.createSystem<ConfiguredKeySystem<GameKeyConfig>>(scancodeMap);

    mainWindow->ecs.createSystem<PlayerSystem>();

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
