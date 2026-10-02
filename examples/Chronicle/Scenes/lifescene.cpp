#include "lifescene.h"

#ifdef __linux__
#include <SDL2/SDL.h>
#elif _WIN32
#include <SDL.h>
#endif

#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>

#include "logger.h"

#include "ECS/entitysystem.h"
#include "ECS/entitysystem_fwd.h"
#include "Input/sdlevents.h"
#include "2D/position.h"
#include "UI/prefab.h"
#include "UI/prefabloader.h"
#include "UI/prefabbuilder.h"
#include "UI/themesystem.h"
#include "Systems/coresystems.h"
#include "Systems/gamefacts.h"
#include "Systems/achievement.h"

#include "UI/label.h"
#include "UI/mark.h"
#include "UI/statline.h"
#include "UI/lifeclock.h"
#include "UI/windowmeter.h"
#include "UI/resourceledger.h"
#include "UI/eventlog.h"
#include "UI/panel.h"
#include "UI/gloss.h"
#include "Core/motion.h"

using namespace pg;

namespace chronicle
{
    namespace
    {
        constexpr const char * const DOM = "Chronicle.Life";

        constexpr const char * const ActivitiesList = "life.activities";

        // The page's frame (life.yaml): margins, the fixed columns, where the columns start, and the
        // chrome of a panel with a heading (16 + the 51 head block + 16)
        constexpr float PageMargin = 48.0f;
        constexpr float ColumnGap = 24.0f;
        constexpr float LeftColumn = 320.0f;
        constexpr float RightColumn = 360.0f;
        constexpr float ColumnsTop = 178.0f;
        constexpr float StackGap = 16.0f;
        constexpr float PanelChrome = 83.0f;
        constexpr float MinMiddle = 420.0f;
        constexpr float MinList = 200.0f;
        constexpr float MinLog = 120.0f;
        constexpr float PartsPanel = 160.0f;       // Three parts, until the panel has measured itself
        constexpr float ClockPanel = 160.0f;       // The clock and his life, until the panel has measured itself
        constexpr float AlertMs = 900.0f;          // How long a part stays in red after a month took from it
        constexpr float RunningPanel = 164.0f;     // "At work now" holding a running row
        constexpr float LogFootnote = 19.0f;       // 4 + a caption line
        constexpr const char * const TabsTag = "life.tabs";

        // Below the three columns' least size the page is life-compact.yaml: margins 24, the head
        // on two lines, the work at hand over the choice, and a side column of one panel at a time
        constexpr float FullMinWidth = 2.0f * PageMargin + LeftColumn + RightColumn + 2.0f * ColumnGap + MinMiddle;          // That is 1244
        constexpr float FullMinHeight = ColumnsTop + ClockPanel + StackGap + RunningPanel + StackGap + PanelChrome + LogFootnote + MinLog + StackGap + PartsPanel + PageMargin;   // That is 980
        constexpr float CompactMargin = 24.0f;
        constexpr float CompactTop = 128.0f;       // Under the tabs
        constexpr float SideColumn = 320.0f;
        constexpr float SideTop = 180.0f;          // Under the side tabs
        constexpr float SideChrome = 32.0f;        // A panel without a heading
        constexpr float AgeWidth = 120.0f;
        constexpr float MinCompactList = 120.0f;
        constexpr const char * const SideTag = "life.side";
        const char * const SidePanels[] = {"parts", "holds", "clockPanel", "happened"};

        bool needsCompact(float width, float height)
        {
            return width < FullMinWidth or height < FullMinHeight;
        }

        void windowSize(EntitySystem* ecs, float& width, float& height)
        {
            width = 1320.0f;
            height = 1020.0f;

            if (auto window = ecs->getEntity("__MainWindow"); window and window->has<PositionComponent>())
            {
                auto pos = window->get<PositionComponent>();

                if (pos->width > 0.0f and pos->height > 0.0f)
                {
                    width = pos->width;
                    height = pos->height;
                }
            }
        }

        int intOf(const ElementType& v)
        {
            switch (v.type)
            {
            case UnionType::INT:    return v.get<int>();
            case UnionType::FLOAT:  return static_cast<int>(v.get<float>());
            case UnionType::DOUBLE: return static_cast<int>(v.get<double>());
            case UnionType::SIZE_T: return static_cast<int>(v.get<size_t>());
            case UnionType::BOOL:   return v.get<bool>() ? 1 : 0;
            default:                return 0;
            }
        }

        float floatOf(const ElementType& v)
        {
            switch (v.type)
            {
            case UnionType::FLOAT:  return v.get<float>();
            case UnionType::DOUBLE: return static_cast<float>(v.get<double>());
            default:                return static_cast<float>(intOf(v));
            }
        }

        std::string textOf(const ElementMap& map, const std::string& key)
        {
            auto it = map.find(key);

            return it == map.end() ? std::string() : it->second.toString();
        }

        int intOf(const ElementMap& map, const std::string& key)
        {
            auto it = map.find(key);

            return it == map.end() ? 0 : intOf(it->second);
        }

        bool boolOf(const ElementMap& map, const std::string& key)
        {
            auto it = map.find(key);

            return it != map.end() and it->second.type == UnionType::BOOL and it->second.get<bool>();
        }

        LogKind kindOf(const std::string& kind)
        {
            if (kind == "gain")      return LogKind::Gain;
            if (kind == "loss")      return LogKind::Loss;
            if (kind == "coin")      return LogKind::Coin;
            if (kind == "milestone") return LogKind::Milestone;

            return LogKind::Note;
        }

        ActivityState stateOf(const std::string& state)
        {
            if (state == "running") return ActivityState::Running;
            if (state == "locked")  return ActivityState::Locked;

            return ActivityState::Idle;
        }

        FactCheckEquality equalityOf(const std::string& op)
        {
            if (op == ">=") return FactCheckEquality::GreaterEqual;
            if (op == ">")  return FactCheckEquality::Greater;
            if (op == "<=") return FactCheckEquality::LesserEqual;
            if (op == "<")  return FactCheckEquality::Lesser;
            if (op == "==") return FactCheckEquality::Equal;

            return FactCheckEquality::None;
        }

        WindowState windowStateOf(const std::string& state)
        {
            if (state == "upcoming") return WindowState::Upcoming;
            if (state == "closed")   return WindowState::Closed;

            return WindowState::Open;
        }

