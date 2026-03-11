#include "app.h"

#include "logger.h"

#include "Audio/audiosystem.h"

#include "Systems/basicsystems.h"

#include "tetromino.h"
#include "titlescreen.h"

#include "2D/simple2dobject.h"
#include "2D/texture.h"

#include "Scene/scenemanager.h"

#include "Systems/oneventcomponent.h"

#include "keyconfig.h"

using namespace pg;

namespace
{
    static const char* const DOM = "Tetris clone app";
}

TetrisApp::TetrisApp(const std::string& appName) : appName(appName)
{
    LOG_THIS_MEMBER(DOM);
}

TetrisApp::~TetrisApp()
{
    LOG_THIS_MEMBER(DOM);
}

std::thread *initThread;
pg::Window *mainWindow = nullptr;
std::atomic<bool> initialized = {false};
bool init = false;
bool running = true;

struct TestClickEvent {};

struct TestScene : public Scene
{
    virtual void init() override
    {
        makeUiTexture(this, 50, 50, "menu");

        makeSentence(this, 50, 50, {"Test scene"});
    }
};

struct TestSceneSystem : public System<Listener<ConfiguredKeyEvent<TetrisConfig>>, Listener<TestClickEvent>,  Listener<OnSDLScanCodeReleased>, StoragePolicy>
{
    virtual void onEvent(const ConfiguredKeyEvent<TetrisConfig>& event) override
    {
        if (event.value == TetrisConfig::MoveLeft)
        {
            LOG_INFO(DOM, "Moving Left");
        }
    }

    virtual void onEvent(const TestClickEvent&) override
    {
        LOG_INFO(DOM, "Click detected");
    }

    virtual void onEvent(const OnSDLScanCodeReleased& event) override
    {
        if (event.key == SDL_SCANCODE_W)
        {
            std::cout << "r" << std::endl;
        }

        if (event.key == SDL_SCANCODE_M)
        {
            ecsRef->sendEvent(LoadScene{"scene1.sc"});
        }

        if (event.key == SDL_SCANCODE_N)
        {
            ecsRef->sendEvent(LoadScene{"scene2.sc"});
        }

        if (event.key == SDL_SCANCODE_COMMA)
        {
            ecsRef->sendEvent(LoadScene{"subscene1.sc"});
        }

        if (event.key == SDL_SCANCODE_SEMICOLON)
        {
            ecsRef->getSystem<SceneElementSystem>()->loadSystemScene<GameCanvas>();
        }

        if (event.key == SDL_SCANCODE_J)
        {
            hscore++;

            std::cout << hscore << std::endl;
        }

        if (event.key == SDL_SCANCODE_K)
        {
            auto saved = ecsRef->getSavedData("highscore");

            if (saved.isNumber())
                hscore = saved.get<int>();
        }

        if (event.key == SDL_SCANCODE_L)
        {
            ecsRef->sendEvent(SaveElementEvent{"highscore", hscore});
        }
    }

    int hscore = 0;
};

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

    mainWindow->ecs.createSystem<ConfiguredKeySystem<TetrisConfig>>(scancodeMap);

    // mainWindow->ecs.createSystem<FpsSystem>();
    
    mainWindow->ecs.start();

    mainWindow->render();

    mainWindow->resize(792, 600);

    // mainWindow->ecs.createSystem<TestSceneSystem>();

    // mainWindow->ecs.createSystem<GameCanvas>();

    // mainWindow->ecs.getSystem<SceneElementSystem>()->loadSystemScene<TestScene>();
    // mainWindow->ecs.getSystem<SceneElementSystem>()->loadSystemScene<GameCanvas>();
    mainWindow->ecs.getSystem<SceneElementSystem>()->loadSystemScene<TitleScreen>();

    // makeSentence(&mainWindow->ecs, 200, 120, {"I am me"});

    // auto rem = makeSentence(&mainWindow->ecs, 0, 0, {"Hello World!"});

    printf("Canvas initialized ...\n");

    // mainWindow->ecs.start();

    // mainWindow->render();

    // mainWindow->resize(792, 600);

    printf("Engine initialized\n");
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

        mainWindow->init(792, 600, false, static_cast<SDL_Window*>(arg));

        printf("Window init done !\n");

        initGame();
    }

    SDL_Event event;

    while (SDL_PollEvent(&event))
    {
        mainWindow->processEvents(event);
    }

    mainWindow->render();

// // Todo don't do this every frame but only when something is written to emscripten filesystem
#ifdef __EMSCRIPTEN__
//         static auto start = std::chrono::steady_clock::now();

//         static auto end = std::chrono::steady_clock::now();

//         end = std::chrono::steady_clock::now();

//         if (std::chrono::duration_cast<std::chrono::seconds>(end - start).count() >= 1)
//         {
            EM_ASM(
                FS.syncfs(false, function (err) {
                    assert(!err);
                });
            );

//             start = end;
//         }
#endif

    if (mainWindow->requestQuit())
    {
        LOG_ERROR("Window", "RequestQuit");
        std::terminate();
    }
}

