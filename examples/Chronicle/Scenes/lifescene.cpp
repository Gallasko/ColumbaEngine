#include "lifescene.h"

#ifdef __EMSCRIPTEN__
#include <SDL2/SDL.h>
#elif __linux__
#include <SDL2/SDL.h>
#elif _WIN32
#include <SDL.h>
#endif

#include <algorithm>
#include <cmath>
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
#include "UI/tabs.h"
#include "UI/placetile.h"
#include "Core/motion.h"

using namespace pg;

namespace chronicle
{
    namespace
    {
        constexpr const char * const DOM = "Chronicle.Life";

        constexpr const char * const ActivitiesList = "life.activities";
        constexpr const char * const MarketList = "town.market";   // The market's rows, on the Town page
        constexpr const char * const WorksList = "town.works";     // And the works that raise a place, under the places

        // The page's pieces the town is made of (life.yaml, life-compact.yaml): the choice and the
        // Town page take each other's place in the middle column
        constexpr const char * const ChoiceName = "activities";
        constexpr const char * const TownName = "town";
        constexpr const char * const PlacesName = "places";
        constexpr const char * const MarketName = "marketRows";
        constexpr const char * const WorksName = "townWorks";
        constexpr const char * const RaisedName = "raised";
        constexpr const char * const GiftTag = "life.gift.";   // Followed by the place's id: the ending's buttons
        constexpr float GiftGap = 6.0f;            // Between two of them
        constexpr int MonthsAYear = 12;
        const std::string PlaceGloss = "place/";
        const std::string SaidFact = "said.";                    // Followed by a guide step's id: 1 once the world has been told it

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
        constexpr float GuideLine = 32.0f;         // And, over it, the line that says what to do in a life that has done nothing yet (20) and the gap (12)
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

        // The guide of a first life: a hand beside what a step points at, and a shade over the rest
        // of the page. Both stand in the Overlay band (100-129), over the panels and under the
        // tooltips; neither takes the mouse, so everything under the shade can still be pressed
        constexpr float HandGap = 4.0f;            // Between the hand and what it points at
        constexpr float HandSway = 4.0f;           // It moves that far either way,
        constexpr float HandSwayMs = 800.0f;       // there and back in that long
        constexpr float HandZ = 125.0f;
        constexpr float VeilZ = 110.0f;
        constexpr float VeilPad = 6.0f;            // Left lit around what is pointed at
        constexpr float Turn = 6.2831853f;         // A whole turn, in radians
        constexpr const char * const HandGlyph = "manicule";
        constexpr const char * const HandColor = "vermilion";
        constexpr const char * const VeilElement = "guide.veil";
        const std::string TilePoint = "tile:";                  // What a step that points at a tile begins with, before the activity's id

        // What a step says stands on a leaf of its own under what it points at (over it when the
        // window ends there), with the two buttons of the guide: the one that leaves it for good,
        // and the one that passes a step said for a time
        constexpr float NoteWidth = 360.0f;
        constexpr float NotePad = 12.0f;           // Around its words and its buttons
        constexpr float NoteGap = 10.0f;           // Between what is pointed at and the leaf, and between its words and its buttons
        constexpr float NoteButton = 36.0f;        // A button's height
        constexpr float NoteMargin = 8.0f;         // The least it keeps from the window's edges
        constexpr float NoteZ = 112.0f;            // Its edge; its ground over it, then its words and its buttons
        constexpr const char * const NoteEdge = "guide.note.edge";
        constexpr const char * const NoteGround = "guide.note.ground";
        constexpr const char * const GuideSkipTag = "life.guide.skip";
        constexpr const char * const GuideNextTag = "life.guide.next";

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

