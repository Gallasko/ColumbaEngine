#include "application.h"

#include "logger.h"

#include "Systems/basicsystems.h"
#include "Systems/oneventcomponent.h"

#include "Scene/scenemanager.h"

#include "2D/simple2dobject.h"

#include "UI/ttftext.h"
#include "UI/progressbar.h"

#include "fightscene.h"
#include "characustomizationscene.h"
#include "locationscene.h"
#include "inventory.h"

#include "gamemodule.h"

using namespace pg;

namespace
{
    static const char* const DOM = "Editor app";
}

GameApp::GameApp(const std::string& appName) : appName(appName)
{
    LOG_THIS_MEMBER(DOM);
}

GameApp::~GameApp()
{
    LOG_THIS_MEMBER(DOM);
}

enum class SceneName
{
    Customization,
    Inventory,
    Location
};

struct SceneToLoad
{
    SceneToLoad(const SceneName& name) : name(name) {}

    SceneName name;
};

struct SceneLoader : public System<Listener<SceneToLoad>, Listener<TickEvent>, StoragePolicy, InitSys, SaveSys>
{
    virtual std::string getSystemName() const override { return "SceneLoader"; }

    virtual void save(Archive& archive) override
    {
        archive.startSerialization("SceneLoader");

        serialize(archive, "fill", fill);

        archive.endSerialization();
    }

    virtual void load(const UnserializedObject& serializedString) override
    {
        if (serializedString.isNull())
        {
            LOG_ERROR(DOM, "Element is null");
        }
        else
        {
            LOG_INFO(DOM, "Deserializing SceneLoader");

            fill = deserialize<float>(serializedString["fill"]);
        }
    }

    virtual void onEvent(const SceneToLoad& event) override
    {
        switch (event.name)
        {
        case SceneName::Customization:
            ecsRef->getSystem<SceneElementSystem>()->loadSystemScene<PlayerCustomizationScene>();
            break;

        case SceneName::Inventory:
            ecsRef->getSystem<SceneElementSystem>()->loadSystemScene<InventoryScene>();
            break;

        case SceneName::Location:
            ecsRef->getSystem<SceneElementSystem>()->loadSystemScene<LocationScene>();
            break;

        default:
            break;
        }
    }

    virtual void onEvent(const TickEvent&) override
    {
        fill += 0.001f;

        if (fill > 1.0f)
            fill = 0.0f;

        LOG_INFO("Event", fill);

        barComp->setFillPercent(fill);
    }

    virtual void init() override
    {
        // Navigation tabs
        auto titleTTF = makeTTFText(ecsRef, 50, 0, "res/font/Inter/static/Inter_28pt-Light.ttf", "Customization", 0.4);
        titleTTF.get<UiComponent>()->setZ(1);
        ecsRef->attach<MouseLeftClickComponent>(titleTTF.entity, makeCallable<SceneToLoad>(SceneName::Customization));

        auto titleTTF2 = makeTTFText(ecsRef, 225, 0, "res/font/Inter/static/Inter_28pt-Light.ttf", "Inventory", 0.4);
        ecsRef->attach<MouseLeftClickComponent>(titleTTF2.entity, makeCallable<SceneToLoad>(SceneName::Inventory));

        auto titleTTF3 = makeTTFText(ecsRef, 330, 0, "res/font/Inter/static/Inter_28pt-Light.ttf", "Location", 0.4);
        ecsRef->attach<MouseLeftClickComponent>(titleTTF3.entity, makeCallable<SceneToLoad>(SceneName::Location));

        auto progressBar = makeProgressBar(ecsRef, 400, 100, "emptyBar", "fullBar", 0.65);
        progressBar.get<UiComponent>()->setY(250);

        barComp = progressBar.get<ProgressBarComponent>();

        barComp->percent = fill;

        auto overProgressBar = makeUiTexture(ecsRef, 400, 100, "overBar");
        overProgressBar.get<UiComponent>()->setY(250);
        overProgressBar.get<UiComponent>()->setZ(1);

        // auto testText = makeSentence(ecsRef, 200, 400, {"Hello World"});

    }

    CompRef<ProgressBarComponent> barComp;

    float fill = 0.0f;
};

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

    mainWindow->ecs.createSystem<InventorySystem>();

    mainWindow->ecs.createSystem<MoveToSystem>();

    // mainWindow->ecs.createSystem<ContextMenu>();
    // mainWindow->ecs.createSystem<InspectorSystem>();
    auto ttfSys = mainWindow->ecs.createSystem<TTFTextSystem>(mainWindow->masterRenderer);

    ttfSys->registerFont("res/font/Inter/static/Inter_28pt-Light.ttf");
    ttfSys->registerFont("res/font/Inter/static/Inter_28pt-Bold.ttf");
    ttfSys->registerFont("res/font/Inter/static/Inter_28pt-Italic.ttf");

    mainWindow->masterRenderer->processTextureRegister();

    auto sTreeDatas = mainWindow->ecs.createSystem<SkillTreeDatabase>();

    sTreeDatas->addSkillTree(AdventurerTree{});
    sTreeDatas->addSkillTree(MageTree{});
    sTreeDatas->addSkillTree(WarriorTree{});

    mainWindow->ecs.createSystem<FightSystem>();

    mainWindow->ecs.succeed<MasterRenderer, TTFTextSystem>();

    mainWindow->ecs.createSystem<PlayerHandlingSystem>();

    mainWindow->ecs.createSystem<LocationSystem>();

    mainWindow->ecs.createSystem<SceneLoader>();

    mainWindow->interpreter->addSystemModule("game", GameModule{&mainWindow->ecs});

    mainWindow->interpreter->interpretFromFile("main.pg");

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

int GameApp::exec()
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
