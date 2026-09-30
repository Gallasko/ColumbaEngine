#include "lifescene.h"

#ifdef __linux__
#include <SDL2/SDL.h>
#elif _WIN32
#include <SDL.h>
#endif

#include <algorithm>
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

#include "UI/label.h"
#include "UI/mark.h"
#include "UI/statline.h"
#include "UI/lifeclock.h"
#include "UI/windowmeter.h"
#include "UI/resourceledger.h"
#include "UI/eventlog.h"
#include "Core/motion.h"

using namespace pg;

namespace chronicle
{
    namespace
    {
        constexpr const char * const DOM = "Chronicle.Life";

        constexpr const char * const ActivitiesList = "life.activities";
        constexpr const char * const TabsTag = "life.tabs";

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

        WindowState windowStateOf(const std::string& state)
        {
            if (state == "upcoming") return WindowState::Upcoming;
            if (state == "closed")   return WindowState::Closed;

            return WindowState::Open;
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

    void LifeScene::init()
    {
        std::vector<std::string> errors;
        PrefabLoadOptions options;
        options.errors = &errors;

        auto spec = loadNodeSpec(ecsRef, opt.pageFile, options);

        if (not spec)
        {
            LOG_ERROR(DOM, "Could not load the page " << opt.pageFile);
            return;
        }

        for (const auto& e : errors)
            LOG_ERROR(DOM, opt.pageFile << ": " << e);

        page = buildTree(ecsRef, *spec);

        if (page.empty())
        {
            LOG_ERROR(DOM, "Could not build the page " << opt.pageFile);
            return;
        }

        // The page leaves with the scene; its pieces follow it as its prefab children.
        ecsRef->attach<SceneElement>(page);

        listenToEvent<ActivitySelectedEvent>([this](const ActivitySelectedEvent& e) { onSelect(e); });
        listenToEvent<ActivityActivatedEvent>([this](const ActivityActivatedEvent& e) { onConfirm(e); });
        listenToEvent<TabSelectedEvent>([this](const TabSelectedEvent& e) { onTab(e); });

        // The month loop: one month every opt.monthMs while it runs
        listenToEvent<TickEvent>([this](const TickEvent& e) {
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

        listenToEvent<ResizeEvent>([this](const ResizeEvent& e) {
            EntityRef background = named("page");

            if (not background.empty())
            {
                background->get<PositionComponent>()->setWidth(e.width);
                background->get<PositionComponent>()->setHeight(e.height);
            }
        });
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

        rebuild();
        wire();
        publish();
    }

    void LifeScene::onLeave()
    {
        if (auto router = ecsRef->getSystem<FactRouter>())
        {
            for (auto id : subs)
                router->off(id);
        }

        subs.clear();
    }

    void LifeScene::execute()
    {
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

        // Who he is
        subs.push_back(router->on("character.name", [this, ecs](const ElementType& v) {
            if (auto name = piece<MarkedLabel>("name"))
                name->setText(ecs, v.toString());

            if (auto title = piece<Label>("title"))
                title->setText(ecs, "The Chronicle of " + v.toString());
        }));

        subs.push_back(router->on("character.profession", [this, ecs](const ElementType& v) {
            if (auto label = piece<Label>("profession"))
                label->setText(ecs, v.toString());
        }));

        subs.push_back(router->on("character.origin", [this, ecs](const ElementType& v) {
            if (auto label = piece<Label>("origin"))
                label->setText(ecs, v.toString());
        }));

        // His parts: character.parts.<p>, and .projected, .threshold, .note
        subs.push_back(router->onPrefix("character.parts.", [this, ecs](const std::string& path, const ElementType& v) {
            std::string key, field;
            splitPath(path.substr(std::string("character.parts.").size()), {"projected", "threshold", "note"}, key, field);

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
        }));

        // His skills: skills.<id>
        subs.push_back(router->onPrefix("skills.", [this, ecs](const std::string& path, const ElementType& v) {
            if (auto skills = piece<ResourceLedger>("skills"))
                skills->setValue(ecs, path.substr(std::string("skills.").size()), v.toString());
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
                row.each = textOf(a.fields, "each");
                row.months = intOf(a.fields, "months");
                row.state = ActivityState::Running;

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
                if (auto row = running->row(ecs, save.running))
                    row->setPercent(ecs, floatOf(v));
            }
        }));

        subs.push_back(router->on("activity.running.caption", [this, ecs](const ElementType& v) {
            auto running = piece<ActivityList>("running");

            if (running and not save.running.empty())
            {
                if (auto row = running->row(ecs, save.running))
                    row->setCaption(ecs, v.toString());
            }
        }));

        // Each activity's state: activity.<id>.state
        subs.push_back(router->onPrefix("activity.", [this, ecs](const std::string& path, const ElementType& v) {
            std::string id, field;

            if (not splitPath(path.substr(std::string("activity.").size()), {"state"}, id, field) or id == "running")
                return;

            if (auto list = piece<ActivityList>("activities"))
                list->setRowState(ecs, id, stateOf(v.toString()));
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
            {
                ledger->addGroup(ecs, r.group, r.groupLabel);

                auto stat = save.stats.find(r.id);

                LedgerRowSpec row;
                row.id = r.id;
                row.glyph = r.glyph;
                row.name = r.name;
                row.value = stat != save.stats.end() ? std::to_string(stat->second) : r.value;
                row.rate = r.rate;
                row.tone = static_cast<LedgerTone>(r.tone);
                row.muted = r.muted;

                ledger->addRow(ecs, r.group, row, r.groupLabel);
            }
        }

        if (auto skills = piece<ResourceLedger>("skills"))
        {
            skills->clear(ecs);

            for (const auto& s : save.skills)
            {
                skills->addGroup(ecs, s.group, s.groupLabel);   // Unlabelled: no heading

                LedgerRowSpec row;
                row.id = s.id;
                row.glyph = s.glyph;
                row.name = s.name;
                row.value = std::to_string(save.stats[s.id]);

                skills->addRow(ecs, s.group, row, s.groupLabel);
            }
        }

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

        // The activities, grouped as the table orders them
        if (rules.activities(save.character(), activities))
        {
            std::vector<ActivityGroup> groups;

            for (const auto& a : activities)
            {
                const std::string group = textOf(a.fields, "group");

                if (groups.empty() or groups.back().label != group)
                    groups.push_back({group, {}});

                ActivityRowSpec row;
                row.id = textOf(a.fields, "id");
                row.name = textOf(a.fields, "name");
                row.glyph = textOf(a.fields, "glyph");
                row.rank = textOf(a.fields, "rank");
                row.each = textOf(a.fields, "each");
                row.months = intOf(a.fields, "months");
                row.state = boolOf(a.fields, "locked") ? ActivityState::Locked : ActivityState::Idle;

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
                list->setRows(ecs, groups);
        }
        else
        {
            for (const auto& e : rules.errors)
                LOG_ERROR(DOM, e);
        }

        if (auto running = piece<ActivityList>("running"))
            running->setRows(ecs, {});

        // The window meters the page holds, found once so life.age reaches them
        for (const auto& w : windows)
            named("window." + textOf(w, "id"));
    }

    // ---- publish: the save and the scripts to paths ------------------------------------------------------

    void LifeScene::publish()
    {
        publishCharacter();
        publishRules();
    }

    void LifeScene::publishCharacter()
    {
        setFact("life.age", save.age);

        setFact("character.name", save.name);
        setFact("character.profession", save.profession);
        setFact("character.origin", save.origin);

        for (const auto& p : save.parts)
            setFact("character.parts." + p, save.stats[p]);

        for (const auto& s : save.skills)
            setFact("skills." + s.id, save.stats[s.id]);

        for (const auto& r : save.resources)
        {
            auto stat = save.stats.find(r.id);

            setFact("resources." + r.id + ".value", stat != save.stats.end() ? std::to_string(stat->second) : r.value);
            setFact("resources." + r.id + ".rate", r.rate);
            setFact("resources." + r.id + ".muted", r.muted);
        }

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
        ElementMap next;
        ElementMap headline;

        if (rules.milestones(save.age, milestones, next, &headline))
        {
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
                        thresholds[textOf(ask, "stat")] = intOf(ask, "needed");
                }
            }

            for (const auto& p : save.parts)
                setFact("character.parts." + p + ".threshold", thresholds.count(p) ? thresholds[p] : 0);
        }

        RecordList windows;

        if (rules.windows(save.age, save.character(), windows))
        {
            for (const auto& w : windows)
            {
                setFact("window." + textOf(w, "id") + ".state", textOf(w, "state"));
                setFact("window." + textOf(w, "id") + ".note", textOf(w, "note"));
            }
        }

        if (rules.activities(save.character(), activities))
        {
            for (const auto& a : activities)
            {
                const std::string id = textOf(a.fields, "id");
                const std::string state = id == save.running ? "running" : boolOf(a.fields, "locked") ? "locked" : "idle";

                setFact("activity." + id + ".state", state);
            }
        }

        for (const auto& e : rules.errors)
            LOG_ERROR(DOM, e);
    }

    // ---- events in ----------------------------------------------------------------------------------------

    void LifeScene::onSelect(const ActivitySelectedEvent& event)
    {
        if (event.list != ActivitiesList)
            return;

        // Nothing chosen: no ghosts on the parts
        if (event.id.empty())
        {
            for (const auto& p : save.parts)
                setFact("character.parts." + p + ".projected", -1);

            return;
        }

        RuleForecast forecast;

        if (not rules.forecast(save.age, save.character(), event.id, 0, forecast))
            return;

        // The ghost of each part the chosen activity would raise
        for (const auto& p : save.parts)
        {
            auto it = forecast.atTerm.find(p);
            const int atTerm = it == forecast.atTerm.end() ? save.stats[p] : intOf(it->second);

            setFact("character.parts." + p + ".projected", atTerm > save.stats[p] ? atTerm : -1);
        }
    }

    void LifeScene::onConfirm(const ActivityActivatedEvent& event)
    {
        if (event.list != ActivitiesList)
            return;

        if (not save.running.empty())
        {
            appendLog({save.age, "Already at work: " + activityName(save.running), LogKind::Note, "", ""});
            return;
        }

        save.running = event.id;
        save.monthsIn = 0;

        if (auto list = piece<ActivityList>("activities"))
            list->select(ecsRef, "");

        for (const auto& p : save.parts)
            setFact("character.parts." + p + ".projected", -1);

        setFact("activity." + event.id + ".state", std::string("running"));

        publishRunning();
    }

    void LifeScene::onTab(const TabSelectedEvent& event)
    {
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

        if (not save.running.empty())
        {
            ++save.monthsIn;

            RuleForecast forecast;

            if (rules.forecast(save.age, save.character(), save.running, save.monthsIn, forecast) and save.monthsIn >= forecast.months)
            {
                // At term: the forecast's numbers become the character's, its lines the log's
                for (const auto& [key, value] : forecast.atTerm)
                    save.stats[key] = intOf(value);

                for (const auto& e : forecast.entries)
                    save.log.push_back({save.age, textOf(e, "text"), kindOf(textOf(e, "kind")), textOf(e, "figure"), textOf(e, "glyph")});

                save.running.clear();
                save.monthsIn = 0;
            }
        }

        publish();
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

        rebuild();
        publish();
    }
}
