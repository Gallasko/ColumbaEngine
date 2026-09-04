#include "profileroverlay.h"

#ifndef PROFILE

namespace pg
{
    void createProfilerOverlay(EntitySystem&, Window&, const std::string&) {}
}

#else

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <map>
#include <unordered_map>
#include <vector>

#include "ECS/entitysystem.h"
#include "ECS/callable.h"
#include "window.h"

#include "profiler.h"
#include "profilerstats.h"

#include "2D/position.h"
#include "2D/simple2dobject.h"
#include "UI/ttftext.h"
#include "UI/utils.h"
#include "Input/inputcomponent.h"
#include "Systems/coresystems.h"

namespace pg
{
    // ------------------------------------------------------------------
    // Events fired by the overlay's clickable controls
    // ------------------------------------------------------------------

    struct ProfilerOverlaySortEvent
    {
        ProfilerOverlaySortEvent(int c = 0) : column(c) {}
        int column;
    };

    struct ProfilerOverlayFpsEvent
    {
        ProfilerOverlayFpsEvent(int f = 0) : fps(f) {}
        int fps;
    };

    struct ProfilerOverlayExportEvent {};

    namespace
    {
        // Panel geometry
        constexpr float PANEL_W = 400.0f;
        constexpr float PAD = 10.0f;
        constexpr float ROW_H = 13.0f;
        constexpr float TITLE_SCALE = 0.32f;
        constexpr float ROW_SCALE = 0.25f;

        constexpr float BASE_Z = 90.0f;
        constexpr float TEXT_Z = 92.0f;

        // Pool sizes
        constexpr size_t STAT_ROWS = 14;
        constexpr size_t EVENT_ROWS = 10;
        constexpr size_t COMP_ROWS = 8;
        constexpr size_t INIT_ROWS = 6;

        // Timeline geometry / pools
        constexpr float TL_H = 150.0f;
        constexpr size_t TL_RECTS = 256;
        constexpr size_t TL_LABELS = 48;
        constexpr float TL_LABEL_MIN_W = 60.0f;
        constexpr float TL_RECT_MIN_W = 2.0f;
        constexpr int TL_MAX_DEPTH = 6;
        constexpr float TL_LANE_HEADER = 14.0f;
        constexpr float TL_DEPTH_H = 11.0f;

        const constant::Vector4D PANEL_BG   {18.0f, 18.0f, 26.0f, 230.0f};
        const constant::Vector4D TL_BG      {12.0f, 12.0f, 18.0f, 230.0f};
        const constant::Vector4D TEXT_MAIN  {230.0f, 230.0f, 235.0f, 255.0f};
        const constant::Vector4D TEXT_DIM   {150.0f, 150.0f, 160.0f, 255.0f};
        const constant::Vector4D TEXT_TITLE {255.0f, 200.0f, 90.0f, 255.0f};
        const constant::Vector4D TEXT_SEL   {120.0f, 220.0f, 140.0f, 255.0f};

        constant::Vector4D categoryColor(const std::string& category, const std::string& name)
        {
            constant::Vector4D base{130.0f, 130.0f, 140.0f, 255.0f};

            if (category == "System")
                base = {90.0f, 140.0f, 220.0f, 255.0f};
            else if (category == "Script")
                base = {90.0f, 190.0f, 120.0f, 255.0f};
            else if (category == "Event" or category == "Command")
                base = {220.0f, 160.0f, 80.0f, 255.0f};
            else if (category == "Render" or category == "Input" or category == "Swap" or category == "GL Error Checking")
                base = {170.0f, 110.0f, 210.0f, 255.0f};
            else if (category == "Init" or category == "Shutdown")
                base = {150.0f, 150.0f, 160.0f, 255.0f};

            // Vary brightness by name so neighbouring scopes are distinguishable
            const auto h = std::hash<std::string>{}(name);
            const float f = 0.75f + 0.4f * static_cast<float>(h % 97) / 97.0f;

            return {std::min(base.x * f, 255.0f), std::min(base.y * f, 255.0f), std::min(base.z * f, 255.0f), 255.0f};
        }

        std::string baseName(const std::string& path)
        {
            const auto pos = path.find_last_of("/\\");
            return pos == std::string::npos ? path : path.substr(pos + 1);
        }
    }

    // ------------------------------------------------------------------
    // The overlay system
    // ------------------------------------------------------------------

