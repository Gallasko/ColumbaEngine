#include "application.h"

#include "logger.h"

#include "Systems/basicsystems.h"
#include "Systems/oneventcomponent.h"

#include "Scene/scenemanager.h"

#include "2D/simple2dobject.h"
#include "2D/animator2d.h"
#include "2D/collisionsystem.h"
// near top of file, after your other includes:
#include "2D/collisionsystem.h"   // for CollisionComponent

#include "Systems/coresystems.h"
#include "Systems/basicsystems.h"

#include "UI/ttftext.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "Helpers/stbi_image_write.h"

#include <iomanip>  // for std::setw and std::setfill
#include <sstream>  // for std::stringstream


using namespace pg;

namespace {
    static const char *const DOM = "Editor app";
}


// define two empty tags to mark sides:
struct InstaFlag  : public Component {};
struct TikTokFlag : public Component {};
struct WallFlag   : public Component {};

const float W = 820, H = 640;
const int FPS = 60;
FILE* ffmpeg;

// -------------------------------------------------------
// TugOfWarSystem
// -------------------------------------------------------
struct TugOfWarSystem : public System<InitSys>
{
    TugOfWarSystem(int leftCount, int rightCount, float speed, float squareSize)
      : leftCount(leftCount),
        rightCount(rightCount),
        speed(speed),
        size(squareSize)
    {}

    virtual void init() override
    {
        // window dims (must match your resize)
        // const float W = 820.f, H = 640.f;
        const float y = H / 2.f - size / 2.f;

        std::vector<size_t> layer = {2};

        // spawn left→right
        for (int i = 0; i < leftCount; ++i)
        {
            auto sq = makeSimple2DShape(ecsRef, Shape2D::Square, size, size, { 0.f, 0.f, 255.f, 255.f });
            // position at left edge
            sq.get<PositionComponent>()->setX((rand() % (int)leftCount ) * size - leftCount * size);
            sq.get<PositionComponent>()->setY((rand() % (int)H - size ));
            // collision + tag
            sq.entity->attach<CollisionComponent>(1, 1.0f, layer);

            sq.entity->attach<InstaFlag>();
            // move to right edge
            sq.entity->attach<MoveDirComponent>(constant::Vector2D{1.f, 0.f}, speed);
        }

        // spawn right→left
        for (int i = 0; i < rightCount; ++i)
        {
            auto sq = makeSimple2DShape(ecsRef, Shape2D::Square, size, size, {255.f, 0.f, 0.f, 255.f});

            // position at right edge
            sq.get<PositionComponent>()->setX((rand() % (int)rightCount ) * size + W);
            sq.get<PositionComponent>()->setY((rand() % (int)H - size ));

            // collision + tag
            sq.entity->attach<CollisionComponent>(1, 1.0f, layer);
            sq.entity->attach<TikTokFlag>();
            // move to left edge
            sq.entity->attach<MoveDirComponent>(constant::Vector2D{-1.f, 0.f}, speed);
        }

        auto line = makeSimple2DShape(ecsRef, Shape2D::Square, 4, H, { 0.f, 0.f, 0.f, 255.f });

        line.get<PositionComponent>()->setX(W / 2.f);

        line.entity->attach<CollisionComponent>(2);

        line.entity->attach<WallFlag>();

        makeCollisionHandlePair(ecsRef, [&](InstaFlag* insta, WallFlag* wall) {
            auto wallEnt = ecsRef->getEntity(wall->entityId);

            wallEnt->get<PositionComponent>()->setX(wallEnt->get<PositionComponent>()->x + 4.f);

            ecsRef->removeEntity(insta->entityId);
        }); 

        makeCollisionHandlePair(ecsRef, [&](TikTokFlag* tiktok, WallFlag* wall) {
            auto wallEnt = ecsRef->getEntity(wall->entityId);

            wallEnt->get<PositionComponent>()->setX(wallEnt->get<PositionComponent>()->x - 4.f);

            ecsRef->removeEntity(tiktok->entityId);
        });

    }