        // +2, −1 (U+2212)
        std::string signedText(int n)
        {
            return (n < 0 ? std::string("\xE2\x88\x92") : std::string("+")) + std::to_string(n < 0 ? -n : n);
        }

        std::string upper(std::string s)
        {
            for (char& c : s)
            {
                if (c >= 'a' and c <= 'z')
                    c = static_cast<char>(c - 'a' + 'A');
            }

            return s;
        }

        // "<prefix><key>" or "<prefix><key>.<field>": the key may hold dots ("guild.bellmoor"),
        // the field never does, and only the listed fields count as fields.
        bool splitPath(const std::string& rest, const std::vector<std::string>& fields, std::string& key, std::string& field)
        {
            for (const auto& f : fields)
            {
                const std::string suffix = "." + f;

                if (rest.size() > suffix.size() and rest.compare(rest.size() - suffix.size(), suffix.size(), suffix) == 0)
                {
                    key = rest.substr(0, rest.size() - suffix.size());
                    field = f;
                    return true;
                }
            }

            key = rest;
            field.clear();
            return false;
        }
    }

    LifeScene::LifeScene(LifeSceneOptions options) : opt(std::move(options)) {}

    template <typename Type>
    void LifeScene::setFact(const std::string& path, const Type& value)
    {
        if (auto facts = ecsRef->getSystem<WorldFacts>())
            facts->setFact(path, value);
    }

    EntityRef LifeScene::named(const std::string& name) const
    {
        auto it = handles.find(name);

        if (it != handles.end())
            return it->second;

        EntityRef root = page;

        if (root.empty() or not root->has<Prefab>())
            return EntityRef{};

        EntityRef ent = root->get<Prefab>()->findEntity(name);
        const_cast<LifeScene*>(this)->handles[name] = ent;

        return ent;
    }

    // ---- init: the page as a file ----------------------------------------------------------------

    bool LifeScene::buildPage(bool compactPage)
    {
        const std::string& file = compactPage ? opt.compactFile : opt.pageFile;

        std::vector<std::string> errors;
        PrefabLoadOptions options;
        options.errors = &errors;

        auto spec = loadNodeSpec(ecsRef, file, options);

        if (not spec)
        {
            LOG_ERROR(DOM, "Could not load the page " << file);
            return false;
        }

        for (const auto& e : errors)
            LOG_ERROR(DOM, file << ": " << e);

        EntityRef built = buildTree(ecsRef, *spec);

        if (built.empty())
        {
            LOG_ERROR(DOM, "Could not build the page " << file);
            return false;
        }

        // The former page goes, its pieces with it as its prefab children
        if (not page.empty())
            ecsRef->removeEntity(page);

        page = built;
        compact = compactPage;
        handles.clear();

        // The page leaves with the scene; its pieces follow it as its prefab children.
        ecsRef->attach<SceneElement>(page);

        return true;
    }

    void LifeScene::init()
    {
        float width, height;
        windowSize(ecsRef, width, height);

        if (not buildPage(needsCompact(width, height)))
            return;

        listenToEvent<ActivitySelectedEvent>([this](const ActivitySelectedEvent& e) { onSelect(e); });
        listenToEvent<ActivityActivatedEvent>([this](const ActivityActivatedEvent& e) { onConfirm(e); });
        listenToEvent<TabSelectedEvent>([this](const TabSelectedEvent& e) { onTab(e); });

        // A deed reached: taken in execute(), never while the facts are being handed around
        listenToStandardEvent(AchievementUnlockEventName, [this](const StandardEvent& e) {
            auto name = e.values.find("name");

            if (name != e.values.end())
                reached.push_back(name->second.toString());
        });

        // The month loop: one month every opt.monthMs while it runs
        listenToEvent<TickEvent>([this](const TickEvent& e) {
            // The red of a loss fades whether the months run or not
            if (alertLeft > 0.0f)
            {
                alertLeft -= e.tick;

                if (alertLeft <= 0.0f)
                    clearAlert();
            }

            if (paused)
                return;

            sinceMonth += e.tick;

            while (sinceMonth >= opt.monthMs)
            {
                sinceMonth -= opt.monthMs;
                onMonth();
            }
        });

        listenToEvent<OnSDLScanCode>([this](const OnSDLScanCode& e) {
            auto theme = ecsRef->getSystem<ThemeSystem>();

            switch (e.key)
            {
            case SDL_SCANCODE_SPACE:
                paused = not paused;
                sinceMonth = 0.0f;
                break;

            case SDL_SCANCODE_M:
                onMonth();
                break;

            case SDL_SCANCODE_T:
                if (theme)
                    theme->setTheme(theme->currentTheme() == "day" ? "candle" : "day");
                break;

            case SDL_SCANCODE_R:
                Motion::setReduced(not Motion::reduced());
                break;

            case SDL_SCANCODE_S:
                saveNow();
                break;

            case SDL_SCANCODE_N:
                newLife();
                break;

            default:
                break;
            }
        });

        listenToEvent<ResizeEvent>([this](const ResizeEvent& e) { fit(e.width, e.height); });
    }

    // ---- startUp: rules, save, rows, wiring, paths ------------------------------------------------

    void LifeScene::startUp()
    {
        if (page.empty())
            return;

        if (not rules.load(ecsRef, opt.rulesRoot))
        {
            for (const auto& e : rules.errors)
                LOG_ERROR(DOM, e);
        }

        if (opt.fresh)
            save = freshLife();
        else if (opt.noSave or not save.load(opt.savePath))
            save = firstLife();

        rules.done = save.terms();

        // A save from before the ledger followed the stats
        refreshHoldings();
        save.holdEarned(holdings);

        // The page to the window as it is now, before the rows: they are built at the widths it
        // sets (a list resizes the rows it holds, not the ones still on their way in). Every
        // resize after this comes as a ResizeEvent.
        float width, height;
        windowSize(ecsRef, width, height);

        fit(width, height);

        rebuild();
        wire();
        publish();
        registerDeeds();
    }

    void LifeScene::fit(float width, float height)
    {
        if (page.empty())
            return;

        windowWidth = width;
        windowHeight = height;
        runningShown = not save.running.empty();

        // Across the breakpoint the other file replaces the page, sized, then its rows and every
        // path again
        const bool swap = needsCompact(width, height) != compact;

        if (swap and not buildPage(not compact))
            return;

        if (EntityRef background = named("page"); not background.empty())
        {
            background->get<PositionComponent>()->setWidth(width);
            background->get<PositionComponent>()->setHeight(height);
        }

        if (compact)
            fitCompact(width, height);
        else
            fitFull(width, height);

        if (swap)
        {
            rebuild();
            publish();
        }
    }