    class ProfilerOverlaySystem : public System<InitSys,
        QueuedListener<TickEvent>,
        QueuedListener<OnSDLScanCode>,
        QueuedListener<OnMouseClick>,
        Listener<ResizeEvent>,
        Listener<ProfilerOverlaySortEvent>,
        Listener<ProfilerOverlayFpsEvent>,
        Listener<ProfilerOverlayExportEvent>>
    {
    public:
        ProfilerOverlaySystem(Window* window, const std::string& fontPath)
            : window(window), fontPath(fontPath)
        {
            windowWidth = static_cast<float>(window->getWidth());
            windowHeight = static_cast<float>(window->getHeight());
        }

        virtual std::string getSystemName() const override { return "ProfilerOverlay"; }

        virtual void init() override;

        virtual void onEvent(const ResizeEvent& event) override
        {
            windowWidth = event.width;
            windowHeight = event.height;
        }

        virtual void onProcessEvent(const TickEvent& event) override;
        virtual void onProcessEvent(const OnSDLScanCode& event) override;
        virtual void onProcessEvent(const OnMouseClick& event) override;

        virtual void onEvent(const ProfilerOverlaySortEvent& event) override
        {
            sortColumn = event.column;
            statsAccumMs = 1e9f; // force refresh on next tick
        }

        virtual void onEvent(const ProfilerOverlayFpsEvent& event) override
        {
            window->renderFrameLimiter.setTargetFPS(event.fps);
            ecsRef->setEcsTargetFPS(event.fps);
            refreshControls();
        }

        virtual void onEvent(const ProfilerOverlayExportEvent&) override { exportCsv(); }

    private:
        // -- build helpers --------------------------------------------------
        _unique_id makePanelText(float xMargin, float yMargin, float scale, const constant::Vector4D& color, const std::string& text);
        void buildPanel();
        void buildTimeline();

        // -- refresh --------------------------------------------------------
        void setVisible(bool v);
        void refreshAll();
        void refreshControls();
        void refreshStatsTable(const std::vector<ProfileEvent>& snapshot);
        void refreshEventPanel();
        void refreshCountersPanel();
        void buildInitReport();
        void refreshTimeline();
        void exportCsv();

        void setRowText(_unique_id id, const std::string& text, const constant::Vector4D& color);
        void hideRow(_unique_id id);

        // -- members --------------------------------------------------------
        Window* window;
        std::string fontPath;

        float windowWidth = 0.0f;
        float windowHeight = 0.0f;

        bool visible = false;
        bool paused = false;
        bool initReportBuilt = false;

        // 0 = name, 1 = last, 2 = avg, 3 = max, 4 = count
        int sortColumn = 2;

        float statsAccumMs = 0.0f;
        float timelineAccumMs = 0.0f;

        // Timeline view state (ms on the profiler session clock)
        double viewSpanMs = 120.0;
        double frozenViewEndMs = 0.0;

        // -- entity pools ---------------------------------------------------
        _unique_id backdropId = 0;
        _unique_id titleId = 0;
        _unique_id fpsLabelId = 0;
        _unique_id btnUncapId = 0, btn30Id = 0, btn60Id = 0, btnCsvId = 0;
        _unique_id headerIds[5] = {0, 0, 0, 0, 0};
        _unique_id statRowIds[STAT_ROWS] = {};
        _unique_id eventsTitleId = 0;
        _unique_id eventRowIds[EVENT_ROWS] = {};
        _unique_id countersTitleId = 0;
        _unique_id entityCountId = 0;
        _unique_id compRowIds[COMP_ROWS] = {};
        _unique_id initTitleId = 0;
        _unique_id initRowIds[INIT_ROWS] = {};

        _unique_id tlBackId = 0;
        _unique_id tlInfoId = 0;
        _unique_id tlRectIds[TL_RECTS] = {};
        _unique_id tlLabelIds[TL_LABELS] = {};

        std::vector<_unique_id> allPanelEntities;
        std::vector<_unique_id> allTimelineEntities;
    };

    // ------------------------------------------------------------------
    // Construction
    // ------------------------------------------------------------------

    _unique_id ProfilerOverlaySystem::makePanelText(float xMargin, float yMargin, float scale, const constant::Vector4D& color, const std::string& text)
    {
        auto t = makeTTFText(ecsRef, 0.0f, 0.0f, TEXT_Z, fontPath, text, scale, color);

        auto a = t.get<UiAnchor>();
        a->setTopAnchor(PosAnchor{backdropId, AnchorType::Top});
        a->setLeftAnchor(PosAnchor{backdropId, AnchorType::Left});
        a->setTopMargin(yMargin);
        a->setLeftMargin(xMargin);

        allPanelEntities.push_back(t.entity->id);

        return t.entity->id;
    }

