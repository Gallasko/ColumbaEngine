#include "application.h"

#include "logger.h"

#include "Systems/basicsystems.h"
#include "Systems/oneventcomponent.h"

#include "Scene/scenemanager.h"

#include "2D/simple2dobject.h"
#include "2D/animator2d.h"
#include "2D/collisionsystem.h"

#include "UI/ttftext.h"
#include "UI/progressbar.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "Helpers/stbi_image_write.h"

#include <iomanip>  // for std::setw and std::setfill
#include <sstream>  // for std::stringstream

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

void flipImageVertically(unsigned char* data, int width, int height)
{
    const int channels = 4;                // RGBA
    int rowSize = width * channels;        // bytes per row
    std::vector<unsigned char> temp(rowSize);

    for (int y = 0; y < height / 2; ++y)
    {
        unsigned char* row       = data + y * rowSize;
        unsigned char* opposite  = data + (height - 1 - y) * rowSize;

        // swap entire rows
        memcpy(temp.data(),   row,       rowSize);
        memcpy(row,           opposite,  rowSize);
        memcpy(opposite,      temp.data(), rowSize);
    }
}

struct ScreenSaverSystem : public System<Listener<SavedFrameData>, StoragePolicy>
{
    virtual void onEvent(const SavedFrameData &event)
    {
        LOG_INFO("ScreenSaverSystem", "Saving frame");

        auto pixels = event.pixels;

        flipImageVertically(pixels.data(), event.width, event.height);

        std::stringstream ss;

        ss << "res/savedFrames/frame_" << std::setw(4) << std::setfill('0') << i++ << ".png";

        std::string frameName = ss.str();

        // std::string frameName = "res/savedFrames/frame_" + std::to_string(i++) + ".png";

        stbi_write_png(frameName.c_str(), event.width, event.height, 4, pixels.data(), event.width * 4);
    }

    size_t i = 0;
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

    // mainWindow->ecs.createSystem<CameraShakeSystem>();

    // mainWindow->ecs.succeed<FollowCamera2DSystem, CameraShakeSystem>();
    // mainWindow->ecs.succeed<FollowCamera2DSystem, PositionComponent>();
    // mainWindow->ecs.succeed<MasterRenderer, FollowCamera2DSystem>();

    mainWindow->ecs.createSystem<MoveToSystem>();

    mainWindow->ecs.createSystem<MoveDirSystem>();

    // mainWindow->ecs.createSystem<TweenSystem>();

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

    mainWindow->ecs.dumbTaskflow();

    // mainWindow->interpreter->interpretFromFile("main.pg");

    // mainWindow->ecs.start();

    mainWindow->ecs.createSystem<ScreenSaverSystem>();

    auto* ecsRef = &mainWindow->ecs;

    auto square = makeSimple2DShape(ecsRef, Shape2D::Square, 50, 50, {255.0f, 0.0f, 0.0f, 255.0f});

    mainWindow->ecs.executeOnce();
    mainWindow->ecs.executeOnce();

    mainWindow->render();

    mainWindow->resize(820, 640);

    for (int i = 0; i < 60; i++)
    {
        mainWindow->ecs.sendEvent(SaveCurrentFrameEvent{});

        mainWindow->ecs.executeOnce();

        square.get<PositionComponent>()->setX(square.get<PositionComponent>()->x + 10);
        square.get<PositionComponent>()->setY(square.get<PositionComponent>()->y + 10);

        mainWindow->render();

        std::cout << "here: " << i << std::endl;
    }

    // Tunable parameters:
    const int runsRight = 3;     // how many times to spawn at left and go right
    const int runsLeft  = 2;     // how many times to spawn at right and go left
    const float speed   = 300;   // pixels per second

    // Inside initGame(), after you’ve set up `square` and resized the window:
    int windowW = 820;
    int windowH = 640;

    // width of your square (as used in makeSimple2DShape)
    const float squareSize = 50;

    // helper to perform one “run” in a given direction, capturing frames:
    auto doRun = [&](float startX, float endX, float dirSign, int runIndex){
        square.get<PositionComponent>()->setX(startX);
        square.get<PositionComponent>()->setY(windowH/2 - squareSize/2);

        // until we reach (or pass) endX:
        while ((dirSign > 0 && square.get<PositionComponent>()->x < endX) ||
            (dirSign < 0 && square.get<PositionComponent>()->x > endX))
        {
            // send frame‑save event
            mainWindow->ecs.sendEvent(SaveCurrentFrameEvent{});
            mainWindow->ecs.executeOnce();

            // advance position by speed·dt (we’ll just pick a fixed dt based on your target FPS)
            const float dt = 1.0f / 30.0f;  // 30 FPS
            float delta = speed * dt * dirSign;
            square.get<PositionComponent>()->setX(square.get<PositionComponent>()->x + delta);

            mainWindow->render();
        }
    };

    // now loop the runs:
    for (int i = 0; i < runsRight; ++i) {
        // spawn at left edge, go to right edge
        doRun(
        /*startX=*/ 0.0f,
        /*endX  =*/ windowW - squareSize,
        /*dirSign=*/ +1.0f,
        /*runIndex=*/ i
        );
    }

    for (int i = 0; i < runsLeft; ++i) {
        // spawn at right edge, go to left edge
        doRun(
        /*startX=*/ windowW - squareSize,
        /*endX  =*/ 0.0f,
        /*dirSign=*/ -1.0f,
        /*runIndex=*/ i
        );
    }
    

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