    float LifeScene::workingHeight() const
    {
        return save.running.empty() ? PanelChrome : RunningPanel;
    }

    void LifeScene::fitFull(float width, float height)
    {
        EntitySystem* ecs = ecsRef;

        // The middle column takes what the two fixed ones leave; the choice fills it to the bottom
        const float middle = std::max(MinMiddle, width - 2.0f * PageMargin - LeftColumn - RightColumn - 2.0f * ColumnGap);
        const float listHeight = std::max(MinList, height - ColumnsTop - PanelChrome - StackGap);

        if (auto may = piece<Panel>("may"))
        {
            may->setWidth(ecs, middle);

            if (auto list = piece<ActivityList>("activities"))
                list->setSize(ecs, may->innerWidth(), listHeight);
        }

        // The log takes the height left between the years and the work at hand above it and his
        // parts under it
        float parts = PartsPanel;
        float clock = ClockPanel;

        if (EntityRef partsPanel = named("parts"); not partsPanel.empty())
            parts = std::max(parts, partsPanel->get<PositionComponent>()->height);

        if (EntityRef clockPanel = named("clockPanel"); not clockPanel.empty())
            clock = std::max(clock, clockPanel->get<PositionComponent>()->height);

        const float logHeight = std::max(MinLog, height - ColumnsTop - clock - StackGap - parts - StackGap - workingHeight() - StackGap - PanelChrome - LogFootnote - StackGap);

        if (auto log = piece<EventLog>("log"))
            log->setHeight(ecs, logHeight);
    }

    void LifeScene::fitCompact(float width, float height)
    {
        EntitySystem* ecs = ecsRef;

        if (auto title = piece<Label>("title"))
            title->setWidth(ecs, std::max(0.0f, width - 2.0f * CompactMargin - AgeWidth - StackGap));

        // The main column takes what the side one leaves; the choice fills it to the bottom under
        // the work at hand
        const float main = std::max(MinCompactList, width - 2.0f * CompactMargin - SideColumn - StackGap);
        const float listHeight = std::max(MinCompactList, height - CompactTop - workingHeight() - StackGap - PanelChrome - CompactMargin);

        if (auto working = piece<Panel>("working"))
        {
            working->setWidth(ecs, main);

            if (auto running = piece<ActivityList>("running"))
                running->setSize(ecs, working->innerWidth(), 0.0f);
        }

        if (auto may = piece<Panel>("may"))
        {
            may->setWidth(ecs, main);

            if (auto list = piece<ActivityList>("activities"))
                list->setSize(ecs, may->innerWidth(), listHeight);
        }

        // The log fills the side column to the bottom
        if (auto log = piece<EventLog>("log"))
            log->setHeight(ecs, std::max(MinLog, height - SideTop - SideChrome - LogFootnote - CompactMargin));

        showSide(sideTab);
    }

    void LifeScene::showSide(int index)
    {
        const int count = static_cast<int>(std::size(SidePanels));

        if (index < 0 or index >= count)
            index = 0;

        sideTab = index;

        if (not compact)
            return;

        for (int i = 0; i < count; ++i)
        {
            if (EntityRef panel = named(SidePanels[i]); not panel.empty())
                panel->get<PositionComponent>()->setVisibility(i == index);
        }

        if (auto tabs = piece<Tabs>("sideTabs"); tabs and tabs->active() != index)
            tabs->setActive(ecsRef, index);
    }

    void LifeScene::onLeave()
    {
        if (auto router = ecsRef->getSystem<FactRouter>())
        {
            for (auto id : subs)
                router->off(id);
        }

        subs.clear();

        if (auto achievements = ecsRef->getSystem<AchievementSys>())
            achievements->clear();
    }

    void LifeScene::execute()
    {
        if (reached.empty())
            return;

        std::vector<std::string> ids;
        ids.swap(reached);

        for (const auto& id : ids)
            reachDeed(id);

        publish();
    }

    // ---- wire: every subscription, and nothing else -------------------------------------------------

