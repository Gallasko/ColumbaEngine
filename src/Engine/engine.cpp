// Fixed engine.cpp
#include "stdafx.h"
#include "engine.h"
#include "window.h"
#include "logger.h"
#include "Systems/basicsystems.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#include <emscripten/wasmfs.h>
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
        try
        {
            if (initThread->joinable())
                initThread->join();
        }
        catch (const std::exception& e)
        {
            printf("Error joining init thread: %s\n", e.what());
        }

        delete initThread;
    }
#endif

    LOG_INFO(DOM, "Engine destroyed");
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
    return mainWindow ? mainWindow->ecs : nullptr;
}

void Engine::setupFilesystem()
{
#ifdef __EMSCRIPTEN__
    printf("Setting up WasmFS filesystem (OPFS mount will happen in init thread)...\n");
#else
    LOG_INFO(DOM, "Desktop save path: " << config.saveFolder);
#endif
}

void Engine::initializeWindow()
{
    printf("Creating window: %s (%dx%d)\n", appName.c_str(), config.width, config.height);

    try
    {
        mainWindow = new pg::Window(appName, savePath);
        printf("Window created successfully with save path: %s\n", savePath.c_str());
        windowReady = true;
    }
    catch (const std::exception& e)
    {
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

    try
    {
        mainWindow->initEngine();

        // Version check: load manifest and compare against saved version
        auto versionResult = versionManager.initialize(
            config.manifestPath, *mainWindow->ecs,
            config.autoWipeSaveOnMajorBump, config.autoRunMigrations);

        if (versionResult.isMajorBump)
        {
            printf("Major version bump: save wiped (%s -> %s)\n",
                   versionResult.oldVersion.toString().c_str(),
                   versionResult.newVersion.toString().c_str());
        }
        else if (versionResult.isNewInstall)
        {
            printf("New install: version %s\n",
                   versionResult.newVersion.toString().c_str());
        }
        else if (!versionResult.isSameVersion)
        {
            printf("Version updated: %s -> %s\n",
                   versionResult.oldVersion.toString().c_str(),
                   versionResult.newVersion.toString().c_str());
        }

        printf("Config: %dx%d\n", config.width, config.height);

        if (setup)
        {
            printf("Setting up systems...\n");
            setup(*mainWindow->ecs, *mainWindow);
        }
        else
        {
            printf("No initializer provided, using default systems...\n");
        }

        if (config.autoStartECS)
        {
            printf("Starting ECS (auto-start enabled)...\n");
            mainWindow->ecs->start();
        }
        else
        {
            printf("ECS auto-start disabled - call getECS()->start() manually when ready\n");
        }
        ecsReady = true;

#ifdef __EMSCRIPTEN__
        // Register browser lifecycle callbacks so data is saved on tab hide / page close.
        // Using a file-scope pointer is safe here: only one Engine exists per page.
        static EntitySystem* s_ecsForSave = nullptr;
        s_ecsForSave = mainWindow->ecs;

        // Save when the tab/window loses visibility (switch tab, minimize, etc.)
        emscripten_set_visibilitychange_callback(nullptr, false,
            [](int, const EmscriptenVisibilityChangeEvent* e, void*) -> EM_BOOL {
                if (e->hidden && s_ecsForSave)
                    s_ecsForSave->forceSaveNow();
                return EM_TRUE;
            });

        // Save on page refresh / close (beforeunload fires synchronously)
        emscripten_set_beforeunload_callback(nullptr,
            [](int, const void*, void*) -> const char* {
                if (s_ecsForSave)
                    s_ecsForSave->forceSaveNow();
                return nullptr; // nullptr = no "Are you sure?" dialog
            });

        printf("Registered browser save callbacks (visibilitychange + beforeunload)\n");
#endif

        if (postInit)
        {
            printf("Running post-init...\n");
            postInit(*mainWindow->ecs, *mainWindow);
        }
        else
        {
            printf("No post-init provided, nothing to be done...\n");
        }

        // mainWindow->ecs->dumbTaskflow();
        printf("Engine initialized successfully\n");
    }
    catch (const std::exception& e)
    {
        printf("ECS initialization failed: %s\n", e.what());
        ecsReady = false;
    }
}

#ifdef __EMSCRIPTEN__
static void mainLoopCallback(void* arg)
{
    void** args = static_cast<void**>(arg);
    Engine* engine = static_cast<Engine*>(args[0]);

    if (not engine->windowReady.load())
    {
        printf("Window not ready, returning early\n");
        return;
    }

    if (not engine->initialized)
    {
        printf("Starting initialization sequence...\n");

        if (engine->mainWindow && args[1])
        {
            printf("Initializing window with SDL context...\n");
            try
            {
                engine->mainWindow->init(engine->config.width, engine->config.height, engine->config.fullscreen, static_cast<SDL_Window*>(args[1]));

                printf("Window SDL init completed\n");
            }
            catch (const std::exception& e)
            {
                printf("Exception during window init: %s\n", e.what());

                return;
            }
        }
        else
        {
            printf("Cannot init window: mainWindow=%p, arg=%p\n", engine->mainWindow, args[1]);
        }

        printf("Initializing ECS...\n");

        try
        {
            engine->initializeECS();

            printf("ECS initialization completed\n");
        }
        catch (const std::exception& e)
        {
            printf("Exception during ECS init: %s\n", e.what());
            return;
        }

        if (engine->mainWindow)
        {
            printf("Resizing window to %dx%d...\n", engine->config.width, engine->config.height);

            try
            {
                engine->mainWindow->resize(engine->config.width, engine->config.height);
                printf("Window resize completed\n");
            }
            catch (const std::exception& e)
            {
                printf("Exception during window resize: %s\n", e.what());
                return;
            }
        }

        engine->initialized = true;

        printf("Full initialization complete - entering main loop\n");
    }

    if (not engine->mainWindow)
    {
        printf("Error: mainWindow is null in main loop\n");
        return;
    }

    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        engine->mainWindow->processEvents(event);

    }

    engine->mainWindow->render();

    if (engine->mainWindow->requestQuit())
    {
        printf("Quit requested, cancelling main loop\n");
        emscripten_cancel_main_loop();
    }
}
#endif

