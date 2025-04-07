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
#include "gamefacts.h"

#include "gamelog.h"

#include "achievement.h"

#include "Helpers/tinyfiledialogs.h"

#include "2D/position.h"

#include "UI/sizer.h"
#include "UI/prefab.h"

#include "managenerator.h"

#include "nexusscene.h"

#include "theme.h"

#include "Systems/tween.h"

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
    Nexus,
    Customization,
    Inventory,
    Location
};

struct SceneToLoad
{
    SceneToLoad(const SceneName& name) : name(name) {}

    SceneName name;
};

struct ChangePortraitEvent {};

struct SceneLoader : public System<Listener<SceneToLoad>, Listener<TickEvent>, StoragePolicy, InitSys, SaveSys>
{
    virtual std::string getSystemName() const override { return "SceneLoader"; }

    virtual void save(Archive& archive) override
    {
        serialize(archive, "fill", fill);

        serialize(archive, "test", test);
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

            test = deserializeVector<int>(serializedString["test"]);
        }
    }

    virtual void onEvent(const SceneToLoad& event) override
    {
        switch (event.name)
        {
        case SceneName::Nexus:
            ecsRef->saveSystems();
            ecsRef->getSystem<SceneElementSystem>()->loadSystemScene<NexusScene>();
            break;

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
        // fill += 0.001f;

        // if (fill > 1.0f)
        //     fill = 0.0f;

        // barComp->setFillPercent(fill);
    }

    virtual void init() override
    {
        // Navigation tabs
        auto windowEnt = ecsRef->getEntity("__MainWindow");

        auto windowAnchor = windowEnt->get<UiAnchor>();

        auto titleTTF0 = makeTTFText(ecsRef, 25, 0, 1, "res/font/Inter/static/Inter_28pt-Light.ttf", "Nexus", 0.4);
        ecsRef->attach<MouseLeftClickComponent>(titleTTF0.entity, makeCallable<SceneToLoad>(SceneName::Nexus));
        auto t0Anchor = titleTTF0.get<UiAnchor>();

        t0Anchor->setTopAnchor(windowAnchor->top);
        t0Anchor->setTopMargin(12);

        auto titleTTF = makeTTFText(ecsRef, 25, 0, 1, "res/font/Inter/static/Inter_28pt-Light.ttf", "Customization", 0.4);
        ecsRef->attach<MouseLeftClickComponent>(titleTTF.entity, makeCallable<SceneToLoad>(SceneName::Customization));
        auto t1Anchor = titleTTF.get<UiAnchor>();

        t1Anchor->setLeftAnchor(t0Anchor->right);
        t1Anchor->setLeftMargin(8);
        t1Anchor->setBottomAnchor(t0Anchor->bottom);

        auto titleTTF2 = makeTTFText(ecsRef, 0, 0, 1, "res/font/Inter/static/Inter_28pt-Light.ttf", "Inventory", 0.4);
        ecsRef->attach<MouseLeftClickComponent>(titleTTF2.entity, makeCallable<SceneToLoad>(SceneName::Inventory));
        auto t2Anchor = titleTTF2.get<UiAnchor>();

        t2Anchor->setLeftAnchor(t1Anchor->right);
        t2Anchor->setLeftMargin(8);
        t2Anchor->setBottomAnchor(t1Anchor->bottom);

        auto titleTTF3 = makeTTFText(ecsRef, 0, 0, 1, "res/font/Inter/static/Inter_28pt-Light.ttf", "Location", 0.4);
        ecsRef->attach<MouseLeftClickComponent>(titleTTF3.entity, makeCallable<SceneToLoad>(SceneName::Location));
        auto t3Anchor = titleTTF3.get<UiAnchor>();

        t3Anchor->setLeftAnchor(t2Anchor->right);
        t3Anchor->setLeftMargin(8);
        t3Anchor->setBottomAnchor(t1Anchor->bottom);

        auto tweenTest = makeUiSimple2DShape(ecsRef, Shape2D::Square, 60, 60, {0.0f, 196.0f, 0.0f, 255.0f});
        tweenTest.get<PositionComponent>()->setX(30);
        tweenTest.get<PositionComponent>()->setY(120);

        ecsRef->attach<TweenComponent>(tweenTest.entity, TweenComponent {
            255.0f,
            0.0f,
            2000.0f,
            [tweenTest](const TweenValue& value){ tweenTest.get<Simple2DObject>()->setColors({0.0f, 196.0f, 0.0f, std::get<float>(value)}); },
            makeCallable<StandardEvent>("gamelog", "message", "Tween complete !")
        });


        /* Clipped progress bar exemple:
        auto spacer = ecsRef->createEntity();
        auto spacerPos = ecsRef->attach<PositionComponent>(spacer);
        spacerPos->setX(100);
        spacerPos->setWidth(150);
        auto spacerAnchor = ecsRef->attach<UiAnchor>(spacer);

        auto clipper = ecsRef->createEntity();
        auto clipperPos = ecsRef->attach<PositionComponent>(clipper);
        clipperPos->setX(350);
        clipperPos->setY(200);
        clipperPos->setWidth(100);
        clipperPos->setHeight(100);


        auto progressBar = makeProgressBar(ecsRef, 400, 100, "emptyBar", "fullBar", 0.65);
        progressBar.get<PositionComponent>()->setY(250);

        progressBar.get<UiAnchor>()->setLeftAnchor(spacerAnchor->right);

        ecsRef->attach<ClippedTo>(progressBar.entity, clipper->id);

        */


        /* Wrapped text and layout
        auto s0 = makeUiSimple2DShape(ecsRef, Shape2D::Square, 60, 60);
        auto s1 = makeUiSimple2DShape(ecsRef, Shape2D::Square, 60, 60);
        auto s2 = makeUiSimple2DShape(ecsRef, Shape2D::Square, 60, 60);
        auto s3 = makeUiSimple2DShape(ecsRef, Shape2D::Square, 60, 60);


        auto s4 = makeUiSimple2DShape(ecsRef, Shape2D::Square, 60, 60, {0.0f, 196.0f, 0.0f, 255.0f});
        s4.get<PositionComponent>()->setX(30);
        s4.get<PositionComponent>()->setY(80);

        auto s5 = makeUiSimple2DShape(ecsRef, Shape2D::Square, 60, 60, {196.0f, 0.0f, 0.0f, 255.0f});
        s5.get<PositionComponent>()->setX(270);
        s5.get<PositionComponent>()->setY(80);

        auto wrappedText = makeTTFText(ecsRef, 30, 145, 1, "res/font/Inter/static/Inter_28pt-Light.ttf", "You just woke up in a strange place... \nYou only see a runic altar nearby.", 0.4f);
        wrappedText.get<TTFText>()->wrap = true;
        wrappedText.get<PositionComponent>()->setWidth(255);

        auto s6 = makeUiSimple2DShape(ecsRef, Shape2D::Square, 60, 60);
        auto s7 = makeUiSimple2DShape(ecsRef, Shape2D::Square, 60, 60);
        auto s8 = makeUiSimple2DShape(ecsRef, Shape2D::Square, 60, 60);

        auto hLayout = makeVerticalLayout(ecsRef, 30, 150, 100, 300);
        // hLayout.get<PositionComponent>()->setVisibility(false);

        hLayout.get<VerticalLayout>()->spacing = 7;

        // hLayout.get<VerticalLayout>()->spacedInWidth = true;
        // hLayout.get<VerticalLayout>()->fitToWidth = true;
        hLayout.get<VerticalLayout>()->spacedInHeight = true;
        hLayout.get<VerticalLayout>()->fitToHeight = true;

        hLayout.get<VerticalLayout>()->addEntity(s0.entity);
        hLayout.get<VerticalLayout>()->addEntity(s1.entity);
        hLayout.get<VerticalLayout>()->addEntity(s2.entity);
        hLayout.get<VerticalLayout>()->addEntity(s3.entity);

        hLayout.get<VerticalLayout>()->addEntity(s6.entity);
        hLayout.get<VerticalLayout>()->addEntity(s7.entity);
        hLayout.get<VerticalLayout>()->addEntity(s8.entity);

        hLayout.get<PositionComponent>()->setWidth(380);

        s3.get<PositionComponent>()->setWidth(150);

        auto buttonPrefab = makeAnchoredPrefab(ecsRef);

        auto buttonTex = makeUiSimple2DShape(ecsRef, Shape2D::Square, 100, 60, {255.0f, 255.0f, 255.0f, 255.0f});
        buttonTex.get<UiAnchor>()->fillIn(buttonPrefab.get<UiAnchor>());
        buttonPrefab.get<UiAnchor>()->setWidthConstrain(PosConstrain{buttonTex.entity.id, AnchorType::Width});
        buttonPrefab.get<UiAnchor>()->setHeightConstrain(PosConstrain{buttonTex.entity.id, AnchorType::Height});

        auto buttonText = makeTTFText(ecsRef, 0, 0, 0, "res/font/Inter/static/Inter_28pt-Light.ttf", "Click Me", 0.4f, {0.0f, 0.0f, 0.0f, 255.f});
        buttonText.get<UiAnchor>()->centeredIn(buttonTex.get<UiAnchor>());
        // buttonText.get<UiAnchor>()->setTopAnchor(buttonTex.get<UiAnchor>()->top);
        // buttonText.get<UiAnchor>()->setHorizontalCenter(buttonTex.get<UiAnchor>()->horizontalCenter);
        // buttonText.get<UiAnchor>()->setLeftAnchor(buttonTex.get<UiAnchor>()->left);
        buttonText.get<UiAnchor>()->setZConstrain(PosConstrain{buttonTex.entity.id, AnchorType::Z, PosOpType::Add, 1});

        buttonPrefab.get<Prefab>()->addToPrefab(buttonTex.entity);
        buttonPrefab.get<Prefab>()->addToPrefab(buttonText.entity);

        hLayout.get<VerticalLayout>()->addEntity(buttonPrefab.entity);

        */

        // barComp = progressBar.get<ProgressBarComponent>();

        // barComp->percent = fill;

        // auto overProgressBar = makeUiTexture(ecsRef, 400, 100, "overBar");
        // overProgressBar.get<UiComponent>()->setY(250);
        // overProgressBar.get<UiComponent>()->setZ(1);

        // auto testText = makeSentence(ecsRef, 200, 400, {"Hello World"});

    }

    // CompRef<ProgressBarComponent> barComp;

    float fill = 0.0f;

    std::vector<int> test = {};
};