int TetrisApp::exec()
{
    // pg::Logger::registerSink<pg::TerminalSink>(true);
#ifdef __EMSCRIPTEN__
    // Todo only do this once !!!
    // Make this save folder configurable
    EM_ASM(
        // Make a directory other than '/'
        FS.mkdir('/save');
        // Then mount with IDBFS type
        FS.mount(IDBFS, {autoPersist: true}, '/save');           
        // Then sync

        FS.syncfs(true, function (err) {
            // Error
        });
    );

    printf("Start init thread...\n");
    initThread = new std::thread(initWindow, appName);
    printf("Detach init thread...\n");

    SDL_Window *pWindow = 
        SDL_CreateWindow("Hello Triangle Minimal", 
                         SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                         792, 600, 
                         SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN);

    // initThread->join();

    emscripten_set_main_loop_arg(mainloop, pWindow, 0, 1);
    // emscripten_set_main_loop(mainloop, 0, 1);

#else
    LOG_THIS_MEMBER(DOM);

    initWindow(appName);

    mainWindow->init(792, 600, false);

    LOG_INFO(DOM, "Window init done !");

    initGame();

    // auto testRect = makeSimple2DShape(&mainWindow->ecs, Shape2D::Square, 50, 50, {255.0f, 0.0f, 0.0f});
    // // makeSimple2DShape(&mainWindow->ecs, Shape2D::Square, 50, 50, {255.0f, 0.0f, 0.0f});
    
    // testRect.get<UiComponent>()->setX(10);
    // testRect.get<UiComponent>()->setY(10);
    // testRect.get<UiComponent>()->setZ(5);

    // mainWindow->ecs.attach<MouseLeftClickComponent>(testRect.entity, makeCallable<TestClickEvent>());

    // std::function<void(const OnSDLTextInput&)> f = [](const OnSDLTextInput& event) { LOG_INFO("Script Test", "Test pressed: " << event.text); };

    // mainWindow->ecs.attach<OnEventComponent>(testRect.entity, f);

    // mainWindow->ecs.attach<EntityName>(testRect.entity, "TestRect");

    // Serializer serializer;

    // serializer.setFile("test.sz");

    // serializer.serializeObject(std::to_string(testRect.entity.id), *testRect.entity.entity);

    // auto s1 = makeSimple2DShape(&mainWindow->ecs, Shape2D::Square, 50, 50, {255.0f, 0.0f, 0.0f});
    // s1.get<UiComponent>()->setX(300);
    // s1.get<UiComponent>()->setY(300);

    // auto s2 = makeSimple2DShape(&mainWindow->ecs, Shape2D::Square, 50, 50, {255.0f, 0.0f, 0.0f});
    // s2.get<UiComponent>()->setTopAnchor(s1.get<UiComponent>()->bottom);
    // s2.get<UiComponent>()->setLeftAnchor(s1.get<UiComponent>()->left);

    // auto s3 = makeSimple2DShape(&mainWindow->ecs, Shape2D::Square, 50, 50, {255.0f, 0.0f, 0.0f});
    // s3.get<UiComponent>()->setTopAnchor(s1.get<UiComponent>()->top);
    // s3.get<UiComponent>()->setLeftAnchor(s1.get<UiComponent>()->right);

    // auto s4 = makeSimple2DShape(&mainWindow->ecs, Shape2D::Square, 50, 50, {255.0f, 0.0f, 0.0f});
    // s4.get<UiComponent>()->setTopAnchor(s3.get<UiComponent>()->bottom);
    // s4.get<UiComponent>()->setLeftAnchor(s2.get<UiComponent>()->right);

    // serializer.serializeObject(std::to_string(s1.entity.id), *s1.entity.entity);
    // serializer.serializeObject(std::to_string(s2.entity.id), *s2.entity.entity);
    // serializer.serializeObject(std::to_string(s3.entity.id), *s3.entity.entity);
    // serializer.serializeObject(std::to_string(s4.entity.id), *s4.entity.entity);

    // makeSimple2DShape(&mainWindow->ecs, Shape2D::Square, 50, 50, {255.0f, 0.0f, 0.0f});

    // auto tex = makeUiTexture(&mainWindow->ecs, 296, 197, "menu");

    // serializer.serializeObject(std::to_string(tex.entity.id), *tex.entity.entity);

    // auto tex2 = makeUiTexture(&mainWindow->ecs, 296, 197, "Next");

    // tex2.get<UiComponent>()->setTopAnchor(tex.get<UiComponent>()->bottom);

    // serializer.serializeObject(std::to_string(tex2.entity.id), *tex2.entity.entity);

    // mainWindow->ecs.createSystem<FpsSystem>();

    // LOG_INFO(DOM, "Loading first scene !");
    // mainWindow->ecs.sendEvent(LoadScene{"scene1.sc"});

    // tex.get<UiComponent>()->setY(250);

    // mainWindow->ecs.start();

    // makeSentence(&mainWindow->ecs, 200, 120, {"I am me"});

    // auto rem = makeSentence(&mainWindow->ecs, 0, 0, {"Hello World!"});

    // makeSentence(&mainWindow->ecs, 0, 120, {"Hola ?!"});

    // auto test = makeSentence(&mainWindow->ecs, 200, 150, {"Hi yours truly"});

    // test.get<SentenceText>()->setText("I am me");

    // mainWindow->ecs.removeEntity(rem.entity);

    // makeSentence(&mainWindow->ecs, 200, 180, {"I am me"});

    // LOG_INFO(DOM, "Initializing engine done !");

    // LOG_INFO(DOM, "Starting SDL event loop, waiting for events...");

    // mainWindow->ecs.start();

    // test.get<SentenceText>()->setText("I am me me");
    // test.get<UiComponent>()->setX(-235);

    // makeSentence(&mainWindow->ecs, 0, 170, {"Hola 2 ?!"});

    // mainWindow->resize(800, 600);

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