    void LifeScene::wire()
    {
        auto router = ecsRef->getSystem<FactRouter>();

        if (not router)
        {
            LOG_ERROR(DOM, "No FactRouter: the page will not follow the life");
            return;
        }

        EntitySystem* ecs = ecsRef;

        // The life: the clock, the doors and the head
        subs.push_back(router->on("life.age", [this, ecs](const ElementType& v) {
            if (auto clock = piece<LifeClock>("clock"))
                clock->setAge(ecs, floatOf(v));

            for (const auto& door : handles)
            {
                if (door.first.rfind("window.", 0) == 0)
                {
                    if (auto meter = piece<WindowMeter>(door.first))
                        meter->setAge(ecs, floatOf(v));
                }
            }
        }));

        subs.push_back(router->on("life.headline.ageText", [this, ecs](const ElementType& v) {
            if (auto label = piece<Label>("age"))
                label->setText(ecs, v.toString());
        }));

        subs.push_back(router->on("life.headline.subtitle", [this, ecs](const ElementType& v) {
            if (auto label = piece<Label>("subtitle"))
                label->setText(ecs, v.toString());
        }));

        subs.push_back(router->on("life.headline.ageNote", [this, ecs](const ElementType& v) {
            if (auto label = piece<Label>("ageNote"))
                label->setText(ecs, v.toString());
        }));

        subs.push_back(router->on("life.next.label", [this, ecs](const ElementType& v) {
            if (auto clock = piece<LifeClock>("clock"))
                clock->setNext(ecs, v.toString(), clock->spec.nextIn);
        }));

        subs.push_back(router->on("life.next.in", [this, ecs](const ElementType& v) {
            if (auto clock = piece<LifeClock>("clock"))
                clock->setNext(ecs, clock->spec.nextLabel, intOf(v));
        }));

        // Who he is: the title, and under it what he is and where he comes from
        subs.push_back(router->on("character.name", [this, ecs](const ElementType& v) {
            if (auto title = piece<Label>("title"))
                title->setText(ecs, "The Chronicle of " + v.toString());
        }));

        subs.push_back(router->on("character.about", [this, ecs](const ElementType& v) {
            if (auto label = piece<Label>("about"))
                label->setText(ecs, v.toString());
        }));

        // His parts: character.parts.<p>, and .projected, .threshold, .note, .alert
        subs.push_back(router->onPrefix("character.parts.", [this, ecs](const std::string& path, const ElementType& v) {
            std::string key, field;
            splitPath(path.substr(std::string("character.parts.").size()), {"projected", "threshold", "note", "alert"}, key, field);

            auto line = piece<StatLine>(key);

            if (not line)
                return;

            if (field.empty())
                line->setValue(ecs, intOf(v));
            else if (field == "projected")
                line->setProjected(ecs, intOf(v));
            else if (field == "threshold")
                line->setThreshold(ecs, intOf(v));
            else if (field == "note")
                line->setNote(ecs, v.toString());
            else if (field == "alert")
                line->setAlert(ecs, v.type == UnionType::BOOL and v.get<bool>());
        }));

        // His skills: skills.<id>, shown once he has any of it
        subs.push_back(router->onPrefix("skills.", [this, ecs](const std::string& path, const ElementType& v) {
            auto skills = piece<ResourceLedger>("skills");

            if (not skills)
                return;

            const std::string id = path.substr(std::string("skills.").size());
            const bool shown = skills->row(id) != nullptr;

            if (shown != (intOf(v) > 0))
                fillSkills();
            else if (shown)
                skills->setValue(ecs, id, v.toString());
        }));

        // What he holds: resources.<id>.value, .rate, .muted
        subs.push_back(router->onPrefix("resources.", [this, ecs](const std::string& path, const ElementType& v) {
            std::string id, field;

            if (not splitPath(path.substr(std::string("resources.").size()), {"value", "rate", "muted"}, id, field))
                return;

            auto ledger = piece<ResourceLedger>("ledger");

            if (not ledger or not ledger->row(id))
                return;

            if (field == "value")
                ledger->setValue(ecs, id, v.toString());
            else if (field == "rate")
                ledger->setRate(ecs, id, v.toString());
            else if (field == "muted")
                ledger->setMuted(ecs, id, v.type == UnionType::BOOL and v.get<bool>());
        }));

        // The work at hand: activity.running.id, .months, .percent, .caption
        subs.push_back(router->on("activity.running.id", [this, ecs](const ElementType& v) {
            auto running = piece<ActivityList>("running");

            if (not running)
                return;

            const std::string id = v.toString();

            // The work at hand starts or ends: "At work now" grows or shrinks, what is under it
            // takes the rest
            if (id.empty() == runningShown)
            {
                runningShown = not id.empty();
                fit(windowWidth, windowHeight);
            }

            if (id.empty())
            {
                running->setRows(ecs, {});
                return;
            }

            // Republished every month: the row is already there, its rule keeps its tween
            if (not running->spec.groups.empty() and not running->spec.groups[0].rows.empty() and running->spec.groups[0].rows[0].id == id)
                return;

            for (const auto& a : activities)
            {
                if (textOf(a.fields, "id") != id)
                    continue;

                ActivityRowSpec row;
                row.id = id;
                row.name = textOf(a.fields, "name");
                row.glyph = textOf(a.fields, "glyph");
                row.rank = textOf(a.fields, "rank");
                row.count = textOf(a.fields, "tally");
                row.each = textOf(a.fields, "each");
                row.months = intOf(a.fields, "months");
                row.state = ActivityState::Running;
                row.glossKey = "activity/" + id;

                // Built with the percent and caption of the same update: the row is not in its list
                // until the next frame, so their own handlers would not find it
                if (auto facts = ecs->getSystem<WorldFacts>())
                {
                    row.percent = facts->hasFact("activity.running.percent") ? floatOf(facts->factMap["activity.running.percent"]) : 0.0f;
                    row.caption = facts->getFact<std::string>("activity.running.caption", "");
                }

                running->setRows(ecs, {{"", {row}}});
            }
        }));

        subs.push_back(router->on("activity.running.months", [this, ecs](const ElementType& v) {
            if (auto clock = piece<LifeClock>("clock"))
                clock->setRunning(ecs, floatOf(v));
        }));

        subs.push_back(router->on("activity.running.percent", [this, ecs](const ElementType& v) {
            auto running = piece<ActivityList>("running");

            if (running and not save.running.empty())
            {
                if (auto row = running->find(ecs, save.running))
                    row->setPercent(ecs, floatOf(v));
            }
        }));

        subs.push_back(router->on("activity.running.caption", [this, ecs](const ElementType& v) {
            auto running = piece<ActivityList>("running");

            if (running and not save.running.empty())
            {
                if (auto row = running->find(ecs, save.running))
                    row->setCaption(ecs, v.toString());
            }
        }));

        // Each activity's state, how often it was done and what it still asks: activity.<id>.state,
        // .count, .requirement.<n>
        subs.push_back(router->onPrefix("activity.", [this, ecs](const std::string& path, const ElementType& v) {
            const std::string rest = path.substr(std::string("activity.").size());
            const std::string asked = ".requirement.";
            const size_t at = rest.rfind(asked);

            if (at != std::string::npos)
            {
                auto list = piece<ActivityList>("activities");
                auto row = list ? list->find(ecs, rest.substr(0, at)) : nullptr;
                const size_t index = static_cast<size_t>(std::atoi(rest.substr(at + asked.size()).c_str()));

                // A row that is no longer locked keeps the pair for the day it is again
                if (row and index < row->spec.requirements.size())
                    row->setRequirement(ecs, index, intOf(v), row->spec.requirements[index].needed);

                return;
            }

            std::string id, field;

            if (not splitPath(rest, {"state", "count"}, id, field) or id == "running")
                return;

            auto list = piece<ActivityList>("activities");

            if (not list)
                return;

            if (field == "state")
            {
                list->setRowState(ecs, id, stateOf(v.toString()));
            }
            else if (auto row = list->find(ecs, id))
            {
                row->setCount(ecs, v.toString());
            }
        }));

        // The doors: window.<id>.state, .note
        subs.push_back(router->onPrefix("window.", [this, ecs](const std::string& path, const ElementType& v) {
            std::string id, field;

            if (not splitPath(path.substr(std::string("window.").size()), {"state", "note"}, id, field))
                return;

            if (auto meter = piece<WindowMeter>("window." + id))
            {
                if (field == "state")
                    meter->setState(ecs, windowStateOf(v.toString()));
                else
                    meter->setNote(ecs, v.toString());
            }

            if (auto clock = piece<LifeClock>("clock"); clock and field == "state")
            {
                for (size_t i = 0; i < clock->windows.size(); ++i)
                {
                    if (clock->windows[i].spec.label == id)
                        clock->setWindowClosed(ecs, i, v.toString() == "closed");
                }
            }
        }));

        // What has happened: log.size, the save's entries the log does not show yet
        subs.push_back(router->on("log.size", [this, ecs](const ElementType& v) {
            auto log = piece<EventLog>("log");

            if (not log)
                return;

            const size_t n = std::min(static_cast<size_t>(intOf(v)), save.log.size());

            for (size_t i = log->size(); i < n; ++i)
                log->append(ecs, save.log[i]);
        }));
    }