        std::string lower(std::string s)
        {
            for (char& c : s)
            {
                if (c >= 'A' and c <= 'Z')
                    c = static_cast<char>(c - 'A' + 'a');
            }

            return s;
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

        // A place's tile clicked: it is the one lit, and the work that raises it is chosen with it,
        // under the places; clicked again it is let go of
        listenToEvent<PlaceSelectedEvent>([this](const PlaceSelectedEvent& e) { onPlace(e); });

        // At nothing, a month passes when he says so; at work the months run on their own. The
        // ending's one button begins the next life
        listenToEvent<ButtonActivatedEvent>([this](const ButtonActivatedEvent& e) {
            if (e.tag == AgainTag)
                beginAgain();
            else if (e.tag == GuideSkipTag)
                skipGuide();
            else if (e.tag == GuideNextTag)
                nextStep();
            else if (e.tag.rfind(GiftTag, 0) == 0)
                chooseGift(e.tag.substr(std::string(GiftTag).size()));
            else if (e.tag == SkipTag and save.running.empty())
            {
                if (not ended)
                    ++save.skips;

                onMonth();
            }
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

            // The guide's hand moves whether the months run or not, and a step said for a time
            // runs it out
            handMs = std::fmod(handMs + e.tick, HandSwayMs);

            if (step.active and step.until.empty())
                step.left -= e.tick;

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

        // A life made here is the first of its world, unless a later one was asked for
        if (opt.fresh)
        {
            const bool first = opt.lives <= 1;

            save = freshLife(first);
            save.lives = std::max(1, opt.lives);
            save.guide = first ? std::max(0, opt.guide) : 0;
        }
        else if (opt.noSave)
            save = firstLife();
        else if (not save.load(opt.savePath))
            save = opt.freshWithoutSave ? freshLife(true) : firstLife();

        rules.done = save.terms();
        rules.world = save.world;
        rules.lives = save.lives;
        rules.running = save.running;

        // The guide's length, before the page is filled: a first life's guide line waits for its step
        readDeeds();

        // The page is arriving: what a save was already short of is not news to say
        quiet = QuietMs;

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

        // A life opened past its first frame (a save, the mockup's) has lore that already holds.
        // Before the facts are written: what the world was told is one of them
        takeAsRead();
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

        guideFitted = guideRoom();

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

        // Hung on what it stands over, not put where that stood: the month that brings the gain may
        // move the line too (a new row over it), and the figure goes with it
        auto anchor = gain.entity.get<UiAnchor>();
        anchor->setTopAnchor(PosAnchor{over.id, AnchorType::Top});
        anchor->setLeftAnchor(PosAnchor{over.id, AnchorType::Right});
        anchor->setLeftMargin(-GainInset);

        const _unique_id id = gain.entity.id;

        // It lifts off the figure and goes
        passing.push_back({GainMs,
            [this, id, risen = 0.0f](float ms) mutable {
                if (auto entity = ecsRef->getEntity(id))
                {
                    risen += ms * GainRise;

                    entity->get<UiAnchor>()->setTopMargin(-risen);
                }
            },
            [this, id]() {
                if (ecsRef->getEntity(id))
                    ecsRef->removeEntity(id);
            }});
    }

    void LifeScene::toast(const std::string& text, float ms)
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

        passing.push_back({ms > 0.0f ? ms : ToastMs, nullptr, [this, textId, groundId]() {
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
        // chosen in the list, or the one that passes a month when nothing is. A life that has done
        // nothing yet is led to its first work: no month to pass before one is begun, and a line
        // that says how
        const bool idle = save.running.empty();
        const RuleActivity* pick = chosen.empty() ? nullptr : activityOf(chosen);
        const bool begins = idle and pick and not ended;
        const bool leads = led() and not firstWork().empty();

        auto show = [this, &working](const char* name, bool shown) {
            if (EntityRef button = named(name); not button.empty())
            {
                wrapIn(working->body, button)->get<PositionComponent>()->setVisibility(shown);
                button->get<PositionComponent>()->setVisibility(shown);
            }

            if (auto button = piece<Button>(name))
                button->setDisabled(ecsRef, not shown);
        };

        // A first life has its guide to say it, on a leaf of its own
        show("guide", leads and not guiding());
        show("skip", idle and not begins and not leads);
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
        return (save.running.empty() ? PanelChrome + SkipButton : RunningPanel) + guideRoom();
    }

    bool LifeScene::guiding() const
    {
        return save.lives == 1 and save.guide < guideSteps;
    }

    bool LifeScene::guideSaid() const
    {
        return led() and not guiding();
    }

    float LifeScene::guideRoom() const
    {
        return guideSaid() ? GuideLine : 0.0f;
    }

    bool LifeScene::led() const
    {
        return not ended and save.running.empty() and save.done.empty();
    }

    std::string LifeScene::firstWork() const
    {
        // As the table orders them: the first he may begin that takes months
        for (const auto& a : activities)
        {
            if (boolOf(a.fields, "listed") and not boolOf(a.fields, "locked") and intOf(a.fields, "months") > 0)
                return textOf(a.fields, "id");
        }

        return "";
    }

    void LifeScene::lead()
    {
        leadDue = false;

        if (not led())
            return;

        // A first life is led by its guide: its hand shows him the task, then the button that
        // begins it, and he presses both himself
        if (guiding())
            return;

        // Chosen for him: Begin is the button the page opens on. Its tile is lit once the list
        // holds its rows (execute)
        chosen = firstWork();
        leadDue = not chosen.empty();
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

            fitTown(may->innerWidth(), listHeight);
        }

        // The log takes the height left under the years and the work at hand, down to the bottom:
        // his parts stand with what he has learned, in the left column
        float clock = ClockPanel;

        if (EntityRef clockPanel = named("clockPanel"); not clockPanel.empty())
            clock = std::max(clock, clockPanel->get<PositionComponent>()->height);

        clockFitted = clock;

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

            fitTown(may->innerWidth(), listHeight);
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

        // The ending leaves with the scene, as a scene element; so do the guide's hand and its shade
        ending = EntityRef{};

        step = GuideStep{};
        stepsDue.clear();
        hand = EntityRef{};
        handOn = 0;
        veil.clear();
        veiled = false;
        shaded.clear();
        note = EntityRef{};
        noteEdge = EntityRef{};
        noteGround = EntityRef{};
        noteWords = Label{};
        noteSkip = Button{};
        noteNext = Button{};
        noteShown = false;
        nextShown = false;

        // And what was passing ends now
        endPassing();
    }

    void LifeScene::execute()
    {
        // A page just built stacked its buttons before its layouts held them: the one that is not
        // shown still had its room. Shown again now that they do
        if (buttonsDue)
        {
            buttonsDue = false;
            showWorkButtons();
            showTown(townShown);
        }

        // The work chosen for a life that has done nothing: its tile is lit once its row is in the
        // list, unless he has chosen another meanwhile
        if (leadDue)
        {
            auto list = piece<ActivityList>("activities");

            if (not list or chosen.empty() or not led())
                leadDue = false;
            else if (list->find(ecsRef, chosen))
            {
                leadDue = false;
                list->select(ecsRef, chosen);
            }
        }

        // The years' panel measured itself after the log was fitted under it: fitted again
        if (not compact and not page.empty())
        {
            if (EntityRef clockPanel = named("clockPanel"); not clockPanel.empty() and std::abs(std::max(ClockPanel, clockPanel->get<PositionComponent>()->height) - clockFitted) > 0.5f)
                fit(windowWidth, windowHeight);
        }

        if (not reached.empty())
        {
            std::vector<std::string> ids;
            ids.swap(reached);

            for (const auto& id : ids)
                reachDeed(id);

            publish();

            // What was reached is kept: opened again, the chronicle does not reach it twice
            if (not ended)
                autoSave();
        }

        runGuide();
    }

    // ---- the guide of a first life -------------------------------------------------------------------

    ElementMap LifeScene::watched() const
    {
        // The paths publishCharacter and publishRules write for what watches the life, read from
        // the save itself: in the game a fact set is on its way to the WorldFacts for the rest of
        // the pass, and what the scene asks of the life it asks now. An activity never done has no
        // path here, and an ask about it does not hold
        ElementMap facts;

        // His stats, and the town's with them, as the rules read them
        for (const auto& [key, value] : save.character())
            facts["stat." + key] = value;

        for (const auto& [id, count] : save.done)
            facts["done." + id] = ElementType{count};

        facts["life.age"] = ElementType{save.age};
        facts["life.terms"] = ElementType{save.termsDone()};
        facts["life.working"] = ElementType{save.running.empty() ? 0 : 1};
        facts["life.lives"] = ElementType{save.lives};
        facts["life.guide"] = ElementType{save.guide};
        facts["life.town"] = ElementType{townShown ? 1 : 0};

        for (const auto& id : save.told)
            facts[SaidFact + id] = ElementType{1};

        return facts;
    }

    bool LifeScene::holds(const RecordList& asks) const
    {
        const ElementMap facts = watched();

        for (const auto& ask : asks)
        {
            auto value = ask.find("value");

            if (not FactChecker(textOf(ask, "fact"), value == ask.end() ? ElementType{0} : value->second, equalityOf(textOf(ask, "op"))).check(facts))
                return false;
        }

        return true;
    }

    void LifeScene::reachStep(const RuleAchievement& entry)
    {
        auto hold = entry.fields.find("hold");

        GuideStep reachedStep;
        reachedStep.id = textOf(entry.fields, "id");
        reachedStep.order = entry.order;
        reachedStep.say = textOf(entry.fields, "say");
        reachedStep.point = textOf(entry.fields, "point");
        reachedStep.pointChosen = textOf(entry.fields, "pointChosen");
        reachedStep.sayChosen = textOf(entry.fields, "sayChosen");
        reachedStep.until = entry.until;
        reachedStep.world = entry.world;
        reachedStep.left = hold == entry.fields.end() ? 0.0f : floatOf(hold->second);

        // A step of a guide that is over (skipped the pass it was reached in) is not said, nor one
        // the world has been told already
        if ((reachedStep.order > 0 and not guiding()) or (reachedStep.world and told(reachedStep.id)))
            return;

        // A step outside the sequence is kept as a deed is: said once in a life. One the world
        // is told once is kept when it has been said (endStep), so that a chronicle opened again
        // in the middle of it is told it still
        if (reachedStep.order == 0 and not reachedStep.world)
            save.achieved.push_back(reachedStep.id);

        // Said once the step before it has ended
        stepsDue.push_back(reachedStep);
    }

    void LifeScene::runGuide()
    {
        if (ended or page.empty())
            return;

        // The step being said ends on what it waits for, or with its time
        if (step.active and (step.until.empty() ? step.left <= 0.0f : holds(step.until)))
            endStep();

        // The next one reached takes its place
        if (not step.active and not stepsDue.empty())
        {
            step = stepsDue.front();
            stepsDue.pop_front();
            step.active = true;
        }

        // The line of a life led with no guide came or went: what stands under "At work now" follows
        if (std::abs(guideRoom() - guideFitted) > 0.5f)
            fit(windowWidth, windowHeight);

        // Found again every time: a page built again has other tiles, a list that scrolled other
        // rows in view
        const EntityRef target = step.active ? pointed(stepChosen() ? step.pointChosen : step.point) : EntityRef{};

        pointAt(target);
        sayNote(target);
    }

    bool LifeScene::stepChosen() const
    {
        // A work is begun in two presses: the hand shows the tile until it is the one chosen, then
        // the button that begins it; let go of, the tile has the hand again
        return step.active and not step.pointChosen.empty() and not chosen.empty() and step.point == TilePoint + chosen;
    }

    void LifeScene::endStep()
    {
        // A step of the sequence takes the guide one further; one outside it leaves it where it was
        if (step.order > 0)
        {
            save.guide = step.order;
            setFact("life.guide", save.guide);
        }

        // Said: kept with the world when it is the world's, and the step that waits for it may come
        if (step.world and not told(step.id))
            save.told.push_back(step.id);

        setFact(SaidFact + step.id, 1);

        step = GuideStep{};

        // The last of them said: a life still at nothing is led as any other
        showWorkButtons();

        // Kept: opened again, the guide goes on from here
        autoSave();
    }

    void LifeScene::clearGuide()
    {
        step = GuideStep{};
        stepsDue.clear();

        pointAt(EntityRef{});
        sayNote(EntityRef{});
    }

    void LifeScene::nextStep()
    {
        // Only what is said for a time: a step that waits for him to do something ends when he does
        if (step.active and step.until.empty())
            endStep();
    }

    void LifeScene::skipGuide()
    {
        if (ended or (not guiding() and not step.active))
            return;

        // Where he was when he had enough of it, for the analytics: the step after the last that ended
        if (guiding())
        {
            save.guideSkipped = save.guide;
            ++save.guideSkipped;
        }

        // Past its last step, for good; and the word it keeps for later is taken as said
        save.guide = guideSteps;
        setFact("life.guide", save.guide);

        for (const auto& deed : deeds)
        {
            const std::string id = textOf(deed.fields, "id");

            if (deed.kind != RuleKind::Guide or deed.order != 0)
                continue;

            if (deed.world)
            {
                if (not told(id))
                    save.told.push_back(id);
            }
            else if (std::find(save.achieved.begin(), save.achieved.end(), id) == save.achieved.end())
            {
                save.achieved.push_back(id);
            }
        }

        clearGuide();

        // A life that has done nothing yet is led as any later one is: its first work chosen
        if (chosen.empty())
            lead();

        showWorkButtons();

        autoSave();
    }

    void LifeScene::makeNote()
    {
        // The leaf: an edge a pixel wider than its ground all round, its words, and under them its
        // two buttons. Everything hangs on the root, which the scene sizes and places
        auto root = makeAnchoredPrefab(ecsRef, 0.0f, 0.0f, NoteZ);

        root.get<PositionComponent>()->setWidth(NoteWidth);
        root.get<PositionComponent>()->setVisibility(false);

        note = root.entity;

        // It leaves with the scene, as the page does
        ecsRef->attach<SceneElement>(note);

        auto prefab = root.get<Prefab>();

        auto edge = makeUiSimple2DShape(ecsRef, Shape2D::Square, NoteWidth, 1.0f);

        edge.get<PositionComponent>()->setZ(NoteZ);
        edge.get<UiAnchor>()->setTopAnchor(PosAnchor{note.id, AnchorType::Top});
        edge.get<UiAnchor>()->setLeftAnchor(PosAnchor{note.id, AnchorType::Left});
        ecsRef->attach<ThemeComponent>(edge.entity, NoteEdge);
        prefab->addToPrefab(edge.entity, "edge");

        noteEdge = edge.entity;

        auto ground = makeUiSimple2DShape(ecsRef, Shape2D::Square, NoteWidth - 2.0f, 1.0f);

        ground.get<PositionComponent>()->setZ(NoteZ + 1.0f);
        ground.get<UiAnchor>()->setTopAnchor(PosAnchor{note.id, AnchorType::Top});
        ground.get<UiAnchor>()->setTopMargin(1.0f);
        ground.get<UiAnchor>()->setLeftAnchor(PosAnchor{note.id, AnchorType::Left});
        ground.get<UiAnchor>()->setLeftMargin(1.0f);
        ecsRef->attach<ThemeComponent>(ground.entity, NoteGround);
        prefab->addToPrefab(ground.entity, "ground");

        noteGround = ground.entity;

        // It takes the mouse: what is under the leaf is neither hovered nor clicked through it
        ecsRef->attach<MouseEnterComponent>(noteGround, makeCallable<EndingNoOp>(EndingNoOp{}));
        ecsRef->attach<MouseLeaveComponent>(noteGround, makeCallable<EndingNoOp>(EndingNoOp{}));

        const int z = static_cast<int>(NoteZ) + 4;

        LabelSpec words;
        words.style = "body";
        words.color = "ink";
        words.overflow = Overflow::Wrap;
        words.width = NoteWidth - 2.0f * NotePad;
        words.z = z;

        noteWords = makeLabel(ecsRef, words);

        auto wordsAnchor = noteWords.entity->get<UiAnchor>();
        wordsAnchor->setTopAnchor(PosAnchor{note.id, AnchorType::Top});
        wordsAnchor->setTopMargin(NotePad);
        wordsAnchor->setLeftAnchor(PosAnchor{note.id, AnchorType::Left});
        wordsAnchor->setLeftMargin(NotePad);
        prefab->addToPrefab(noteWords.entity, "words");

        ButtonSpec skip;
        skip.variant = ButtonVariant::Quiet;
        skip.label = "Skip tutorial";
        skip.tag = GuideSkipTag;
        skip.z = z;

        noteSkip = makeButton(ecsRef, skip);

        auto skipAnchor = noteSkip.root->get<UiAnchor>();
        skipAnchor->setBottomAnchor(PosAnchor{note.id, AnchorType::Bottom});
        skipAnchor->setBottomMargin(NotePad);
        skipAnchor->setLeftAnchor(PosAnchor{note.id, AnchorType::Left});
        skipAnchor->setLeftMargin(NotePad);
        prefab->addToPrefab(noteSkip.root, "skip");

        ButtonSpec next;
        next.variant = ButtonVariant::Seal;
        next.label = "Next";
        next.tag = GuideNextTag;
        next.z = z;

        noteNext = makeButton(ecsRef, next);

        auto nextAnchor = noteNext.root->get<UiAnchor>();
        nextAnchor->setBottomAnchor(PosAnchor{note.id, AnchorType::Bottom});
        nextAnchor->setBottomMargin(NotePad);
        nextAnchor->setRightAnchor(PosAnchor{note.id, AnchorType::Right});
        nextAnchor->setRightMargin(NotePad);
        prefab->addToPrefab(noteNext.root, "next");

        noteShown = false;
        nextShown = true;
    }

    void LifeScene::sayNote(EntityRef target)
    {
        if (not step.active)
        {
            // Nothing to say: the leaf goes, and its buttons take no key meanwhile
            if (noteShown)
            {
                note->get<PositionComponent>()->setVisibility(false);
                noteSkip.setDisabled(ecsRef, true);
                noteNext.setDisabled(ecsRef, true);

                noteShown = false;
            }

            return;
        }

        if (note.empty())
            makeNote();

        // The step's sentence, or the one of its second half once its tile is chosen
        const std::string words = stepChosen() and not step.sayChosen.empty() ? step.sayChosen : step.say;
        // Only a step said for a time can be passed: one that waits for him ends when he does it
        const bool timed = step.until.empty();

        auto pos = note->get<PositionComponent>();

        if (not noteShown or noteWords.spec.text != words or nextShown != timed)
        {
            if (noteWords.spec.text != words)
                noteWords.setText(ecsRef, words);

            // As tall as its words, with its buttons under them
            const float height = NotePad + noteWords.entity->get<PositionComponent>()->height + NoteGap + NoteButton + NotePad;

            pos->setHeight(height);
            noteEdge->get<PositionComponent>()->setHeight(height);
            noteGround->get<PositionComponent>()->setHeight(height - 2.0f);

            pos->setVisibility(true);

            noteSkip.setDisabled(ecsRef, false);
            noteNext.setDisabled(ecsRef, not timed);
            noteNext.root->get<PositionComponent>()->setVisibility(timed);

            noteShown = true;
            nextShown = timed;
        }

        // Under what is pointed at, from its left edge; over it when the window ends there; never
        // out of the window. A step that points at nothing is said at the head of the page
        float x = std::max(NoteMargin, (windowWidth - pos->width) / 2.0f);
        float y = ColumnsTop;

        if (not target.empty())
        {
            auto where = target->get<PositionComponent>();

            x = std::max(NoteMargin, std::min(where->x, windowWidth - pos->width - NoteMargin));
            y = where->y + where->height + NoteGap;

            if (y + pos->height > windowHeight - NoteMargin)
                y = where->y - NoteGap - pos->height;

            y = std::max(NoteMargin, y);
        }

        if (std::abs(pos->x - x) > 0.5f or std::abs(pos->y - y) > 0.5f)
        {
            pos->setX(x);
            pos->setY(y);
        }
    }

    EntityRef LifeScene::pointed(const std::string& point) const
    {
        // "tile:carters": a kind of thing on the page and which one; "clock": a thing there is one of
        const size_t colon = point.find(':');
        const std::string kind = point.substr(0, colon);
        const std::string name = colon == std::string::npos ? std::string() : point.substr(colon + 1);

        EntityRef target;

        if (kind == "button")
        {
            if (auto button = piece<Button>(name))
                target = button->root;
        }
        else if (kind == "tile")
        {
            auto list = listHolding(name);

            if (auto row = list ? list->find(ecsRef, name) : nullptr)
                target = row->root;
        }
        else if (kind == "place")
        {
            auto grid = piece<PlaceGrid>(PlacesName);

            if (auto tile = grid ? grid->tile(name) : nullptr)
                target = tile->root;
        }
        else if (kind == "tab")
        {
            // A page's tab, by the page's name in small letters
            if (auto tabs = piece<Tabs>("tabs"))
            {
                for (size_t i = 0; i < tabs->tabs.size() and i < tabs->spec.items.size(); ++i)
                {
                    if (lower(tabs->spec.items[i].label) == name)
                        target = tabs->tabs[i].face;
                }
            }
        }
        else if (kind == "holding")
        {
            if (auto ledger = piece<ResourceLedger>("ledger"))
                target = ledger->rowEntity(name);
        }
        else if (kind == "stat")
        {
            if (auto line = piece<StatLine>(name))
                target = line->root;
        }
        else if (kind == "clock")
        {
            if (auto clock = piece<LifeClock>("clock"))
                target = clock->root;
        }
        else if (kind == "log")
        {
            target = named("happened");
        }

        // What is not drawn is not pointed at: a row not listed yet, a button not shown, a tile
        // scrolled out of its list, a panel the compact page keeps for another tab
        if (target.empty() or not target->has<PositionComponent>() or not target->get<PositionComponent>()->isRenderable())
            return EntityRef{};

        return target;
    }

    void LifeScene::pointAt(EntityRef target)
    {
        if (target.empty())
        {
            if (not hand.empty() and handOn != 0)
            {
                auto anchor = hand->get<UiAnchor>();

                anchor->clearRightAnchor();
                anchor->clearVerticalCenter();

                hand->get<PositionComponent>()->setVisibility(false);
            }

            handOn = 0;

            shadeAround({});

            return;
        }

        if (hand.empty())
        {
            Mark mark = makeMark(ecsRef, {HandGlyph, MarkSize::S24, HandColor, static_cast<int>(HandZ)});

            hand = mark.entity;

            // It leaves with the scene, as the page does
            ecsRef->attach<SceneElement>(hand);
        }

        auto anchor = hand->get<UiAnchor>();
        auto where = target->get<PositionComponent>();

        // Beside its left edge, at mid-height, and hung on it: it goes where its target goes
        if (handOn != target.id)
        {
            anchor->setRightAnchor(PosAnchor{target.id, AnchorType::Left});
            anchor->setVerticalCenter(PosAnchor{target.id, AnchorType::VerticalCenter});

            hand->get<PositionComponent>()->setVisibility(true);

            handOn = target.id;
            handMs = 0.0f;
        }

        // It moves a little, to be seen; and what stands at the window's edge has it over its start
        // rather than out of the window
        float gap = HandGap;

        if (not Motion::reduced())
            gap += HandSway * std::sin(handMs * Turn / HandSwayMs);

        anchor->setRightMargin(std::min(gap, where->x - hand->get<PositionComponent>()->width));

        // The rest of the page under a shade: what is pointed at stays lit
        shadeAround({target});
    }

    void LifeScene::shadeAround(const std::vector<EntityRef>& lit)
    {
        struct Box
        {
            float left, top, right, bottom;
        };

        // What stays lit, a little wider than it is, on whole pixels and inside the window
        std::vector<Box> holes;
        std::string around;

        for (EntityRef entity : lit)
        {
            if (entity.empty() or not entity->has<PositionComponent>())
                continue;

            auto pos = entity->get<PositionComponent>();

            const Box hole{std::max(0.0f, std::floor(pos->x - VeilPad)), std::max(0.0f, std::floor(pos->y - VeilPad)),
                           std::min(windowWidth, std::ceil(pos->x + pos->width + VeilPad)), std::min(windowHeight, std::ceil(pos->y + pos->height + VeilPad))};

            if (hole.right <= hole.left or hole.bottom <= hole.top)
                continue;

            holes.push_back(hole);

            for (float edge : {hole.left, hole.top, hole.right, hole.bottom})
                around += std::to_string(static_cast<int>(edge)) + ",";
        }

        if (not holes.empty())
            around += std::to_string(static_cast<int>(windowWidth)) + "x" + std::to_string(static_cast<int>(windowHeight));

        // Laid again only when what it goes around has moved
        if (around == shaded)
            return;

        shaded = around;

        // The window cut in bands at every top and bottom of what is lit; in each band, a strip of
        // shade between two lit spans. Nothing lit: no shade at all
        std::vector<Box> strips;

        if (not holes.empty())
        {
            std::vector<float> cuts = {0.0f, windowHeight};

            for (const auto& hole : holes)
            {
                cuts.push_back(hole.top);
                cuts.push_back(hole.bottom);
            }

            std::sort(cuts.begin(), cuts.end());

            for (size_t i = 0; i + 1 < cuts.size(); ++i)
            {
                const float top = cuts[i];
                const float bottom = cuts[i + 1];

                if (bottom <= top)
                    continue;

                std::vector<std::pair<float, float>> spans;

                for (const auto& hole : holes)
                {
                    if (hole.top <= top and hole.bottom >= bottom)
                        spans.push_back({hole.left, hole.right});
                }

                std::sort(spans.begin(), spans.end());

                float from = 0.0f;

                for (const auto& span : spans)
                {
                    if (span.first > from)
                        strips.push_back({from, top, span.first, bottom});

                    from = std::max(from, span.second);
                }

                if (from < windowWidth)
                    strips.push_back({from, top, windowWidth, bottom});
            }
        }

        while (veil.size() < strips.size())
        {
            auto strip = makeUiSimple2DShape(ecsRef, Shape2D::Square, 1.0f, 1.0f);

            strip.get<PositionComponent>()->setZ(VeilZ);
            strip.get<PositionComponent>()->setVisibility(false);
            ecsRef->attach<ThemeComponent>(strip.entity, VeilElement);
            ecsRef->attach<SceneElement>(strip.entity);

            veil.push_back(strip.entity);
        }

        for (size_t i = 0; i < veil.size(); ++i)
        {
            auto pos = veil[i]->get<PositionComponent>();

            if (i >= strips.size())
            {
                pos->setVisibility(false);

                continue;
            }

            pos->setX(strips[i].left);
            pos->setY(strips[i].top);
            pos->setWidth(strips[i].right - strips[i].left);
            pos->setHeight(strips[i].bottom - strips[i].top);
            pos->setVisibility(true);
        }

        veiled = not strips.empty();
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
                auto list = listHolding(rest.substr(0, at));
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

            // In the choice, or at the market
            auto list = listHolding(id);

            if (not list)
                return;

            if (field == "state")
            {
                // A row the list was just given is not in its layout until the next pass, and was
                // built in the state it is told here: nothing to tell it, and nothing to log
                if (list->find(ecs, id))
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
        fillTown();

        // The town's tab is not there at all until a life has explored the town, and its page
        // stands where it stood
        if (auto tabs = piece<Tabs>("tabs"); tabs and townTab() >= 0)
            tabs->setShown(ecs, townTab(), save.townKnown);

        showTown(townShown);

        if (auto running = piece<ActivityList>("running"))
            running->setRows(ecs, {});

        // A list just filled has nothing chosen, and its figures are arriving, not rising
        chosen.clear();
        chosenList.clear();
        hovered.clear();
        figures.clear();
        quiet = QuietMs;
        lead();
        showWorkButtons();
        buttonsDue = true;
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

        // What the list did not have a moment ago is new: said by a toast. The first list of a page
        // is all he has always had
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

                // What comes with words of its own is written down as well: it is news of his life
                const RuleActivity* arrived = activityOf(id);
                const std::string opens = arrived ? textOf(arrived->fields, "opens") : "";

                // The town's own rows come a dozen at a time, the month it opens or a place is
                // raised: the line the town writes then says it for all of them
                if (arrived and not (textOf(arrived->fields, "raises") + textOf(arrived->fields, "place") + textOf(arrived->fields, "goto")).empty())
                    continue;

                if (opens.empty())
                {
                    toast("New: " + activityName(id));
                }
                else
                {
                    appendLog({save.age, opens, LogKind::Milestone, "", ""});
                    toast(opens);
                }
            }
        }

        known = ids;

        // A row that has never stood on a page is new to it, open to him or not: on the page that
        // is not the one in view, its tab says so. Once: what was there and can be done again (the
        // coin came back for what the market sells) is no news
        const bool filledBefore = not stood.empty();

        for (const auto& a : activities)
        {
            const std::string id = textOf(a.fields, "id");

            if (not boolOf(a.fields, "listed") or std::find(stood.begin(), stood.end(), id) != stood.end())
                continue;

            stood.push_back(id);

            const bool onTown = save.townKnown and not (textOf(a.fields, "place") + textOf(a.fields, "raises")).empty();

            if (filledBefore and onTown != townShown)
                ++(onTown ? newInTown : newInLife);
        }

        // Grouped as the table orders them; what the rules do not list (spent, too late, another
        // path's) has no row
        std::vector<ActivityGroup> groups;

        // What is done at a place of the town stands on the Town page once the town is known:
        // the market's rows, what is bought on the spot, and the works that raise a place
        std::vector<ActivityRowSpec> stalls;
        std::vector<ActivityRowSpec> works;

        listedRows.clear();

        for (const auto& a : activities)
        {
            // The work he is at keeps its place, even when it could no longer be begun
            if (not boolOf(a.fields, "listed") and textOf(a.fields, "id") != save.running)
                continue;

            listedRows += textOf(a.fields, "id") + ":" + std::to_string(a.requires.size()) + ";";

            const std::string group = textOf(a.fields, "group");
            const bool raising = not textOf(a.fields, "raises").empty();
            const bool inTown = save.townKnown and (raising or not textOf(a.fields, "place").empty());

            if (not inTown and (groups.empty() or groups.back().label != group))
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
            row.featured = boolOf(a.fields, "featured");
            row.major = boolOf(a.fields, "major");

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

            if (inTown)
                (raising ? works : stalls).push_back(row);
            else
                groups.back().rows.push_back(row);
        }

        if (auto list = piece<ActivityList>("activities"))
            list->setRows(ecsRef, groups);

        if (auto market = piece<ActivityList>(MarketName))
            market->setRows(ecsRef, stalls.empty() ? std::vector<ActivityGroup>{} : std::vector<ActivityGroup>{{"", stalls}});

        if (auto raise = piece<ActivityList>(WorksName))
            raise->setRows(ecsRef, works.empty() ? std::vector<ActivityGroup>{} : std::vector<ActivityGroup>{{"", works}});

        showNotices();
    }

    bool LifeScene::fromLists(const std::string& list) const
    {
        return list == ActivitiesList or list == MarketList or list == WorksList;
    }

    ActivityList* LifeScene::listHolding(const std::string& id) const
    {
        for (const char* name : {ChoiceName, MarketName, WorksName})
        {
            auto list = piece<ActivityList>(name);

            if (not list)
                continue;

            for (const auto& group : list->spec.groups)
            {
                for (const auto& row : group.rows)
                {
                    if (row.id == id)
                        return list;
                }
            }
        }

        return nullptr;
    }

    void LifeScene::clearChoice()
    {
        for (const char* name : {ChoiceName, MarketName, WorksName})
        {
            if (auto list = piece<ActivityList>(name); list and not list->selected().empty())
                list->select(ecsRef, "");
        }

        chosen.clear();
        chosenList.clear();
    }

    std::string LifeScene::tileNote(const RuleActivity& activity, bool& urgent) const
    {
        // When it closes, once that is near, and nothing else: what it asks of him (an age, a
        // stat, coin, room) is its gloss's to say, and its ground says that he cannot do it yet
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

            // What to do about it, said when it begins to run out and again when it has
            if (month.advice != advice)
            {
                advice = month.advice;
                toast(advice);
            }

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

        // And the guide has no one left to lead
        clearGuide();

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
            // Not the lore he read, nor what the guide told him
            for (const auto& deed : deeds)
            {
                if (textOf(deed.fields, "id") == id and deed.kind == RuleKind::Deed)
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

        fillGifts();

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

        // Its buttons went with it
        gifts.clear();
        gift.clear();
    }

    void LifeScene::beginAgain()
    {
        if (not ended)
            return;

        const std::string line = endedLine;
        const std::string age = endedAge;

        // What he held of it at his death, left to the place he chose: counted toward its next
        // level, for whoever raises it
        if (not gift.empty())
        {
            if (auto held = save.stats.find(giftStat()); held != save.stats.end() and held->second > 0)
                save.fund[gift] += held->second;
        }

        newLife();

        appendLog({save.age, line, LogKind::Loss, age, ""});

        // The inn tells a new life of the one before, once it keeps its book
        if (not town.epitaphLine.empty() and not epitaph.cause.empty())
            appendLog({save.age, town.epitaphLine + epitaph.cause, LogKind::Lore, "", ""});

        autoSave();
    }

    void LifeScene::readDeeds()
    {
        guideSteps = 0;

        if (not rules.achievements(deeds))
        {
            for (const auto& e : rules.errors)
                LOG_ERROR(DOM, e);

            return;
        }

        // The steps of the sequence: a save past the last of them has no guide left
        for (const auto& deed : deeds)
        {
            if (deed.kind == RuleKind::Guide and deed.order > 0)
                ++guideSteps;
        }
    }

    void LifeScene::takeAsRead()
    {
        // What is already so when the chronicle is opened is not news: a life from before the lore
        // was written, or the mockup's, does not open on a page of it, nor on a word of the guide
        // about something long past. A life at its first frame has nothing of the kind
        bool worldTold = false;

        // A step the world was told out of turn, before the one it waits for (a save written when
        // a fact of a world before stood in for it), is told again in its turn. In the table's
        // order: what waits for a step untold here is untold after it
        for (const auto& deed : deeds)
        {
            const std::string id = textOf(deed.fields, "id");

            if (deed.kind != RuleKind::Guide or not deed.world or not told(id))
                continue;

            for (const auto& ask : deed.asks)
            {
                const std::string fact = textOf(ask, "fact");

                if (fact.rfind(SaidFact, 0) == 0 and not told(fact.substr(SaidFact.size())))
                {
                    save.told.erase(std::remove(save.told.begin(), save.told.end(), id), save.told.end());
                    break;
                }
            }
        }

        for (const auto& deed : deeds)
        {
            const std::string id = textOf(deed.fields, "id");
            const bool aside = deed.kind == RuleKind::Lore or (deed.kind == RuleKind::Guide and deed.order == 0);

            if (not aside or not holds(deed.asks))
                continue;

            if (deed.world)
                worldTold = worldTold or not told(id);
            else if (std::find(save.achieved.begin(), save.achieved.end(), id) == save.achieved.end())
                save.achieved.push_back(id);
        }

        // A world that is already past the start of what its guide tells (a town known before the
        // guide spoke of it) has all of it taken as said: its steps follow one another
        if (worldTold)
        {
            for (const auto& deed : deeds)
            {
                if (deed.kind == RuleKind::Guide and deed.world and not told(textOf(deed.fields, "id")))
                    save.told.push_back(textOf(deed.fields, "id"));
            }
        }
    }

    bool LifeScene::told(const std::string& id) const
    {
        return std::find(save.told.begin(), save.told.end(), id) != save.told.end();
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

        readDeeds();

        for (const auto& deed : deeds)
        {
            const std::string id = textOf(deed.fields, "id");

            if (std::find(save.achieved.begin(), save.achieved.end(), id) != save.achieved.end())
                continue;

            // The guide is a first life's, and a step that has ended is not said again
            if (deed.kind == RuleKind::Guide)
            {
                // A step said once in a world is whichever life's meets it, until it has been said
                if (deed.world ? told(id) : (save.lives > 1 or (deed.order > 0 and deed.order <= save.guide)))
                    continue;
            }

            watchDeed(deed);
        }
    }

    void LifeScene::watchDeed(const RuleAchievement& deed)
    {
        auto achievements = ecsRef->getSystem<AchievementSys>();

        if (not achievements)
            return;

        Achievement achievement;
        achievement.name = textOf(deed.fields, "id");

        for (const auto& ask : deed.asks)
        {
            auto value = ask.find("value");

            achievement.prerequisiteFacts.push_back(FactChecker(textOf(ask, "fact"), value == ask.end() ? ElementType{0} : value->second, equalityOf(textOf(ask, "op"))));
        }

        achievements->addNewAchivement(achievement);
    }

    void LifeScene::reachDeed(const std::string& id)
    {
        if (std::find(save.achieved.begin(), save.achieved.end(), id) != save.achieved.end())
            return;

        for (const auto& deed : deeds)
        {
            if (textOf(deed.fields, "id") != id)
                continue;

            // A step of the guide: a sentence and a hand, no line and no toast
            if (deed.kind == RuleKind::Guide)
            {
                // What the world is told once waits for the save to say so, not for a fact alone:
                // reached on a fact that was not this world's, it is watched for again
                if (deed.world and not holds(deed.asks))
                    watchDeed(deed);
                else
                    reachStep(deed);

                return;
            }

            save.achieved.push_back(id);

            // A line of the town's story, and nothing else
            if (deed.kind == RuleKind::Lore)
            {
                save.log.push_back({save.age, textOf(deed.fields, "entry"), LogKind::Lore, "", ""});
                return;
            }

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
        {
            // The town comes back with his stats, as it went: it is the world's, and not written
            // among them
            if (save.takeTown(key, intOf(value)))
                continue;

            save.stats[key] = intOf(value);
        }
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
        row.value = titled(resource.id) ? std::string() : stat != save.stats.end() ? holdingText(resource.id, stat->second) : resource.value;
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
        publishTown();

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
            setFact("resources." + r.id + ".value", titled(r.id) ? std::string() : stat != save.stats.end() ? holdingText(r.id, stat->second) : r.value);
            setFact("resources." + r.id + ".rate", rate);
            setFact("resources." + r.id + ".muted", r.muted);
        }

        // Every stat as a number, for what watches the life rather than shows it: the town's too
        // ("stat.town_known", "stat.town.<place>"), which the rules read with his
        for (const auto& [key, value] : save.character())
            setFact("stat." + key, intOf(value));

        // And where the life stands, for the deeds, the lore and the guide: the terms he has done,
        // whether he is at work, which life this is and the step of the guide he is past
        setFact("life.terms", save.termsDone());
        setFact("life.working", save.running.empty() ? 0 : 1);
        setFact("life.lives", save.lives);
        setFact("life.guide", save.guide);
        setFact("life.town", townShown ? 1 : 0);

        // What the world has been told of its guide: the step that waits for another reads it.
        // Written both ways: the facts outlive a world (they are saved with the engine's), and
        // one left at 1 by a world before would have this one told the market before the town
        for (const auto& deed : deeds)
        {
            if (deed.kind == RuleKind::Guide and deed.world)
                setFact(SaidFact + textOf(deed.fields, "id"), told(textOf(deed.fields, "id")) ? 1 : 0);
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

                // When it closes, month by month: nothing to say of the one he is at
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
        if (not fromLists(event.list))
            return;

        // A list that lets go of what it did not hold: the choice is the other list's
        if (event.id.empty() and not chosenList.empty() and event.list != chosenList)
            return;

        // A tile he chose: not the one the page chose for him, which is `chosen` already
        if (not event.id.empty() and event.id != chosen)
            ++save.picks;

        chosen = event.id;
        chosenList = event.id.empty() ? std::string() : event.list;

        // One choice on the page: the other list lets go of its own
        if (not chosen.empty())
        {
            for (const char* name : {ChoiceName, MarketName, WorksName})
            {
                if (auto list = piece<ActivityList>(name); list and list->spec.id != event.list and not list->selected().empty())
                    list->select(ecsRef, "");
            }
        }

        // The place whose work is chosen is the one lit, and no other
        if (auto grid = piece<PlaceGrid>(PlacesName))
        {
            const RuleActivity* picked = chosen.empty() ? nullptr : activityOf(chosen);
            const std::string place = picked ? textOf(picked->fields, "raises") : std::string();

            if (grid->selected() != place)
                grid->select(ecsRef, place);
        }

        showWorkButtons();
        preview();
    }

    void LifeScene::onPlace(const PlaceSelectedEvent& event)
    {
        auto grid = piece<PlaceGrid>(PlacesName);
        auto works = piece<ActivityList>(WorksName);

        if (not grid)
            return;

        // Clicked again: let go of, and its work with it
        if (grid->selected() == event.id)
        {
            grid->select(ecsRef, "");

            if (works and not works->selected().empty())
                works->select(ecsRef, "");

            return;
        }

        grid->select(ecsRef, event.id);

        // The work that raises it, chosen when he can begin it: Begin is then one press away. One
        // he cannot begin yet leaves the tile lit, and its gloss to say what it asks
        for (const auto& place : town.places)
        {
            if (textOf(place.fields, "id") != event.id)
                continue;

            const std::string work = textOf(place.fields, "nextWork");
            ActivityRow* row = works and not work.empty() ? works->find(ecsRef, work) : nullptr;

            if (row and row->spec.state == ActivityState::Idle)
                works->select(ecsRef, work);
            else if (works and not works->selected().empty())
                works->select(ecsRef, "");
        }
    }

    void LifeScene::onHover(const ActivityHoveredEvent& event)
    {
        if (not fromLists(event.list))
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

    std::string LifeScene::holdingText(const std::string& id, int amount) const
    {
        // Over the most he can hold when the rules give one: "12/60", and "64/60" past it
        for (const auto& h : holdings)
        {
            if (textOf(h, "id") == id and intOf(h, "limit") > 0)
                return std::to_string(amount) + "/" + std::to_string(intOf(h, "limit"));
        }

        return std::to_string(amount);
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

            if (forecast and now != save.stats.end() and value == holdingText(r.id, now->second))
            {
                auto after = forecast->atTerm.find(r.id);

                // Only what it would add: what it costs is its gloss's to say, not his purse's
                // Written as the figure is, most and all: a work that raises the most will show it here
                if (after != forecast->atTerm.end() and intOf(after->second) > now->second)
                    value += Arrow + holdingText(r.id, intOf(after->second));
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
        if (not fromLists(event.list) or ended)
            return;

        // A row that only goes to a page: no term, no month, no line
        if (const RuleActivity* asked = activityOf(event.id); asked and not textOf(asked->fields, "goto").empty())
        {
            if (boolOf(asked->fields, "listed") and not boolOf(asked->fields, "locked"))
            {
                clearChoice();
                showTown(true);
                showWorkButtons();
            }

            return;
        }

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
        ++save.begun;
        rules.running = save.running;

        // What it costs is taken as it begins: the years of his life some works for the town ask,
        // then what it takes of what he holds
        giveYears(intOf(chosen->fields, "yearMonths"));
        takeStats(forecast.atStart);

        // Nothing is chosen any more, whether or not the list had its tile lit
        clearChoice();
        leadDue = false;

        setFact("activity." + event.id + ".state", std::string("running"));

        // The months run on their own while he works, from the start of a month
        paused = false;
        sinceMonth = 0.0f;

        publish();

        // A step of the guide that waited for a work to begin ends with it, not a frame later: the
        // term may be over by then
        if (step.active and not step.until.empty() and holds(step.until))
            endStep();

        autoSave();
    }

    void LifeScene::doAtOnce(const std::string& id, const RuleForecast& forecast)
    {
        // The row as it stands, to see whether doing it changes the row itself
        std::string rank;
        std::vector<int> amounts;
        std::string raised;
        int raisedTo = 0;
        int years = 0;

        for (const auto& a : activities)
        {
            if (textOf(a.fields, "id") != id)
                continue;

            // The list refuses a locked row, a key or a script may not
            if (boolOf(a.fields, "locked") or not boolOf(a.fields, "listed"))
                return;

            rank = textOf(a.fields, "rank");
            raised = textOf(a.fields, "raises");
            raisedTo = intOf(a.fields, "level");
            years = intOf(a.fields, "yearMonths");

            for (const auto& g : a.gains)
                amounts.push_back(intOf(g, "amount"));
        }

        const bool knew = save.townKnown;

        giveYears(years);
        takeStats(forecast.atTerm);
        writeEntries(forecast.entries);

        ++save.done[id];
        ++save.atOnce;
        rules.done = save.terms();

        // What it did to the town: known now, or a place raised
        if (not knew and save.townKnown)
            openTown();

        if (not raised.empty())
            raisePlace(raised, raisedTo);

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

        if (event.tag != TabsTag)
            return;

        // The Life page, and the town's once a life has explored it: they share the middle column
        if (event.index == 0)
        {
            showTown(false);
            return;
        }

        if (event.index == townTab() and save.townKnown)
        {
            showTown(true);
            return;
        }

        auto tabs = piece<Tabs>("tabs");

        if (not tabs)
            return;

        showTown(false);

        const std::string label = event.index >= 0 and static_cast<size_t>(event.index) < tabs->tabs.size() ? tabs->tabs[event.index].label.spec.text : "That page";

        tabs->setActive(ecsRef, 0);

        appendLog({save.age, label + " has no page yet", LogKind::Note, "", ""});
    }

    // ---- the town -------------------------------------------------------------------------------------------

    int LifeScene::townTab() const
    {
        auto tabs = piece<Tabs>("tabs");

        if (not tabs)
            return -1;

        for (size_t i = 0; i < tabs->spec.items.size(); ++i)
        {
            if (lower(tabs->spec.items[i].label) == TownName)
                return static_cast<int>(i);
        }

        return -1;
    }

    void LifeScene::showTown(bool shown)
    {
        townShown = shown and save.townKnown;

        auto may = piece<Panel>("may");

        if (not may)
            return;

        // The choice and the Town page take each other's place: one is in the column, the other
        // out of its stack
        auto show = [this, &may](const char* name, bool visible) {
            if (EntityRef node = named(name); not node.empty())
            {
                wrapIn(may->body, node)->get<PositionComponent>()->setVisibility(visible);
                node->get<PositionComponent>()->setVisibility(visible);
            }
        };

        show(ChoiceName, not townShown);
        show(TownName, townShown);

        // Seen: what was new on the page now in view is new no longer
        if (townShown)
            newInTown = 0;
        else
            newInLife = 0;

        showNotices();

        setFact("life.town", townShown ? 1 : 0);

        const int tab = townShown ? townTab() : 0;

        if (auto tabs = piece<Tabs>("tabs"); tabs and tab >= 0 and tabs->active() != tab)
            tabs->setActive(ecsRef, tab);
    }

    void LifeScene::fitTown(float width, float height)
    {
        if (EntityRef body = named(TownName); not body.empty())
        {
            body->get<PositionComponent>()->setWidth(width);
            body->get<PositionComponent>()->setHeight(height);
        }

        if (auto grid = piece<PlaceGrid>(PlacesName))
            grid->setWidth(ecsRef, width);

        for (const char* name : {MarketName, WorksName})
        {
            if (auto list = piece<ActivityList>(name))
                list->setSize(ecsRef, width, 0.0f);
        }

        for (const char* name : {"worksHead", "marketHead", RaisedName})
        {
            if (auto label = piece<Label>(name))
                label->setWidth(ecsRef, width);
        }
    }

    void LifeScene::fillTown()
    {
        if (not rules.town(save.age, save.character(), town))
        {
            for (const auto& e : rules.errors)
                LOG_ERROR(DOM, e);

            return;
        }

        layTown();
        publishTown();
    }

    void LifeScene::layTown()
    {
        auto grid = piece<PlaceGrid>(PlacesName);

        if (not grid)
            return;

        // The places on his page: the town grows with what he does, and the rules say
        // which of them he has
        std::vector<PlaceTileSpec> places;

        for (const auto& place : town.places)
        {
            if (not boolOf(place.fields, "shown"))
                continue;

            PlaceTileSpec tile;
            tile.id = textOf(place.fields, "id");
            tile.name = textOf(place.fields, "name");
            tile.glyph = textOf(place.fields, "glyph");
            tile.level = intOf(place.fields, "level");
            tile.of = intOf(place.fields, "most");
            tile.line = textOf(place.fields, "line");
            tile.glossKey = PlaceGloss + tile.id;

            places.push_back(tile);
        }

        grid->setPlaces(ecsRef, places);
    }

    void LifeScene::seeTown()
    {
        // What the page shows for the first time is kept with the town: shown once, a place stays
        // shown, in this life and in the next. The first look at the town is no news; a place
        // that comes later is said, in the log and by a toast
        const bool first = save.seen.empty();

        std::vector<const RulePlace*> found;

        for (const auto& place : town.places)
        {
            const std::string id = textOf(place.fields, "id");

            if (boolOf(place.fields, "shown") and std::find(save.seen.begin(), save.seen.end(), id) == save.seen.end())
            {
                save.seen.push_back(id);
                found.push_back(&place);
            }
        }

        if (found.empty())
            return;

        if (not first)
        {
            if (not townShown)
                newInTown += static_cast<int>(found.size());

            showNotices();

            for (const RulePlace* place : found)
            {
                const std::string line = textOf(place->fields, "name") + town.foundLine;

                appendLog({save.age, line, LogKind::Milestone, "", textOf(place->fields, "glyph")});
                toast(line);
            }
        }

        // The rules read what was seen with the town: asked again, as it stands now
        if (not rules.town(save.age, save.character(), town))
        {
            for (const auto& e : rules.errors)
                LOG_ERROR(DOM, e);
        }
    }

    void LifeScene::publishTown()
    {
        // Its tab is on the page for who knows the town, and for no one else
        if (auto tabs = piece<Tabs>("tabs"); tabs and townTab() >= 0 and tabs->tabs[townTab()].shown != save.townKnown)
            tabs->setShown(ecsRef, townTab(), save.townKnown);

        // Nothing to say of a town nobody knows
        if (not save.townKnown or not rules.town(save.age, save.character(), town))
            return;

        seeTown();

        auto grid = piece<PlaceGrid>(PlacesName);
        auto registry = ecsRef->getSystem<GlossRegistry>();

        // A place that has come onto the page, with his years or with his class: its tile with it
        if (grid)
        {
            size_t shown = 0;
            bool laid = true;

            for (const auto& place : town.places)
            {
                if (not boolOf(place.fields, "shown"))
                    continue;

                laid = laid and shown < grid->tiles.size() and grid->tiles[shown].spec.id == textOf(place.fields, "id");
                ++shown;
            }

            if (not laid or shown != grid->tiles.size())
                layTown();
        }

        std::string raised;

        for (const auto& place : town.places)
        {
            // Not on his page: nothing to say of it yet
            if (not boolOf(place.fields, "shown"))
                continue;

            const std::string id = textOf(place.fields, "id");
            const int level = intOf(place.fields, "level");
            const int most = intOf(place.fields, "most");

            if (auto tile = grid ? grid->tile(id) : nullptr)
            {
                if (tile->spec.level != level or tile->spec.of != most)
                    tile->setLevel(ecsRef, level, most);

                if (tile->spec.line != textOf(place.fields, "line"))
                    tile->setLine(ecsRef, textOf(place.fields, "line"));
            }

            // Who raised it, and when: under the grid for every place, and in its own gloss
            auto by = save.raisedBy.find(id);

            if (by != save.raisedBy.end())
                raised += (raised.empty() ? "" : "\n") + upper(textOf(place.fields, "name") + " \xC2\xB7 " + by->second);

            if (not registry)
                continue;

            // The gloss: what it is and gives, then its next level: what it asks of him against
            // what he has, what it will give, and the work that raises it
            GlossSpec gloss;
            gloss.inlineValues = true;
            gloss.title = textOf(place.fields, "name");
            gloss.aside = std::to_string(level) + "/" + std::to_string(most);
            gloss.text = textOf(place.fields, "about");

            if (level > 0)
            {
                gloss.rows.push_back({"IT GIVES", "", "", true});
                gloss.rows.push_back({textOf(place.fields, "gives"), "", "gain"});
            }

            if (by != save.raisedBy.end())
                gloss.rows.push_back({"Raised by", by->second, "muted"});

            const std::string work = textOf(place.fields, "nextName");

            if (not work.empty())
            {
                gloss.rows.push_back({"THE NEXT LEVEL", "", "", true});
                gloss.rows.push_back({"It takes", textOf(place.fields, "nextCosts"), "time"});

                for (const auto& gap : place.gaps)
                    gloss.rows.push_back({textOf(gap, "label"), std::to_string(intOf(gap, "current")) + " / " + std::to_string(intOf(gap, "needed")), intOf(gap, "current") < intOf(gap, "needed") ? "loss" : "gain"});

                if (intOf(place.fields, "fund") > 0)
                    gloss.rows.push_back({town.fundLine, std::to_string(intOf(place.fields, "fund")), "gain"});

                gloss.rows.push_back({"It will give", textOf(place.fields, "nextGives"), ""});
                gloss.footnote = upper("Raised by \"" + work + "\", under the places");
            }

            registry->set(PlaceGloss + id, gloss);
        }

        if (auto label = piece<Label>(RaisedName); label and label->spec.text != raised)
            label->setText(ecsRef, raised);
    }

    void LifeScene::openTown()
    {
        if (not rules.town(save.age, save.character(), town))
        {
            for (const auto& e : rules.errors)
                LOG_ERROR(DOM, e);
        }

        // Said once, the month he comes to know it: its tab opens, for this life and the next
        if (not town.opened.empty())
        {
            save.log.push_back({save.age, town.opened, LogKind::Milestone, "", ""});
            toast(town.opened);
        }

        if (auto tabs = piece<Tabs>("tabs"); tabs and townTab() >= 0)
            tabs->setShown(ecsRef, townTab(), true);

        fillTown();

        // Its tab says how much there is to see behind it: the places, and what the market sells
        // (counted as its rows come, fillActivities)
        if (not townShown)
        {
            for (const auto& place : town.places)
            {
                if (boolOf(place.fields, "shown"))
                    ++newInTown;
            }
        }

        showNotices();
    }

    void LifeScene::showNotices()
    {
        auto tabs = piece<Tabs>("tabs");

        if (not tabs)
            return;

        // What is new on the page that is not in view, at its tab's corner
        tabs->setNotice(ecsRef, 0, townShown ? newInLife : 0);

        if (townTab() >= 0)
            tabs->setNotice(ecsRef, townTab(), townShown ? 0 : newInTown);
    }

    void LifeScene::raisePlace(const std::string& place, int level)
    {
        save.town[place] = level;
        save.fund.erase(place);

        // Who raised it, and when: the world's year as the head writes it
        std::vector<RuleMilestone> passed;
        ElementMap ahead;
        ElementMap headline;

        if (rules.milestones(save.age, passed, ahead, &headline))
            save.raisedBy[place] = save.name + " \xC2\xB7 " + textOf(headline, "date");

        // The town as it stands now: what was built, and the line the place keeps of it
        if (not rules.town(save.age, save.character(), town))
        {
            for (const auto& e : rules.errors)
                LOG_ERROR(DOM, e);

            return;
        }

        for (const auto& raised : town.places)
        {
            if (textOf(raised.fields, "id") != place)
                continue;

            const std::string built = textOf(raised.fields, "built") + town.raisedLine;

            save.log.push_back({save.age, built, LogKind::Milestone, "", textOf(raised.fields, "glyph")});
            toast(built);

            if (const std::string tale = textOf(raised.fields, "tale"); not tale.empty())
                save.log.push_back({save.age, tale, LogKind::Lore, "", ""});
        }

        fillTown();
    }

    void LifeScene::giveYears(int months)
    {
        // The clock jumps: years of his life given at once, and the world's with them
        for (int given = 0; given < months; given += MonthsAYear)
            save.age += 1.0f;

        save.world += months;
        rules.world = save.world;
    }

    std::string LifeScene::giftStat() const
    {
        return town.giftStat;
    }

    void LifeScene::fillGifts()
    {
        gifts.clear();
        gift.clear();

        if (ending.empty() or not ending->has<Prefab>())
            return;

        auto prefab = ending->get<Prefab>();

        EntityRef line = prefab->findEntity("giftLabel");
        EntityRef rows = prefab->findEntity("gifts");

        if (line.empty() or rows.empty() or not rows->has<VerticalLayout>())
            return;

        // Only who knows the town has a town to leave anything to, and only what he holds of it
        auto held = save.stats.find(giftStat());
        const int amount = save.townKnown and held != save.stats.end() ? held->second : 0;

        std::vector<const RulePlace*> open;

        if (amount > 0 and rules.town(save.age, save.character(), town))
        {
            for (const auto& place : town.places)
            {
                // The places on his page that can still be raised
                if (boolOf(place.fields, "shown") and intOf(place.fields, "level") < intOf(place.fields, "most"))
                    open.push_back(&place);
            }
        }

        const bool offered = not open.empty();

        // Out of the leaf's stack when there is nothing to offer: it is their wraps that are hidden
        EntityRef leaf = prefab->findEntity("leaf");

        for (EntityRef node : {line, rows})
        {
            if (not leaf.empty() and leaf->has<Panel>())
                wrapIn(leaf->get<Panel>()->body, node)->get<PositionComponent>()->setVisibility(offered);

            node->get<PositionComponent>()->setVisibility(offered);
        }

        if (not offered)
            return;

        if (line->has<Label>())
            line->get<Label>()->setText(ecsRef, town.giftBefore + std::to_string(amount) + town.giftAfter);

        // One small button a place, as many to a line as the leaf holds
        const float width = rows->get<PositionComponent>()->width;
        const int z = static_cast<int>(rows->get<PositionComponent>()->z);

        EntityRef row;
        float used = 0.0f;

        for (const RulePlace* place : open)
        {
            ButtonSpec spec;
            spec.variant = ButtonVariant::Quiet;
            spec.label = textOf(place->fields, "name");
            spec.glyph = textOf(place->fields, "glyph");
            spec.tag = GiftTag + textOf(place->fields, "id");
            spec.z = z;

            Button button = makeButton(ecsRef, spec);

            const float face = button.root->get<PositionComponent>()->width;

            if (row.empty() or used + face > width)
            {
                auto made = makeAnchoredPrefab(ecsRef, 0.0f, 0.0f, static_cast<float>(z));

                made.get<PositionComponent>()->setWidth(width);
                made.get<PositionComponent>()->setHeight(button.root->get<PositionComponent>()->height);

                row = made.entity;
                used = 0.0f;

                rows->get<VerticalLayout>()->addEntity(row);

                // It leaves with the ending, as everything on its leaf does
                prefab->addToPrefab(row, "giftRow" + std::to_string(gifts.size()));
            }

            auto anchor = button.root->get<UiAnchor>();
            anchor->setTopAnchor(PosAnchor{row.id, AnchorType::Top});
            anchor->setLeftAnchor(PosAnchor{row.id, AnchorType::Left});
            anchor->setLeftMargin(used);

            row->get<Prefab>()->addToPrefab(button.root, textOf(place->fields, "id"));

            used += face + GiftGap;

            gifts.push_back({textOf(place->fields, "id"), button});
        }
    }

    void LifeScene::chooseGift(const std::string& place)
    {
        if (not ended)
            return;

        // Chosen again, it is let go of: a gift is his to make or not
        gift = gift == place ? std::string() : place;

        for (auto& [id, button] : gifts)
            button.setDisabled(ecsRef, not gift.empty() and id != gift);

        if (ending.empty() or not ending->has<Prefab>())
            return;

        if (EntityRef again = ending->get<Prefab>()->findEntity("again"); not again.empty() and again->has<Button>())
            again->get<Button>()->setLabel(ecsRef, gift.empty() ? town.giftNone : town.giftChosen);
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
                const bool knew = save.townKnown;

                takeStats(forecast.atTerm);
                writeEntries(forecast.entries);

                // The way into a path, done: he is one of it, and its asks stand on his parts
                const RuleActivity* at = activityOf(save.running);

                if (at and boolOf(at->fields, "enters"))
                    save.aim = textOf(at->fields, "path");

                // A work for the town: the place it raises, and the level it raises it to
                const std::string raised = at ? textOf(at->fields, "raises") : std::string();
                const int raisedTo = at ? intOf(at->fields, "level") : 0;

                ++save.done[save.running];
                rules.done = save.terms();

                save.running.clear();
                save.monthsIn = 0;
                rules.running.clear();

                // The work is done: the months wait for the next choice
                paused = true;
                sinceMonth = 0.0f;

                // What it did to the town: known now, or a place raised. Either is the world's from
                // here on, and what reads the town changes on the spot
                if (not knew and save.townKnown)
                    openTown();

                if (not raised.empty())
                    raisePlace(raised, raisedTo);

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

            // What turns a life says what it does, and a word of the town's, in that place
            if (not textOf(a.fields, "about").empty())
                gloss.text = textOf(a.fields, "about");

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

        // Never a first life: it opens as every later one does, at 7 with a year of rations and
        // every task in the list, and no guide
        clearGuide();

        save = freshLife();

        // The world did not begin again with him: its date runs on from the last life's, and he
        // is one more of its lives. The town is the world's too: what was raised stays raised
        save.world = last.world;
        save.lives = last.lives + 1;
        save.townKnown = last.townKnown;
        save.town = last.town;
        save.raisedBy = last.raisedBy;
        save.fund = last.fund;
        save.seen = last.seen;
        save.told = last.told;

        newInLife = 0;
        newInTown = 0;

        rules.world = save.world;
        rules.lives = save.lives;
        rules.running = save.running;

        // And what the town gives a life at its birth: a stat, a holding, a tie
        if (rules.town(save.age, save.character(), town))
        {
            for (const auto& given : town.start)
                save.stats[textOf(given, "stat")] += intOf(given, "amount");
        }

        // The page does not open on the town
        townShown = false;

        // "At work now" has the line of a life that has done nothing again
        fit(windowWidth, windowHeight);

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
        stood.clear();

        // What he is born holding has its row from the first frame
        refreshHoldings();
        save.holdEarned(holdings);

        rebuild();
        publish();
        registerDeeds();
    }
}
