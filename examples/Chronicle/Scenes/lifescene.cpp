#include "lifescene.h"

#ifdef __EMSCRIPTEN__
#include <SDL2/SDL.h>
#elif __linux__
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
#include "ECS/callable.h"
#include "Input/sdlevents.h"
#include "2D/position.h"
#include "2D/simple2dobject.h"
#include "Core/analytics.h"
#include "UI/prefab.h"
#include "UI/sizer.h"
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
#include "UI/button.h"
#include "UI/gloss.h"
#include "Core/motion.h"

using namespace pg;

namespace chronicle
{
    namespace
    {
        constexpr const char * const DOM = "Chronicle.Life";

        constexpr const char * const ActivitiesList = "life.activities";

        // The page's frame (life.yaml): where the columns start, and the chrome of a panel with a
        // heading (16 + the 51 head block + 16). The columns' own measures are the steps below
        constexpr float ColumnsTop = 70.0f;        // Under a head of two lines
        constexpr float StackGap = 16.0f;          // The compact page's, between two panels
        constexpr float PanelChrome = 83.0f;
        constexpr float PlainChrome = 32.0f;       // A panel without a heading: 16 + 16
        constexpr float TabsRow = 56.0f;           // The chapters at the head of the choice (44) and the body's gap (12)
        constexpr float MinList = 200.0f;
        constexpr float MinLog = 120.0f;           // The compact page's log
        constexpr float DateRoom = 300.0f;         // The world's date at the top right, and air before it
        constexpr float EachRoom = 320.0f;         // The inner width of "At work now" from which its row says the line under its time
        constexpr float TallyRoom = 420.0f;        // And from which it says its tally too
        constexpr float ClockPanel = 160.0f;       // The clock and his life, until the panel has measured itself
        constexpr float AlertMs = 900.0f;          // How long a part stays in red after a month took from it
        constexpr float RunningPanel = 164.0f;     // "At work now" holding a running row
        constexpr float SkipButton = 48.0f;        // "At work now" idle: the button to pass a month (36) and the body's gap (12)
        constexpr const char * const SkipTag = "life.skip";
        constexpr const char * const BeginTag = "life.begin";
        constexpr const char * const AgainTag = "life.again";

        // What the ending's veil calls when the mouse enters or leaves it: nothing
        struct EndingNoOp {};

        constexpr float LogFootnote = 19.0f;       // 4 + a caption line
        constexpr const char * const TabsTag = "life.tabs";

        // The three columns are the page at every size they can be: as the window shrinks the two
        // fixed columns narrow and everything stands closer, a step at a time, the roomiest step the
        // window holds. life.yaml is drawn at the first step; the scene writes the others into the
        // page before it is built (shapeColumns), so a step is a page built again, as the compact one is.
        struct ColumnWidths
        {
            float left;                // The left column
            float right;               // The right column
            float margin;              // From the window's left and right edges
            float gap;                 // Between two columns
            float minMiddle;           // The least the choice takes: a tile is 148 and the panel's chrome 32
            bool logFootnote;          // The line under the log: it takes two lines in the narrowest column, and goes

            constexpr float least() const { return 2.0f * margin + left + right + 2.0f * gap + minMiddle; }
        };

        // The height is the right column's: the years, the work at hand with a row in it, and a log of
        // its least size. The left column has no least height: it grows with what he holds and has
        // learned, and scrolls when the window is too short for it
        struct ColumnHeights
        {
            float stack;               // Between two panels of a column
            float minLog;              // The least the log's well takes
            float margin;              // Under the columns

            constexpr float least() const { return ColumnsTop + ClockPanel + stack + RunningPanel + stack + PanelChrome + LogFootnote + minLog + margin; }
        };

        constexpr ColumnWidths Widths[] = {
            {300.0f, 360.0f, 16.0f, 12.0f, 420.0f, true},    // From 1136: the page as it is drawn
            {248.0f, 296.0f, 10.0f, 8.0f, 336.0f, true},     // From 916: two tiles to a line
            {200.0f, 280.0f, 6.0f, 4.0f, 180.0f, false},     // From 680: one tile to a line
        };

        constexpr ColumnHeights Heights[] = {
            {16.0f, 120.0f, 16.0f},                   // From 664
            {10.0f, 84.0f, 10.0f},                    // From 610
            {4.0f, 56.0f, 6.0f},                      // From 566
        };

        // The roomiest step a size holds, -1 when it holds none: the compact page
        // (life-compact.yaml: the work at hand over the choice, a side column of one panel at a time)
        int widthStepFor(float width)
        {
            for (int i = 0; i < static_cast<int>(std::size(Widths)); ++i)
            {
                if (width >= Widths[i].least())
                    return i;
            }

            return -1;
        }

        int heightStepFor(float height)
        {
            for (int i = 0; i < static_cast<int>(std::size(Heights)); ++i)
            {
                if (height >= Heights[i].least())
                    return i;
            }

            return -1;
        }

        // What happens is shown as it happens (nothing of it under reduced motion but the toast)
        constexpr float QuietMs = 700.0f;          // After a page is filled, what its figures do is not a gain: they are only arriving
        constexpr float GainMs = 1000.0f;          // A "+2" beside the figure that rose: it lifts and goes
        constexpr float GainRise = 0.01f;          // By so many pixels a millisecond: less than a row in all
        constexpr float GainInset = 46.0f;         // Its left edge, from the right edge of what it stands over
        constexpr float GainZ = 150.0f;            // Over the page, under the tooltips
        constexpr float FollowMs = 120.0f;         // Before the log is taken to its new end: the list has to measure the line first
        constexpr float LitMs = 1800.0f;           // The line the log has just written keeps its light that long
        constexpr float ToastMs = 3600.0f;         // A toast: a deed, a milestone, something new to do
        constexpr float ToastZ = 170.0f;
        constexpr float ToastHeight = 36.0f;
        constexpr float ToastPad = 18.0f;          // Left and right of its text
        constexpr float ToastStep = 44.0f;         // One toast over another
        constexpr float ToastBottom = 26.0f;       // The lowest one, from the window's bottom
        const std::string Arrow = " \xE2\x86\x92 ";  // U+2192 between spaces: what a figure would become

        constexpr float LeftScroll = 4.0f;         // The left column's thumb, past the panels' right edge
        constexpr float LeftScrollZ = 60.0f;       // Over the panels and what they hold
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
            return widthStepFor(width) < 0 or heightStepFor(height) < 0;
        }

        // A node of the page by its name, the ones a layout holds as well
        // The whole number a figure starts with ("46", "46 -> 52" while a choice is previewed), -1 for none
        int figureOf(const std::string& text)
        {
            size_t digits = 0;

            while (digits < text.size() and text[digits] >= '0' and text[digits] <= '9')
                ++digits;

            if (digits == 0 or digits > 9)
                return -1;

            return std::atoi(text.substr(0, digits).c_str());
        }

        NodeSpec* childNamed(NodeSpec& page, const std::string& name)
        {
            for (auto& child : page.children)
            {
                if (child.name == name)
                    return &child;

                if (child.kind.rfind("Layout:", 0) == 0)
                {
                    if (NodeSpec* inner = childNamed(child, name))
                        return inner;
                }
            }

            return nullptr;
        }