    // ---- rebuild: the rows the save and the rules hold --------------------------------------------------

    void LifeScene::rebuild()
    {
        EntitySystem* ecs = ecsRef;

        if (auto ledger = piece<ResourceLedger>("ledger"))
        {
            ledger->clear(ecs);

            for (const auto& r : save.resources)
                addToLedger(r);
        }

        fillSkills();

        if (auto log = piece<EventLog>("log"))
            log->clear(ecs);

        // The clock's ticks and bands, the doors' ranges
        std::vector<RuleMilestone> milestones;
        ElementMap next;

        if (rules.milestones(save.age, milestones, next))
        {
            std::vector<ClockMilestone> ticks;

            for (const auto& m : milestones)
                ticks.push_back({static_cast<float>(intOf(m.fields, "age")), textOf(m.fields, "label")});

            if (auto clock = piece<LifeClock>("clock"))
                clock->setMilestones(ecs, ticks);
        }

        RecordList windows;

        if (rules.windows(save.age, save.character(), windows))
        {
            std::vector<ClockWindow> bands;

            for (const auto& w : windows)
            {
                const std::string id = textOf(w, "id");

                bands.push_back({static_cast<float>(intOf(w, "from")), static_cast<float>(intOf(w, "to")), id, textOf(w, "state") == "closed"});

                if (auto meter = piece<WindowMeter>("window." + id))
                    meter->setRange(ecs, static_cast<float>(intOf(w, "from")), static_cast<float>(intOf(w, "to")));
            }

            if (auto clock = piece<LifeClock>("clock"))
                clock->setWindows(ecs, bands);
        }

        fillActivities();

        if (auto running = piece<ActivityList>("running"))
            running->setRows(ecs, {});

        // The window meters the page holds, found once so life.age reaches them
        for (const auto& w : windows)
            named("window." + textOf(w, "id"));
    }

    void LifeScene::fillActivities()
    {
        if (not rules.activities(save.character(), activities))
        {
            for (const auto& e : rules.errors)
                LOG_ERROR(DOM, e);

            return;
        }

        // Grouped as the table orders them; what he can no longer do is not listed
        std::vector<ActivityGroup> groups;

        for (const auto& a : activities)
        {
            if (boolOf(a.fields, "spent"))
                continue;

            const std::string group = textOf(a.fields, "group");

            if (groups.empty() or groups.back().label != group)
                groups.push_back({group, {}});

            ActivityRowSpec row;
            row.id = textOf(a.fields, "id");
            row.name = textOf(a.fields, "name");
            row.glyph = textOf(a.fields, "glyph");
            row.rank = textOf(a.fields, "rank");
            row.count = textOf(a.fields, "tally");
            row.each = textOf(a.fields, "each");
            row.months = intOf(a.fields, "months");
            row.state = boolOf(a.fields, "locked") ? ActivityState::Locked : ActivityState::Idle;
            row.glossKey = "activity/" + row.id;

            for (const auto& g : a.gains)
                row.gains.push_back({upper(textOf(g, "stat")), intOf(g, "amount")});

            for (const auto& r : a.requires)
            {
                Requirement req;
                req.label = textOf(r, "label");
                req.current = intOf(r, "current");
                req.needed = intOf(r, "needed");
                row.requirements.push_back(req);
            }

            groups.back().rows.push_back(row);
        }

        if (auto list = piece<ActivityList>("activities"))
            list->setRows(ecsRef, groups);
    }

    void LifeScene::refreshHoldings()
    {
        RuleMonth month;

        if (rules.month(save.character(), month))
        {
            holdings = month.rows;
            holdingGlosses = month.glosses;
            death = month.death;

            caps.clear();

            for (const auto& cap : month.caps)
                caps[textOf(cap, "stat")] = intOf(cap, "most");
            return;
        }

        for (const auto& e : rules.errors)
            LOG_ERROR(DOM, e);
    }

    void LifeScene::endLife()
    {
        // The line the new life opens with, and the age the last one ended at as the head wrote it
        const std::string ending = death;
        std::string age;

        if (auto facts = ecsRef->getSystem<WorldFacts>())
            age = facts->getFact<std::string>("life.headline.ageText", "");

        LOG_INFO(DOM, "The life ended at " << age << ": " << ending);

        // The months stop: the new life starts when the player says so
        paused = true;
        sinceMonth = 0.0f;

        clearAlert();
        newLife();

        appendLog({save.age, ending, LogKind::Loss, age, ""});
    }

    void LifeScene::registerDeeds()
    {
        auto achievements = ecsRef->getSystem<AchievementSys>();

        if (not achievements)
        {
            LOG_ERROR(DOM, "No AchievementSys: no deed will be reached");
            return;
        }

        achievements->clear();
        reached.clear();

        if (not rules.achievements(deeds))
        {
            for (const auto& e : rules.errors)
                LOG_ERROR(DOM, e);

            return;
        }

        for (const auto& deed : deeds)
        {
            const std::string id = textOf(deed.fields, "id");

            if (std::find(save.achieved.begin(), save.achieved.end(), id) != save.achieved.end())
                continue;

            Achievement achievement;
            achievement.name = id;

            for (const auto& ask : deed.asks)
            {
                auto value = ask.find("value");

                achievement.prerequisiteFacts.push_back(FactChecker(textOf(ask, "fact"), value == ask.end() ? ElementType{0} : value->second, equalityOf(textOf(ask, "op"))));
            }

            achievements->addNewAchivement(achievement);
        }
    }