int Engine::exec()
{
    printf("Starting engine with config - App: %s, Size: %dx%d, Save: %s\n",
           appName.c_str(), config.width, config.height, config.saveFolder.c_str());

    setupFilesystem();

#ifdef __EMSCRIPTEN__
    printf("Starting Emscripten build...\n");

    // OPFS must be mounted from a pthread (not the main thread)
    printf("Mounting OPFS backend at /%s...\n", config.saveFolder.c_str());
    std::string savePath = "/" + config.saveFolder;
    backend_t backend = wasmfs_create_opfs_backend();
    int err = wasmfs_create_directory(savePath.c_str(), 0777, backend);
    if (err != 0 && errno != EEXIST)
        printf("Warning: OPFS directory creation returned %d (errno=%d)\n", err, errno);
    else
        printf("OPFS backend mounted at %s\n", savePath.c_str());

    printf("Initializing SDL...\n");
    if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) != 0)
    {
        printf("SDL_Init failed: %s\n", SDL_GetError());
        return -1;
    }
    printf("SDL initialized\n");

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BUFFER_SIZE, 32);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

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

    if (not pWindow)
    {
        printf("Failed to create SDL window for Emscripten\n");
        return -1;
    }

    printf("SDL window created, starting init thread...\n");

    // Start the init thread AFTER SDL setup so OPFS promises can resolve
    // once the browser event loop is running (after emscripten_set_main_loop_arg)
    initThread = new std::thread([this]()
    {
        printf("Window init thread started...\n");

        this->initializeWindow();

        printf("Window init thread completed\n");
    });

    auto args = new void*[2]{this, pWindow};

    emscripten_set_main_loop_arg(mainLoopCallback, args, 0, 1);

#else
    printf("Starting desktop build...\n");

    initializeWindow();
    if (not mainWindow)
    {
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