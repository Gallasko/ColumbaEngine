// Fixed engine.cpp
#include "stdafx.h"

#include "engine.h"

#include "window.h"
#include "logger.h"
#include "Systems/basicsystems.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

using namespace pg;

namespace
{
    static const char* const DOM = "Engine";
    
#ifdef __EMSCRIPTEN__
    // Global static variables for Emscripten callback
    static Engine* g_engine = nullptr;
    static SDL_Window* g_window = nullptr;
#endif
}

Engine::Engine(const std::string& name, const EngineConfig& engineConfig)
    : appName(name), config(engineConfig)
{
    LOG_THIS_MEMBER(DOM);
    savePath = constructSavePath();
    
#ifdef __EMSCRIPTEN__
    g_engine = this; // Set global reference for callbacks
#endif
}

Engine::~Engine()
{
    LOG_THIS_MEMBER(DOM);

    if (mainWindow)
    {
        delete mainWindow;
    }

#ifdef __EMSCRIPTEN__
    if (initThread)
    {
        initThread->join();
        delete initThread;
    }
    g_engine = nullptr; // Clear global reference
#endif
}

std::string Engine::constructSavePath() const
{
#ifdef __EMSCRIPTEN__
    return "/" + config.saveFolder + "/" + config.saveSystemFile;
#else
    return config.saveFolder + "/" + config.saveSystemFile;
#endif
}

Engine& Engine::setInitializer(std::unique_ptr<AppInitializer> init)
{
    initializer = std::move(init);
    return *this;
}

Engine& Engine::setSetupFunction(
    std::function<void(EntitySystem&, Window&)> setup,
    std::function<void(EntitySystem&, Window&)> postInit
)
{
    initializer = std::make_unique<FunctionInitializer>(setup, postInit);
    return *this;
}

EntitySystem* Engine::getECS() const
{
    return mainWindow ? &mainWindow->ecs : nullptr;
}

void Engine::setupFilesystem()
{
#ifdef __EMSCRIPTEN__
    printf("Setting up Emscripten filesystem...\n");
    EM_ASM({
        try {
            var saveFolder = UTF8ToString($0);
            console.log("Creating save folder:", saveFolder);
            
            if (!FS.analyzePath('/' + saveFolder).exists) {
                FS.mkdir('/' + saveFolder);
            }
            
            FS.mount(IDBFS, {autoPersist: true}, '/' + saveFolder);

            FS.syncfs(true, function (err) {
                if (err) {
                    console.error("Initial filesystem sync error:", err);
                } else {
                    console.log("Filesystem initialized and synced for folder: /" + saveFolder);
                }
            });
        } catch (e) {
            console.error("Filesystem setup error:", e);
        }
    }, config.saveFolder.c_str());
#else
    LOG_INFO(DOM, "Desktop save path: " << config.saveFolder);
#endif
}

void Engine::initializeWindow()
{
    printf("Creating window: %s (%dx%d)\n", appName.c_str(), config.width, config.height);

    try {
        mainWindow = new pg::Window(appName, savePath);
        printf("Window created successfully with save path: %s\n", savePath.c_str());
        windowReady = true;
    } catch (const std::exception& e) {
        printf("Failed to create window: %s\n", e.what());
        windowReady = false;
    }
}

void Engine::initializeECS()
{
    if (not mainWindow)
    {
        printf("Cannot initialize ECS without window\n");
        return;
    }

    printf("Initializing engine...\n");
    
    try {
        mainWindow->initEngine();

        if (initializer)
        {
            printf("Setting up systems...\n");
            initializer->setupSystems(mainWindow->ecs, *mainWindow);
        }

        printf("Starting ECS...\n");
        mainWindow->ecs.start();
        ecsReady = true;

        if (initializer)
        {
            printf("Running post-init...\n");
            initializer->postInit(mainWindow->ecs, *mainWindow);
        }

        printf("Engine initialized successfully\n");
    } catch (const std::exception& e) {
        printf("ECS initialization failed: %s\n", e.what());
        ecsReady = false;
    }
}