    void ProfilerOverlaySystem::buildPanel()
    {
        auto windowEnt = ecsRef->getEntity("__MainWindow");

        auto backdrop = makeUiSimple2DShape(ecsRef, Shape2D::Square, PANEL_W, 100.0f, PANEL_BG);
        backdropId = backdrop.entity->id;
        backdrop.get<PositionComponent>()->setZ(BASE_Z);

        {
            auto a = backdrop.get<UiAnchor>();
            a->setTopAnchor(PosAnchor{windowEnt->id, AnchorType::Top});
            a->setRightAnchor(PosAnchor{windowEnt->id, AnchorType::Right});
            a->setBottomAnchor(PosAnchor{windowEnt->id, AnchorType::Bottom});
        }

        allPanelEntities.push_back(backdropId);

        float y = 8.0f;

        titleId = makePanelText(PAD, y, TITLE_SCALE, TEXT_TITLE, "PROFILER   F10 hide  F11 pause  F12 csv");
        y += 22.0f;

        fpsLabelId = makePanelText(PAD, y, ROW_SCALE, TEXT_DIM, "FPS cap:");
        btnUncapId = makePanelText(PAD + 60.0f, y, ROW_SCALE, TEXT_SEL, "Uncap");
        btn30Id = makePanelText(PAD + 110.0f, y, ROW_SCALE, TEXT_DIM, "30");
        btn60Id = makePanelText(PAD + 140.0f, y, ROW_SCALE, TEXT_DIM, "60");
        btnCsvId = makePanelText(PAD + 200.0f, y, ROW_SCALE, TEXT_MAIN, "[Export CSV]");

        auto attachClick = [this](_unique_id id, CallablePtr callable) {
            auto ent = ecsRef->getEntity(id);
            ecsRef->attach<MouseLeftClickComponent>(ent, callable, MouseStateTrigger::OnPress);
        };

        attachClick(btnUncapId, makeCallable<ProfilerOverlayFpsEvent>(0));
        attachClick(btn30Id, makeCallable<ProfilerOverlayFpsEvent>(30));
        attachClick(btn60Id, makeCallable<ProfilerOverlayFpsEvent>(60));
        attachClick(btnCsvId, makeCallable<ProfilerOverlayExportEvent>());

        y += 20.0f;

        // Stats table header (clickable cells select the sort column)
        static constexpr const char* headers[5] = {"System", "last", "avg", "max", "n"};
        static constexpr float headerX[5] = {PAD, PAD + 200.0f, PAD + 250.0f, PAD + 300.0f, PAD + 350.0f};

        for (int i = 0; i < 5; i++)
        {
            headerIds[i] = makePanelText(headerX[i], y, ROW_SCALE, i == sortColumn ? TEXT_SEL : TEXT_DIM, headers[i]);
            attachClick(headerIds[i], makeCallable<ProfilerOverlaySortEvent>(i));
        }

        y += 16.0f;

        for (size_t i = 0; i < STAT_ROWS; i++)
        {
            statRowIds[i] = makePanelText(PAD, y, ROW_SCALE, TEXT_MAIN, " ");
            y += ROW_H;
        }

        y += 6.0f;
        eventsTitleId = makePanelText(PAD, y, TITLE_SCALE, TEXT_TITLE, "Events / pass");
        y += 18.0f;

        for (size_t i = 0; i < EVENT_ROWS; i++)
        {
            eventRowIds[i] = makePanelText(PAD, y, ROW_SCALE, TEXT_MAIN, " ");
            y += ROW_H;
        }

        y += 6.0f;
        countersTitleId = makePanelText(PAD, y, TITLE_SCALE, TEXT_TITLE, "Entities & components");
        y += 18.0f;

        entityCountId = makePanelText(PAD, y, ROW_SCALE, TEXT_MAIN, " ");
        y += ROW_H;

        for (size_t i = 0; i < COMP_ROWS; i++)
        {
            compRowIds[i] = makePanelText(PAD, y, ROW_SCALE, TEXT_MAIN, " ");
            y += ROW_H;
        }

        y += 6.0f;
        initTitleId = makePanelText(PAD, y, TITLE_SCALE, TEXT_TITLE, "Init phases (ms)");
        y += 18.0f;

        for (size_t i = 0; i < INIT_ROWS; i++)
        {
            initRowIds[i] = makePanelText(PAD, y, ROW_SCALE, TEXT_MAIN, " ");
            y += ROW_H;
        }
    }

