#include "application.h"

#include "logger.h"

#include "Systems/basicsystems.h"
#include "Systems/oneventcomponent.h"

#include "Gui/contextmenu.h"
#include "Gui/inspector.h"
#include "Gui/projectmanager.h"

#include "Scene/scenemanager.h"

#include "UI/ttftext.h"

#include "UI/utils.h"

using namespace pg;
using namespace editor;

namespace
{
    static const char* const DOM = "Editor app";
}

struct SelectedEntity
{
    SelectedEntity() : id(0) {}
    SelectedEntity(_unique_id id) : id(id) {}

    _unique_id id;
};

struct EntityFinder : public System<Listener<OnMouseClick>, Own<SelectedEntity>, Ref<PositionComponent>, Ref<SceneElement>, InitSys>
{
    EntityRef selectionOutline;

    virtual void init() override
    {
        registerGroup<PositionComponent, SceneElement>();

        auto outline = makeSelectionOutlinePrefab(ecsRef, 2.f, {255.0f, 255.0f, 0.0f, 255.0f}, false);

        outline.get<PositionComponent>()->setZ(25.f);

        selectionOutline = outline.entity;
        ecsRef->attach<EntityName>(selectionOutline, "SelectionOutline");
        ecsRef->attach<SelectedEntity>(selectionOutline);
    }

    virtual void onEvent(const OnMouseClick& event) override
    {
        bool hit = false;

        // scan all scene elements under the click
        for (const auto& elem : viewGroup<PositionComponent, SceneElement>())
        {
            if (inClipBound(elem->entity, event.pos.x, event.pos.y))
            {
                hit = true;

                LOG_INFO(DOM, "Clicked on entity: " << elem->entityId);

                // send inspect event
                ecsRef->sendEvent(InspectEvent{ elem->entity });

                // position & size our outline to wrap this entity
                auto pos = elem->get<PositionComponent>();
                auto outlinePos = selectionOutline.get<PositionComponent>();
                outlinePos->setX(pos->x - 2.0f);
                outlinePos->setY(pos->y - 2.0f);
                outlinePos->setWidth(pos->width + 4.0f);
                outlinePos->setHeight(pos->height + 4.0f);

                // show it
                selectionOutline.get<Prefab>()->setVisibility(true);
                selectionOutline.get<SelectedEntity>()->id = elem->entity.id;

                break;
            }
        }

        // If we clicked on empty space, hide the outline (and maybe clear inspect)
        if (not hit)
        {
            selectionOutline.get<Prefab>()->setVisibility(false);
        }
    }
};

struct DragSystem : public System<Listener<OnMouseClick>, Listener<OnMouseMove>, Listener<OnMouseRelease>, Ref<PositionComponent>, Ref<SceneElement>, InitSys>
{
    _unique_id draggingEntity = 0;
    float offsetX = 0.f, offsetY = 0.f;

    virtual std::string getSystemName() const override { return "Drag System"; }

    virtual void init() override
    {
        // we’ll want to query all draggable scene elements
        registerGroup<PositionComponent, SceneElement>();
    }

    virtual void onEvent(const OnMouseClick& e) override
    {
        // only start drag on left button
        if (e.button != SDL_BUTTON_LEFT) return;

        // find topmost element under cursor
        for (const auto& elem : viewGroup<PositionComponent, SceneElement>())
        {
            auto pos = elem->get<PositionComponent>();
            if (inClipBound(elem->entity, e.pos.x, e.pos.y))
            {
                draggingEntity = elem->entity.id;
                // remember offset so entity doesn't jump under cursor
                offsetX = e.pos.x - pos->x;
                offsetY = e.pos.y - pos->y;
                break;
            }
        }
    }

    virtual void onEvent(const OnMouseMove& e) override
    {
        if (draggingEntity == 0) return;

        // update position each frame
        auto pos = ecsRef->getComponent<PositionComponent>(draggingEntity);
        if (not pos) return;

        pos->setX(e.pos.x - offsetX);
        pos->setY(e.pos.y - offsetY);

        auto ent = ecsRef->getEntity("SelectionOutline");
        if (not ent) return;

        if (ent->get<SelectedEntity>()->id == draggingEntity)
        {
            auto outlinePos = ent->get<PositionComponent>();

            outlinePos->setX(pos->x - 2.f);
            outlinePos->setY(pos->y - 2.f);
            outlinePos->setWidth(pos->width + 4.f);
            outlinePos->setHeight(pos->height + 4.f);
        }
    }

    virtual void onEvent(const OnMouseRelease& e) override
    {
        // only stop drag on left button
        if (e.button != SDL_BUTTON_LEFT) return;

        draggingEntity = 0;
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

    mainWindow->ecs.createSystem<FpsSystem>();

    auto ttfSys = mainWindow->ecs.createSystem<TTFTextSystem>(mainWindow->masterRenderer);
    ttfSys->registerFont("res/font/Inter/static/Inter_28pt-Light.ttf");
    ttfSys->registerFont("res/font/Inter/static/Inter_28pt-Bold.ttf");
    ttfSys->registerFont("res/font/Inter/static/Inter_28pt-Italic.ttf");

    mainWindow->ecs.createSystem<MoveToSystem>();
    mainWindow->ecs.createSystem<ContextMenu>();
    mainWindow->ecs.createSystem<InspectorSystem>();

    // mainWindow->ecs.succeed<InspectorSystem, ListViewSystem>();
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
    mainWindow->ecs.createSystem<DragSystem>();

    mainWindow->ecs.start();

    mainWindow->render();

    mainWindow->resize(820, 640);

    // mainWindow->ecs.getSystem<SceneElementSystem>()->loadSystemScene<ProjectSelectorScene>();

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
