#include "application.h"

#include "logger.h"

#include "UI/prefab.h"
#include "UI/textinput.h"
#include "UI/sizer.h"
#include "2D/simple2dobject.h"

using namespace pg;

namespace {
    static const char *const DOM = "App";
}

GameApp::GameApp(const std::string &appName) : appName(appName) {
    LOG_THIS_MEMBER(DOM);
}

GameApp::~GameApp() {
    LOG_THIS_MEMBER(DOM);
}

std::thread *initThread;
pg::Window *mainWindow = nullptr;
std::atomic<bool> initialized = {false};
bool init = false;
bool running = true;

void initWindow(const std::string &appName) {
#ifdef __EMSCRIPTEN__
    mainWindow = new pg::Window(appName, "/save/savedData.sz");
#else
    mainWindow = new pg::Window(appName);
#endif

    LOG_INFO(DOM, "Window init...");

    initialized = true;
}

EntityRef makeLinePrefab(EntitySystem *ecsRef, CompRef<UiAnchor> anchor, size_t lineNumber)
{
    auto prefabEnt = makeAnchoredPrefab(ecsRef);
    auto prefab = prefabEnt.get<Prefab>();
    auto prefabAnchor = prefabEnt.get<UiAnchor>();

    auto color = (lineNumber % 2) ? constant::Vector4D{167.f, 167.f, 167.f, 255.f} : constant::Vector4D{218.f, 218.f, 218.f, 255.f};

    auto square = makeUiSimple2DShape(ecsRef, Shape2D::Square, 25, 25, color);
    auto squareAnchor = square.get<UiAnchor>();

    prefabAnchor->setHeightConstrain(PosConstrain{square.entity.id, AnchorType::Height});
    prefabEnt.get<PositionComponent>()->setWidth(50);

    squareAnchor->setTopAnchor(prefabAnchor->top);
    squareAnchor->setLeftAnchor(prefabAnchor->left);

    auto s2 = makeUiSimple2DShape(ecsRef, Shape2D::Square, 25, 25, constant::Vector4D{0.f, 0.f, 0.f, 255.f});
    auto s2Anchor = s2.get<UiAnchor>();

    s2Anchor->setTopAnchor(prefabAnchor->top);
    s2Anchor->setLeftAnchor(prefabAnchor->left);
    s2Anchor->setLeftMargin(30);
    s2Anchor->setRightAnchor(anchor->right);

    prefabAnchor->setWidthConstrain(PosConstrain{s2.entity.id, AnchorType::Width});

    prefab->addToPrefab(square.entity);
    prefab->addToPrefab(s2.entity, "TextBg");

    return prefabEnt.entity;
}

struct TextHandlingSys : public System<Listener<CurrentTextInputTextChanged>, QueuedListener<OnSDLScanCode>, InitSys>
{
    virtual void onEvent(const CurrentTextInputTextChanged& event) override
    {
        if (event.id == textInputEnt.id)
        {
            printf("%s\n", event.text.c_str());
        }
    }

    virtual void onProcessEvent(const OnSDLScanCode& event) override
    {
        if (event.key == SDL_SCANCODE_RETURN)
        {
            auto anchor = textInputEnt.get<UiAnchor>();

            currentLine++;
            lineNumber++;

            auto linePrefab = makeLinePrefab(ecsRef, anchor, currentLine);

            auto listViewComp = listViewEnt.get<VerticalLayout>();

            // Todo need to add an insert in listView
            // listViewComp->addEntity(linePrefab);
            listViewComp->insertEntity(linePrefab, currentLine - 1);
        }
        else if (event.key == SDL_SCANCODE_BACKSPACE)
        {
            // if (textInputEnt.has<TextInputComponent>())
            // {
            //     auto textInputComp = textInputEnt.get<TextInputComponent>();
            //     textInputComp->removeLastCharacter();
            // }
        }
        else if (event.key == SDL_SCANCODE_UP)
        {
            if (currentLine > 1)
            {
                auto listViewComp = listViewEnt.get<VerticalLayout>();

                auto entBefore = listViewComp->entities[currentLine - 1];
                auto prefabBefore = entBefore.get<Prefab>();
                prefabBefore->getEntity("TextBg")->get<Simple2DObject>()->setColors(constant::Vector4D{0.f, 0.f, 0.f, 255.f});

                currentLine--;

                auto entAfter = listViewComp->entities[currentLine - 1];
                auto prefabAfter = entAfter.get<Prefab>();

                prefabAfter->getEntity("TextBg")->get<Simple2DObject>()->setColors(constant::Vector4D{255.f, 0.f, 0.f, 255.f});
            }
        }
        else if (event.key == SDL_SCANCODE_DOWN)
        {
            if (currentLine < lineNumber - 1)
            {
                auto listViewComp = listViewEnt.get<VerticalLayout>();

                auto entBefore = listViewComp->entities[currentLine - 1];
                auto prefabBefore = entBefore.get<Prefab>();
                prefabBefore->getEntity("TextBg")->get<Simple2DObject>()->setColors(constant::Vector4D{0.f, 0.f, 0.f, 255.f});

                currentLine++;

                auto entAfter = listViewComp->entities[currentLine - 1];
                auto prefabAfter = entAfter.get<Prefab>();
                prefabAfter->getEntity("TextBg")->get<Simple2DObject>()->setColors(constant::Vector4D{255.f, 0.f, 0.f, 255.f});
            }
        }
    }

    virtual void init() override
    {
        textInputEnt = ecsRef->createEntity();

        auto ui = ecsRef->attach<PositionComponent>(textInputEnt);

        auto anchor = ecsRef->attach<UiAnchor>(textInputEnt);

        auto windowAnchor = ecsRef->getEntity("__MainWindow")->get<UiAnchor>();

        anchor->fillIn(windowAnchor);

        auto focused = ecsRef->attach<FocusableComponent>(textInputEnt);

        ecsRef->attach<MouseLeftClickComponent>(textInputEnt, makeCallable<OnFocus>(OnFocus{textInputEnt.id}) );

        StandardEvent event {"TerminalNewLine"};

        auto textInputComp = ecsRef->attach<TextInputComponent>(textInputEnt, event, "");

        auto listView = makeVerticalLayout(ecsRef, 0, 0, 500, 500);

        // listView.attach<Simple2DObject>(Shape2D::Square, constant::Vector4D{0.f, 192.f, 0.f, 255.f});

        auto listViewAnchor = listView.get<UiAnchor>();

        listViewAnchor->fillIn(anchor);

        auto listViewComp = listView.get<VerticalLayout>();

        auto linePrefab = makeLinePrefab(ecsRef, anchor, lineNumber++);
        auto linePrefab2 = makeLinePrefab(ecsRef, anchor, lineNumber++);
        auto linePrefab3 = makeLinePrefab(ecsRef, anchor, lineNumber++);

        // listViewComp->addEntity(linePrefab);

        // auto testCube = makeUiSimple2DShape(ecsRef, Shape2D::Square, 50, 50, constant::Vector4D{192.f, 0.f, 0.f, 255.f});

        // listViewComp->addEntity(testCube.entity);

        listViewComp->addEntity(linePrefab);
        listViewComp->addEntity(linePrefab2);
        listViewComp->addEntity(linePrefab3);

        currentLine = 3;

        listViewEnt = listView.entity;
    }

    EntityRef textInputEnt;

    EntityRef listViewEnt;

    size_t lineNumber = 1;
    size_t currentLine = 1;
};

void initGame() {
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

    mainWindow->ecs.createSystem<TextHandlingSys>();

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

void mainloop(void *arg) {
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

        initGame();
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

int GameApp::exec() {
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