    void ProfilerOverlaySystem::buildTimeline()
    {
        auto windowEnt = ecsRef->getEntity("__MainWindow");

        auto back = makeUiSimple2DShape(ecsRef, Shape2D::Square, 100.0f, TL_H, TL_BG);
        tlBackId = back.entity->id;
        back.get<PositionComponent>()->setZ(BASE_Z);

        {
            auto a = back.get<UiAnchor>();
            a->setLeftAnchor(PosAnchor{windowEnt->id, AnchorType::Left});
            a->setBottomAnchor(PosAnchor{windowEnt->id, AnchorType::Bottom});
        }

        allTimelineEntities.push_back(tlBackId);

        auto info = makeTTFText(ecsRef, 0.0f, 0.0f, TEXT_Z, fontPath, " ", ROW_SCALE, TEXT_DIM);
        tlInfoId = info.entity->id;
        {
            auto a = info.get<UiAnchor>();
            a->setLeftAnchor(PosAnchor{tlBackId, AnchorType::Left});
            a->setTopAnchor(PosAnchor{tlBackId, AnchorType::Top});
            a->setLeftMargin(6.0f);
            a->setTopMargin(2.0f);
        }
        allTimelineEntities.push_back(tlInfoId);

        // Scope rects and labels: absolute positioning, updated every refresh
        for (size_t i = 0; i < TL_RECTS; i++)
        {
            auto r = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f, TEXT_MAIN);
            r.get<PositionComponent>()->setZ(BASE_Z + 1.0f);
            tlRectIds[i] = r.entity->id;
            allTimelineEntities.push_back(tlRectIds[i]);
        }