    void LifeScene::reachDeed(const std::string& id)
    {
        if (std::find(save.achieved.begin(), save.achieved.end(), id) != save.achieved.end())
            return;

        for (const auto& deed : deeds)
        {
            if (textOf(deed.fields, "id") != id)
                continue;

            save.achieved.push_back(id);

            for (const auto& g : deed.gives)
                save.stats[textOf(g, "stat")] += intOf(g, "amount");

            save.log.push_back({save.age, textOf(deed.fields, "entry"), LogKind::Milestone, "", ""});

            refreshHoldings();

            for (const auto& r : save.holdEarned(holdings))
                addToLedger(r);

            return;
        }
    }

    void LifeScene::takeStats(const ElementMap& stats)
    {
        for (const auto& [key, value] : stats)
            save.stats[key] = intOf(value);
    }

    void LifeScene::writeEntries(const RecordList& entries)
    {
        for (const auto& e : entries)
            save.log.push_back({save.age, textOf(e, "text"), kindOf(textOf(e, "kind")), textOf(e, "figure"), textOf(e, "glyph")});
    }

    void LifeScene::addToLedger(const LifeResource& resource)
    {
        auto ledger = piece<ResourceLedger>("ledger");

        if (not ledger)
            return;

        ledger->addGroup(ecsRef, resource.group, resource.groupLabel);

        auto stat = save.stats.find(resource.id);

        LedgerRowSpec row;
        row.id = resource.id;
        row.glyph = resource.glyph;
        row.name = resource.name;
        row.value = stat != save.stats.end() ? std::to_string(stat->second) : resource.value;
        row.rate = resource.rate;
        row.tone = static_cast<LedgerTone>(resource.tone);
        row.muted = resource.muted;
        row.glossKey = "resource/" + resource.id;

        ledger->addRow(ecsRef, resource.group, row, resource.groupLabel);
    }

    void LifeScene::fillSkills()
    {
        auto skills = piece<ResourceLedger>("skills");

        if (not skills)
            return;

        skills->clear(ecsRef);

        // Rebuilt whole, so the skills keep the save's order whenever one appears
        for (const auto& s : save.skills)
        {
            const int value = save.stats[s.id];

            if (value <= 0)
                continue;

            skills->addGroup(ecsRef, s.group, s.groupLabel);   // Unlabelled: no heading

            LedgerRowSpec row;
            row.id = s.id;
            row.glyph = s.glyph;
            row.name = s.name;
            row.value = std::to_string(value);

            skills->addRow(ecsRef, s.group, row, s.groupLabel);
        }
    }

    void LifeScene::alert(const std::vector<std::string>& stats)
    {
        // A month that took from what the life hangs on: the part turns red for a moment, and
        // the months stop the first time, so the loss is not run past
        for (const auto& stat : stats)
        {
            setFact("character.parts." + stat + ".alert", true);

            if (std::find(alerted.begin(), alerted.end(), stat) == alerted.end())
                alerted.push_back(stat);
        }

        alertLeft = AlertMs;

        if (not endangered)
        {
            paused = true;
            sinceMonth = 0.0f;
        }
    }

    void LifeScene::clearAlert()
    {
        for (const auto& stat : alerted)
            setFact("character.parts." + stat + ".alert", false);

        alerted.clear();
        alertLeft = 0.0f;
    }

    // ---- publish: the save and the scripts to paths ------------------------------------------------------

    void LifeScene::publish()
    {
        refreshHoldings();
        publishAll();
    }

    void LifeScene::publishAll()
    {
        glossHoldings();
        publishCharacter();
        publishRules();

        // The ghosts: of the activity chosen if one is, else of what mends
        auto list = piece<ActivityList>("activities");
        const std::string chosen = list ? list->selected() : std::string();

        RuleForecast forecast;

        if (not chosen.empty() and rules.forecast(save.age, save.character(), chosen, 0, forecast))
            publishProjected(&forecast);
        else
            publishProjected(nullptr);
    }

    void LifeScene::publishProjected(const RuleForecast* forecast)
    {
        for (const auto& p : save.parts)
        {
            const int now = save.stats[p];
            int ghost = -1;

            // A part that was taken from mends back to the most it can be
            if (auto cap = caps.find(p); cap != caps.end() and cap->second > now)
                ghost = cap->second;

            // What the chosen activity would bring it to says more
            if (forecast)
            {
                auto it = forecast->atTerm.find(p);

                if (it != forecast->atTerm.end() and intOf(it->second) > now)
                    ghost = intOf(it->second);
            }

            setFact("character.parts." + p + ".projected", ghost);
        }
    }

    void LifeScene::publishCharacter()
    {
        setFact("life.age", save.age);

        setFact("character.name", save.name);
        setFact("character.about", save.profession + " \xC2\xB7 " + save.origin);

        for (const auto& p : save.parts)
            setFact("character.parts." + p, save.stats[p]);

        for (const auto& s : save.skills)
            setFact("skills." + s.id, save.stats[s.id]);

        for (const auto& r : save.resources)
        {
            auto stat = save.stats.find(r.id);

            // The rules' rate for what they follow, the save's own for the rest
            std::string rate = r.rate;

            for (const auto& h : holdings)
            {
                if (textOf(h, "id") == r.id)
                    rate = textOf(h, "rate");
            }

            setFact("resources." + r.id + ".value", stat != save.stats.end() ? std::to_string(stat->second) : r.value);
            setFact("resources." + r.id + ".rate", rate);
            setFact("resources." + r.id + ".muted", r.muted);
        }

        // Every stat as a number, for what watches the life rather than shows it
        for (const auto& [key, value] : save.stats)
            setFact("stat." + key, value);

        setFact("log.size", static_cast<int>(save.log.size()));

        publishRunning();
    }

    void LifeScene::publishRunning()
    {
        if (save.running.empty())
        {
            setFact("activity.running.id", std::string());
            setFact("activity.running.months", 0.0f);
            return;
        }

        RuleForecast forecast;

        if (not rules.forecast(save.age, save.character(), save.running, save.monthsIn, forecast))
        {
            for (const auto& e : rules.errors)
                LOG_ERROR(DOM, e);

            return;
        }

        setFact("activity.running.id", save.running);
        setFact("activity.running.months", static_cast<float>(forecast.months - save.monthsIn));
        setFact("activity.running.percent", forecast.percent);
        setFact("activity.running.caption", forecast.caption);
    }

