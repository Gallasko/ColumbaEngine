#include "stdafx.h"

#include "engine.h"

#include "application.h"
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

Engine& Engine::setInitializer(std::unique_ptr<AppInitializer> init)
{
    initializer = std::move(init);
    return *this;
}

Engine& Engine::setSetupFunction(
    std::function<void(EntitySystem&, Window&)> setup,
    std::function<void(EntitySystem&, Window&)> postInit
) {
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
    EM_ASM({
        var saveFolder = UTF8ToString($0);

        FS.mkdir('/' + saveFolder);
        FS.mount(IDBFS, {autoPersist: true}, '/' + saveFolder);

        FS.syncfs(true, function (err) {
            if (err) {
                console.error("Initial filesystem sync error:", err);
            } else {
                console.log("Filesystem initialized and synced for folder: /" + saveFolder);
            }
        });
    }, config.saveFolder.c_str());
#else
    LOG_INFO(DOM, "Desktop save path: " << config.saveFolder);
#endif
}

void Engine::initializeWindow()
{
    LOG_INFO(DOM, "Creating window: " << appName << " (" << config.width << "x" << config.height << ")");

    mainWindow = new pg::Window(appName, savePath);

    LOG_INFO(DOM, "Window created with save path: " << savePath);
    windowReady = true;
}

void Engine::initializeECS()
{
    if (not mainWindow)
    {
        LOG_ERROR(DOM, "Cannot initialize ECS without window");
        return;
    }

    LOG_INFO(DOM, "Initializing engine...");
    mainWindow->initEngine();

    if (initializer)
    {
        initializer->setupSystems(mainWindow->ecs, *mainWindow);
    }

    mainWindow->ecs.start();
    ecsReady = true;

    if (initializer)
    {
        initializer->postInit(mainWindow->ecs, *mainWindow);
    }

    LOG_INFO(DOM, "Engine initialized");
}

void Engine::windowInitCallback()
{
    initializeWindow();
}

void Engine::mainLoopCallback(void* arg)
{
    if (not windowReady.load())
        return;

    if (not initialized)
    {
#ifdef __EMSCRIPTEN__
        if (initThread)
        {
            initThread->join();
            delete initThread;
            initThread = nullptr;
        }

        mainWindow->init(config.width, config.height, config.fullscreen, static_cast<SDL_Window*>(arg));
#endif

        initializeECS();
        mainWindow->resize(config.width, config.height);
        initialized = true;

        LOG_INFO(DOM, "Full initialization complete");
    }

    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        mainWindow->processEvents(event);

#ifdef __EMSCRIPTEN__
        if (event.type == SDL_QUIT)
        {
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
        emscripten_cancel_main_loop();
#endif
    }
}

int Engine::exec()
{
    LOG_INFO(DOM, "Starting engine with config - App: " << appName
             << ", Size: " << config.width << "x" << config.height
             << ", Save: " << config.saveFolder);

    setupFilesystem();

#ifdef __EMSCRIPTEN__
    initThread = new std::thread([this](){ this->windowInitCallback(); });

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
        LOG_ERROR(DOM, "Failed to create SDL window for Emscripten");
        return -1;
    }

    emscripten_set_main_loop_arg([](void* arg) {
        void** args = static_cast<void**>(arg);
        Engine* engine = static_cast<Engine*>(args[1]);
        engine->mainLoopCallback(args[0]);
    }, new void*[2]{pWindow, this}, 0, 1);

#else
    initializeWindow();
    mainWindow->init(config.width, config.height, config.fullscreen);
    initializeECS();
    mainWindow->resize(config.width, config.height);
    initialized = true;

    LOG_INFO(DOM, "Desktop initialization complete");

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