        for (size_t i = 0; i < TL_LABELS; i++)
        {
            auto t = makeTTFText(ecsRef, 0.0f, 0.0f, TEXT_Z, fontPath, " ", 0.22f, TEXT_MAIN);
            tlLabelIds[i] = t.entity->id;
            allTimelineEntities.push_back(tlLabelIds[i]);
        }
    }

    void ProfilerOverlaySystem::init()
    {
        buildPanel();
        buildTimeline();

        setVisible(false);

        // Frame boundary events are needed for the timeline lane markers
        Profiler::instance().setRecordFrameEvents(true);
    }

    // ------------------------------------------------------------------
    // Visibility / input
    // ------------------------------------------------------------------

    void ProfilerOverlaySystem::setVisible(bool v)
    {
        visible = v;

        for (auto id : allPanelEntities)
            setEntityVisibility(ecsRef, id, v);

        for (auto id : allTimelineEntities)
            setEntityVisibility(ecsRef, id, v);

        if (v)
        {
            statsAccumMs = 1e9f;
            timelineAccumMs = 1e9f;
        }
    }

    void ProfilerOverlaySystem::onProcessEvent(const TickEvent& event)
    {
        if (not visible)
            return;

        statsAccumMs += event.tick;
        timelineAccumMs += event.tick;

        const bool refreshStats = statsAccumMs >= 250.0f;
        const bool refreshTl = timelineAccumMs >= 500.0f and not paused;

        if (not refreshStats and not refreshTl)
            return;

        PROFILE_SCOPE("OverlayRefresh", "Profiler");

        if (refreshStats)
        {
            statsAccumMs = 0.0f;
            refreshAll();
        }

        if (refreshTl)
        {
            timelineAccumMs = 0.0f;
            refreshTimeline();
        }
    }

    void ProfilerOverlaySystem::onProcessEvent(const OnSDLScanCode& event)
    {
        switch (event.key)
        {
            case SDL_SCANCODE_F10:
                setVisible(not visible);
                break;

            case SDL_SCANCODE_F11:
                if (visible)
                {
                    paused = not paused;
                    if (paused)
                        frozenViewEndMs = Profiler::instance().nowMs();
                    refreshControls();
                    refreshTimeline();
                }
                break;

            case SDL_SCANCODE_F12:
                if (visible)
                    exportCsv();
                break;

            case SDL_SCANCODE_LEFT:
            case SDL_SCANCODE_RIGHT:
                if (visible)
                {
                    if (not paused)
                    {
                        paused = true;
                        frozenViewEndMs = Profiler::instance().nowMs();
                    }

                    frozenViewEndMs += (event.key == SDL_SCANCODE_RIGHT ? 1.0 : -1.0) * viewSpanMs * 0.25;
                    refreshControls();
                    refreshTimeline();
                }
                break;

            case SDL_SCANCODE_EQUALS:
            case SDL_SCANCODE_KP_PLUS:
                if (visible)
                {
                    viewSpanMs = std::max(25.0, viewSpanMs / 1.25);
                    refreshTimeline();
                }
                break;

            case SDL_SCANCODE_MINUS:
            case SDL_SCANCODE_KP_MINUS:
                if (visible)
                {
                    viewSpanMs = std::min(1000.0, viewSpanMs * 1.25);
                    refreshTimeline();
                }
                break;

            default:
                break;
        }
    }

    void ProfilerOverlaySystem::onProcessEvent(const OnMouseClick& event)
    {
        if (not visible)
            return;

        // Click inside the timeline strip: pause and center the view there
        const float tlTop = windowHeight - TL_H;
        const float tlWidth = std::max(50.0f, windowWidth - PANEL_W);

        if (event.pos.y < tlTop or event.pos.x > tlWidth)
            return;

        const double viewEnd = paused ? frozenViewEndMs : Profiler::instance().nowMs();
        const double viewStart = viewEnd - viewSpanMs;
        const double t = viewStart + (event.pos.x / tlWidth) * viewSpanMs;

        paused = true;
        frozenViewEndMs = t + viewSpanMs / 2.0;

        refreshControls();
        refreshTimeline();
    }

    // ------------------------------------------------------------------
    // Panel refresh
    // ------------------------------------------------------------------

    void ProfilerOverlaySystem::setRowText(_unique_id id, const std::string& text, const constant::Vector4D& color)
    {
        auto ent = ecsRef->getEntity(id);

        if (not ent)
            return;

        ent->get<PositionComponent>()->setVisibility(true);

        auto ttf = ent->get<TTFText>();
        ttf->setColors(color);
        ttf->setText(text);
    }

    void ProfilerOverlaySystem::hideRow(_unique_id id)
    {
        setEntityVisibility(ecsRef, id, false);
    }

    void ProfilerOverlaySystem::refreshAll()
    {
        // One snapshot feeds the stats table; keep the window at 1s
        const double now = Profiler::instance().nowMs();
        auto snapshot = Profiler::instance().snapshotSince(now - 1000.0);

        refreshStatsTable(snapshot);
        refreshEventPanel();
        refreshCountersPanel();

        if (not initReportBuilt)
            buildInitReport();

        refreshControls();
    }

    void ProfilerOverlaySystem::refreshControls()
    {
        const int fps = window->renderFrameLimiter.getTargetFPS();

        auto setColor = [this](_unique_id id, bool selected) {
            auto ent = ecsRef->getEntity(id);
            if (ent)
                ent->get<TTFText>()->setColors(selected ? TEXT_SEL : TEXT_DIM);
        };

        setColor(btnUncapId, fps == 0);
        setColor(btn30Id, fps == 30);
        setColor(btn60Id, fps == 60);

        for (int i = 0; i < 5; i++)
            setColor(headerIds[i], i == sortColumn);

        auto title = ecsRef->getEntity(titleId);
        if (title)
            title->get<TTFText>()->setText(paused
                ? "PROFILER (paused)   F11 resume  arrows scrub  +/- zoom"
                : "PROFILER   F10 hide  F11 pause  F12 csv");
    }

    void ProfilerOverlaySystem::refreshStatsTable(const std::vector<ProfileEvent>& snapshot)
    {
        struct Row
        {
            double last = 0.0, total = 0.0, max = 0.0, lastStart = -1.0;
            size_t count = 0;
        };

        std::map<std::string, Row> systems;
        std::map<std::string, Row> scripts;

        // Script -> owning system, resolved by interval containment (same thread)
        std::map<std::string, std::string> scriptParent;

        for (const auto& e : snapshot)
        {
            std::map<std::string, Row>* target = nullptr;

            if (e.category == "System" and e.name != "ProfilerOverlay")
                target = &systems;
            else if (e.category == "Script")
                target = &scripts;
            else
                continue;

            auto& row = (*target)[e.name];
            row.total += e.durationMs;
            row.count++;
            row.max = std::max(row.max, e.durationMs);

            if (e.startMs > row.lastStart)
            {
                row.lastStart = e.startMs;
                row.last = e.durationMs;
            }
        }

        for (const auto& e : snapshot)
        {
            if (e.category != "Script" or scriptParent.count(e.name))
                continue;

            for (const auto& s : snapshot)
            {
                if (s.category == "System" and s.threadId == e.threadId
                    and s.startMs <= e.startMs
                    and s.startMs + s.durationMs >= e.startMs + e.durationMs)
                {
                    scriptParent[e.name] = s.name;
                    break;
                }
            }
        }

        auto sortKey = [this](const Row& r) -> double {
            switch (sortColumn)
            {
                case 1: return r.last;
                case 3: return r.max;
                case 4: return static_cast<double>(r.count);
                case 2:
                default: return r.count ? r.total / r.count : 0.0;
            }
        };

        std::vector<std::pair<std::string, Row>> ordered(systems.begin(), systems.end());

        if (sortColumn == 0)
            std::sort(ordered.begin(), ordered.end(),
                [](const auto& a, const auto& b) { return a.first < b.first; });
        else
            std::sort(ordered.begin(), ordered.end(),
                [&sortKey](const auto& a, const auto& b) { return sortKey(a.second) > sortKey(b.second); });

        // Interleave script rows under their parent system
        struct DisplayRow { std::string label; Row row; bool isScript; };
        std::vector<DisplayRow> display;

        for (const auto& [name, row] : ordered)
        {
            display.push_back({name, row, false});

            for (const auto& [scriptName, parent] : scriptParent)
            {
                if (parent == name and scripts.count(scriptName))
                    display.push_back({"  > " + baseName(scriptName), scripts.at(scriptName), true});
            }
        }

        // Orphan scripts (no containing system interval found)
        for (const auto& [scriptName, row] : scripts)
        {
            if (not scriptParent.count(scriptName))
                display.push_back({"> " + baseName(scriptName), row, true});
        }

        char buf[128];

        for (size_t i = 0; i < STAT_ROWS; i++)
        {
            if (i < display.size())
            {
                const auto& d = display[i];
                const double avg = d.row.count ? d.row.total / d.row.count : 0.0;

                snprintf(buf, sizeof(buf), "%-26.26s %7.3f %7.3f %7.3f %5zu",
                         d.label.c_str(), d.row.last, avg, d.row.max, d.row.count);

                setRowText(statRowIds[i], buf, d.isScript
                    ? constant::Vector4D{140.0f, 220.0f, 160.0f, 255.0f}
                    : TEXT_MAIN);
            }
            else
            {
                hideRow(statRowIds[i]);
            }
        }
    }

    void ProfilerOverlaySystem::refreshEventPanel()
    {
        // Average over the last 64 passes, plus the latest pass's count
        auto passes = ProfilerStats::instance().lastPasses(64);

        std::map<std::string, std::pair<uint64_t, uint32_t>> agg; // name -> {sum, last}

        for (const auto& pass : passes)
        {
            for (const auto& [name, count] : pass.eventCounts)
            {
                agg[name].first += count;
            }
        }

        if (not passes.empty())
        {
            for (const auto& [name, count] : passes.back().eventCounts)
                agg[name].second = count;
        }

        std::vector<std::pair<std::string, std::pair<uint64_t, uint32_t>>> ordered(agg.begin(), agg.end());
        std::sort(ordered.begin(), ordered.end(),
            [](const auto& a, const auto& b) { return a.second.first > b.second.first; });

        char buf[128];

        for (size_t i = 0; i < EVENT_ROWS; i++)
        {
            if (i < ordered.size())
            {
                const double avg = passes.empty() ? 0.0
                    : static_cast<double>(ordered[i].second.first) / passes.size();

                snprintf(buf, sizeof(buf), "%-30.30s last %4u  avg %6.2f",
                         ordered[i].first.c_str(), ordered[i].second.second, avg);

                setRowText(eventRowIds[i], buf, TEXT_MAIN);
            }
            else
            {
                hideRow(eventRowIds[i]);
            }
        }
    }

    void ProfilerOverlaySystem::refreshCountersPanel()
    {
        auto passes = ProfilerStats::instance().lastPasses(64);

        char buf[128];

        if (passes.empty())
        {
            hideRow(entityCountId);
            for (size_t i = 0; i < COMP_ROWS; i++)
                hideRow(compRowIds[i]);
            return;
        }

        snprintf(buf, sizeof(buf), "Entities: %zu    ECS pass: %llu",
                 passes.back().entityCount,
                 static_cast<unsigned long long>(passes.back().pass));
        setRowText(entityCountId, buf, TEXT_MAIN);

        // Component counts are sampled every ~32 passes: use the freshest set
        const std::vector<std::pair<std::string, size_t>>* counts = nullptr;

        for (auto it = passes.rbegin(); it != passes.rend(); ++it)
        {
            if (not it->componentCounts.empty())
            {
                counts = &it->componentCounts;
                break;
            }
        }

        std::vector<std::pair<std::string, size_t>> ordered;
        if (counts)
        {
            ordered = *counts;
            std::sort(ordered.begin(), ordered.end(),
                [](const auto& a, const auto& b) { return a.second > b.second; });
        }

        for (size_t i = 0; i < COMP_ROWS; i++)
        {
            if (i < ordered.size())
            {
                snprintf(buf, sizeof(buf), "%-34.34s %6zu", ordered[i].first.c_str(), ordered[i].second);
                setRowText(compRowIds[i], buf, TEXT_MAIN);
            }
            else
            {
                hideRow(compRowIds[i]);
            }
        }
    }

    void ProfilerOverlaySystem::buildInitReport()
    {
        // Init events are at the very front of the ring buffer; grab them once
        // before the buffer's drop-oldest policy can discard them.
        auto all = Profiler::instance().snapshotSince(0.0);

        std::vector<ProfileEvent> init;

        for (const auto& e : all)
        {
            if (e.category == "Init")
                init.push_back(e);
        }

        if (init.empty())
            return;

        std::sort(init.begin(), init.end(),
            [](const auto& a, const auto& b) { return a.durationMs > b.durationMs; });

        char buf[128];

        for (size_t i = 0; i < INIT_ROWS; i++)
        {
            if (i < init.size())
            {
                snprintf(buf, sizeof(buf), "%-32.32s %8.2f", init[i].name.c_str(), init[i].durationMs);
                setRowText(initRowIds[i], buf, TEXT_MAIN);
            }
            else
            {
                hideRow(initRowIds[i]);
            }
        }

        initReportBuilt = true;
    }

    // ------------------------------------------------------------------
    // Timeline
    // ------------------------------------------------------------------

    void ProfilerOverlaySystem::refreshTimeline()
    {
        if (not visible)
            return;

        const float tlWidth = std::max(50.0f, windowWidth - PANEL_W);
        const float tlTop = windowHeight - TL_H;

        {
            auto back = ecsRef->getEntity(tlBackId);
            if (back)
                back->get<PositionComponent>()->setWidth(tlWidth);
        }

        const double viewEnd = paused ? frozenViewEndMs : Profiler::instance().nowMs();
        const double viewStart = viewEnd - viewSpanMs;

        auto snapshot = Profiler::instance().snapshotSince(viewStart - 50.0);

        // ---- lane assignment: render thread (has "Frame"/"Render" events) first
        std::map<uint32_t, int> laneOf;
        uint32_t renderThread = 0;

        for (const auto& e : snapshot)
        {
            if (e.category == "Frame" or e.category == "Render")
            {
                renderThread = e.threadId;
                break;
            }
        }

        if (renderThread != 0)
            laneOf[renderThread] = 0;

        for (const auto& e : snapshot)
        {
            if (e.category == "Profiler")
                continue;

            if (not laneOf.count(e.threadId))
                laneOf[e.threadId] = static_cast<int>(laneOf.size());
        }

        // ---- collect drawable intervals inside the view window
        struct Visible { const ProfileEvent* e; int lane; int depth; };
        std::vector<Visible> drawables;

        {
            // Per-lane depth via a stack of scope end times
            std::map<uint32_t, std::vector<double>> stacks;

            // snapshot is in completion order; sort by start for nesting
            std::vector<const ProfileEvent*> sorted;
            sorted.reserve(snapshot.size());

            for (const auto& e : snapshot)
            {
                if (e.category == "Profiler" or e.category == "Marker" or e.category == "Frame")
                    continue;

                if (e.startMs + e.durationMs < viewStart or e.startMs > viewEnd)
                    continue;

                sorted.push_back(&e);
            }

            std::sort(sorted.begin(), sorted.end(),
                [](const ProfileEvent* a, const ProfileEvent* b) {
                    if (a->startMs != b->startMs)
                        return a->startMs < b->startMs;
                    return a->durationMs > b->durationMs;
                });

            for (const auto* e : sorted)
            {
                auto& stack = stacks[e->threadId];

                while (not stack.empty() and stack.back() <= e->startMs)
                    stack.pop_back();

                const int depth = static_cast<int>(stack.size());
                stack.push_back(e->startMs + e->durationMs);

                if (depth >= TL_MAX_DEPTH)
                    continue;

                drawables.push_back({e, laneOf[e->threadId], depth});
            }
        }

        // ---- draw
        const double pxPerMs = tlWidth / viewSpanMs;

        size_t rectIdx = 0;
        size_t labelIdx = 0;
        size_t clipped = 0;

        auto placeRect = [&](float x, float y, float w, float h, const constant::Vector4D& color) -> bool {
            if (rectIdx >= TL_RECTS)
                return false;

            auto ent = ecsRef->getEntity(tlRectIds[rectIdx++]);
            if (not ent)
                return false;

            auto pos = ent->get<PositionComponent>();
            pos->setX(x);
            pos->setY(y);
            pos->setWidth(w);
            pos->setHeight(h);
            pos->setVisibility(true);

            ent->get<Simple2DObject>()->setColors(color);

            return true;
        };

        // Frame boundary ticks (render frames + ECS passes)
        for (const auto& e : snapshot)
        {
            bool isFrameMark = (e.category == "Frame");
            bool isPassMark = (e.category == "Marker" and e.name == "ECSPass");

            if (not isFrameMark and not isPassMark)
                continue;

            if (e.startMs < viewStart or e.startMs > viewEnd)
                continue;

            const float x = static_cast<float>((e.startMs - viewStart) * pxPerMs);

            if (not placeRect(x, tlTop + TL_LANE_HEADER, 1.0f, TL_H - TL_LANE_HEADER,
                isFrameMark ? constant::Vector4D{255.0f, 255.0f, 255.0f, 40.0f}
                            : constant::Vector4D{255.0f, 200.0f, 90.0f, 30.0f}))
            {
                clipped++;
            }
        }

        const float laneHeight = laneOf.empty() ? TL_H
            : (TL_H - TL_LANE_HEADER) / static_cast<float>(laneOf.size());

        for (const auto& d : drawables)
        {
            const double clampedStart = std::max(d.e->startMs, viewStart);
            const double clampedEnd = std::min(d.e->startMs + d.e->durationMs, viewEnd);

            const float w = static_cast<float>((clampedEnd - clampedStart) * pxPerMs);

            if (w < TL_RECT_MIN_W)
                continue;

            const float x = static_cast<float>((clampedStart - viewStart) * pxPerMs);
            const float y = tlTop + TL_LANE_HEADER + d.lane * laneHeight + d.depth * TL_DEPTH_H;

            if (y + TL_DEPTH_H > windowHeight)
                continue;

            if (not placeRect(x, y, w, TL_DEPTH_H - 1.0f, categoryColor(d.e->category, d.e->name)))
            {
                clipped++;
                continue;
            }

            if (w >= TL_LABEL_MIN_W and labelIdx < TL_LABELS)
            {
                auto ent = ecsRef->getEntity(tlLabelIds[labelIdx++]);
                if (ent)
                {
                    auto pos = ent->get<PositionComponent>();
                    pos->setX(x + 2.0f);
                    pos->setY(y);
                    pos->setVisibility(true);

                    char lbl[64];
                    snprintf(lbl, sizeof(lbl), "%.28s %.2f", baseName(d.e->name).c_str(), d.e->durationMs);
                    ent->get<TTFText>()->setText(lbl);
                }
            }
        }

        // Hide unused pool entries
        for (size_t i = rectIdx; i < TL_RECTS; i++)
            hideRow(tlRectIds[i]);

        for (size_t i = labelIdx; i < TL_LABELS; i++)
            hideRow(tlLabelIds[i]);

        char info[160];
        snprintf(info, sizeof(info), "Timeline  %s  span %.0f ms  lanes %zu  clipped %zu",
                 paused ? "[PAUSED]" : "[LIVE]", viewSpanMs, laneOf.size(), clipped);

        setRowText(tlInfoId, info, TEXT_DIM);
    }

    // ------------------------------------------------------------------
    // Export
    // ------------------------------------------------------------------

    void ProfilerOverlaySystem::exportCsv()
    {
        const auto stamp = static_cast<long long>(std::time(nullptr));

        char name[96];

        snprintf(name, sizeof(name), "profile_capture_%lld.csv", stamp);
        Profiler::instance().exportAllToCSV(name);

        snprintf(name, sizeof(name), "profile_capture_%lld_stats.csv", stamp);
        ProfilerStats::instance().exportToCSV(name);
    }

    // ------------------------------------------------------------------
    // Factory
    // ------------------------------------------------------------------

    void createProfilerOverlay(EntitySystem& ecs, Window& window, const std::string& fontPath)
    {
        auto* ttf = ecs.getSystem<TTFTextSystem>();

        if (not ttf)
        {
            ttf = ecs.createSystem<TTFTextSystem>(window.masterRenderer);
            ttf->registerFont(fontPath);
        }

        ecs.createSystem<ProfilerOverlaySystem>(&window, fontPath);
    }
}

#endif // PROFILE