    void LifeScene::publishRules()
    {
        std::vector<RuleMilestone> milestones;
        ElementMap headline;

        if (rules.milestones(save.age, milestones, next, &headline))
        {
            nextAsks.clear();

            setFact("life.next.label", textOf(next, "label"));
            setFact("life.next.in", intOf(next, "in"));

            for (const auto& [key, value] : headline)
                setFact("life.headline." + key, value.toString());

            // What the next milestone asks of the path he is headed for stands on his parts
            std::unordered_map<std::string, int> thresholds;

            for (const auto& m : milestones)
            {
                if (textOf(m.fields, "id") != textOf(next, "id"))
                    continue;

                for (const auto& ask : m.asks)
                {
                    if (textOf(ask, "path") == save.aim)
                    {
                        thresholds[textOf(ask, "stat")] = intOf(ask, "needed");
                        nextAsks.push_back(ask);
                    }
                }
            }

            for (const auto& p : save.parts)
                setFact("character.parts." + p + ".threshold", thresholds.count(p) ? thresholds[p] : 0);

            glossParts(nullptr, "");
        }

        RecordList windows;

        if (rules.windows(save.age, save.character(), windows))
        {
            for (const auto& w : windows)
            {
                setFact("window." + textOf(w, "id") + ".state", textOf(w, "state"));
                setFact("window." + textOf(w, "id") + ".note", textOf(w, "note"));
            }

            glossWindows(windows);
        }

        if (rules.activities(save.character(), activities))
        {
            for (const auto& a : activities)
            {
                const std::string id = textOf(a.fields, "id");
                const std::string state = id == save.running ? "running" : boolOf(a.fields, "locked") ? "locked" : "idle";

                setFact("done." + id, intOf(a.fields, "done"));

                // A spent activity has no row to tell
                if (boolOf(a.fields, "spent"))
                    continue;

                setFact("activity." + id + ".state", state);
                setFact("activity." + id + ".count", textOf(a.fields, "tally"));

                for (size_t i = 0; i < a.requires.size(); ++i)
                    setFact("activity." + id + ".requirement." + std::to_string(i), intOf(a.requires[i], "current"));
            }

            glossActivities();
        }

        for (const auto& e : rules.errors)
            LOG_ERROR(DOM, e);
    }

    // ---- events in ----------------------------------------------------------------------------------------

    void LifeScene::onSelect(const ActivitySelectedEvent& event)
    {
        if (event.list != ActivitiesList)
            return;

        // Nothing chosen: no ghosts on the parts but what mends
        if (event.id.empty())
        {
            publishProjected(nullptr);

            glossParts(nullptr, "");
            return;
        }

        RuleForecast forecast;

        if (not rules.forecast(save.age, save.character(), event.id, 0, forecast))
            return;

        // The ghost of each part the chosen activity would raise
        publishProjected(&forecast);

        glossParts(&forecast, activityName(event.id));
    }

    void LifeScene::onConfirm(const ActivityActivatedEvent& event)
    {
        if (event.list != ActivitiesList)
            return;

        // What takes no time is done now, whatever else he is at
        RuleForecast forecast;

        if (rules.forecast(save.age, save.character(), event.id, 0, forecast) and forecast.months <= 0)
        {
            doAtOnce(event.id, forecast);
            return;
        }

        if (not save.running.empty())
        {
            appendLog({save.age, "Already at work: " + activityName(save.running), LogKind::Note, "", ""});
            return;
        }

        save.running = event.id;
        save.monthsIn = 0;

        if (auto list = piece<ActivityList>("activities"))
            list->select(ecsRef, "");

        publishProjected(nullptr);

        setFact("activity." + event.id + ".state", std::string("running"));

        publishRunning();
    }

    void LifeScene::doAtOnce(const std::string& id, const RuleForecast& forecast)
    {
        // The row as it stands, to see whether doing it changes the row itself
        std::string rank;
        std::vector<int> amounts;

        for (const auto& a : activities)
        {
            if (textOf(a.fields, "id") != id)
                continue;

            // The list refuses a locked row, a key or a script may not
            if (boolOf(a.fields, "locked") or boolOf(a.fields, "spent"))
                return;

            rank = textOf(a.fields, "rank");

            for (const auto& g : a.gains)
                amounts.push_back(intOf(g, "amount"));
        }

        takeStats(forecast.atTerm);
        writeEntries(forecast.entries);

        ++save.done[id];
        rules.done = save.terms();

        refreshHoldings();

        // What it took was the last of what he lived on
        if (not death.empty())
        {
            endLife();
            return;
        }

        for (const auto& r : save.holdEarned(holdings))
            addToLedger(r);

        // Rebuilt only when it left or changed: the selection stays for the next time otherwise
        bool changed = not rules.activities(save.character(), activities);

        for (const auto& a : activities)
        {
            if (textOf(a.fields, "id") != id)
                continue;

            std::vector<int> now;

            for (const auto& g : a.gains)
                now.push_back(intOf(g, "amount"));

            changed = changed or boolOf(a.fields, "spent") or textOf(a.fields, "rank") != rank or now != amounts;
        }

        if (changed)
            fillActivities();

        publishAll();
    }

    void LifeScene::onTab(const TabSelectedEvent& event)
    {
        if (event.tag == SideTag)
        {
            showSide(event.index);
            return;
        }

        if (event.tag != TabsTag or event.index == 0)
            return;

        auto tabs = piece<Tabs>("tabs");

        if (not tabs)
            return;

        const std::string label = event.index >= 0 and static_cast<size_t>(event.index) < tabs->tabs.size() ? tabs->tabs[event.index].label.spec.text : "That page";

        tabs->setActive(ecsRef, 0);

        appendLog({save.age, label + " has no page yet", LogKind::Note, "", ""});
    }

    // ---- the month ----------------------------------------------------------------------------------------

