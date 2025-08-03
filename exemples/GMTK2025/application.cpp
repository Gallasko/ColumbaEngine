#include "application.h"

#include "logger.h"

#include "Systems/basicsystems.h"

#include "UI/ttftext.h"

#include "ribbonmesh.h"
#include "polygonmesh.h"

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

bool doSegmentsIntersect(const pg::Segment2D& s1, const pg::Segment2D& s2)
{
    auto orientation = [](const pg::Point2D& a, const pg::Point2D& b, const pg::Point2D& c) {
        float val = (b.y - a.y) * (c.x - b.x) - 
                    (b.x - a.x) * (c.y - b.y);
        if (std::abs(val) < 1e-6f)
            return 0;  // colinear

        return (val > 0) ? 1 : 2;             // clockwise or counterclockwise
    };

    auto onSegment = [](const pg::Point2D& p, const pg::Point2D& q, const pg::Point2D& r) {
        return q.x <= std::max(p.x, r.x) && q.x >= std::min(p.x, r.x) &&
               q.y <= std::max(p.y, r.y) && q.y >= std::min(p.y, r.y);
    };

    const pg::Point2D& p1 = s1.start;
    const pg::Point2D& q1 = s1.end;
    const pg::Point2D& p2 = s2.start;
    const pg::Point2D& q2 = s2.end;

    int o1 = orientation(p1, q1, p2);
    int o2 = orientation(p1, q1, q2);
    int o3 = orientation(p2, q2, p1);
    int o4 = orientation(p2, q2, q1);

    // General case
    if (o1 != o2 && o3 != o4)
        return true;

    // Special cases (colinear overlaps)
    if (o1 == 0 && onSegment(p1, p2, q1)) return true;
    if (o2 == 0 && onSegment(p1, q2, q1)) return true;
    if (o3 == 0 && onSegment(p2, p1, q2)) return true;
    if (o4 == 0 && onSegment(p2, q1, q2)) return true;

    return false;
}

std::optional<std::pair<size_t, size_t>> findLoopSegment(const std::vector<pg::Point2D>& path)
{
    if (path.size() < 4) return std::nullopt;

    // Only check the latest segment against previous ones
    pg::Segment2D newSegment(path[path.size() - 2], path[path.size() - 1]);

    for (size_t i = 0; i + 2 < path.size() - 1; ++i) { // avoid checking with adjacent segments
        pg::Segment2D oldSegment(path[i], path[i + 1]);

        if (doSegmentsIntersect(newSegment, oldSegment)) {
            return std::make_pair(i, path.size() - 2); // intersection between [i, i+1] and last segment
        }
    }

    return std::nullopt; // No loop detected
}

//— 1. Cross‐product helper for 2D vectors
static float cross(const Point2D& a, const Point2D& b) {
    return a.x * b.y - a.y * b.x;
}

//— 2. Compute intersection point of two segments (assumed to intersect)
std::optional<Point2D> segmentIntersectionPoint(const Segment2D& s1,
                                                const Segment2D& s2)
{
    Point2D p = s1.start;
    Point2D r = Point2D(s1.end.x - s1.start.x, s1.end.y - s1.start.y);
    Point2D q = s2.start;
    Point2D s = Point2D(s2.end.x - s2.start.x, s2.end.y - s2.start.y);

    float rxs = cross(r, s);
    if (std::abs(rxs) < 1e-6f)
        return std::nullopt; // parallel or colinear

    Point2D qp = Point2D(q.x - p.x, q.y - p.y);
    float t = cross(qp, s) / rxs;
    // float u = cross(qp, r) / rxs; // could check 0<=u<=1

    return Point2D(p.x + t * r.x, p.y + t * r.y);
}

//— 3. Extract the loop polygon between indices [i+1 … j] plus the intersection
std::vector<Point2D> extractLoopPolygon(const std::vector<Point2D>& path,
                                         size_t i, size_t j,
                                         const Point2D& ip)
{
    assert(i + 1 < path.size() && j + 1 < path.size());
    // The loop polygon goes: ip → path[i+1] → … → path[j] → ip
    std::vector<Point2D> poly;
    poly.reserve((j - i) + 2);
    poly.push_back(ip);
    for (size_t k = i+1; k <= j; ++k)
        poly.push_back(path[k]);
    poly.push_back(ip);
    return poly;
}

//— 4. Build the clipped path: prefix [0…i], ip, suffix [j+1…end]
std::vector<Point2D> buildClippedPath(const std::vector<Point2D>& path,
                                      size_t i, size_t j,
                                      const Point2D& ip)
{
    std::vector<Point2D> out;
    out.reserve(i + 1 + (path.size() - (j+1)) + 1);
    // prefix up to i
    for (size_t k = 0; k <= i; ++k)
        out.push_back(path[k]);
    // intersection point
    out.push_back(ip);
    // suffix from j+1 to end
    for (size_t k = j+1; k < path.size(); ++k)
        out.push_back(path[k]);
    return out;
}