        void setMargin(NodeSpec* node, AnchorType side, float margin)
        {
            if (not node)
                return;

            for (auto& anchor : node->anchors)
            {
                if (anchor.side == side)
                    anchor.margin = margin;
            }
        }

        void setProp(NodeSpec* node, const std::string& key, float value)
        {
            if (node)
                node->props[key] = value;
        }

        // The three columns' page at a step: the fixed columns' widths, the margins and the gaps,
        // written over the ones life.yaml is drawn with. What stands in a panel takes its width
        // from it. The choice's width and the heights are the fit's, on every resize.
        void shapeColumns(NodeSpec& page, const ColumnWidths& widths, const ColumnHeights& heights)
        {
            for (const char* name : {"title", "about"})
                setProp(childNamed(page, name), "x", widths.margin);

            for (const char* name : {"date", "dateNote"})
                setMargin(childNamed(page, name), AnchorType::Right, widths.margin);

            // The left column and its two panels, the thumb's lane past them
            setProp(childNamed(page, "left"), "x", widths.margin);
            setProp(childNamed(page, "left"), "width", widths.left + LeftScroll);
            setProp(childNamed(page, "left"), "spacing", heights.stack);
            setProp(childNamed(page, "holds"), "width", widths.left);
            setProp(childNamed(page, "learned"), "width", widths.left);

            setProp(childNamed(page, "may"), "x", widths.margin + widths.left + widths.gap);

            setProp(childNamed(page, "clockPanel"), "width", widths.right);
            setMargin(childNamed(page, "clockPanel"), AnchorType::Right, widths.margin);

            for (const char* name : {"working", "happened"})
            {
                setProp(childNamed(page, name), "width", widths.right);
                setMargin(childNamed(page, name), AnchorType::Top, heights.stack);
            }

            if (not widths.logFootnote)
            {
                if (NodeSpec* happened = childNamed(page, "happened"))
                {
                    if (NodeSpec* log = childNamed(*happened, "log"))
                        log->props["footnote"] = std::string("");
                }
            }
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

        // In a file a piece stands in its panel's body inside a wrap, and the wrap is what the
        // layout holds: to take the piece out of the stack it is the wrap that is hidden
        EntityRef wrapIn(EntityRef body, EntityRef piece)
        {
            if (body.empty() or not body->has<VerticalLayout>())
                return piece;

            for (auto& child : body->get<VerticalLayout>()->entities)
            {
                if (child.id == piece.id)
                    return child;

                if (not child->has<Prefab>())
                    continue;

                auto prefab = child->get<Prefab>();

                if (prefab->namedChildrenIds.count("MainEntity") > 0 and prefab->getEntity("MainEntity").id == piece.id)
                    return child;
            }

            return piece;
        }

        // A running row is built with the percent and caption already published: a row is not in
        // its list until the next frame, so the paths' own handlers would not find it
        void runningProgress(EntitySystem* ecs, ActivityRowSpec& row)
        {
            auto facts = ecs->getSystem<WorldFacts>();

            if (not facts)
                return;

            row.percent = facts->hasFact("activity.running.percent") ? floatOf(facts->factMap["activity.running.percent"]) : 0.0f;
            row.caption = facts->getFact<std::string>("activity.running.caption", "");
            row.glideTo = facts->hasFact("activity.running.toward") ? floatOf(facts->factMap["activity.running.toward"]) : 0.0f;
            row.glideMs = facts->hasFact("activity.running.glideMs") ? floatOf(facts->factMap["activity.running.glideMs"]) : 0.0f;
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

        if (not compactPage)
            shapeColumns(*spec, Widths[widthStep], Heights[heightStep]);

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

        // The left column's thumb: a faint bar at its right edge, sized to what is in view and hidden
        // while everything fits. The layout places it, and dragging it scrolls the column.
        if (EntityRef left = named("left"); not compactPage and not left.empty() and left->has<VerticalLayout>() and page->has<Prefab>())
        {
            auto thumb = makeUiSimple2DShape(ecsRef, Shape2D::Square, LeftScroll, 1.0f);

            thumb.get<PositionComponent>()->setZ(LeftScrollZ);
            thumb.get<PositionComponent>()->setVisibility(false);
            ecsRef->attach<ThemeComponent>(thumb.entity, "log.scroll");
            page->get<Prefab>()->addToPrefab(thumb.entity, "leftScroll");

            left->get<VerticalLayout>()->setVerticalScrollBar(thumb.entity);
        }

        return true;
    }

    void LifeScene::init()
    {
        float width, height;
        windowSize(ecsRef, width, height);

        widthStep = std::max(0, widthStepFor(width));
        heightStep = std::max(0, heightStepFor(height));

        if (not buildPage(needsCompact(width, height)))
            return;

        listenToEvent<ActivitySelectedEvent>([this](const ActivitySelectedEvent& e) { onSelect(e); });
        listenToEvent<ActivityHoveredEvent>([this](const ActivityHoveredEvent& e) { onHover(e); });
        listenToEvent<ActivityActivatedEvent>([this](const ActivityActivatedEvent& e) { onConfirm(e); });
        listenToEvent<TabSelectedEvent>([this](const TabSelectedEvent& e) { onTab(e); });

        // At nothing, a month passes when he says so; at work the months run on their own. The
        // ending's one button begins the next life
        listenToEvent<ButtonActivatedEvent>([this](const ButtonActivatedEvent& e) {
            if (e.tag == AgainTag)
                beginAgain();
            else if (e.tag == SkipTag and save.running.empty())
                onMonth();
            else if (e.tag == BeginTag and not chosen.empty())
                onConfirm(ActivityActivatedEvent{ActivitiesList, chosen});
        });

        // A deed reached: taken in execute(), never while the facts are being handed around
        listenToStandardEvent(AchievementUnlockEventName, [this](const StandardEvent& e) {
            auto name = e.values.find("name");

            if (name != e.values.end())
                reached.push_back(name->second.toString());
        });

        // The month loop: one month every opt.monthMs while it runs
        listenToEvent<TickEvent>([this](const TickEvent& e) {
            // What lasts a moment runs its course whether the months run or not
            if (quiet > 0.0f)
                quiet -= e.tick;

            runPassing(e.tick);

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
            // Work only: at nothing a month passes by the button, never on its own. The month
            // under way keeps what it ran: the rule goes on from where it stopped
            case SDL_SCANCODE_SPACE:
                if (not save.running.empty() and not ended)
                    pause(not paused);
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

            // From an ending the next life says how the last one ended, as its button does
            case SDL_SCANCODE_N:
                if (ended)
                    beginAgain();
                else
                {
                    newLife();
                    autoSave();
                }
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
        else if (opt.noSave)
            save = firstLife();
        else if (not save.load(opt.savePath))
            save = opt.freshWithoutSave ? freshLife() : firstLife();

        rules.done = save.terms();
        rules.world = save.world;

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

        tellAnalytics();

        // A save written at an ending opens on it
        if (not death.empty())
            endLife();
    }

    void LifeScene::fit(float width, float height)
    {
        if (page.empty())
            return;

        windowWidth = width;
        windowHeight = height;
        runningShown = not save.running.empty();

        // Across a breakpoint the page is built again, sized, then its rows and every path again:
        // the other file for the compact page, the same at another step for the three columns
        const bool wantCompact = needsCompact(width, height);
        const int wantedWidth = widthStepFor(width);
        const int wantedHeight = heightStepFor(height);

        const bool swap = wantCompact != compact or (not wantCompact and (wantedWidth != widthStep or wantedHeight != heightStep));

        if (swap and not wantCompact)
        {
            widthStep = wantedWidth;
            heightStep = wantedHeight;
        }

        if (swap and not buildPage(wantCompact))
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

        fitEnding(width, height);

        if (swap)
        {
            rebuild();
            publish();
        }
        else if (not save.running.empty() and workRoom() != rowRoom)
        {
            // The panel of the work at hand is wide enough for its row to say more, or no longer is
            if (auto running = piece<ActivityList>("running"))
                running->setRows(ecsRef, {});

            showRunning(save.running);
        }
    }

    void LifeScene::fitEnding(float width, float height)
    {
        if (ending.empty() or not ending->has<Prefab>())
            return;

        // The veil is the window's size, from its corner: nothing of the page shows past it
        if (EntityRef veil = ending->get<Prefab>()->findEntity("veil"); not veil.empty())
        {
            auto pos = veil->get<PositionComponent>();

            pos->setX(0.0f);
            pos->setY(0.0f);
            pos->setWidth(width);
            pos->setHeight(height);
        }
    }

    void LifeScene::followLog()
    {
        // Once the list has measured its lines and its own height: sooner, the end is not yet where it will be
        passing.push_back({FollowMs, nullptr, [this]() {
            if (auto log = piece<EventLog>("log"))
                log->scrollToEnd(ecsRef);
        }});
    }

    void LifeScene::runPassing(float ms)
    {
        // By index: what ends may start something else
        for (size_t i = 0; i < passing.size();)
        {
            passing[i].left -= ms;

            if (passing[i].each)
                passing[i].each(ms);

            if (passing[i].left > 0.0f)
            {
                ++i;

                continue;
            }

            auto done = passing[i].done;

            passing.erase(passing.begin() + i);

            if (done)
                done();
        }
    }

    void LifeScene::endPassing()
    {
        std::vector<Passing> ending;
        ending.swap(passing);

        for (const auto& p : ending)
        {
            if (p.done)
                p.done();
        }
    }

    int LifeScene::roseOf(const std::string& key, const std::string& text)
    {
        // Against the figure it last had here, not the one on its row: a preview may be written there
        const int is = figureOf(text);
        const auto last = figures.find(key);
        const int was = last == figures.end() ? -1 : last->second;

        figures[key] = is;

        return was >= 0 and is > was ? is - was : 0;
    }

    void LifeScene::showGain(EntityRef over, int amount)
    {
        if (amount <= 0 or quiet > 0.0f or Motion::reduced() or over.empty() or not over->has<PositionComponent>())
            return;

        auto where = over->get<PositionComponent>();

        // Scrolled out of its column, or out of the window: nothing to stand over
        if (where->y < ColumnsTop or where->y > windowHeight)
            return;

        LabelSpec spec;
        spec.style = "figure-sm";
        spec.color = "status-gain";
        spec.text = "+" + std::to_string(amount);
        spec.z = static_cast<int>(GainZ);

        Label gain = makeLabel(ecsRef, spec);

        auto pos = gain.entity.get<PositionComponent>();
        pos->setX(where->x + where->width - GainInset);
        pos->setY(where->y);

        const _unique_id id = gain.entity.id;

        // It lifts off the figure and goes
        passing.push_back({GainMs,
            [this, id](float ms) {
                if (auto entity = ecsRef->getEntity(id))
                {
                    auto p = entity->get<PositionComponent>();

                    p->setY(p->y - ms * GainRise);
                }
            },
            [this, id]() {
                if (ecsRef->getEntity(id))
                    ecsRef->removeEntity(id);
            }});
    }

    void LifeScene::toast(const std::string& text)
    {
        auto window = ecsRef->getEntity("__MainWindow");

        if (text.empty() or quiet > 0.0f or not window)
            return;

        // A dark slip at the foot of the window, its text in the paper's colour: one over another
        // when two come together
        LabelSpec spec;
        spec.style = "control";
        spec.color = "folio";
        spec.text = text;
        spec.z = static_cast<int>(ToastZ) + 1;

        Label label = makeLabel(ecsRef, spec);

        auto anchor = label.entity.get<UiAnchor>();
        anchor->setHorizontalCenter(PosAnchor{window->id, AnchorType::HorizontalCenter});
        anchor->setBottomAnchor(PosAnchor{window->id, AnchorType::Bottom});
        anchor->setBottomMargin(ToastBottom + static_cast<float>(toasts) * ToastStep);

        auto ground = makeUiSimple2DShape(ecsRef, Shape2D::Square, 1.0f, ToastHeight);
        ground.get<PositionComponent>()->setZ(ToastZ);
        ecsRef->attach<ThemeComponent>(ground.entity, "toast.ground");

        auto under = ground.get<UiAnchor>();
        under->setHorizontalCenter(PosAnchor{label.entity.id, AnchorType::HorizontalCenter});
        under->setVerticalCenter(PosAnchor{label.entity.id, AnchorType::VerticalCenter});
        under->setWidthConstrain(PosConstrain{label.entity.id, AnchorType::Width, PosOpType::Add, 2.0f * ToastPad});

        ++toasts;

        const _unique_id textId = label.entity.id;
        const _unique_id groundId = ground.entity.id;

        passing.push_back({ToastMs, nullptr, [this, textId, groundId]() {
            for (auto id : {textId, groundId})
            {
                if (ecsRef->getEntity(id))
                    ecsRef->removeEntity(id);
            }

            --toasts;
        }});
    }

    void LifeScene::showWorkButtons()
    {
        auto working = piece<Panel>("working");

        if (not working)
            return;

        // At nothing one button stands in the running row's place: the one that begins what is
        // chosen in the list, or the one that passes a month when nothing is
        const bool idle = save.running.empty();
        const RuleActivity* pick = chosen.empty() ? nullptr : activityOf(chosen);
        const bool begins = idle and pick and not ended;

        auto show = [this, &working](const char* name, bool shown) {
            if (EntityRef button = named(name); not button.empty())
            {
                wrapIn(working->body, button)->get<PositionComponent>()->setVisibility(shown);
                button->get<PositionComponent>()->setVisibility(shown);
            }

            if (auto button = piece<Button>(name))
                button->setDisabled(ecsRef, not shown);
        };

        show("skip", idle and not begins);
        show("begin", begins);

        if (not begins)
            return;

        if (auto begin = piece<Button>("begin"))
        {
            // What takes no time is done on the spot, and says so
            const int months = intOf(pick->fields, "months");

            begin->setLabel(ecsRef, months > 0 ? "Begin" : "Do it now");
            begin->setMonths(ecsRef, months > 0 ? months : -1);
        }
    }

    int LifeScene::workRoom()
    {
        auto working = piece<Panel>("working");

        if (not working)
            return 0;

        if (working->innerWidth() >= TallyRoom)
            return 2;

        return working->innerWidth() >= EachRoom ? 1 : 0;
    }

    void LifeScene::showRunning(const std::string& id)
    {
        auto running = piece<ActivityList>("running");

        if (not running)
            return;

        for (const auto& a : activities)
        {
            if (textOf(a.fields, "id") != id)
                continue;

            const int room = workRoom();

            ActivityRowSpec row;
            row.id = id;
            row.name = textOf(a.fields, "name");
            row.glyph = textOf(a.fields, "glyph");
            row.rank = textOf(a.fields, "rank");
            row.months = intOf(a.fields, "months");
            row.state = ActivityState::Running;
            row.glossKey = "activity/" + id;

            // The name first. The line under the time and the tally are said where the panel is
            // wide enough to leave the name its room, and by the gloss everywhere
            if (room >= 1)
                row.each = textOf(a.fields, "each");

            if (room >= 2)
                row.count = textOf(a.fields, "tally");

            runningProgress(ecsRef, row);

            running->setRows(ecsRef, {{"", {row}}});

            rowRoom = room;
        }
    }

    float LifeScene::workingHeight() const
    {
        return save.running.empty() ? PanelChrome + SkipButton : RunningPanel;
    }

    void LifeScene::fitFull(float width, float height)
    {
        EntitySystem* ecs = ecsRef;

        const ColumnWidths& widths = Widths[widthStep];
        const ColumnHeights& heights = Heights[heightStep];

        // What he is, beside the date: it ends where the date's own room begins
        if (auto about = piece<Label>("about"))
            about->setWidth(ecs, std::max(0.0f, width - 2.0f * widths.margin - DateRoom));

        // The left column runs to the bottom, and scrolls what does not fit
        if (EntityRef left = named("left"); not left.empty())
            left->get<PositionComponent>()->setHeight(std::max(1.0f, height - ColumnsTop - heights.margin));

        // The middle column takes what the two fixed ones leave; the choice fills it to the bottom
        const float middle = std::max(widths.minMiddle, width - 2.0f * widths.margin - widths.left - widths.right - 2.0f * widths.gap);
        // The choice has no heading: the chapters stand at its head, over the list
        const float listHeight = std::max(MinList, height - ColumnsTop - PlainChrome - TabsRow - heights.margin);

        if (auto may = piece<Panel>("may"))
        {
            may->setWidth(ecs, middle);

            if (auto tabs = piece<Tabs>("tabs"))
                tabs->setWidth(ecs, may->innerWidth());

            if (auto list = piece<ActivityList>("activities"))
                list->setSize(ecs, may->innerWidth(), listHeight);
        }

        // The log takes the height left under the years and the work at hand, down to the bottom:
        // his parts stand with what he has learned, in the left column
        float clock = ClockPanel;

        if (EntityRef clockPanel = named("clockPanel"); not clockPanel.empty())
            clock = std::max(clock, clockPanel->get<PositionComponent>()->height);

        const float logHeight = std::max(heights.minLog, height - ColumnsTop - clock - heights.stack - workingHeight() - heights.stack - PanelChrome - LogFootnote - heights.margin);

        if (auto log = piece<EventLog>("log"))
        {
            // A reader at the end stays at the end when the well changes its height: "At work now"
            // takes room from it as a work begins, and the last lines would be cut off
            const bool atEnd = log->atEnd(ecs);

            log->setHeight(ecs, logHeight);

            if (atEnd)
                followLog();
        }
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
        {
            const bool atEnd = log->atEnd(ecs);

            log->setHeight(ecs, std::max(MinLog, height - SideTop - SideChrome - LogFootnote - CompactMargin));

            if (atEnd)
                followLog();
        }

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

        // The ending leaves with the scene, as a scene element
        ending = EntityRef{};

        // And what was passing ends now
        endPassing();
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

        // The world's date, at the top right of the page: its year, then the month and its season
        subs.push_back(router->on("life.headline.date", [this, ecs](const ElementType& v) {
            if (auto label = piece<Label>("date"))
                label->setText(ecs, v.toString());
        }));

        subs.push_back(router->on("life.headline.dateNote", [this, ecs](const ElementType& v) {
            if (auto label = piece<Label>("dateNote"))
                label->setText(ecs, v.toString());
        }));

        // His age on the clock, to the month: the figure is the clock's, what follows it the
        // rules'. The next milestone is not said there: its tick is on the track
        subs.push_back(router->on("life.headline.ageUnit", [this, ecs](const ElementType& v) {
            if (auto clock = piece<LifeClock>("clock"))
                clock->setUnit(ecs, v.toString());
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
            {
                const int before = line->spec.value;

                line->setValue(ecs, intOf(v));
                showGain(line->root, line->spec.value - before);
            }
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
            {
                fillSkills();
            }
            else if (shown)
            {
                skills->setValue(ecs, id, v.toString());
                showGain(skills->row(id)->line, roseOf("skills." + id, v.toString()));
            }
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
            {
                ledger->setValue(ecs, id, v.toString());
                showGain(ledger->row(id)->line, roseOf("resources." + id, v.toString()));
            }
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

            showRunning(id);
        }));

        subs.push_back(router->on("activity.running.months", [this, ecs](const ElementType& v) {
            if (auto clock = piece<LifeClock>("clock"))
                clock->setRunning(ecs, floatOf(v));
        }));

        // The running row is in both lists: "At work now" and the choice, where it stands in its place
        subs.push_back(router->on("activity.running.percent", [this, ecs](const ElementType& v) {
            if (save.running.empty())
                return;

            for (const char* name : {"running", "activities"})
            {
                auto list = piece<ActivityList>(name);

                if (not list)
                    continue;

                // Republished unchanged within the month: the glide goes on
                if (auto row = list->find(ecs, save.running); row and row->spec.percent != floatOf(v))
                    row->setPercent(ecs, floatOf(v));
            }
        }));

        // The month under way: the rule moves to activity.running.toward as the month runs out,
        // and stays where it is while the months stop
        subs.push_back(router->on("activity.running.glideMs", [this, ecs](const ElementType& v) {
            auto facts = ecs->getSystem<WorldFacts>();

            if (save.running.empty() or not facts)
                return;

            const float toward = facts->hasFact("activity.running.toward") ? floatOf(facts->factMap["activity.running.toward"]) : 0.0f;

            for (const char* name : {"running", "activities"})
            {
                auto list = piece<ActivityList>(name);

                if (not list)
                    continue;

                if (auto row = list->find(ecs, save.running))
                    row->setGlide(ecs, toward, floatOf(v));
            }
        }));

        // Whether the months run, said in the head of "At work now"; stopped, with the key that
        // goes on
        subs.push_back(router->on("activity.running.pace", [this, ecs](const ElementType& v) {
            auto working = piece<Panel>("working");

            if (not working)
                return;

            const std::string pace = v.toString();

            if (pace == "paused")
            {
                // The key that goes on, where the head has the room to say it
                working->setAside(ecs, workRoom() == 0 ? "PAUSED" : "PAUSED \xC2\xB7 SPACE");
                working->setAsideColor(ecs, "status-loss");
            }
            else
            {
                working->setAside(ecs, pace == "running" ? "RUNNING" : "IDLE");
                working->setAsideColor(ecs, "ink-muted");
            }

            showWorkButtons();
        }));

        // What the coming month would do, said before it does: a line under his life
        subs.push_back(router->on("life.warning", [this, ecs](const ElementType& v) {
            if (auto life = piece<StatLine>("vit"))
                life->setNote(ecs, v.toString());
        }));

        subs.push_back(router->on("activity.running.caption", [this, ecs](const ElementType& v) {
            if (save.running.empty())
                return;

            for (const char* name : {"running", "activities"})
            {
                auto list = piece<ActivityList>(name);

                if (not list)
                    continue;

                if (auto row = list->find(ecs, save.running))
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

            if (not splitPath(rest, {"state", "count", "until"}, id, field) or id == "running")
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
                if (field == "count")
                {
                    row->setCount(ecs, v.toString());
                }
                else
                {
                    // When it closes, and whether that is near: activity.<id>.urgent came with it
                    auto facts = ecs->getSystem<WorldFacts>();
                    const std::string urgent = "activity." + id + ".urgent";

                    row->setUntil(ecs, v.toString(), facts and facts->getFact<bool>(urgent, false));
                }
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

            const bool wrote = log->size() < n;
            const bool follows = wrote and log->atEnd(ecs);

            for (size_t i = log->size(); i < n; ++i)
                log->append(ecs, save.log[i]);

            // A reader at the end is taken to the new end, once the list has measured what it was
            // given: the layout's own following falls a line short on a long log
            if (follows)
                followLog();

            // The line just written is lit for a moment: the eye finds what is new
            if (wrote and quiet <= 0.0f and not Motion::reduced())
            {
                if (EntityRef light = log->lightLast(ecs); not light.empty())
                {
                    const _unique_id id = light.id;

                    passing.push_back({LitMs, nullptr, [this, id]() {
                        if (auto log = piece<EventLog>("log"))
                            log->dim(ecsRef, id);
                    }});
                }
            }
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
        if (rules.milestones(save.age, milestones, next))
        {
            std::vector<ClockMilestone> ticks;

            for (const auto& m : milestones)
                ticks.push_back({static_cast<float>(intOf(m.fields, "age")), textOf(m.fields, "label")});

            if (auto clock = piece<LifeClock>("clock"))
                clock->setMilestones(ecs, ticks);
        }

        fillWindows();
        fillActivities();

        if (auto running = piece<ActivityList>("running"))
            running->setRows(ecs, {});

        // A list just filled has nothing chosen, and its figures are arriving, not rising
        chosen.clear();
        hovered.clear();
        figures.clear();
        quiet = QuietMs;
        showWorkButtons();
    }

    void LifeScene::fillWindows()
    {
        EntitySystem* ecs = ecsRef;
        RecordList windows;

        if (not rules.windows(save.age, save.character(), windows))
        {
            for (const auto& e : rules.errors)
                LOG_ERROR(DOM, e);

            return;
        }

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

        // The window meters the page holds, found once so life.age reaches them
        for (const auto& w : windows)
            named("window." + textOf(w, "id"));
    }

    void LifeScene::fillActivities()
    {
        if (not rules.activities(save.age, save.character(), activities))
        {
            for (const auto& e : rules.errors)
                LOG_ERROR(DOM, e);

            return;
        }

        // What the list did not have a moment ago is new: said by a toast, and on its tile until
        // it is looked at. The first list of a page is all he has always had
        std::vector<std::string> ids;

        for (const auto& a : activities)
        {
            if (boolOf(a.fields, "listed") and not boolOf(a.fields, "locked"))
                ids.push_back(textOf(a.fields, "id"));
        }

        if (not known.empty())
        {
            for (const auto& id : ids)
            {
                if (std::find(known.begin(), known.end(), id) != known.end())
                    continue;

                fresh.push_back(id);
                toast("New: " + activityName(id));
            }
        }

        known = ids;

        // Grouped as the table orders them; what the rules do not list (spent, too late, another
        // path's) has no row
        std::vector<ActivityGroup> groups;

        listedRows.clear();

        for (const auto& a : activities)
        {
            // The work he is at keeps its place, even when it could no longer be begun
            if (not boolOf(a.fields, "listed") and textOf(a.fields, "id") != save.running)
                continue;

            listedRows += textOf(a.fields, "id") + ":" + std::to_string(a.requires.size()) + ";";

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
            row.state = row.id == save.running ? ActivityState::Running : boolOf(a.fields, "locked") ? ActivityState::Locked : ActivityState::Idle;
            row.glossKey = "activity/" + row.id;

            if (row.state == ActivityState::Running)
            {
                runningProgress(ecsRef, row);
            }
            else
            {
                // When it closes, once that is near: nothing to say of the one he is at
                row.until = textOf(a.fields, "until");
                row.urgent = boolOf(a.fields, "urgent");
            }

            for (const auto& g : a.gains)
                row.gains.push_back({textOf(g, "label"), intOf(g, "amount")});

            // What it takes when it begins, beside what it brings
            for (const auto& c : a.costs)
                row.gains.push_back({textOf(c, "label"), -intOf(c, "amount")});

            for (const auto& r : a.requires)
            {
                Requirement req;
                req.label = textOf(r, "label");
                req.current = intOf(r, "current");
                req.needed = intOf(r, "needed");
                row.requirements.push_back(req);
            }

            if (row.state != ActivityState::Running)
                row.until = tileNote(a, row.urgent);

            groups.back().rows.push_back(row);
        }

        if (auto list = piece<ActivityList>("activities"))
            list->setRows(ecsRef, groups);
    }

    std::string LifeScene::asksOf(const RuleActivity& activity) const
    {
        if (not boolOf(activity.fields, "locked"))
            return "";

        // The first thing it still asks of him: the whole list is its gloss's
        for (const auto& r : activity.requires)
        {
            if (intOf(r, "current") < intOf(r, "needed"))
                return "NEEDS " + upper(textOf(r, "label")) + " " + std::to_string(intOf(r, "needed"));
        }

        return "";
    }

    std::string LifeScene::tileNote(const RuleActivity& activity, bool& urgent) const
    {
        urgent = false;

        // What he cannot do yet says what it still asks of him; what has just come says it is new
        if (const std::string asks = asksOf(activity); not asks.empty())
            return asks;

        if (std::find(fresh.begin(), fresh.end(), textOf(activity.fields, "id")) != fresh.end())
            return "NEW";

        // When it closes, once that is near
        urgent = boolOf(activity.fields, "urgent");

        return textOf(activity.fields, "until");
    }

    void LifeScene::refreshHoldings()
    {
        RuleMonth month;

        if (rules.month(save.age, save.character(), boarded(), month))
        {
            holdings = month.rows;
            holdingGlosses = month.glosses;
            death = month.death;

            // The coming month, as things stand: what it would take from, and what the rules say of it
            threat = month.hurt;
            warning = month.warning;

            caps.clear();

            for (const auto& cap : month.caps)
                caps[textOf(cap, "stat")] = intOf(cap, "most");
            return;
        }

        for (const auto& e : rules.errors)
            LOG_ERROR(DOM, e);
    }

    bool LifeScene::titled(const std::string& id) const
    {
        for (const auto& h : holdings)
        {
            if (textOf(h, "id") == id)
                return boolOf(h, "title");
        }

        return false;
    }

    const RuleActivity* LifeScene::activityOf(const std::string& id) const
    {
        for (const auto& a : activities)
        {
            if (textOf(a.fields, "id") == id)
                return &a;
        }

        return nullptr;
    }

    bool LifeScene::boarded() const
    {
        if (save.running.empty())
            return false;

        const RuleActivity* at = activityOf(save.running);

        return at and boolOf(at->fields, "board");
    }

    void LifeScene::endLife()
    {
        // The months stop, and the page stays as the life left it under its ending: the next
        // life starts when the player says so
        sinceMonth = 0.0f;
        clearAlert();
        paused = true;

        publishAll();

        // The line the new life opens with, and the age the last one ended at as the head wrote it
        endedLine = death;
        endedAge.clear();

        if (auto facts = ecsRef->getSystem<WorldFacts>())
            endedAge = facts->getFact<std::string>("life.headline.ageText", "");

        LOG_INFO(DOM, "The life ended at " << endedAge << ": " << endedLine);

        ended = true;

        // Without its ending the life would have no way on: the next one begins at once
        if (not showEnding())
            beginAgain();
    }

    bool LifeScene::showEnding()
    {
        closeEnding();

        // The deeds he reached, by the names they are told with
        std::vector<std::string> told;

        for (const auto& id : save.achieved)
        {
            for (const auto& deed : deeds)
            {
                if (textOf(deed.fields, "id") == id)
                    told.push_back(textOf(deed.fields, "name"));
            }
        }

        if (not rules.epitaph(save.age, save.character(), told, epitaph))
        {
            for (const auto& e : rules.errors)
                LOG_ERROR(DOM, e);

            return false;
        }

        std::vector<std::string> errors;
        PrefabLoadOptions options;
        options.errors = &errors;

        auto spec = loadNodeSpec(ecsRef, opt.endingFile, options);

        if (not spec)
        {
            LOG_ERROR(DOM, "Could not load the ending " << opt.endingFile);
            return false;
        }

        for (const auto& e : errors)
            LOG_ERROR(DOM, opt.endingFile << ": " << e);

        ending = buildTree(ecsRef, *spec);

        if (ending.empty() or not ending->has<Prefab>())
        {
            LOG_ERROR(DOM, "Could not build the ending " << opt.endingFile);
            ending = EntityRef{};

            return false;
        }

        // It leaves with the scene, like the page
        ecsRef->attach<SceneElement>(ending);

        auto prefab = ending->get<Prefab>();

        auto write = [this, &prefab](const std::string& name, const std::string& text) {
            EntityRef ent = prefab->findEntity(name);

            if (not ent.empty() and ent->has<Label>())
                ent->get<Label>()->setText(ecsRef, text);
        };

        write("endName", save.name);
        write("endCause", epitaph.cause);
        write("endStory", epitaph.text);
        write("endTally", epitaph.tally);

        fitEnding(windowWidth, windowHeight);

        // The veil takes the mouse: what is under it is neither hovered nor clicked
        if (EntityRef veil = prefab->findEntity("veil"); not veil.empty())
        {
            if (not veil->has<MouseEnterComponent>())
                ecsRef->attach<MouseEnterComponent>(veil, makeCallable<EndingNoOp>(EndingNoOp{}));

            if (not veil->has<MouseLeaveComponent>())
                ecsRef->attach<MouseLeaveComponent>(veil, makeCallable<EndingNoOp>(EndingNoOp{}));
        }

        return true;
    }

    void LifeScene::closeEnding()
    {
        if (not ending.empty())
            ecsRef->removeEntity(ending);

        ending = EntityRef{};
    }

    void LifeScene::beginAgain()
    {
        if (not ended)
            return;

        const std::string line = endedLine;
        const std::string age = endedAge;

        newLife();

        appendLog({save.age, line, LogKind::Loss, age, ""});

        autoSave();
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

            toast("Deed: " + textOf(deed.fields, "name"));

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
        row.value = titled(resource.id) ? std::string() : stat != save.stats.end() ? std::to_string(stat->second) : resource.value;
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

        if (endangered)
            return;

        // The log says why the months stopped, and which key goes on
        if (not paused)
        {
            std::string parts;

            for (const auto& stat : stats)
            {
                const auto line = piece<StatLine>(stat);
                const std::string name = line ? line->spec.label : stat;

                parts += parts.empty() ? name : " and " + name;
            }

            appendLog({save.age, parts + " is failing: the months stop. SPACE goes on", LogKind::Note, "", ""});
        }

        sinceMonth = 0.0f;
        pause(true);
    }

    void LifeScene::clearAlert()
    {
        // A part the coming month would take from again keeps its red
        for (const auto& stat : alerted)
            setFact("character.parts." + stat + ".alert", std::find(threat.begin(), threat.end(), stat) != threat.end());

        alerted.clear();
        alertLeft = 0.0f;
    }

    void LifeScene::publishThreat()
    {
        setFact("life.warning", warning);

        // In red before the month takes from it, not only after; back to ink when it no longer would
        for (const auto& stat : threatShown)
        {
            const bool still = std::find(threat.begin(), threat.end(), stat) != threat.end();
            const bool flashing = std::find(alerted.begin(), alerted.end(), stat) != alerted.end();

            if (not still and not flashing)
                setFact("character.parts." + stat + ".alert", false);
        }

        for (const auto& stat : threat)
            setFact("character.parts." + stat + ".alert", true);

        threatShown = threat;
    }

    // ---- publish: the save and the scripts to paths ------------------------------------------------------

    void LifeScene::publish()
    {
        refreshHoldings();
        publishAll();
    }

    void LifeScene::publishAll()
    {
        publishThreat();
        glossHoldings();
        publishCharacter();
        publishRules();

        // The ghosts: of the activity chosen if one is, of the one the mouse is on, else of what mends
        preview();
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

            // A title is had or not: its row shows no figure
            setFact("resources." + r.id + ".value", titled(r.id) ? std::string() : stat != save.stats.end() ? std::to_string(stat->second) : r.value);
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
            publishPace();
            return;
        }

        RuleForecast forecast;

        // Where the rule stands, and where the month under way takes it
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
        setFact("activity.running.toward", forecast.toward);

        publishPace();
    }

    void LifeScene::publishPace()
    {
        // At rest between two works is not a pause: only stopped work, or the months running on
        // their own, are said
        if (save.running.empty())
        {
            setFact("activity.running.pace", std::string(paused ? "idle" : "running"));
            return;
        }

        setFact("activity.running.pace", std::string(paused ? "paused" : "running"));

        // The rule reaches next month's figure as the month ends, and holds while the months stop
        setFact("activity.running.glideMs", paused ? 0.0f : std::max(0.0f, opt.monthMs - sinceMonth));
    }

    void LifeScene::pause(bool on)
    {
        paused = on;

        publishPace();
    }

    void LifeScene::publishRules()
    {
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

        if (rules.activities(save.age, save.character(), activities))
        {
            std::string listed;

            for (const auto& a : activities)
            {
                const std::string id = textOf(a.fields, "id");
                const std::string state = id == save.running ? "running" : boolOf(a.fields, "locked") ? "locked" : "idle";

                setFact("done." + id, intOf(a.fields, "done"));

                // An activity the rules do not list has no row to tell, but the one he is at
                if (not boolOf(a.fields, "listed") and id != save.running)
                    continue;

                listed += id + ":" + std::to_string(a.requires.size()) + ";";

                setFact("activity." + id + ".state", state);
                setFact("activity." + id + ".count", textOf(a.fields, "tally"));

                // When it closes, month by month, or what it still asks of him: nothing to say of the
                // one he is at
                bool urgent = false;
                const std::string note = tileNote(a, urgent);

                setFact("activity." + id + ".urgent", urgent);
                setFact("activity." + id + ".until", id == save.running ? std::string() : note);

                for (size_t i = 0; i < a.requires.size(); ++i)
                    setFact("activity." + id + ".requirement." + std::to_string(i), intOf(a.requires[i], "current"));
            }

            // A door opened or closed with the months, or a row asks more or less than its row
            // shows: the rows again
            if (listed != listedRows)
                fillActivities();

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

        chosen = event.id;

        // Looked at: no longer new
        if (auto it = std::find(fresh.begin(), fresh.end(), chosen); it != fresh.end())
        {
            fresh.erase(it);

            if (const RuleActivity* seen = activityOf(chosen))
            {
                bool urgent = false;
                const std::string note = tileNote(*seen, urgent);

                setFact("activity." + chosen + ".urgent", urgent);
                setFact("activity." + chosen + ".until", note);
            }
        }

        showWorkButtons();
        preview();
    }

    void LifeScene::onHover(const ActivityHoveredEvent& event)
    {
        if (event.list != ActivitiesList)
            return;

        hovered = event.id;

        preview();
    }

    void LifeScene::preview()
    {
        // What is chosen says more than what the mouse is on. Nothing of either: no ghosts on the
        // parts but what mends, and what he holds as it is
        const std::string id = chosen.empty() ? hovered : chosen;

        RuleForecast forecast;

        if (id.empty() or ended or not rules.forecast(save.age, save.character(), id, 0, forecast))
        {
            publishProjected(nullptr);
            previewHoldings(nullptr);
            glossParts(nullptr, "");

            return;
        }

        // The ghost of each part it would raise, and what it would leave of what he holds
        publishProjected(&forecast);
        previewHoldings(&forecast);
        glossParts(&forecast, activityName(id));
    }

    void LifeScene::previewHoldings(const RuleForecast* forecast)
    {
        auto ledger = piece<ResourceLedger>("ledger");
        auto facts = ecsRef->getSystem<WorldFacts>();

        if (not ledger or not facts)
            return;

        for (const auto& r : save.resources)
        {
            if (not ledger->row(r.id))
                continue;

            // The figure as the page has it, and after it what the activity would make of it
            std::string value = facts->getFact<std::string>("resources." + r.id + ".value", "");

            auto now = save.stats.find(r.id);

            if (forecast and now != save.stats.end() and value == std::to_string(now->second))
            {
                auto after = forecast->atTerm.find(r.id);

                if (after != forecast->atTerm.end() and intOf(after->second) != now->second)
                    value += Arrow + std::to_string(intOf(after->second));
            }

            // Only to write what it would become, or to take that away: the figure itself is the page's to write
            const std::string& shown = ledger->row(r.id)->spec.value;
            const bool previewed = shown.find(Arrow) != std::string::npos;
            const bool previews = value.find(Arrow) != std::string::npos;

            if ((previews or previewed) and shown != value)
                ledger->setValue(ecsRef, r.id, value);
        }
    }

    void LifeScene::onConfirm(const ActivityActivatedEvent& event)
    {
        // Over: the keyboard may still reach a row under the ending
        if (event.list != ActivitiesList or ended)
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

        // The list refuses a locked row, a key or a script may not
        const RuleActivity* chosen = activityOf(event.id);

        if (not chosen or boolOf(chosen->fields, "locked") or not boolOf(chosen->fields, "listed"))
            return;

        save.running = event.id;
        save.monthsIn = 0;

        // What it costs is taken as it begins
        takeStats(forecast.atStart);

        if (auto list = piece<ActivityList>("activities"))
            list->select(ecsRef, "");

        setFact("activity." + event.id + ".state", std::string("running"));

        // The months run on their own while he works, from the start of a month
        paused = false;
        sinceMonth = 0.0f;

        publish();

        autoSave();
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
            if (boolOf(a.fields, "locked") or not boolOf(a.fields, "listed"))
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
            tellAnalytics("life_end");
            endLife();
            return;
        }

        for (const auto& r : save.holdEarned(holdings))
            addToLedger(r);

        // Rebuilt only when it left or changed: the selection stays for the next time otherwise
        bool changed = not rules.activities(save.age, save.character(), activities);

        for (const auto& a : activities)
        {
            if (textOf(a.fields, "id") != id)
                continue;

            std::vector<int> now;

            for (const auto& g : a.gains)
                now.push_back(intOf(g, "amount"));

            changed = changed or not boolOf(a.fields, "listed") or textOf(a.fields, "rank") != rank or now != amounts;
        }

        if (changed)
            fillActivities();

        publishAll();

        autoSave();
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
        // Over: no month passes until the next life begins
        if (ended)
            return;

        // The milestones already passed, to write the line of one the month passes
        std::vector<std::string> passed;
        std::vector<RuleMilestone> before;
        ElementMap ahead;

        if (rules.milestones(save.age, before, ahead))
        {
            for (const auto& m : before)
            {
                if (boolOf(m.fields, "passed"))
                    passed.push_back(textOf(m.fields, "id"));
            }
        }

        save.age += 1.0f / 12.0f;

        // The world's calendar moves with him, and goes on when he is gone
        ++save.world;
        rules.world = save.world;

        // What he holds works or wastes, whatever he is doing; work that feeds him spares his
        // rations
        RuleMonth month;

        if (rules.month(save.age, save.character(), boarded(), month))
        {
            takeStats(month.after);
            writeEntries(month.entries);
        }
        else
        {
            for (const auto& e : rules.errors)
                LOG_ERROR(DOM, e);
        }

        // The character as the month leaves him: with nothing left to live on, this life is lost
        // before any term pays him, and a new one begins
        refreshHoldings();

        if (not death.empty())
        {
            tellAnalytics("life_end");
            endLife();
            return;
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

                // The way into a path, done: he is one of it, and its asks stand on his parts
                if (const RuleActivity* at = activityOf(save.running); at and boolOf(at->fields, "enters"))
                    save.aim = textOf(at->fields, "path");

                ++save.done[save.running];
                rules.done = save.terms();

                save.running.clear();
                save.monthsIn = 0;

                // The work is done: the months wait for the next choice
                paused = true;
                sinceMonth = 0.0f;

                // One more term done: a row may have left, another may have changed, a path's
                // doors may be his
                fillActivities();
                fillWindows();
            }

            refreshHoldings();
        }

        // What he holds for the first time gets its row
        for (const auto& r : save.holdEarned(holdings))
            addToLedger(r);

        publishAll();

        // A milestone passed: the line it writes. The last one ends nothing: old age begins, and
        // the life goes on until it has nothing left to live on
        for (const auto& m : milestones)
        {
            const std::string id = textOf(m.fields, "id");

            if (boolOf(m.fields, "passed") and std::find(passed.begin(), passed.end(), id) == passed.end() and not textOf(m.fields, "entry").empty())
            {
                appendLog({save.age, textOf(m.fields, "entry"), LogKind::Milestone, "", ""});
                toast(textOf(m.fields, "entry"));
            }
        }

        // The month took from what the life hangs on: he is in danger, and the page says so
        if (not month.hurt.empty())
            alert(month.hurt);

        endangered = not month.hurt.empty();

        // A life that ended above is not written: loaded again, its last month ends it again
        autoSave();
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
            gloss.inlineValues = true;
            gloss.title = line ? line->spec.label : p;
            gloss.rows.push_back({"Now", std::to_string(save.stats[p])});

            if (auto cap = caps.find(p); cap != caps.end())
                gloss.rows.push_back({"At most", std::to_string(cap->second), "muted"});

            // What the next milestone asks of it: met in the gain's colour, short in the loss's
            for (const auto& ask : nextAsks)
            {
                if (textOf(ask, "stat") == p)
                    gloss.rows.push_back({textOf(next, "label") + " asks", std::to_string(intOf(ask, "needed")), save.stats[p] < intOf(ask, "needed") ? "loss" : "gain"});
            }

            if (forecast)
            {
                auto it = forecast->atTerm.find(p);

                if (it != forecast->atTerm.end() and intOf(it->second) != save.stats[p])
                    gloss.rows.push_back({activity + " brings it to", std::to_string(intOf(it->second)), intOf(it->second) < save.stats[p] ? "loss" : "gain"});
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
            gloss.inlineValues = true;
            // The head: its name, how often it was done beside it, the kind of thing it is under it.
            // The row itself says none of this: the gloss carries everything but the name, the time
            // and the closing
            gloss.title = textOf(a.fields, "name");
            gloss.text = textOf(a.fields, "group");

            // The number of times alone, over how many it can be done when that is limited (0: as
            // often as he likes)
            gloss.aside = std::to_string(intOf(a.fields, "done"));

            if (intOf(a.fields, "uses") > 0)
                gloss.aside += "/" + std::to_string(intOf(a.fields, "uses"));

            // A rule under the head, then what it is in time: how long, and who feeds him. When it
            // closes is the tile's to say
            gloss.rows.push_back({"", "", "", true});
            gloss.rows.push_back({"Time", intOf(a.fields, "months") > 0 ? std::to_string(intOf(a.fields, "months")) + " mo" : std::string("At once"), "time"});

            if (intOf(a.fields, "months") > 0)
                gloss.rows.push_back({"Meals", boolOf(a.fields, "board") ? std::string("Provided") : std::string("His own rations"), boolOf(a.fields, "board") ? "gain" : ""});

            // In sections, a rule between two, each figure in the colour of the way it goes and
            // under the stat's full name: the section says whether it is brought or taken
            if (not a.gains.empty())
                gloss.rows.push_back({"IT BRINGS", "", "", true});

            for (const auto& g : a.gains)
                gloss.rows.push_back({textOf(g, "name"), signedText(intOf(g, "amount")), intOf(g, "amount") < 0 ? "loss" : "gain"});

            if (not a.costs.empty())
                gloss.rows.push_back({"IT TAKES", "", "", true});

            for (const auto& c : a.costs)
                gloss.rows.push_back({textOf(c, "name"), signedText(-intOf(c, "amount")), "loss"});

            if (not a.requires.empty())
                gloss.rows.push_back({"IT ASKS", "", "", true});

            // What he has of it against what it asks: met in the gain's colour, short in the loss's
            for (const auto& r : a.requires)
                gloss.rows.push_back({textOf(r, "label"), std::to_string(intOf(r, "current")) + " / " + std::to_string(intOf(r, "needed")), intOf(r, "current") < intOf(r, "needed") ? "loss" : "gain"});

            // A footnote only for the work he is at: what a locked one still asks is in red above
            if (textOf(a.fields, "id") == save.running)
                gloss.footnote = "AT WORK NOW";

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
            gloss.inlineValues = true;
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

                // The rules say which way each figure goes
                for (const auto& row : g.rows)
                    gloss.rows.push_back({textOf(row, "label"), textOf(row, "value"), textOf(row, "tone")});
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
            gloss.inlineValues = true;
            gloss.title = textOf(w, "name");
            gloss.text = textOf(w, "note");
            gloss.rows.push_back({"Opens at", std::to_string(intOf(w, "from")), "time"});
            gloss.rows.push_back({"Closes at", std::to_string(intOf(w, "to")), "time"});

            // None left to fit is the loss's colour
            if (textOf(w, "state") != "closed")
                gloss.rows.push_back({"Attempts that fit", std::to_string(intOf(w, "attempts")), intOf(w, "attempts") > 0 ? "" : "loss"});

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

    void LifeScene::tellAnalytics(const std::string& event)
    {
        auto analytics = ecsRef->getSystem<Analytics>();

        if (not analytics)
            return;

        analytics->note(save.digest());

        if (not event.empty())
            analytics->send(event);
    }

    void LifeScene::autoSave()
    {
        // Told whether or not the life is written: what is played is played
        tellAnalytics();

        if (opt.noSave)
            return;

        if (not save.save(opt.savePath))
            LOG_ERROR(DOM, "Could not save to " << opt.savePath);
    }

    void LifeScene::newLife()
    {
        const LifeSave last = save;

        // A life begins at rest, whatever the last one was doing, and the ending of the last one
        // leaves with it
        paused = true;
        sinceMonth = 0.0f;

        closeEnding();
        ended = false;

        save = freshLife();

        // The world did not begin again with him: its date runs on from the last life's
        save.world = last.world;
        rules.world = save.world;

        // What the last life held and this one has no stat for reads 0, not the last life's
        // figure: a deed must not be reached on what is gone
        for (const auto& [key, value] : last.stats)
        {
            if (save.stats.count(key) == 0)
                setFact("stat." + key, 0);
        }
        endangered = false;

        rules.done = save.terms();

        known.clear();
        fresh.clear();

        rebuild();
        publish();
        registerDeeds();
    }
}
