#include "application.h"

#include "logger.h"

#include "Networking/network_system.h"
#include "Networking/backend_sdl.h"


using namespace pg;

namespace {
    static const char *const DOM = "App";
}

GameApp::GameApp(const std::string &appName) : appName(appName)
{
    LOG_THIS_MEMBER(DOM);
}

GameApp::~GameApp()
{
    LOG_THIS_MEMBER(DOM);
}

std::thread *initThread;
pg::Window *mainWindow = nullptr;
std::atomic<bool> initialized = {false};
bool init = false;
bool running = true;

void initWindow(const std::string &appName)
{
#ifdef __EMSCRIPTEN__
    mainWindow = new pg::Window(appName, "/save/savedData.sz");
#else
    mainWindow = new pg::Window(appName);
#endif

    LOG_INFO(DOM, "Window init...");

    initialized = true;
}

struct TestSystem : public System<Listener<TickEvent>>
{
    float deltaTime = 0.0f;

    virtual void onEvent(const TickEvent& event) override
    {
        deltaTime += event.tick;
    }

    virtual void execute() override
    {
        if (deltaTime >= 1000.0f)
        {
            LOG_INFO("Event", "Sending data to server");

            std::string str = "Hello world ! That is reeeeeaaaaaaly reeeeeeaaalllly loong that will ovesize the actual tcp buffer data of our system to see if it is actually capable of handling realllly long input because otherwise it will be definitly impossible to see this if it is not correctly cut into smaller chunks !";
            std::vector<uint8_t> vec(str.begin(), str.end());

            LOG_INFO("Event", "Data size: " << vec.size());

            ecsRef->sendEvent(SendDataToServer{vec, true});

            deltaTime = 0.0f;
        }
    }
};

void initGame(int argc, char** argv)
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

    // ——— 1) Parse mode ———
    bool isServer = false;

    for (int i = 1; i < argc; ++i)
    {
        std::string a = argv[i];
        if (a == "--mode=server") isServer = true;
        if (a == "--mode=client") isServer = false;
    }


    NetworkConfig netCfg;

    netCfg.isServer     = isServer;
    netCfg.peerAddress  = "127.0.0.1";
    netCfg.udpLocalPort = isServer ? 9001 : 0;    // server binds; client gets ephemeral
    netCfg.udpPeerPort  = isServer ? 0    : 9001; // client → server port
    netCfg.tcpEnabled   = true;                   // disable TCP for PoC
    netCfg.tcpPort  = 9000;
    // you can tweak defaultSystemFlags here:
    netCfg.defaultSystemFlags.networked       = true;
    netCfg.defaultSystemFlags.reliableChannel = false;
    netCfg.defaultSystemFlags.updateRateHz    = 30.0f;

    // Todo delete this somewhere (should be when the system is deleted)
    auto *backend = new SdlNetworkBackend(netCfg);

    mainWindow->ecs.createSystem<NetworkSystem>(backend, netCfg);

    if (not isServer)
    {
        mainWindow->ecs.createSystem<TestSystem>();
    }

    // SDL_Quit();

    printf("Engine initialized ...\n");

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

void mainloop(int argc, char** argv, void *arg)
{
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

        initGame(argc, argv);
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

int GameApp::exec(int argc, char** argv)
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

    emscripten_set_main_loop_arg(mainloop, argc, argv, pWindow, 0, 1);

#else
    LOG_THIS_MEMBER(DOM);

    initWindow(appName);

    mainWindow->init(820, 640, false);

    LOG_INFO(DOM, "Window init done !");

    initGame(argc, argv);

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