//— 5. High-level: detect loop, compute both results
struct LoopResult {
    size_t segStart;               // i
    size_t segEnd;                 // j
    Point2D intersection;
    std::vector<Point2D> polygon;  // closed loop poly
    std::vector<Point2D> clipped;  // the new path
};

std::optional<LoopResult> detectAndClipLoop(const std::vector<Point2D>& path)
{
    auto loopPair = findLoopSegment(path);
    if (!loopPair) return std::nullopt;
    size_t i = loopPair->first;    // old segment index
    size_t j = loopPair->second;   // new segment index

    Segment2D s1(path[i],   path[i+1]);
    Segment2D s2(path[j],   path[j+1]);

    auto ipOpt = segmentIntersectionPoint(s1, s2);
    if (!ipOpt) return std::nullopt;

    Point2D ip = *ipOpt;
    LoopResult result;
    result.segStart    = i;
    result.segEnd      = j;
    result.intersection = ip;
    result.polygon     = extractLoopPolygon(path, i, j, ip);
    result.clipped     = buildClippedPath(path, i, j, ip);
    return result;
}

// Todo add an integer to Listener and QueuedListener (Listener<Event, N>) with the N being the number of cycle or time
// that we need to wait before the event becomes triggerable again.

struct PointAggregator : public System<Listener<OnMouseMove>, Listener<OnMouseClick>, Listener<OnMouseRelease>, Listener<TickEvent>>
{
    virtual void onEvent(const OnMouseMove& event)
    {
        if (pressed)
        {
            LOG_INFO("PointAggregator", "Mouse moved");

            currentMousePos = event.pos;
        }
    }

    virtual void onEvent(const OnMouseClick& event)
    {
        LOG_INFO("PointAggregator", "Mouse clicked");

        if (event.button == SDL_BUTTON_LEFT)
        {
            tick = 0.0f;

            pressed = true;

            currentMousePos = event.pos;

            mousePosList.emplace_back(event.pos);

            auto ent = ecsRef->createEntity();

            ent.attach<RibbonComponent>("cursor", mousePosList, 10.0f, true, 1.0f);

            currentEnt = ent;
        }
    }

    virtual void onEvent(const OnMouseRelease& event)
    {
        if (event.button == SDL_BUTTON_LEFT)
        {
            pressed = false;

            for (const auto& pos : mousePosList)
            {
                LOG_INFO("PointAggregator", "-> Stored Mouse position: " << pos.x << ", " << pos.y);
            }

            mousePosList.clear();

            ecsRef->removeEntity(currentEnt);
        }
    }

    virtual void onEvent(const TickEvent& event)
    {
        if (pressed)
        {
            tick += event.tick;

            if (tick >= 40 and currentMousePos != mousePosList.back())
            {
                tick = 0.0f;
                LOG_INFO("PointAggregator", "Mouse moved to: " << currentMousePos.x << ", " << currentMousePos.y);
                mousePosList.emplace_back(currentMousePos);

                if (auto lr = detectAndClipLoop(mousePosList))
                {
                    // The enclosed polygon:
                    auto polygon = lr->polygon;

                    auto entity = ecsRef->createEntity();
                    entity->attach<PolygonComponent>(polygon, 0); 

                    // The new, clipped path:
                    auto newPath = lr->clipped;       

                    // Now update your RibbonComponent:
                    // Todo do this if you want the clipped path to be used
                    // mousePosList = newPath;
                    // else if you just reset the whole path
                    mousePosList = std::vector<Point2D>{ currentMousePos };

                    // And (optionally) handle the polygon for game logic.
                }

                currentEnt.get<RibbonComponent>()->setPath(mousePosList);
            }
        }
    }

    bool pressed = false;

    float tick = 0.0f;

    Point2D currentMousePos;

    std::vector<Point2D> mousePosList;

    EntityRef currentEnt;
};

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

    auto ttfSys = mainWindow->ecs.createSystem<TTFTextSystem>(mainWindow->masterRenderer);

    // Need to fix this
    ttfSys->registerFont("res/font/Inter/static/Inter_28pt-Light.ttf", "light");
    ttfSys->registerFont("res/font/Inter/static/Inter_28pt-Bold.ttf", "bold");
    ttfSys->registerFont("res/font/Inter/static/Inter_28pt-Italic.ttf", "italic");

    // mainWindow->masterRenderer->processTextureRegister();

    mainWindow->ecs.createSystem<FpsSystem>();

    mainWindow->ecs.createSystem<TexturedRibbonComponentSystem>(mainWindow->masterRenderer);
    mainWindow->ecs.createSystem<PolygonComponentSystem>(mainWindow->masterRenderer);

    mainWindow->ecs.succeed<MasterRenderer, TexturedRibbonComponentSystem>();
    mainWindow->ecs.succeed<MasterRenderer, PolygonComponentSystem>();
    
    mainWindow->ecs.createSystem<PointAggregator>();

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