    void LifeScene::onMonth()
    {
        save.age += 1.0f / 12.0f;

        // What he holds works or wastes, whatever he is doing
        RuleMonth month;

        if (rules.month(save.character(), month))
        {
            takeStats(month.after);
            writeEntries(month.entries);
        }
        else
        {
            for (const auto& e : rules.errors)
                LOG_ERROR(DOM, e);
        }

        if (not save.running.empty())
        {
            ++save.monthsIn;

            RuleForecast forecast;

            if (rules.forecast(save.age, save.character(), save.running, save.monthsIn, forecast) and save.monthsIn >= forecast.months)
            {
                // At term: the forecast's numbers become the character's, its lines the log's
                takeStats(forecast.atTerm);
                writeEntries(forecast.entries);

                ++save.done[save.running];
                rules.done = save.terms();

                save.running.clear();
                save.monthsIn = 0;

                // One more term done: a row may have left, another may have changed
                fillActivities();
            }
        }

        // The character as the month leaves him: with nothing left to live on, this life is lost
        // and a new one begins
        refreshHoldings();

        if (not death.empty())
        {
            endLife();
            return;
        }

        // What he holds for the first time gets its row
        for (const auto& r : save.holdEarned(month.rows))
            addToLedger(r);

        publishAll();

        // The month took from what the life hangs on: he is in danger, and the page says so
        if (not month.hurt.empty())
            alert(month.hurt);

        endangered = not month.hurt.empty();
    }

    // ---- the glosses -----------------------------------------------------------------------------------------

    void LifeScene::glossParts(const RuleForecast* forecast, const std::string& activity)
    {
        auto registry = ecsRef->getSystem<GlossRegistry>();

        if (not registry)
            return;

        for (const auto& p : save.parts)
        {
            auto line = piece<StatLine>(p);

            GlossSpec gloss;
            gloss.title = line ? line->spec.label : p;
            gloss.rows.push_back({"Now", std::to_string(save.stats[p])});

            if (auto cap = caps.find(p); cap != caps.end())
                gloss.rows.push_back({"At most", std::to_string(cap->second)});

            for (const auto& ask : nextAsks)
            {
                if (textOf(ask, "stat") == p)
                    gloss.rows.push_back({textOf(next, "label") + " asks", std::to_string(intOf(ask, "needed"))});
            }

            if (forecast)
            {
                auto it = forecast->atTerm.find(p);

                if (it != forecast->atTerm.end() and intOf(it->second) != save.stats[p])
                    gloss.rows.push_back({activity + " brings it to", std::to_string(intOf(it->second))});
            }

            if (not textOf(next, "label").empty())
                gloss.footnote = "IN " + std::to_string(intOf(next, "in")) + " MO: " + upper(textOf(next, "label"));

            registry->set("parts/" + p, gloss);
        }
    }

    void LifeScene::glossActivities()
    {
        auto registry = ecsRef->getSystem<GlossRegistry>();

        if (not registry)
            return;

        for (const auto& a : activities)
        {
            GlossSpec gloss;
            gloss.title = textOf(a.fields, "name");
            gloss.rows.push_back({"Time", intOf(a.fields, "months") > 0 ? std::to_string(intOf(a.fields, "months")) + " mo" : std::string("At once")});

            for (const auto& g : a.gains)
                gloss.rows.push_back({upper(textOf(g, "stat")) + " at term", signedText(intOf(g, "amount"))});

            for (const auto& r : a.requires)
                gloss.rows.push_back({textOf(r, "label"), std::to_string(intOf(r, "current")) + " / " + std::to_string(intOf(r, "needed"))});

            gloss.rows.push_back({"Done", std::to_string(intOf(a.fields, "done"))});

            // -1: as often as he likes
            if (intOf(a.fields, "left") >= 0)
                gloss.rows.push_back({"Left", std::to_string(intOf(a.fields, "left"))});

            if (textOf(a.fields, "id") == save.running)
                gloss.footnote = "AT WORK NOW";
            else if (boolOf(a.fields, "locked"))
                gloss.footnote = "NOT YET: IT ASKS MORE THAN HE HAS";
            else
                gloss.footnote = "SELECT, THEN CONFIRM TO BEGIN";

            registry->set("activity/" + textOf(a.fields, "id"), gloss);
        }
    }

    void LifeScene::glossHoldings()
    {
        auto registry = ecsRef->getSystem<GlossRegistry>();

        if (not registry)
            return;

        // Every row of the ledger has one: what the rules follow says what it does, the rest what
        // he holds of it
        for (const auto& r : save.resources)
        {
            auto stat = save.stats.find(r.id);

            GlossSpec gloss;
            gloss.title = r.name;
            gloss.rows.push_back({"Held", stat != save.stats.end() ? std::to_string(stat->second) : r.value});

            for (const auto& g : holdingGlosses)
            {
                if (textOf(g.fields, "id") != r.id)
                    continue;

                gloss.title = textOf(g.fields, "title");
                gloss.text = textOf(g.fields, "text");
                gloss.footnote = textOf(g.fields, "footnote");
                gloss.rows.clear();

                for (const auto& row : g.rows)
                    gloss.rows.push_back({textOf(row, "label"), textOf(row, "value")});
            }

            registry->set("resource/" + r.id, gloss);
        }
    }

    void LifeScene::glossWindows(const RecordList& windows)
    {
        auto registry = ecsRef->getSystem<GlossRegistry>();

        if (not registry)
            return;

        for (const auto& w : windows)
        {
            GlossSpec gloss;
            gloss.title = textOf(w, "name");
            gloss.text = textOf(w, "note");
            gloss.rows.push_back({"Opens at", std::to_string(intOf(w, "from"))});
            gloss.rows.push_back({"Closes at", std::to_string(intOf(w, "to"))});

            if (textOf(w, "state") != "closed")
                gloss.rows.push_back({"Attempts that fit", std::to_string(intOf(w, "attempts"))});

            gloss.footnote = upper(textOf(w, "state"));

            registry->set("window/" + textOf(w, "id"), gloss);
        }
    }

    // ---- the rest -------------------------------------------------------------------------------------------

    void LifeScene::appendLog(const LogEntry& entry)
    {
        save.log.push_back(entry);
        setFact("log.size", static_cast<int>(save.log.size()));
    }

    std::string LifeScene::activityName(const std::string& id) const
    {
        for (const auto& a : activities)
        {
            if (textOf(a.fields, "id") == id)
                return textOf(a.fields, "name");
        }

        return id;
    }

    bool LifeScene::saveNow()
    {
        if (opt.noSave)
        {
            LOG_INFO(DOM, "Not saved: --no-save");
            return false;
        }

        const bool ok = save.save(opt.savePath);

        if (ok)
            appendLog({save.age, "The chronicle is written up to here", LogKind::Note, "", ""});
        else
            LOG_ERROR(DOM, "Could not save to " << opt.savePath);

        return ok;
    }

    void LifeScene::newLife()
    {
        save = freshLife();
        endangered = false;

        rules.done = save.terms();

        rebuild();
        publish();
        registerDeeds();
    }
}
