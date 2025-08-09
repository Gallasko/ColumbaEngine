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
}

Engine::Engine(const std::string& name, const EngineConfig& engineConfig)
    : appName(name), config(engineConfig)
{
    LOG_THIS_MEMBER(DOM);
    savePath = constructSavePath();
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

Engine& Engine::setSetupFunction(std::function<void(EntitySystem&, Window&)> setup)
{
    this->setup = setup;
    return *this;
}

Engine& Engine::setPostInitFunction(std::function<void(EntitySystem&, Window&)> postInit)
{
    this->postInit = postInit;
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
    if (!mainWindow)
    {
        printf("Cannot initialize ECS without window\n");
        return;
    }

    printf("Initializing engine...\n");
    
    try {
        mainWindow->initEngine();
        printf("Config: %dx%d", config.width, config.height);

        if (setup)
        {
            printf("Setting up systems...\n");
            setup(mainWindow->ecs, *mainWindow);
        }
        else
        {
            printf("No initializer provided, using default systems...\n");
        }

        printf("Starting ECS...\n");
        mainWindow->ecs.start();
        ecsReady = true;

        if (postInit)
        {
            printf("Running post-init...\n");
            postInit(mainWindow->ecs, *mainWindow);
        }

        mainWindow->ecs.dumbTaskflow();
        printf("Engine initialized successfully\n");
    } catch (const std::exception& e) {
        printf("ECS initialization failed: %s\n", e.what());
        ecsReady = false;
    }
}

static void mainLoopCallback(void* arg)
{
    void** args = static_cast<void**>(arg);
    Engine* engine = static_cast<Engine*>(args[0]);

    printf("mainLoopCallback called, windowReady: %s, initialized: %s\n", 
           engine->windowReady.load() ? "true" : "false", 
           engine->initialized ? "true" : "false");

    if (!engine->windowReady.load()) {
        printf("Window not ready, returning early\n");
        return;
    }

    if (!engine->initialized)
    {
        printf("Starting initialization sequence...\n");

#ifdef __EMSCRIPTEN__
        printf("Completing Emscripten initialization...\n");

        if (engine->mainWindow && args[1]) {
            printf("Initializing window with SDL context...\n");
            try {
                engine->mainWindow->init(engine->config.width, engine->config.height, engine->config.fullscreen, static_cast<SDL_Window*>(args[1]));
                printf("Window SDL init completed\n");
            } catch (const std::exception& e) {
                printf("Exception during window init: %s\n", e.what());
                return;
            }
        } else {
            printf("Cannot init window: mainWindow=%p, arg=%p\n", engine->mainWindow, args[1]);
        }
#endif

        printf("Initializing ECS...\n");
        try {
            engine->initializeECS();
            printf("ECS initialization completed\n");
        } catch (const std::exception& e) {
            printf("Exception during ECS init: %s\n", e.what());
            return;
        }
        
        if (engine->mainWindow) {
            printf("Resizing window to %dx%d...\n", engine->config.width, engine->config.height);
            try {
                engine->mainWindow->resize(engine->config.width, engine->config.height);
                printf("Window resize completed\n");
            } catch (const std::exception& e) {
                printf("Exception during window resize: %s\n", e.what());
                return;
            }
        }
        
        engine->initialized = true;
        printf("Full initialization complete - entering main loop\n");
    }

    if (!engine->mainWindow) {
        printf("Error: mainWindow is null in main loop\n");
        return;
    }

    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        engine->mainWindow->processEvents(event);

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
            }, engine->config.saveFolder.c_str());
        }
#endif
    }

    engine->mainWindow->render();

    if (engine->mainWindow->requestQuit())
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
    printf("Starting Emscripten build...\n");
    
    initThread = new std::thread([this](){ 
        printf("Window init thread started...\n");
        this->initializeWindow(); 
        printf("Window init thread completed\n");
    });

    Uint32 windowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN;
    if (config.resizable)
        windowFlags |= SDL_WINDOW_RESIZABLE;
    if (config.fullscreen)
        windowFlags |= SDL_WINDOW_FULLSCREEN;

    printf("Creating SDL window...\n");
    SDL_Window* pWindow = SDL_CreateWindow(
        appName.c_str(),
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        config.width, config.height,
        windowFlags
    );

    if (!pWindow) {
        printf("Failed to create SDL window for Emscripten\n");
        return -1;
    }

    emscripten_set_main_loop_arg(mainLoopCallback, new void*[2]{this, pWindow}, 0, 1);

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