/* Portrait loading exemple:
struct PortraitLoader : public System<Listener<ChangePortraitEvent>, StoragePolicy, InitSys>
{
    MasterRenderer *renderer;
    CompRef<Texture2DComponent> tex;

    PortraitLoader(MasterRenderer *renderer) : renderer(renderer)
    {

    }

    void init() override
    {
        auto titleTTF = makeTTFText(ecsRef, 0, 300, 1, "res/font/Inter/static/Inter_28pt-Light.ttf", "ChangePortrait", 0.4);
        ecsRef->attach<MouseLeftClickComponent>(titleTTF.entity, makeCallable<ChangePortraitEvent>());

        auto portrait = makeUiTexture(ecsRef, 100, 100, "NoneIcon");

        portrait.get<PositionComponent>()->setX(250);
        portrait.get<PositionComponent>()->setY(300);

        tex = portrait.get<Texture2DComponent>();

        auto fee = makeUiTexture(ecsRef, 100, 100, "Hahahah");

        fee.get<PositionComponent>()->setX(250);
        fee.get<PositionComponent>()->setY(420);
    }

    virtual void onEvent(const ChangePortraitEvent&) override
    {
        char * lTheOpenFileName;
    	char const * lFilterPatterns[1] = { "*.png" };

        lTheOpenFileName = tinyfd_openFileDialog(
            "Open an image file",
            "", // Starting path
            1, // Number of patterns
            lFilterPatterns, // List of patterns
            "Image (.png)",
            1);

        if (lTheOpenFileName)
        {
            LOG_INFO("Change Portrait", lTheOpenFileName);
            changeImage(lTheOpenFileName);
        }
    }

    void changeImage(const std::string& filePath)
    {
        renderer->queueRegisterTexture("Portrait", filePath.c_str());

        tex->setTexture("Portrait");
    }
};
*/

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

    mainWindow->ecs.createSystem<ThemeSystem>();

    mainWindow->ecs.createSystem<FpsSystem>();

    mainWindow->ecs.createSystem<InventorySystem>();

    mainWindow->ecs.createSystem<MoveToSystem>();

    mainWindow->ecs.createSystem<TweenSystem>();

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

    auto pDatas = mainWindow->ecs.createSystem<PassiveDatabase>();

    pDatas->storePassive(BurnPassive{});

    mainWindow->ecs.createSystem<PlayerHandlingSystem>();

    mainWindow->ecs.createSystem<LocationSystem>();

    mainWindow->ecs.createSystem<SceneLoader>();

    // mainWindow->ecs.createSystem<PortraitLoader>(mainWindow->masterRenderer);

    auto worldFacts = mainWindow->ecs.createSystem<WorldFacts>();

    worldFacts->setDefaultFact("startTuto", true);
    worldFacts->setDefaultFact("altar_touched", false);
    worldFacts->setDefaultFact("mage_tier", 0);

    auto achievementSys = mainWindow->ecs.createSystem<AchievementSys>();

    Achievement slimeSlayed;

    slimeSlayed.name = "SlimeSlayed";
    slimeSlayed.prerequisiteFacts = { FactChecker{"Slime_defeated", 10, FactCheckEquality::GreaterEqual} };

    achievementSys->setDefaultAchievement(slimeSlayed);

    Achievement tutoStarted;

    tutoStarted.name = "TutoStarted";
    tutoStarted.prerequisiteFacts = { FactChecker{"startTuto", true, FactCheckEquality::Equal} };
    tutoStarted.rewardFacts = { StandardEvent{"gamelog", "message", "You just woke up in a strange place... \nYou only see a runic altar nearby."} };

    achievementSys->setDefaultAchievement(tutoStarted);

    Achievement knowledgeFirstCap;

    knowledgeFirstCap.name = "knowledgeFirstCap";
    knowledgeFirstCap.prerequisiteFacts = { FactChecker{"knowledge", 10, FactCheckEquality::GreaterEqual} };
    knowledgeFirstCap.rewardFacts = { StandardEvent{"gamelog", "message", "You seem to have hit a wall in your study.\nTaking some notes may help you in the future..."}, AddFact{"first_knowledge_cap", ElementType{true}} };

    achievementSys->setDefaultAchievement(knowledgeFirstCap);

    mainWindow->ecs.createSystem<GameLog>();

    mainWindow->ecs.createSystem<RessourceGeneratorSystem>();
    mainWindow->ecs.createSystem<ConverterSystem>();
    mainWindow->ecs.createSystem<NexusSystem>();

    mainWindow->ecs.succeed<AchievementSys, WorldFacts>();

    mainWindow->ecs.dumbTaskflow();

    mainWindow->interpreter->addSystemModule("game", GameModule{&mainWindow->ecs});

    mainWindow->interpreter->interpretFromFile("main.pg");

    LOG_INFO(DOM, "Size of UiComponent: " << sizeof(UiComponent) << " vs size of Position: " << sizeof(PositionComponent) << " and size of Anchor: " << sizeof(UiAnchor));

    // hLayout.get<HorizontalLayout>()->addEntity(s4.entity);
    // hLayout.get<HorizontalLayout>()->addEntity(s5.entity);

    mainWindow->ecs.getSystem<SceneElementSystem>()->loadSystemScene<NexusScene>();

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
