#include "application.h"

#include "logger.h"

#include "Systems/basicsystems.h"
#include "Systems/oneventcomponent.h"

#include "Gui/contextmenu.h"
#include "Gui/inspector.h"

#include "Scene/scenemanager.h"

#include "UI/ttftext.h"

using namespace pg;
using namespace editor;

namespace
{
    static const char* const DOM = "Editor app";
}

struct EntityFinder : public System<Listener<OnMouseClick>, Ref<UiComponent>, Ref<SceneElement>, InitSys>
{
    virtual void init() override
    {
        registerGroup<UiComponent, SceneElement>();
    }

    virtual void onEvent(const OnMouseClick& event) override
    {
        for (const auto& elem : viewGroup<UiComponent, SceneElement>())
        {
            auto ui = elem->get<UiComponent>();

            if (ui->inClipBound(event.pos.x, event.pos.y))
            {
                LOG_INFO(DOM, "Clicked on entity: " << ui.entityId);

                ecsRef->sendEvent(InspectEvent{elem->entity});
                return;
            }
        }
    }
};

EditorApp::EditorApp(const std::string& appName) : appName(appName)
{
    LOG_THIS_MEMBER(DOM);
}

EditorApp::~EditorApp()
{
    LOG_THIS_MEMBER(DOM);
}

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

    mainWindow->ecs.createSystem<MoveToSystem>();
    mainWindow->ecs.createSystem<ContextMenu>();
    mainWindow->ecs.createSystem<InspectorSystem>();
    auto ttfSys = mainWindow->ecs.createSystem<TTFTextSystem>(mainWindow->masterRenderer);

    ttfSys->registerFont("res/font/Inter/static/Inter_28pt-Light.ttf");

    mainWindow->ecs.succeed<InspectorSystem, ListViewSystem>();
    mainWindow->ecs.succeed<MasterRenderer, TTFTextSystem>();

    auto ent = mainWindow->ecs.createEntity();

    std::function callback = [](const OnMouseClick& event) {
        if (event.button == SDL_BUTTON_RIGHT)
        {
            mainWindow->ecs.sendEvent(ShowContextMenu{mainWindow->getInputHandler()});
        }
    };

    mainWindow->ecs.attach<OnEventComponent>(ent, callback);

    mainWindow->ecs.createSystem<EntityFinder>();

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

int EditorApp::exec()
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