    int   leftCount, rightCount;
    float speed, size;
};

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
        LOG_INFO("ScreenSaverSystem", "Saving frame to " << frameName);
        stbi_write_png(frameName.c_str(), event.width, event.height, 4, pixels.data(), event.width * 4);
        LOG_INFO("ScreenSaverSystem", "Frame saved");
    }

    size_t i = 0;
};

struct VideoCreatorSystem : public System<Listener<SavedFrameData>, StoragePolicy>
{
    virtual void onEvent(const SavedFrameData &event)
    {
        LOG_INFO("ScreenSaverSystem", "Saving frame");

        auto pixels = event.pixels;

        flipImageVertically(pixels.data(), event.width, event.height);

        size_t written = fwrite(pixels.data(), 1, pixels.size(), ffmpeg);
        
        if (written != pixels.size()) {
            fprintf(stderr, "Short write to ffmpeg pipe\n");
        }
    }
};


std::thread *initThread;
pg::Window *mainWindow = nullptr;
std::atomic<bool> initialized = {false};
bool init = false;
bool running = true;

struct FlagSystem : public System<StoragePolicy, Own<WallFlag>, Own<InstaFlag>, Own<TikTokFlag>>
{
};

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

    mainWindow->ecs.succeed<PositionComponent, MoveDirSystem>();

    // mainWindow->ecs.succeed<CollisionSystem, PositionComponent>();
    mainWindow->ecs.succeed<CollisionSystem, PositionComponent>();
    mainWindow->ecs.succeed<MasterRenderer, CollisionSystem>();

    // mainWindow->ecs.createSystem<ContextMenu>();
    // mainWindow->ecs.createSystem<InspectorSystem>();
    auto ttfSys = mainWindow->ecs.createSystem<TTFTextSystem>(mainWindow->masterRenderer);

    ttfSys->registerFont("res/font/Inter/static/Inter_28pt-Light.ttf");
    ttfSys->registerFont("res/font/Inter/static/Inter_28pt-Bold.ttf");
    ttfSys->registerFont("res/font/Inter/static/Inter_28pt-Italic.ttf");

    mainWindow->masterRenderer->processTextureRegister();

    mainWindow->ecs.succeed<MasterRenderer, TTFTextSystem>();

    mainWindow->ecs.createSystem<FlagSystem>();

    mainWindow->ecs.dumbTaskflow();

    // mainWindow->interpreter->interpretFromFile("main.pg");

    // mainWindow->ecs.start();

    ffmpeg = popen(
        ("ffmpeg -y "
         "-f rawvideo "
         "-pixel_format rgba "
         "-video_size " + std::to_string(int(W)) + "x" + std::to_string(int(H)) + " "
         "-framerate " + std::to_string(FPS) + " "
         "-i - "
         "-c:v libx264 "
         "-pix_fmt yuv420p "
         "output.mp4").c_str(),
        "w"
    );

    if (not ffmpeg) {
        fprintf(stderr, "Failed to open ffmpeg pipe\n");
        std::exit(1);
    }

    srand(time(nullptr));

    // mainWindow->ecs.createSystem<ScreenSaverSystem>();
    mainWindow->ecs.createSystem<VideoCreatorSystem>();

    mainWindow->ecs.createSystem<MoveDirSystem>();

    mainWindow->ecs.createSystem<TugOfWarSystem>(270, 270, 45, 20);

    auto* ecsRef = &mainWindow->ecs;

    // Pause the ticking system so that we can feed our own delta
    ecsRef->getSystem<TickingSystem>()->pause();

    auto square = makeSimple2DShape(ecsRef, Shape2D::Square, 50, 50, {255.0f, 0.0f, 0.0f, 255.0f});

    mainWindow->ecs.executeOnce();
    mainWindow->ecs.executeOnce();

    mainWindow->render();

    mainWindow->resize(820, 640);

    for (int i = 0; i < FPS * 61; i++)
    {
        mainWindow->ecs.sendEvent(SaveCurrentFrameEvent{});

        mainWindow->ecs.executeOnce();

        mainWindow->render();

        ecsRef->sendEvent(TickEvent{1000 / FPS});
    }

    pclose(ffmpeg);

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
