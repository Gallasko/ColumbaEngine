#include "application.h"

#include "logger.h"

#include "Systems/basicsystems.h"
#include "Systems/tween.h"

#include "UI/ttftext.h"

#include "2D/simple2dobject.h"

#include "ribbonmesh.h"
#include "polygonmesh.h"
#include "enemyspawner.h"
#include "pointaggregator.h"
#include "bgscroller.h"

using namespace pg;

namespace {
    static const char *const DOM = "App";
}

GameApp::GameApp(const std::string &appName) : appName(appName) {
    LOG_THIS_MEMBER(DOM);
}

GameApp::~GameApp() {
    LOG_THIS_MEMBER(DOM);
}

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

    auto ttfSys = mainWindow->ecs.createSystem<TTFTextSystem>(mainWindow->masterRenderer);

#ifdef __EMSCRIPTEN__
    // Need to fix this
    ttfSys->registerFont("/res/font/Inter/static/Inter_28pt-Light.ttf", "light");
    ttfSys->registerFont("/res/font/Inter/static/Inter_28pt-Bold.ttf", "bold");
    ttfSys->registerFont("/res/font/Inter/static/Inter_28pt-Italic.ttf", "italic");
#else
    ttfSys->registerFont("res/font/Inter/static/Inter_28pt-Light.ttf", "light");
    ttfSys->registerFont("res/font/Inter/static/Inter_28pt-Bold.ttf", "bold");
    ttfSys->registerFont("res/font/Inter/static/Inter_28pt-Italic.ttf", "italic");
#endif
    // mainWindow->masterRenderer->processTextureRegister();

    mainWindow->ecs.createSystem<TweenSystem>();

    mainWindow->ecs.createSystem<FpsSystem>();

    mainWindow->ecs.createSystem<BackgroundScrollerSystem>();

    mainWindow->ecs.createSystem<MainCameraShake>(mainWindow->masterRenderer);

    mainWindow->ecs.createSystem<TexturedRibbonComponentSystem>(mainWindow->masterRenderer);
    mainWindow->ecs.createSystem<PolygonComponentSystem>(mainWindow->masterRenderer);

    mainWindow->ecs.createSystem<EnemySpawnerSystem>();

    mainWindow->ecs.succeed<MasterRenderer, MainCameraShake>();
    mainWindow->ecs.succeed<MasterRenderer, TexturedRibbonComponentSystem>();
    mainWindow->ecs.succeed<MasterRenderer, PolygonComponentSystem>();

    mainWindow->ecs.createSystem<PointAggregator>();

    mainWindow->ecs.succeed<PointAggregator, TexturedRibbonComponentSystem>();

    mainWindow->ecs.succeed<PointAggregator, PolygonComponentSystem>();
    mainWindow->ecs.succeed<MasterRenderer, PointAggregator>();

    mainWindow->ecs.dumbTaskflow();

    mainWindow->render();

    mainWindow->resize(820, 640);

    mainWindow->ecs.start();

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