#ifdef __EMSCRIPTEN__
// Static callback function for Emscripten
static void emscripten_main_loop()
{
    if (!g_engine) {
        printf("Error: g_engine is null in main loop\n");
        return;
    }
    
    g_engine->mainLoopCallback(g_window);
}
#endif

void Engine::mainLoopCallback(void* arg)
{
    if (not windowReady.load())
        return;

    if (not initialized)
    {
#ifdef __EMSCRIPTEN__
        printf("Completing Emscripten initialization...\n");
        
        if (initThread)
        {
            printf("Joining init thread...\n");
            initThread->join();
            delete initThread;
            initThread = nullptr;
            printf("Init thread joined\n");
        }

        if (mainWindow && arg) {
            printf("Initializing window with SDL context...\n");
            mainWindow->init(config.width, config.height, config.fullscreen, static_cast<SDL_Window*>(arg));
        }
#endif

        printf("Initializing ECS...\n");
        initializeECS();
        
        if (mainWindow) {
            printf("Resizing window to %dx%d...\n", config.width, config.height);
            mainWindow->resize(config.width, config.height);
        }
        
        initialized = true;
        printf("Full initialization complete\n");
    }

    if (!mainWindow) {
        printf("Error: mainWindow is null in main loop\n");
        return;
    }

    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        mainWindow->processEvents(event);

#ifdef __EMSCRIPTEN__
        if (event.type == SDL_QUIT)
        {
            printf("Quit event received, syncing filesystem...\n");
            EM_ASM({
                var saveFolder = UTF8ToString($0);

                FS.syncfs(false, function (err)
                {
                    if (err) {
                        console.error("Final filesystem sync error:", err);
                    } else {
                        console.log("Filesystem synced on quit for folder: /" + saveFolder);
                    }
                });
            }, config.saveFolder.c_str());
        }
#endif
    }

    mainWindow->render();

    if (mainWindow->requestQuit())
    {
#ifdef __EMSCRIPTEN__
        printf("Quit requested, cancelling main loop\n");
        emscripten_cancel_main_loop();
#endif
    }
}

int Engine::exec()
{
    printf("Starting engine with config - App: %s, Size: %dx%d, Save: %s\n", 
           appName.c_str(), config.width, config.height, config.saveFolder.c_str());

    setupFilesystem();

#ifdef __EMSCRIPTEN__
    printf("Starting Emscripten build (no threading)...\n");
    
    // Initialize window immediately without threading
    printf("Initializing window directly...\n");
    initializeWindow();
    
    if (!mainWindow) {
        printf("Failed to create window\n");
        return -1;
    }

    // Create SDL window for Emscripten
    Uint32 windowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN;
    if (config.resizable)
        windowFlags |= SDL_WINDOW_RESIZABLE;
    if (config.fullscreen)
        windowFlags |= SDL_WINDOW_FULLSCREEN;

    SDL_Window* pWindow = SDL_CreateWindow(
        appName.c_str(),
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        config.width, config.height,
        windowFlags
    );

    if (not pWindow) {
        printf("Failed to create SDL window for Emscripten\n");
        return -1;
    }

    printf("SDL window created, starting main loop...\n");

    // Use the array-based callback approach
    emscripten_set_main_loop_arg([](void* arg) {
        void** args = static_cast<void**>(arg);
        SDL_Window* window = static_cast<SDL_Window*>(args[0]);
        Engine* engine = static_cast<Engine*>(args[1]);
        
        if (!engine) {
            printf("Error: Engine pointer is null in callback\n");
            return;
        }
        
        engine->mainLoopCallback(window);
    }, new void*[2]{pWindow, this}, 0, 1);

#else
    printf("Starting desktop build...\n");
    
    initializeWindow();
    if (!mainWindow) {
        printf("Failed to create window on desktop\n");
        return -1;
    }
    
    mainWindow->init(config.width, config.height, config.fullscreen);
    initializeECS();
    mainWindow->resize(config.width, config.height);
    initialized = true;

    printf("Desktop initialization complete\n");

    bool running = true;
    while (running)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            mainWindow->processEvents(event);
        }

        mainWindow->render();

        if (mainWindow->requestQuit())
        {
            running = false;
        }
    }
#endif

    return 0;
}