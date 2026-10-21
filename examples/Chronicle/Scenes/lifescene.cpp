#include "lifescene.h"

#include <sstream>

#ifdef __EMSCRIPTEN__
#include <SDL2/SDL.h>
#elif __linux__
#include <SDL2/SDL.h>
#elif _WIN32
#include <SDL.h>
#endif

#include <cctype>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

#include "logger.h"

#include "ECS/entitysystem.h"
#include "ECS/entitysystem_fwd.h"
#include "ECS/callable.h"
#include "Input/sdlevents.h"
#include "Input/inputcomponent.h"
#include "2D/position.h"
#include "2D/simple2dobject.h"
#include "Core/analytics.h"
#include "UI/prefab.h"
#include "UI/sizer.h"
#include "UI/prefabloader.h"
#include "UI/prefabbuilder.h"
#include "UI/themesystem.h"
#include "UI/tooltip.h"
#include "UI/ttftext.h"
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
        constexpr const char * const PrimeStat = "prime_end";      // Where his old age begins, settled the month he reaches thirty (lib.pg's `primeStat`)

        // The page's pieces the town is made of (life.yaml, life-compact.yaml): the choice and the
        // Town page take each other's place in the middle column
        constexpr const char * const ChoiceName = "activities";
        constexpr const char * const TownName = "town";
        constexpr const char * const AdventureName = "adventure";   // The Adventure page: the records of his walks (lifewalk.cpp)
        constexpr const char * const OptionsName = "options";       // The Options page (lifeoptions.cpp)
        constexpr const char * const PlacesName = "places";
        constexpr const char * const PlacesList = "town.places";   // What `chosenList` says of a place's work chosen by its tile
        constexpr const char * const MarketName = "marketRows";
        constexpr const char * const RaisedName = "raised";
        constexpr const char * const GiftTag = "life.gift.";   // Followed by the place's id: the ending's buttons
        constexpr float GiftGap = 6.0f;            // Between two of them
        constexpr int MonthsAYear = 12;
        constexpr float LateFromAge = 16.0f;       // The age the classes open at: what opens then or later is the end of a life, for the analytics
        constexpr float EarlyUnderAge = 7.0f;      // And what opens before this age is his sixth year: the choices that shape him
        constexpr float EarlyPace = 0.5f;          // His sixth year runs at half opt.monthMs: a month a second
        constexpr float ChildPace = 0.75f;         // And his childhood at three quarters of it: a month every second and a half
        constexpr float ChildUnderAge = 10.0f;     // From ten a month is opt.monthMs whole
        constexpr size_t NewNamed = 3;             // New tasks are toasted by name up to this many at once, counted past it
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
        constexpr const char * const HasteTag = "life.haste";   // The button that spends written time
        constexpr float MsASecond = 1000.0f;       // Written time is held in seconds and spent in ms
        constexpr unsigned long long AwayLeast = 60;   // A chronicle shut for less than a minute has written nothing: a page loaded again is not time away
        constexpr const char * const BeginTag = "life.begin";
        constexpr const char * const AllTag = "life.all";       // Beside it, for what is done at once from what he holds: all he can
        constexpr int MostAtOnce = 200;            // The most times All does a thing in one press
        constexpr float AllGap = 8.0f;             // Between the button and All, at its right
        constexpr float DoubleClickMs = 400.0f;    // A second click on a chosen place's tile within it begins its work, as a row's does
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
        constexpr float ToastGap = 8.0f;           // Between one toast and the one over it
        constexpr float ToastSide = 16.0f;         // The least room between a toast and the window's side
        constexpr size_t ToastMost = 4;            // Up at once: one more pushes out the oldest
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
        // Over a walk's screen (expedition.yaml, fight.yaml: its panels at 104, what they hold at
        // 114 and its buttons' words a few over) the three stand higher, still in the band: under
        // the dialogs (130), the bug report's button (145, lit and pressable through the shade),
        // the gains and the toasts
        constexpr float WalkVeilZ = 120.0f;
        constexpr float WalkNoteZ = 121.0f;        // Its buttons' words at 128
        constexpr float WalkHandZ = 129.0f;
        constexpr float VeilPad = 6.0f;            // Left lit around what is pointed at
        constexpr float Turn = 6.2831853f;         // A whole turn, in radians
        constexpr const char * const HandGlyph = "manicule";
        constexpr const char * const HandColor = "vermilion";
        constexpr const char * const VeilElement = "guide.veil";
        const std::string TilePoint = "tile:";                  // What a step that points at a tile begins with, before the activity's id
        const std::string WorkPoint = "work:";                  // And at a work of a walk's day, before its id

        // What a step points at on a walk's screens, not on the page: a work of the day, an answer
        // of the card, a move, a beast, the order of the turn, a button of the screen
        const std::vector<std::string> WalkPoints = {"work", "option", "move", "foe", "order", "walkbutton"};

        // A save from before the outskirts had their steps (its format 1 or older) numbered its
        // guide with thirteen, the last two the classes and the last word: it is past them still
        constexpr int OldGuideSteps = 13;
        constexpr int OldGuideLast = 11;           // The last step whose number has not moved: his age
        constexpr int GuideNumbered = 2;           // The save's format from which the guide is numbered as now
        const std::string BackToLife = "Go back to the Life tab: there is a new task for him.";   // Said with the hand on that tab, for a step about a work of the Life page

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

        // The phone page (life-phone.yaml): one column as wide as the window, the work at hand
        // over one row of tabs, and under them one thing at a time: the choice (or the town in
        // its place), or one of the panels the other pages keep beside it
        constexpr float PhoneBelow = 600.0f;       // A window narrower than this is a phone held upright
        constexpr float PhoneMargin = 12.0f;
        constexpr float PhoneTop = 84.0f;          // The work at hand, under the title and its two lines
        constexpr float PhoneTabs = 48.0f;         // The row of tabs, and the gap under it
        constexpr int PageSettle = 4;              // Passes before a page just built holds its pieces in its layouts
        constexpr float PhoneAge = 96.0f;          // The age, at the title's right
        constexpr float PhoneHeadGap = 12.0f;      // Between the title and his age as it is drawn

        // The note on his age as a phone's head says it, on a line of its own: what goes on from
        // the figure ("years · ") is left out, "42 mo to the twenty-first"
        std::string phoneAgeNote(const std::string& note)
        {
            const std::string from = "years \xC2\xB7 ";

            if (note.rfind(from, 0) == 0)
                return note.substr(from.size());

            return note == "years" ? std::string() : note;
        }

        struct PhoneTab
        {
            const char * label;        // The tab's, in small letters
            const char * panel;        // What it shows in the choice's place
        };

        constexpr PhoneTab PhoneTabsOf[] = {{"stats", "parts"}, {"items", "holds"}, {"years", "clockPanel"}, {"log", "happened"}};

        bool needsCompact(float width, float height)
        {
            return widthStepFor(width) < 0 or heightStepFor(height) < 0;
        }

        bool needsPhone(float width)
        {
            return width < PhoneBelow;
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

        // The phone page at a window's width: every panel is the column, and what stands in the
        // town's page (a layout, which takes no width from its panel) is the column less the
        // panel's padding. Written before the page is built, as the three columns' steps are
        void shapePhone(NodeSpec& page, float width)
        {
            const float column = std::max(MinCompactList, width - 2.0f * PhoneMargin);
            const float inner = column - SideChrome;

            for (const char* name : {"working", "may", "parts", "holds", "clockPanel", "happened", "tabs", "ageNote"})
                setProp(childNamed(page, name), "width", column);

            setProp(childNamed(page, "title"), "width", std::max(1.0f, column - PhoneAge));

            if (NodeSpec* may = childNamed(page, "may"))
            {
                for (const char* name : {"town", "places", "marketRows", "raised",
                                         "adventure", "recordsHead", "records", "recordsNote", "queueHead", "queueTitle", "queueCount", "queueTake", "queueUse",
                                         "queueBring", "queueTeach", "queueWarn", "queueGo", "atWalk"})
                    setProp(childNamed(*may, name), "width", inner);
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

        // How wide a text stands on one line in a style, unwrapped
        float lineWidthOf(EntitySystem* ecs, const std::string& style, const std::string& text)
        {
            auto* theme = ecs->getSystem<ThemeSystem>();
            auto* ttf = ecs->getSystem<TTFTextSystem>();

            if (text.empty() or not theme or not ttf)
                return 0.0f;

            const TextStyle& s = theme->style(style);

            TextLayoutParams params;
            params.spacing = s.lineSpacingPx;
            params.letterSpacing = s.letterSpacingPx;

            return ttf->measureText(s.fontAlias, text, params).width;
        }

        // A place whose next level a click on its tile would begin now: he can pay for it, and is
        // not at another work (one that takes no months is done whatever he is at)
        bool readyToRaise(const RulePlace& place, const std::string& running)
        {
            const std::string work = textOf(place.fields, "nextWork");

            return not work.empty() and work != running and not boolOf(place.fields, "nextLocked")
                and (running.empty() or intOf(place.fields, "nextMonths") <= 0);
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

        // A gloss's line for a thing taken from him, once: what it takes and what he holds of it,
        // "Berries −3 (has 4)", in the loss's colour while he holds too little (-1 held: nothing
        // to say of what he holds, the line says what it takes alone)
        GlossRow costRow(const std::string& name, int amount, int held)
        {
            if (held < 0)
                return {name, signedText(-amount), ""};

            return {name, signedText(-amount) + " (has " + std::to_string(held) + ")", held < amount ? "loss" : ""};
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

    bool LifeScene::buildPage(bool compactPage, bool phonePage, float width)
    {
        const std::string& file = phonePage ? opt.phoneFile : compactPage ? opt.compactFile : opt.pageFile;

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

        if (phonePage)
            shapePhone(*spec, width);
        else if (not compactPage)
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
        compact = compactPage or phonePage;
        phone = phonePage;
        phoneWidth = width;
        phoneFitted = width;
        pageSettling = PageSettle;
        phoneRefit = false;
        phonePanel.clear();
        handles.clear();

        // The town's page scrolls under a finger, or a mouse held down, as the lists do
        if (EntityRef townBody = named(TownName); not townBody.empty() and townBody->has<VerticalLayout>())
            townBody->get<VerticalLayout>()->dragToScroll = true;

        // A finger has no hover: on the phone page, or once a finger has been on the screen, a tap
        // on a thing shows its gloss, which stays until the next tap
        if (auto tooltips = ecsRef->getSystem<TooltipSystem>())
            tooltips->showOnPress = phone or touched;

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

        // Made again in the place of another scene (a save loaded, the chronicle burnt): what the
        // last one wrote of its life is not this one's
        if (opt.clearFacts)
        {
            if (auto facts = ecsRef->getSystem<WorldFacts>())
                facts->factMap.clear();
        }

        // How the player plays: before the page, which is built with the way a click begins a work
        readSettings();

        if (not buildPage(needsCompact(width, height), needsPhone(width), width))
            return;

        // The bug report's button, in the window's corner over every screen (lifereport.cpp): made
        // once with the scene, anchored to the window as the toasts are
        makeReportButton();

        // The Options page's controls (lifeoptions.cpp)
        listenToEvent<SwitchToggledEvent>([this](const SwitchToggledEvent& e) {
            // Under the bug report's dialog nothing of the game answers (lifereport.cpp)
            if (reportOpen)
                return;

            if (e.tag == "options.click")
            {
                settings.beginOnFirstClick = e.on;
                writeSettings();
                applyClickMode();
            }
        });
        listenToEvent<RadioChosenEvent>([this](const RadioChosenEvent& e) {
            if (reportOpen)
                return;

            if (e.tag == "options.light")
                chooseTheme(e.id);
        });
        listenToEvent<WordChangedEvent>([this](const WordChangedEvent& e) {
            if (e.tag == "options.burnWord")
                burnWordChanged(e.text);
            else if (e.tag == "report.words")
                reportFlow.message = e.text;   // Kept as he types: a dialog shut keeps them for the next time
        });

        // The lists of a walk's screens and of the Adventure page are the walk's to hear (lifewalk.cpp)
        // (none of them while the bug report's dialog is up: a key or a click that reached a list or
        // the tabs under it is not played)
        listenToEvent<ActivitySelectedEvent>([this](const ActivitySelectedEvent& e) { if (reportOpen) return; onSelect(e); onWalkSelect(e); });
        listenToEvent<ActivityHoveredEvent>([this](const ActivityHoveredEvent& e) { onHover(e); });
        listenToEvent<ActivityActivatedEvent>([this](const ActivityActivatedEvent& e) { if (reportOpen) return; onConfirm(e); onWalkConfirm(e); });
        listenToEvent<TabSelectedEvent>([this](const TabSelectedEvent& e) { if (reportOpen) return; onTab(e); });

        // A place's tile clicked: the work that raises it to its next level begins
        listenToEvent<PlaceSelectedEvent>([this](const PlaceSelectedEvent& e) { if (reportOpen) return; onPlace(e); });

        // At nothing, a month passes when he says so; at work the months run on their own. The
        // ending's one button begins the next life
        listenToEvent<ButtonActivatedEvent>([this](const ButtonActivatedEvent& e) {
            // The bug report's button, and its dialog's (lifereport.cpp)
            if (onReportButton(e.tag))
                return;

            // Its dialog up (on the web the page's own, not the `dialog` slot): nothing under it answers
            if (reportOpen)
                return;

            if (onOptionsButton(e.tag))
                return;

            // A question over the page: nothing under it answers
            if (not dialog.empty())
                return;

            if (onWalkButton(e.tag))
                return;

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
            else if (e.tag == HasteTag)
                pressHaste();
            else if (e.tag == AllTag and not chosen.empty())
            {
                doneNow = chosen;
                doAll(chosen);
                showWorkButtons();
            }
            else if (e.tag == BeginTag and resumes() and takenAtWork(activityOf(chosen)))
            {
                // Stopped at a work, with something chosen that may be taken while at it: that,
                // and not the work, is what the button goes on with
                doneNow = chosen;
                onConfirm(ActivityActivatedEvent{ActivitiesList, chosen});
                showWorkButtons();
            }
            else if (e.tag == BeginTag and resumes())
                pause(false);
            else if (e.tag == BeginTag and not chosen.empty())
            {
                // What it did at once stays chosen for the next press; once it can be done no more,
                // its button stays where it was, greyed (`spentChoice`)
                doneNow = chosen;
                onConfirm(ActivityActivatedEvent{ActivitiesList, chosen});
                showWorkButtons();
            }
        });

        // A deed reached: taken in execute(), never while the facts are being handed around
        listenToStandardEvent(AchievementUnlockEventName, [this](const StandardEvent& e) {
            auto name = e.values.find("name");

            if (name != e.values.end())
                reached.push_back(name->second.toString());
        });

        // The month loop: one month every monthMs() while it runs
        listenToEvent<TickEvent>([this](const TickEvent& e) {
            // The scene is being made again: nothing more passes here
            if (leaving)
                return;

            // The frame a bug report waits for may not come: the dialog opens without it in time
            runReport(e.tick);

            // An OS dialog held the game still: what it held is no time played, and no month runs on it
            if (skipTick)
            {
                skipTick = false;
                return;
            }

            // "Exported to" stays a moment in the place of the last-written line
            if (exportedLeft > 0.0f)
            {
                exportedLeft -= e.tick;

                if (exportedLeft <= 0.0f)
                    fillOptions();
            }

            // A file chosen in a browser is read by the browser: taken once it has come
            if (uploading)
                runUpload();

            sincePlaceClick += e.tick;

            // What lasts a moment runs its course whether the months run or not
            if (quiet > 0.0f)
            {
                quiet -= e.tick;

                // What was written while the chronicle stayed shut is said once the page is there
                if (quiet <= 0.0f and not awayLine.empty())
                {
                    toast(awayLine);
                    awayLine.clear();
                }
            }

            // The guide's hand moves whether the months run or not, and a step said for a time
            // runs it out (not under the bug report: the guide waits there with the game)
            handMs = std::fmod(handMs + e.tick, HandSwayMs);

            if (step.active and step.until.empty() and not reportAsked and not reportOpen)
                step.left -= e.tick;

            runPassing(e.tick);
            runToasts(e.tick);

            // A fight handed to his stance plays on, months or no months (not under the report:
            // runWalk holds it there)
            runWalk(e.tick);

            // The red of a loss fades whether the months run or not
            if (alertLeft > 0.0f)
            {
                alertLeft -= e.tick;

                if (alertLeft <= 0.0f)
                    clearAlert();
            }

            // Under the bug report the game waits: no month, no written time, no turn of a fight,
            // so the save that goes is the life in the picture. He is at the game, writing; the
            // months take up again when it shuts, with nothing owed for the time it stood open
            if (reportAsked or reportOpen)
            {
                stirred();
                return;
            }

            // Months that stand still with nobody at the game write time, as a game shut does
            runStill(e.tick);

            if (paused)
                return;

            // Written time runs the months so many times as fast. A month run so takes its
            // `aMonth` of it, at ×2 as at ×3 (a week of the player's for a month of his): what it
            // adds to the frame is what it pays for, and the frame it runs out the months are back
            // at their own pace
            sinceMonth += e.tick;

            if (haste > 1)
            {
                const float extra = e.tick * static_cast<float>(haste - 1);

                if (writtenAMonth > 0)
                {
                    const float wanted = (e.tick + extra) * static_cast<float>(writtenAMonth) * MsASecond / monthMs();

                    if (wanted > 0.0f)
                        sinceMonth += extra * spendWritten(wanted) / wanted;
                }
                else
                {
                    sinceMonth += spendWritten(extra);
                }

                if (written() <= 0)
                    setHaste(1);
            }

            // Asked again after each month: the one that makes him seven, or ten, slows the next
            for (float ms = monthMs(); sinceMonth >= ms; ms = monthMs())
            {
                sinceMonth -= ms;
                onMonth();
            }
        });

        // Somebody is at the game: the time the months stand still is play, not time away
        listenToEvent<OnMouseClick>([this](const OnMouseClick& e) {
            stirred();

            // A spent choice's greyed button goes at a press anywhere else: a press on it, of a
            // mouse still pressing where it did, finds it there and does nothing
            if (spentChoice() and not pressedOn("begin", e.pos) and not pressedOn("all", e.pos))
            {
                clearChoice();
                showWorkButtons();
                preview();
            }
        });

        // A finger has no hover: once one is on the screen, a press on a thing shows its gloss,
        // whichever page the window's width has built
        listenToEvent<OnSDLFingerDown>([this](const OnSDLFingerDown&) {
            if (touched)
                return;

            touched = true;

            if (auto tooltips = ecsRef->getSystem<TooltipSystem>())
                tooltips->showOnPress = true;
        });
        listenToEvent<OnMouseMove>([this](const OnMouseMove&) { stirred(); });
        listenToEvent<OnSDLMouseWheel>([this](const OnSDLMouseWheel&) { stirred(); });

        listenToEvent<OnSDLScanCode>([this](const OnSDLScanCode& e) {
            stirred();

            // The bug report's dialog takes the keys: they are his words. Escape shuts it, and what
            // came in the same pass with it is not played either (the browser's own dialog keeps
            // them from the game; this is the native one, and a key that slipped through)
            if (reportKeysShut)
                return;

            if (reportOpen)
            {
                if (e.key == SDL_SCANCODE_ESCAPE)
                {
                    closeReport();
                    reportKeysShut = true;
                }

                return;
            }

            // A question over the page takes the keys: Escape answers no, and nothing else is a shortcut
            if (not dialog.empty())
            {
                if (e.key == SDL_SCANCODE_ESCAPE)
                    closeDialog();

                return;
            }

            // A field being typed in takes the letters
            if (auto fields = ecsRef->getSystem<WordFieldSystem>(); fields and fields->anyTyping())
                return;

            auto theme = ecsRef->getSystem<ThemeSystem>();

            switch (e.key)
            {
            // Work only: at nothing a month passes by the button, never on its own. The month
            // under way keeps what it ran: the rule goes on from where it stopped
            case SDL_SCANCODE_SPACE:
                if (not save.running.empty() and not ended)
                    pause(not paused);
                break;

            // On a walk the days are his to choose: no month passes under them
            case SDL_SCANCODE_M:
                if (not onWalk())
                    onMonth();
                break;

            // Written time, as its button spends it
            case SDL_SCANCODE_F:
                if (hastens())
                    pressHaste();
                break;

            // The other light, as the Options page chooses it: kept for the next start too
            case SDL_SCANCODE_T:
                if (theme)
                    chooseTheme(theme->currentTheme() == "day" ? "candle" : "day");
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

        // A life opened on a walk or of a build is a later one, made here and never written, unless
        // --save asked for it
        if (not opt.walk.empty() or not opt.build.empty())
        {
            opt.fresh = true;
            opt.noSave = not opt.saveAsked;
            opt.lives = std::max(2, opt.lives);
        }

        // A save loaded from a file (the Options page) is the chronicle now, whatever else was asked
        if (opt.opened)
            save = *opt.opened;
        // A life made here is the first of its world, unless a later one was asked for
        else if (opt.fresh)
        {
            const bool first = opt.lives <= 1;

            save = freshLife();

            // Past his sixth year, for who wants the town's work at once
            if (opt.age >= 7.0f)
            {
                save = boyOfSeven();
                save.age = opt.age;
            }

            save.lives = std::max(1, opt.lives);
            save.guide = first ? std::max(0, opt.guide) : 0;
        }
        else if (opt.noSave)
            save = firstLife();
        else if (not save.load(opt.savePath))
            save = opt.freshWithoutSave ? freshLife() : firstLife();

        rules.done = save.terms();
        rules.world = save.world;
        rules.lives = save.lives;
        rules.running = save.running;
        rules.inn = save.town.count("inn") > 0 ? save.town.at("inn") : 0;
        rules.prime = save.stats.count(PrimeStat) > 0 ? save.stats.at(PrimeStat) : 0;

        // The guide's length, before the page is filled: a first life's guide line waits for its step
        readDeeds();
        renumberGuide();

        // A save from before the rations were given up front, in the middle of a work that feeds him.
        // After the guide is numbered as now: what is given is written at once, in this format
        catchUpRations();

        // The page is arriving: what a save was already short of is not news to say
        quiet = QuietMs;

        // A save from before the ledger followed the stats. What was written while it stayed shut
        // is held with them
        refreshHoldings();
        creditAway();
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

        // An event already open in a save opened again has been said: the months do not stop for it twice
        callEvents(false);

        tellAnalytics();

        // A save written at an ending opens on it
        if (not death.empty())
            endLife();

        // And one written on a walk opens on its screen, where he stood
        if (onWalk() and not ended)
        {
            paused = true;
            showWalk();
        }

        // Asked for a build (--build): he is that man, every place known, before anything else
        if (not opt.build.empty() and not onWalk() and not ended)
            applyBuild();

        // Asked to open on a walk: he is made fit for the zone, and sets out
        if (not opt.walk.empty() and not onWalk() and not ended)
            setOutAt(opt.walk);
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
        const bool wantPhone = needsPhone(width);
        const bool wantCompact = wantPhone or needsCompact(width, height);
        const int wantedWidth = widthStepFor(width);
        const int wantedHeight = heightStepFor(height);

        // The phone page is built at the window's width; at another its panels are given theirs
        // where they stand (fitPhone)
        const bool swap = wantCompact != compact or wantPhone != phone or (not wantCompact and (wantedWidth != widthStep or wantedHeight != heightStep));

        if (swap and not wantCompact)
        {
            widthStep = wantedWidth;
            heightStep = wantedHeight;
        }

        if (swap and not buildPage(wantCompact, wantPhone, width))
            return;

        if (EntityRef background = named("page"); not background.empty())
        {
            background->get<PositionComponent>()->setWidth(width);
            background->get<PositionComponent>()->setHeight(height);
        }

        guideFitted = guideRoom() + resumeRoom() + hasteRoom();

        if (phone)
            fitPhone(width, height);
        else if (compact)
            fitCompact(width, height);
        else
            fitFull(width, height);

        fitEnding(width, height);
        fitWalk(width, height);
        fitDialog(width, height);

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
        // The toasts with the rest
        while (not shownToasts.empty())
            dropToast(shownToasts.size() - 1);

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

        const float lasts = ms > 0.0f ? ms : ToastMs;

        // The same line again while it is up is not said twice: the one up stays longer
        for (auto& up : shownToasts)
        {
            if (up.text == text)
            {
                up.left = std::max(up.left, lasts);
                return;
            }
        }

        // A few at most: what comes in a crowd pushes the oldest out
        while (shownToasts.size() >= ToastMost)
            dropToast(0);

        // A dark slip at the foot of the window, its text in the paper's colour, one over another
        // when several come together. A line wider than the window wraps inside it
        const float room = std::max(1.0f, window->get<PositionComponent>()->width - 2.0f * (ToastSide + ToastPad));

        LabelSpec spec;
        spec.style = "control";
        spec.color = "folio";
        spec.text = text;
        spec.z = static_cast<int>(ToastZ) + 1;

        if (lineWidthOf(ecsRef, spec.style, text) > room)
        {
            spec.overflow = Overflow::Wrap;
            spec.width = room;
        }

        Label label = makeLabel(ecsRef, spec);

        auto anchor = label.entity.get<UiAnchor>();
        anchor->setHorizontalCenter(PosAnchor{window->id, AnchorType::HorizontalCenter});
        anchor->setBottomAnchor(PosAnchor{window->id, AnchorType::Bottom});

        // A line's slip is ToastHeight tall: one of more lines keeps the same room around its text
        auto theme = ecsRef->getSystem<ThemeSystem>();
        const float line = theme ? static_cast<float>(theme->style(spec.style).lineHeightPx) : 0.0f;
        const float height = std::max(ToastHeight, label.entity->get<PositionComponent>()->height + ToastHeight - line);

        auto ground = makeUiSimple2DShape(ecsRef, Shape2D::Square, 1.0f, height);
        ground.get<PositionComponent>()->setZ(ToastZ);
        ecsRef->attach<ThemeComponent>(ground.entity, "toast.ground");

        auto under = ground.get<UiAnchor>();
        under->setHorizontalCenter(PosAnchor{label.entity.id, AnchorType::HorizontalCenter});
        under->setVerticalCenter(PosAnchor{label.entity.id, AnchorType::VerticalCenter});
        under->setWidthConstrain(PosConstrain{label.entity.id, AnchorType::Width, PosOpType::Add, 2.0f * ToastPad});

        shownToasts.push_back({text, label.entity.id, ground.entity.id, height, lasts});

        stackToasts();
    }

    void LifeScene::runToasts(float ms)
    {
        bool gone = false;

        for (size_t i = 0; i < shownToasts.size();)
        {
            shownToasts[i].left -= ms;

            if (shownToasts[i].left > 0.0f)
            {
                ++i;

                continue;
            }

            dropToast(i);
            gone = true;
        }

        // The ones over a toast that went come down into its room
        if (gone)
            stackToasts();
    }

    void LifeScene::dropToast(size_t index)
    {
        if (index >= shownToasts.size())
            return;

        for (auto id : {shownToasts[index].textId, shownToasts[index].groundId})
        {
            if (ecsRef->getEntity(id))
                ecsRef->removeEntity(id);
        }

        shownToasts.erase(shownToasts.begin() + static_cast<std::ptrdiff_t>(index));
    }

    void LifeScene::stackToasts()
    {
        // The lowest at the foot of the window, each of the others over the ones under it. The room
        // under a slip's text is the same whatever its lines: its text stands where a line's would
        float below = ToastBottom;

        for (const auto& up : shownToasts)
        {
            if (auto text = ecsRef->getEntity(up.textId); text and text->has<UiAnchor>())
                text->get<UiAnchor>()->setBottomMargin(below);

            below += up.height + ToastGap;
        }
    }

    void LifeScene::showWorkButtons()
    {
        auto working = piece<Panel>("working");

        if (not working)
            return;

        // At nothing one button stands in the running row's place: the one that begins what is
        // chosen in the list (Buy, Sell or Make for a trade of the market, Begin for a work), or the
        // one that passes a month when nothing is. A life that has done nothing yet is led to its
        // first work: no month to pass before one is begun, and a line that says how
        const bool idle = save.running.empty();
        const RuleActivity* pick = chosen.empty() ? nullptr : activityOf(chosen);
        const bool leads = led() and not firstWork().empty();

        // What its button did, chosen still and to be done no more (the last lot sold): the button
        // stays where it was, greyed, and nothing comes into its place under a mouse still pressing
        const bool spent = spentChoice();

        // A work that is stopped has the same button under its row, to go on: a key is not
        // something every screen has. What is chosen that may be taken while at it (a thing done
        // at once, an answer to an event) has it too, stopped or not: then the button takes that.
        // A click on a tile only chooses it, here as in the town
        const bool stopped = resumes();
        const bool begins = pick and not spent and not ended and (idle or takenAtWork(pick));

        auto show = [this, &working](const char* name, bool shown) {
            if (EntityRef button = named(name); not button.empty())
            {
                wrapIn(working->body, button)->get<PositionComponent>()->setVisibility(shown);
                button->get<PositionComponent>()->setVisibility(shown);
            }

            if (auto button = piece<Button>(name))
                button->setDisabled(ecsRef, not shown);
        };

        // On the phone page the panel keeps its button's room while nothing is chosen yet: the
        // tabs and the choice under it stay where they are when a task is tapped, and a second
        // tap finds the task where the first did. The button waits there, greyed
        const bool waits = phone and idle and not begins and leads and not ended;

        // A first life has its guide to say it, on a leaf of its own
        show("guide", leads and not guiding() and not waits);
        show("skip", idle and not begins and not leads and not spent);
        show("begin", begins or waits or stopped or spent);

        // At its right, for what is done at once from what he holds, the button that does it as
        // often as he can. Outside the panel's stack, as the one below. A trade of the market
        // names it: Sell all, Buy all, Make all (the rules' `trade`)
        const bool all = begins and repeats(pick);
        const std::string trade = pick ? textOf(pick->fields, "trade") : std::string();

        if (EntityRef button = named("all"); not button.empty())
        {
            button->get<PositionComponent>()->setVisibility(all);

            // Hung at the right of the button it repeats, on its line. The page's file cannot
            // hang it there (that button is inside "At work now", out of reach of a name at the
            // page's top), nor can the page as it is built: what the file makes of a piece hangs
            // it on its holder a frame later. Once shown it follows the button wherever it goes
            if (EntityRef begin = named("begin"); all and not begin.empty() and button->has<UiAnchor>())
            {
                auto anchor = button->get<UiAnchor>();
                anchor->setLeftAnchor(PosAnchor{begin.id, AnchorType::Right});
                anchor->setLeftMargin(AllGap);
                anchor->setTopAnchor(PosAnchor{begin.id, AnchorType::Top});
            }
        }

        if (auto button = piece<Button>("all"))
        {
            const char* label = trade == "sell" ? "Sell all" : trade == "buy" ? "Buy all" : trade == "make" ? "Make all" : "All";

            if (all and button->spec.label != label)
                button->setLabel(ecsRef, label);

            button->setDisabled(ecsRef, not all);
        }

        // At their right, on their line, the button that spends written time, while any is held.
        // It stands outside the panel's stack: where none of them is shown (a running row, the
        // line of a life that has done nothing yet) the panel keeps it the room of one
        show("hasteRow", hastens() and not ((idle and not begins and not leads) or begins or waits or stopped or spent));

        if (EntityRef fast = named("haste"); not fast.empty())
            fast->get<PositionComponent>()->setVisibility(hastens());

        if (auto fast = piece<Button>("haste"))
        {
            fast->setLabel(ecsRef, "Months \xC3\x97" + std::to_string(haste));
            fast->setDisabled(ecsRef, not hastens());
        }

        if (stopped)
        {
            if (auto begin = piece<Button>("begin"))
            {
                begin->setLabel(ecsRef, "Go on");
                begin->setMonths(ecsRef, -1);
            }
        }

        if (waits)
        {
            if (auto begin = piece<Button>("begin"))
            {
                begin->setLabel(ecsRef, "Begin");
                begin->setMonths(ecsRef, -1);
                begin->setDisabled(ecsRef, true);
            }
        }

        if (spent)
        {
            if (auto begin = piece<Button>("begin"))
            {
                // A sale with nothing left to sell, or a thing bought that is bought once
                const RuleActivity* left = activityOf(chosen);

                begin->setLabel(ecsRef, left and (boolOf(left->fields, "listed") or repeats(left)) ? "None left" : "Done");
                begin->setMonths(ecsRef, -1);
                begin->setDisabled(ecsRef, true);
            }
        }

        if (not begins)
            return;

        if (auto begin = piece<Button>("begin"))
        {
            // What takes no time is done on the spot, and says so: a trade of the market by its
            // name (Buy, Sell, Make)
            const int months = intOf(pick->fields, "months");
            const char* now = trade == "buy" ? "Buy" : trade == "sell" ? "Sell" : trade == "make" ? "Make" : "Do it now";

            begin->setLabel(ecsRef, months <= 0 ? now : idle ? "Begin" : "Answer now");
            begin->setMonths(ecsRef, months > 0 ? months : -1);
        }
    }

    bool LifeScene::answersEvent(const RuleActivity* activity) const
    {
        return activity and not textOf(activity->fields, "event").empty() and boolOf(activity->fields, "listed") and not boolOf(activity->fields, "locked");
    }

    bool LifeScene::takenAtWork(const RuleActivity* activity) const
    {
        if (not activity or ended or onWalk() or not boolOf(activity->fields, "listed") or boolOf(activity->fields, "locked"))
            return false;

        if (textOf(activity->fields, "id") == save.running)
            return false;

        // What takes no time is done whatever else he is at
        if (intOf(activity->fields, "months") <= 0)
            return true;

        // An answer to an event sets the work aside, once at a time, and never a walk's months
        const RuleActivity* at = activityOf(save.running);

        return answersEvent(activity) and save.setAside.empty() and at and textOf(at->fields, "zone").empty();
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
        return (save.running.empty() ? PanelChrome + SkipButton : RunningPanel) + guideRoom() + resumeRoom() + hasteRoom();
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

    bool LifeScene::resumes() const
    {
        // A work stopped, by the key or by a part that fails. At nothing the months do not run on
        // their own: there is nothing to go on with
        // On a story's screen its row runs, but its days move the months, not the calendar
        return not save.running.empty() and paused and not ended and not onWalk();
    }

    float LifeScene::resumeRoom() const
    {
        return buttonAtWork() ? SkipButton : 0.0f;
    }

    bool LifeScene::buttonAtWork() const
    {
        if (save.running.empty() or ended)
            return false;

        if (resumes() or spentChoice())
            return true;

        const RuleActivity* pick = chosen.empty() ? nullptr : activityOf(chosen);

        return pick and takenAtWork(pick);
    }

    bool LifeScene::spentChoice() const
    {
        // Only what its button did: a work that left the list on its own is let go of as before
        if (chosen.empty() or chosen != doneNow or ended)
            return false;

        const RuleActivity* pick = activityOf(chosen);

        return not pick or boolOf(pick->fields, "locked") or not boolOf(pick->fields, "listed");
    }

    bool LifeScene::repeats(const RuleActivity* activity) const
    {
        // Done at once from what he holds, as often as he holds it: a sale, berries dried, a
        // remedy brewed; and a buy or a making of the market (the rules' `trade`), as often as his coin and
        // his room let him. Never else what spends coin, nor what is done a set number of times
        // (a blade, a horse: bought once)
        if (not activity or intOf(activity->fields, "months") > 0 or intOf(activity->fields, "uses") > 0 or activity->costs.empty())
            return false;

        if (not textOf(activity->fields, "event").empty() or not textOf(activity->fields, "goto").empty())
            return false;

        if (textOf(activity->fields, "trade") == "buy" or textOf(activity->fields, "trade") == "make")
            return true;

        for (const auto& cost : activity->costs)
        {
            if (textOf(cost, "stat") == "coin")
                return false;
        }

        return true;
    }

    bool LifeScene::pressedOn(const char* name, Point2D at) const
    {
        auto button = piece<Button>(name);

        if (not button or button->face.empty() or not button->face->has<PositionComponent>())
            return false;

        auto pos = button->face->get<PositionComponent>();

        return pos->isRenderable() and at.x >= pos->x and at.x <= pos->x + pos->width and at.y >= pos->y and at.y <= pos->y + pos->height;
    }

    bool LifeScene::hastens() const
    {
        // A first life is walked through its first works before anything else is put under them
        return written() > 0 and not ended and not guiding();
    }

    float LifeScene::hasteRoom() const
    {
        // At nothing the panel counts a button's line already (Pass a month, Begin), and a
        // stopped work has Go on's, as what is chosen that may be taken while at it has Do it now's:
        // only a running row alone leaves it none
        return hastens() and not save.running.empty() and not buttonAtWork() ? SkipButton : 0.0f;
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

    void LifeScene::fitPhoneHead()
    {
        // His age stands at the title's right: the title ends a gap before it as it is drawn, not
        // before the widest it could be ("The Chronicle of Aldren of Bellm…" beside a 17.5 of
        // half that room)
        auto title = piece<Label>("title");
        auto age = piece<Label>("age");

        if (not title or not age or age->entity.empty() or not age->entity->has<TTFText>())
            return;

        const float column = std::max(MinCompactList, windowWidth - 2.0f * PhoneMargin);
        const float drawn = age->entity->get<TTFText>()->textWidth;

        // And the bug report's button stands between them (lifereport.cpp)
        const float to = std::max(1.0f, column - (drawn > 0.0f ? drawn + PhoneHeadGap : PhoneAge) - reportRoom());

        if (std::abs(title->spec.width - to) > 0.5f)
            title->setWidth(ecsRef, to);
    }

    void LifeScene::fitPhone(float width, float height)
    {
        EntitySystem* ecs = ecsRef;

        const float column = std::max(MinCompactList, width - 2.0f * PhoneMargin);

        // What is under the tabs has the page to its bottom: the work at hand grows and shrinks
        // over it
        const float under = height - PhoneTop - workingHeight() - PhoneTabs - PhoneMargin;
        const float listHeight = std::max(MinCompactList, under - SideChrome);

        // The head, the tabs and every panel are the column. At another width than they were
        // made or last fitted at, each is given its own where it stands, and what stands in it
        // follows. A page just built does not hold its pieces yet: once it does (execute)
        if (std::abs(width - phoneFitted) > 0.5f)
        {
            if (pageSettling > 0)
            {
                phoneRefit = true;
            }
            else
            {
                phoneFitted = width;

                if (auto title = piece<Label>("title"))
                    title->setWidth(ecs, std::max(1.0f, column - PhoneAge));

                if (auto note = piece<Label>("ageNote"))
                    note->setWidth(ecs, column);

                if (auto tabs = piece<Tabs>("tabs"))
                    tabs->setWidth(ecs, column);

                for (const char* name : {"working", "parts", "holds", "clockPanel", "happened"})
                    widen(piece<Panel>(name), column);
            }
        }

        if (auto working = piece<Panel>("working"))
        {
            working->setWidth(ecs, column);

            if (auto running = piece<ActivityList>("running"))
                running->setSize(ecs, working->innerWidth(), 0.0f);
        }

        if (auto may = piece<Panel>("may"))
        {
            may->setWidth(ecs, column);

            if (auto list = piece<ActivityList>("activities"))
                list->setSize(ecs, may->innerWidth(), listHeight);

            fitTown(may->innerWidth(), listHeight);
        }

        if (auto log = piece<EventLog>("log"))
        {
            const bool atEnd = log->atEnd(ecs);

            log->setHeight(ecs, std::max(MinLog, under - SideChrome - LogFootnote));

            if (atEnd)
                followLog();
        }

        showPhone();
    }

    void LifeScene::showPhone()
    {
        if (not phone)
            return;

        // One thing under the tabs: the choice (the town when its page is the one open), or a panel
        if (EntityRef may = named("may"); not may.empty())
            may->get<PositionComponent>()->setVisibility(phonePanel.empty());

        for (const auto& tab : PhoneTabsOf)
        {
            if (EntityRef panel = named(tab.panel); not panel.empty())
                panel->get<PositionComponent>()->setVisibility(phonePanel == tab.panel);
        }
    }

    void LifeScene::showSide(int index)
    {
        const int count = static_cast<int>(std::size(SidePanels));

        if (index < 0 or index >= count)
            index = 0;

        sideTab = index;

        if (not compact or phone)
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

        // The ending leaves with the scene, as a scene element; so do a walk's screen, the guide's
        // hand and its shade
        ending = EntityRef{};
        walkPage = EntityRef{};
        walkScreen.clear();
        walkFilled.clear();
        walkPicks.clear();

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

        // Nothing the bug report asked for answers into a scene that is gone (lifereport.cpp)
        dropReport();

        // And what was passing ends now
        endPassing();
    }

    void LifeScene::execute()
    {
        // A page built a few passes ago holds its pieces now: a width that came meanwhile is
        // given to them
        if (pageSettling > 0 and --pageSettling == 0 and phoneRefit)
        {
            phoneRefit = false;

            if (phone)
                fit(windowWidth, windowHeight);
        }

        if (phone and pageSettling <= 0)
            fitPhoneHead();

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

        // The bug report: its sends' answers and its dialog pressed, then its button beside the head
        // of whatever screen is up now (lifereport.cpp)
        pollReport();
        placeReportButton();
        reportKeysShut = false;
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

        // (and which one: a step that waits for a given work to begin asks after it)
        if (not save.running.empty())
            facts["working." + save.running] = ElementType{1};
        facts["life.lives"] = ElementType{save.lives};
        facts["life.guide"] = ElementType{save.guide};
        facts["life.town"] = ElementType{townShown ? 1 : 0};

        // Where he stands on a walk's screens, and in a story or a fight
        walkFacts(facts);

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

        // What stays lit with it: a list with commas
        std::istringstream lights(textOf(entry.fields, "light"));

        for (std::string one; std::getline(lights, one, ',');)
        {
            if (not one.empty())
                reachedStep.light.push_back(one);
        }
        reachedStep.sayChosen = textOf(entry.fields, "sayChosen");
        reachedStep.until = entry.until;
        reachedStep.world = entry.world;
        reachedStep.left = hold == entry.fields.end() ? 0.0f : floatOf(hold->second);

        // A step of a guide that is over (skipped the pass it was reached in) is not said, nor one
        // the world has been told already
        if ((reachedStep.order > 0 and not guiding()) or (reachedStep.world and told(reachedStep.id)))
            return;

        // Nor a step the guide is already past: reached on the facts of a moment ago, while the
        // steps he is too old for were being passed over
        if (reachedStep.order > 0 and reachedStep.order <= save.guide)
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

        // Under the bug report the game waits, and so does the guide: no step ends or begins there
        // (its time stands still too: the tick holds it)
        if (reportAsked or reportOpen)
            return;

        passOutgrownSteps();

        // The step being said ends on what it waits for, or with its time
        if (step.active and (step.until.empty() ? step.left <= 0.0f : holds(step.until)))
            endStep();

        // The next one reached takes its place
        if (not step.active and not stepsDue.empty())
        {
            step = stepsDue.front();
            stepsDue.pop_front();
            step.active = true;

            // A press held on the leaf's Next was for the step before: let go over the same leaf,
            // now this one's, it does not pass this one too
            if (not noteNext.face.empty() and noteNext.face->has<ButtonState>())
                noteNext.face->get<ButtonState>()->pressed = false;
        }

        // The line of a life led with no guide came or went: what stands under "At work now" follows
        // (the page's own: under a walk's screen it waits for him to be back)
        if (not onWalk() and std::abs(guideRoom() + resumeRoom() + hasteRoom() - guideFitted) > 0.5f)
            fit(windowWidth, windowHeight);

        // A step is said on the screen it is about: over a walk's screen the shade, the leaf and
        // the hand stand over it, and a step of the page waits under it (and the other way round)
        liftGuide(onWalk());

        const bool said = step.active and onScreen(step.point);

        // Found again every time: a page built again has other tiles, a list that scrolled other
        // rows in view, a fight's screen made again other buttons
        const bool onward = stepChosen();
        const EntityRef target = said ? pointed(onward ? step.pointChosen : step.point) : EntityRef{};

        // What is lit with it, while the hand is on the thing itself: once it has gone on to the
        // button, the button alone
        std::vector<EntityRef> also;

        if (said and not onward)
        {
            for (const auto& one : step.light)
            {
                if (EntityRef other = pointed(one); not other.empty() and other.id != target.id)
                    also.push_back(other);
            }
        }

        pointAt(target, also);
        sayNote(target, said, also);
    }

    bool LifeScene::onScreen(const std::string& point) const
    {
        const std::string kind = point.substr(0, point.find(':'));
        const bool walks = std::find(WalkPoints.begin(), WalkPoints.end(), kind) != WalkPoints.end();

        return walks == onWalk();
    }

    void LifeScene::liftGuide(bool over)
    {
        if (over == guideLifted)
            return;

        guideLifted = over;

        for (EntityRef strip : veil)
            strip->get<PositionComponent>()->setZ(over ? WalkVeilZ : VeilZ);

        if (not hand.empty())
            hand->get<PositionComponent>()->setZ(over ? WalkHandZ : HandZ);

        // The leaf, piece by piece as it was made: its edge, its ground, its words and its buttons
        // (what a button draws follows its face)
        if (not note.empty())
        {
            const float z = over ? WalkNoteZ : NoteZ;

            note->get<PositionComponent>()->setZ(z);
            noteEdge->get<PositionComponent>()->setZ(z);
            noteGround->get<PositionComponent>()->setZ(z + 1.0f);
            noteWords.entity->get<PositionComponent>()->setZ(z + 4.0f);

            for (Button* button : {&noteSkip, &noteNext})
            {
                button->root->get<PositionComponent>()->setZ(z + 4.0f);
                button->face->get<PositionComponent>()->setZ(z + 4.0f);
            }
        }
    }

    void LifeScene::passOutgrownSteps()
    {
        // A step of the sequence is for before an age when it says so (the steps of his sixth
        // year): a boy of seven who let the year go by, or a save from before the year, is not
        // told of works that are closed to him
        bool passed = false;

        while (guiding())
        {
            const RuleAchievement* next = nullptr;

            for (const auto& deed : deeds)
            {
                if (deed.kind == RuleKind::Guide and deed.order == save.guide + 1)
                    next = &deed;
            }

            if (not next)
                break;

            const auto until = next->fields.find("untilAge");
            const float age = until == next->fields.end() ? 0.0f : floatOf(until->second);

            // (an age is a sum of twelfths: seven is seven, give or take the last bit)
            if (age <= 0.0f or save.age + 0.001f < age)
                break;

            save.guide = next->order;
            passed = true;
        }

        if (passed)
            forgetPassedSteps();
    }

    void LifeScene::passStorySteps()
    {
        // The story is over before its steps were all said (he turned back from it): the steps
        // of the sequence that wait for a walk's screens are passed over unsaid, so that none of
        // them waits on the Life page for a screen he will not see again
        bool passed = false;

        while (guiding())
        {
            const RuleAchievement* next = nullptr;

            for (const auto& deed : deeds)
            {
                if (deed.kind == RuleKind::Guide and deed.order == save.guide + 1)
                    next = &deed;
            }

            if (not next)
                break;

            const std::string point = textOf(next->fields, "point");
            const std::string kind = point.substr(0, point.find(':'));

            if (std::find(WalkPoints.begin(), WalkPoints.end(), kind) == WalkPoints.end())
                break;

            save.guide = next->order;
            passed = true;
        }

        if (passed)
            forgetPassedSteps();
    }

    void LifeScene::forgetPassedSteps()
    {
        // The one being said and the ones waiting, when they are among those passed
        if (step.active and step.order > 0 and step.order <= save.guide)
            step = GuideStep{};

        stepsDue.erase(std::remove_if(stepsDue.begin(), stepsDue.end(), [this](const GuideStep& due) { return due.order > 0 and due.order <= save.guide; }), stepsDue.end());

        setFact("life.guide", save.guide);
    }

    bool LifeScene::stepChosen() const
    {
        // A work is begun in two presses: the hand shows the tile until it is the one chosen, then
        // the button that begins it; let go of, the tile has the hand again
        if (not step.active or step.pointChosen.empty())
            return false;

        // On a walk's screen, a work of its day chosen in its own list
        if (onWalk())
            return walkChosen(step.point) or std::any_of(step.light.begin(), step.light.end(), [this](const std::string& one) { return walkChosen(one); });

        if (chosen.empty())
            return false;

        // (of a choice, whichever of its tiles he takes)
        return step.point == TilePoint + chosen or std::find(step.light.begin(), step.light.end(), TilePoint + chosen) != step.light.end();
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
        const float noteZ = guideLifted ? WalkNoteZ : NoteZ;   // Over a walk's screen, or over the page

        // As wide as it is drawn, or as the window lets it be
        const float width = std::max(MinCompactList, std::min(NoteWidth, windowWidth - 2.0f * NoteMargin));

        auto root = makeAnchoredPrefab(ecsRef, 0.0f, 0.0f, noteZ);

        root.get<PositionComponent>()->setWidth(width);
        root.get<PositionComponent>()->setVisibility(false);

        note = root.entity;

        // It leaves with the scene, as the page does
        ecsRef->attach<SceneElement>(note);

        auto prefab = root.get<Prefab>();

        auto edge = makeUiSimple2DShape(ecsRef, Shape2D::Square, width, 1.0f);

        edge.get<PositionComponent>()->setZ(noteZ);
        edge.get<UiAnchor>()->setTopAnchor(PosAnchor{note.id, AnchorType::Top});
        edge.get<UiAnchor>()->setLeftAnchor(PosAnchor{note.id, AnchorType::Left});
        ecsRef->attach<ThemeComponent>(edge.entity, NoteEdge);
        prefab->addToPrefab(edge.entity, "edge");

        noteEdge = edge.entity;

        auto ground = makeUiSimple2DShape(ecsRef, Shape2D::Square, width - 2.0f, 1.0f);

        ground.get<PositionComponent>()->setZ(noteZ + 1.0f);
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

        const int z = static_cast<int>(noteZ) + 4;

        LabelSpec words;
        words.style = "body";
        words.color = "ink";
        words.overflow = Overflow::Wrap;
        words.width = width - 2.0f * NotePad;
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

    void LifeScene::sayNote(EntityRef target, bool said, const std::vector<EntityRef>& also)
    {
        if (not step.active or not said)
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

        // The step's sentence, or the one of its second half once its tile is chosen; and for a
        // work of the Life page he has to go back to, the way there
        std::string words = stepChosen() and not step.sayChosen.empty() ? step.sayChosen : step.say;

        if (not stepChosen() and behindPage(step.point))
            words = BackToLife;
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

        // Under what is pointed at, from its left edge, and under what is lit with it (the tiles of
        // a choice may wrap onto a line under it); over them when the window ends there; never out
        // of the window. A step that points at nothing is said at the head of the page
        float x = std::max(NoteMargin, (windowWidth - pos->width) / 2.0f);
        float y = ColumnsTop;

        if (not target.empty())
        {
            auto where = target->get<PositionComponent>();
            float top = where->y;
            float bottom = where->y + where->height;

            for (EntityRef other : also)
            {
                if (other.empty() or not other->has<PositionComponent>())
                    continue;

                auto lit = other->get<PositionComponent>();

                top = std::min(top, lit->y);
                bottom = std::max(bottom, lit->y + lit->height);
            }

            x = std::max(NoteMargin, std::min(where->x, windowWidth - pos->width - NoteMargin));
            y = bottom + NoteGap;

            if (y + pos->height > windowHeight - NoteMargin)
                y = top - NoteGap - pos->height;

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

        // A thing of a walk's screen: found there, and only while it is up
        if (std::find(WalkPoints.begin(), WalkPoints.end(), kind) != WalkPoints.end())
        {
            target = walkPointed(kind, name);

            if (target.empty() or not target->has<PositionComponent>() or not target->get<PositionComponent>()->isRenderable())
                return EntityRef{};

            return target;
        }

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

        // A work of the Life page while the Town page or the Adventure page is the one in view (on
        // a phone, another tab's panel): the hand goes to the Life tab, and to the tile once he is
        // back
        if (behindPage(point))
        {
            if (auto tabs = piece<Tabs>("tabs"); tabs and not tabs->tabs.empty())
                target = tabs->tabs[0].face;
        }

        // On the phone page what the guide speaks of may be behind a tab: the hand goes to the tab,
        // and to the thing itself once the tab is open
        if (phone and not target.empty() and target->has<PositionComponent>() and not target->get<PositionComponent>()->isRenderable())
        {
            const char* panel = kind == "holding" ? "holds" : kind == "stat" ? "parts" : kind == "clock" ? "clockPanel" : kind == "log" ? "happened" : "";
            auto tabs = piece<Tabs>("tabs");

            for (const auto& tab : PhoneTabsOf)
            {
                if (not tabs or std::string(tab.panel) != panel)
                    continue;

                for (size_t i = 0; i < tabs->tabs.size() and i < tabs->spec.items.size(); ++i)
                {
                    if (lower(tabs->spec.items[i].label) == tab.label)
                        target = tabs->tabs[i].face;
                }
            }
        }

        // What is not drawn is not pointed at: a row not listed yet, a button not shown, a tile
        // scrolled out of its list, a panel the compact page keeps for another tab
        if (target.empty() or not target->has<PositionComponent>() or not target->get<PositionComponent>()->isRenderable())
            return EntityRef{};

        return target;
    }

    bool LifeScene::behindPage(const std::string& point) const
    {
        // (on a phone his stats, his things, his years or the log may stand in the choice's place)
        if (point.rfind(TilePoint, 0) != 0 or not (townShown or adventureShown or optionsShown or (phone and not phonePanel.empty())))
            return false;

        auto list = listHolding(point.substr(TilePoint.size()));

        return list != nullptr and list == piece<ActivityList>(ChoiceName);
    }

    void LifeScene::pointAt(EntityRef target, const std::vector<EntityRef>& also)
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
            Mark mark = makeMark(ecsRef, {HandGlyph, MarkSize::S24, HandColor, static_cast<int>(guideLifted ? WalkHandZ : HandZ)});

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

        // The rest of the page under a shade: what is pointed at stays lit, and what goes with it
        std::vector<EntityRef> lit = {target};

        lit.insert(lit.end(), also.begin(), also.end());

        shadeAround(lit);
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

            strip.get<PositionComponent>()->setZ(guideLifted ? WalkVeilZ : VeilZ);
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

        // (beside his age the note goes on from it, "years · 6 mo to the eighteenth"; on a phone
        // it is a line of its own under the year, and says the months alone)
        subs.push_back(router->on("life.headline.ageNote", [this, ecs](const ElementType& v) {
            if (auto label = piece<Label>("ageNote"))
                label->setText(ecs, phone ? phoneAgeNote(v.toString()) : v.toString());
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
                // The key that goes on, where the head has the room to say it and the screen a key
                working->setAside(ecs, workRoom() == 0 or phone ? "PAUSED" : "PAUSED \xC2\xB7 SPACE");
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

            if (not splitPath(rest, {"state", "count", "closes"}, id, field) or id == "running")
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
                    // The months left to begin it, and whether that is few: activity.<id>.urgent came before it
                    auto facts = ecs->getSystem<WorldFacts>();
                    const std::string urgent = "activity." + id + ".urgent";

                    row->setCloses(ecs, intOf(v), facts and facts->getFact<bool>(urgent, false));
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
        fillAdventure(true);

        // The town's tab is not there at all until a life has explored the town, and its page
        // stands where it stood
        if (auto tabs = piece<Tabs>("tabs"); tabs and townTab() >= 0)
            tabs->setShown(ecs, townTab(), save.townKnown);

        showTown(townShown);

        // The walk's screen stands over the page, whichever page it is
        if (onWalk() and walkPage.empty() and not ended)
            showWalk();

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
        std::vector<std::string> listedNow;

        for (const auto& a : activities)
        {
            if (boolOf(a.fields, "listed") and not boolOf(a.fields, "locked"))
                ids.push_back(textOf(a.fields, "id"));

            if (boolOf(a.fields, "listed"))
                listedNow.push_back(textOf(a.fields, "id"));
        }

        if (not known.empty())
        {
            // The ones that come with nothing of their own to say: named one by one while they are
            // few, counted when a birthday opens a list of them at once (the day he is seven)
            std::vector<std::string> plain;

            for (const auto& id : ids)
            {
                if (std::find(known.begin(), known.end(), id) != known.end())
                    continue;

                // A row that stood on the page before and had left it comes back as it was, no news:
                // a bind row while no slot was free. (One that stood locked and opens is news.)
                const bool stoodBefore = std::find(stood.begin(), stood.end(), id) != stood.end();

                if (stoodBefore and std::find(listedLast.begin(), listedLast.end(), id) == listedLast.end())
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
                    plain.push_back(id);
                }
                else
                {
                    appendLog({save.age, opens, LogKind::Milestone, "", ""});
                    toast(opens);
                }
            }

            if (plain.size() > NewNamed)
            {
                toast("New: " + std::to_string(plain.size()) + " tasks");
            }
            else
            {
                for (const auto& id : plain)
                    toast("New: " + activityName(id));
            }
        }

        known = ids;
        listedLast = listedNow;

        // A row that has never stood on a page is new to it, open to him or not: on the page that
        // is not the one in view, its tab says so. Once: what was there and can be done again (the
        // coin came back for what the market sells) is no news
        const bool filledBefore = not stood.empty();

        // The Adventure page comes with his first walk: that day its tab says how many places
        // stand behind it, as the Town's tab does the day the town opens
        const bool adventureHere = adventureKnown();

        if (not filledBefore)
            adventureWas = adventureHere;

        const bool adventureCame = adventureHere and not adventureWas;
        int placesThere = 0;

        adventureWas = adventureHere;

        for (const auto& a : activities)
        {
            const std::string id = textOf(a.fields, "id");

            // The Adventure page's own news, counted at its tab's corner while it is not the one
            // in view: a place whose tile comes onto it, open to him or not, and a place that
            // opens to him. Once each, and once for a tile that comes open
            if (not textOf(a.fields, "zone").empty() and boolOf(a.fields, "listed"))
            {
                ++placesThere;

                const bool tileNew = std::find(stoodWalks.begin(), stoodWalks.end(), id) == stoodWalks.end();
                const bool openNew = not boolOf(a.fields, "locked") and std::find(openedWalks.begin(), openedWalks.end(), id) == openedWalks.end();

                if (tileNew)
                    stoodWalks.push_back(id);

                if (openNew)
                    openedWalks.push_back(id);

                if ((tileNew or openNew) and filledBefore and adventureHere and not adventureCame and not adventureShown)
                    ++newInAdventure;
            }

            if (not boolOf(a.fields, "listed") or std::find(stood.begin(), stood.end(), id) != stood.end())
                continue;

            stood.push_back(id);

            const bool onTown = not (textOf(a.fields, "place") + textOf(a.fields, "raises")).empty();

            // A town he does not know has no page: what is done there is not on the Life page either
            if (onTown and not save.townKnown)
                continue;

            // And the walks are the Adventure page's once it is there
            if (not textOf(a.fields, "zone").empty() and adventureKnown())
                continue;

            const bool inView = onTown ? townShown : not townShown and not adventureShown and not optionsShown;

            if (filledBefore and not inView)
                ++(onTown ? newInTown : newInLife);
        }

        if (adventureCame and not adventureShown)
            newInAdventure = placesThere;

        // Grouped as the table orders them; what the rules do not list (spent, too late, another
        // path's) has no row
        std::vector<ActivityGroup> groups;
        std::vector<ActivityGroup> calls;     // The groups of the events open now: over the others

        // What is done at a place of the town stands on the Town page, never on this one: the
        // market's rows, what is bought, sold and made on the spot, and the storehouse's, what is drawn
        // from the stash. The works that raise a place have no row: a click on the place begins
        // its next one (onPlace). Each row with the id of its place
        struct Stall
        {
            std::string place;
            std::string trade;     // The rules' `trade`: "buy", "sell", "make" or "" (the market's lines)
            ActivityRowSpec row;
        };

        std::vector<Stall> stalls;

        listedRows.clear();

        for (const auto& a : activities)
        {
            // The work he is at keeps its place, even when it could no longer be begun
            if (not boolOf(a.fields, "listed") and textOf(a.fields, "id") != save.running)
                continue;

            listedRows += textOf(a.fields, "id") + ":" + std::to_string(a.requires.size()) + ";";

            const std::string group = textOf(a.fields, "group");

            // A work that raises a place is begun from its tile: no row
            if (not textOf(a.fields, "raises").empty())
                continue;

            const bool inTown = not textOf(a.fields, "place").empty();

            // What is done at a place of the town (its market) is on the Town page and
            // nowhere else: while he does not know the town, it has no row at all
            if (inTown and not save.townKnown and textOf(a.fields, "id") != save.running)
                continue;

            // The walks have a page of their own once he has been on one: their tiles are there,
            // with the walks he has made, and the Life page keeps none
            if (not textOf(a.fields, "zone").empty() and adventureKnown())
                continue;

            // An answer to an event of his life is on top of the list, in its event's group: it
            // comes once, and goes
            const bool answer = not textOf(a.fields, "event").empty();
            std::vector<ActivityGroup>& into = answer ? calls : groups;

            if (not inTown and (into.empty() or into.back().label != group))
                into.push_back({group, {}});

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

            // And it wears a mark of its own on its corner, with its red frame (the rules' `urgent`)
            if (answer)
                row.badge = "alert";

            if (row.state == ActivityState::Running)
            {
                runningProgress(ecsRef, row);
            }
            else
            {
                // The months he still has to begin it, counted down on its tile: nothing to say of the one he is at
                row.closes = closesOf(a);
                row.urgent = boolOf(a.fields, "urgent");
            }

            // A walk takes him out of the town: its tile says so
            row.adventure = not textOf(a.fields, "zone").empty();

            // (but for a count the rules keep for themselves: `hidden`)
            for (const auto& g : a.gains)
                if (not boolOf(g, "hidden"))
                    row.gains.push_back({textOf(g, "label"), intOf(g, "amount")});

            // What it takes when it begins, beside what it brings
            for (const auto& c : a.costs)
                row.gains.push_back({textOf(c, "label"), -intOf(c, "amount")});

            // A thing done at once has no months to write: its tile writes its price there
            // instead, beside the mark the ledger gives what it takes
            if (row.months <= 0 and not a.costs.empty())
            {
                const auto& price = a.costs.front();
                const std::string stat = textOf(price, "stat");

                // In the hand of the months it stands for: "20 coin", as "3 mo"
                std::string name = textOf(price, "name");
                std::transform(name.begin(), name.end(), name.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

                row.cost = std::to_string(intOf(price, "amount")) + " " + name;

                for (const auto& h : holdings)
                    if (textOf(h, "id") == stat and not textOf(h, "glyph").empty())
                        row.costGlyph = textOf(h, "glyph");
            }

            for (const auto& r : a.requires)
            {
                Requirement req;
                req.label = textOf(r, "label");
                req.current = intOf(r, "current");
                req.needed = intOf(r, "needed");
                row.requirements.push_back(req);
            }

            if (inTown)
                stalls.push_back({textOf(a.fields, "place"), textOf(a.fields, "trade"), row});
            else
                into.back().rows.push_back(row);
        }

        groups.insert(groups.begin(), calls.begin(), calls.end());

        if (auto list = piece<ActivityList>("activities"))
            list->setRows(ecsRef, groups);

        // The Town page's rows, a group a place in the order of the places: the market's first,
        // on three lines of their own by the rules' `trade`, each under its own heading
        // (what he buys, what he sells, then what is made there), then each other place's under
        // its name. A row of a place the town does not name stands with the market's
        std::vector<ActivityGroup> atPlaces{{"Buy", {}}, {"Sell", {}}, {"Make", {}}};
        const auto marketLines = static_cast<std::ptrdiff_t>(atPlaces.size());

        for (const auto& place : town.places)
        {
            const std::string id = textOf(place.fields, "id");

            if (id == "market")
                continue;

            ActivityGroup group{textOf(place.fields, "name"), {}};

            for (auto& stall : stalls)
            {
                if (stall.place == id)
                {
                    group.rows.push_back(stall.row);
                    stall.place.clear();
                }
            }

            if (not group.rows.empty())
                atPlaces.push_back(group);
        }

        for (const auto& stall : stalls)
        {
            if (stall.place.empty())
                continue;

            ActivityGroup& line = stall.trade == "buy" ? atPlaces[0] : stall.trade == "sell" ? atPlaces[1] : atPlaces[2];
            line.rows.push_back(stall.row);
        }

        // A line of the market with nothing on it has no heading either
        atPlaces.erase(std::remove_if(atPlaces.begin(), atPlaces.begin() + marketLines, [](const ActivityGroup& g) { return g.rows.empty(); }), atPlaces.begin() + marketLines);

        if (auto market = piece<ActivityList>(MarketName))
            market->setRows(ecsRef, atPlaces);

        // A walk that has come into the list brings the Adventure page's tab with it
        markAdventure();

        showNotices();
    }

    bool LifeScene::fromLists(const std::string& list) const
    {
        return list == ActivitiesList or list == MarketList;
    }

    ActivityList* LifeScene::listHolding(const std::string& id) const
    {
        for (const char* name : {ChoiceName, MarketName})
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
        for (const char* name : {ChoiceName, MarketName})
        {
            if (auto list = piece<ActivityList>(name); list and not list->selected().empty())
                list->select(ecsRef, "");
        }

        if (auto grid = piece<PlaceGrid>(PlacesName); grid and not grid->selected().empty())
            grid->select(ecsRef, "");

        chosen.clear();
        chosenList.clear();
    }

    int LifeScene::closesOf(const RuleActivity& activity) const
    {
        // -1 for what does not close, or not soon: what it asks of him (an age, a stat, coin) is
        // its gloss's to say, and its ground says that he cannot do it yet
        return activity.fields.count("closesIn") > 0 ? intOf(activity.fields, "closesIn") : -1;
    }

    void LifeScene::refreshHoldings()
    {
        RuleMonth month;

        if (rules.month(save.age, save.character(), month))
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

            // Written time is the holding the rules give a `haste`: how fast it runs the months,
            // and how much of it can be held
            writtenStat.clear();
            writtenMost = 0;
            hasteMost = 1;
            writtenAMonth = 0;

            for (const auto& h : holdings)
            {
                if (intOf(h, "haste") <= 1)
                    continue;

                writtenStat = textOf(h, "id");
                writtenMost = intOf(h, "limit");
                hasteMost = intOf(h, "haste");
                writtenAMonth = intOf(h, "aMonth");
            }

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

        // A leaf wider than the window (a phone's) is the window's width, less a margin
        float room, tall;
        windowSize(ecsRef, room, tall);
        room -= 2.0f * PhoneMargin;

        if (NodeSpec* leaf = childNamed(*spec, "leaf"))
        {
            auto drawn = leaf->props.find("width");

            if (drawn != leaf->props.end() and floatOf(drawn->second) > room)
                setProp(leaf, "width", std::max(MinCompactList, room));
        }

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
        write("endTally", epitaph.tally);
        write("endAsk", epitaph.ask);

        // The word asked of who played is an old man's ending's alone: out of the leaf's stack otherwise
        if (EntityRef ask = prefab->findEntity("endAsk"); not ask.empty())
        {
            const bool asked = not epitaph.ask.empty();

            if (EntityRef leaf = prefab->findEntity("leaf"); not leaf.empty() and leaf->has<Panel>())
                wrapIn(leaf->get<Panel>()->body, ask)->get<PositionComponent>()->setVisibility(asked);

            ask->get<PositionComponent>()->setVisibility(asked);
        }

        fillGifts();

        // Its story ends on what goes into the stash, which the gifts above have just read
        write("endStory", endingStory());

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

        // What the gift takes of each thing he holds: the stash keeps only the rest
        std::unordered_map<std::string, int> given;

        // What he held of it at his death, left to the place he chose: counted toward its next
        // level, for whoever raises it
        if (not gift.empty())
        {
            if (auto held = save.stats.find(giftStat()); held != save.stats.end() and held->second > 0)
            {
                save.fund[gift] += held->second;
                given[giftStat()] += held->second;
            }

            // And of the things he holds (resources, and what his walks found), what that place's
            // next level still asks: the rest goes into the stash when the stash keeps it, and ends
            // with him otherwise
            for (const auto& place : town.places)
            {
                if (textOf(place.fields, "id") != gift)
                    continue;

                for (const auto& left : place.leaves)
                {
                    save.fund[gift + "." + textOf(left, "stat")] += intOf(left, "amount");
                    given[textOf(left, "stat")] += intOf(left, "amount");
                }
            }
        }

        // What is left of the basics goes into the stash, for the lives after him: the gift is
        // taken first, and the stash keeps no more than its most of any one thing
        for (const auto& kept : town.stash)
        {
            const std::string stat = textOf(kept, "stat");
            auto held = save.stats.find(stat);

            if (held == save.stats.end())
                continue;

            const int left = held->second - given[stat];

            if (left > 0)
                save.stash[stat] = std::min(intOf(kept, "most"), save.stash[stat] + left);
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

    void LifeScene::renumberGuide()
    {
        // Written before the outskirts had their steps: the steps after his age (the classes, the
        // last word) were its 12 and 13, and are the last of this guide too. Once: from here the
        // save is of this build's format
        if (save.version < GuideNumbered and save.guide > OldGuideLast and guideSteps > OldGuideSteps)
            save.guide += guideSteps - OldGuideSteps;

        save.version = LifeSave::SaveVersion;
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

        // Where his old age begins, once the month he reached thirty settled it: the milestones
        // read no character, and are handed it by hand, as the Inn's level
        rules.prime = save.stats.count(PrimeStat) > 0 ? save.stats.at(PrimeStat) : 0;
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

            appendLog({save.age, parts + " is failing: the months stop. Press Go on, or SPACE", LogKind::Note, "", ""});
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

        // The Adventure page in view follows him: what a queue would take against what he holds
        // now, and whether he is free to set out
        if (adventureShown)
            fillAdventure();

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

            // Written time has no month's rate: its row says how fast it is being spent
            if (r.id == writtenStat and haste > 1)
                rate = "\xC3\x97" + std::to_string(haste);

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

        // (and at which one, "working.<id>": the one before reads 0 again)
        if (workSaid != save.running)
        {
            if (not workSaid.empty())
                setFact("working." + workSaid, 0);

            workSaid = save.running;
        }

        if (not save.running.empty())
            setFact("working." + save.running, 1);

        setFact("life.lives", save.lives);
        setFact("life.guide", save.guide);
        setFact("life.town", townShown ? 1 : 0);

        // And on a walk's screens: which one, the story and its day, the fight and its turn
        {
            ElementMap walking;
            walkFacts(walking);

            for (const auto& [path, value] : walking)
                setFact(path, value);
        }

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

        // The rule reaches next month's figure as the month ends, and holds while the months stop.
        // Sooner on written time, by as much as it runs the months faster
        setFact("activity.running.glideMs", paused ? 0.0f : std::max(0.0f, monthMs() - sinceMonth) / static_cast<float>(haste));
    }

    float LifeScene::monthMs() const
    {
        // Twelve months added one by one fall a hair to either side of the year
        if (save.age + 0.001f < EarlyUnderAge)
            return opt.monthMs * EarlyPace;

        if (save.age + 0.001f < ChildUnderAge)
            return opt.monthMs * ChildPace;

        return opt.monthMs;
    }

    int LifeScene::written() const
    {
        const auto held = save.stats.find(writtenStat);

        return writtenStat.empty() or held == save.stats.end() ? 0 : std::max(0, held->second);
    }

    unsigned long long LifeScene::wallClock() const
    {
        if (opt.now > 0)
            return opt.now;

        return static_cast<unsigned long long>(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    }

    void LifeScene::creditAway()
    {
        const unsigned long long since = save.savedAt;

        // A life never written has not stayed shut, and the rules may hold no such thing
        if (writtenStat.empty() or writtenMost <= 0 or since == 0)
            return;

        const unsigned long long now = wallClock();

        // Counted once: the next time away runs from here. A clock set back has written nothing
        save.savedAt = static_cast<size_t>(now);

        if (now < since + AwayLeast)
            return;

        // A second away is a second written, up to what can be held
        const int away = addWritten(static_cast<int>(std::min<unsigned long long>(now - since, static_cast<unsigned long long>(writtenMost))));

        if (away <= 0)
            return;

        // Before the page is built: the log's line is one of its own, the toast waits for it
        awayLine = std::to_string(away) + " seconds were written while the chronicle was shut: they run the months faster";

        save.log.push_back({save.age, awayLine, LogKind::Note, "", ""});
    }

    int LifeScene::addWritten(int seconds)
    {
        if (writtenStat.empty() or writtenMost <= 0)
            return 0;

        const int held = written();
        const int more = std::min(std::max(0, seconds), std::max(0, writtenMost - held));

        save.stats[writtenStat] = held + more;

        return more;
    }

    void LifeScene::stirred()
    {
        stillMs = 0.0f;
        stillLeft = 0.0f;
    }

    void LifeScene::runStill(float ms)
    {
        // Months that run are play, whoever watches them
        if (not paused)
        {
            stirred();
            return;
        }

        const float least = static_cast<float>(AwayLeast) * MsASecond;
        const float before = stillMs;

        stillMs += ms;

        if (stillMs < least)
            return;

        // The minute that made it time away is written with what follows, as a minute shut is
        stillLeft += before < least ? stillMs : ms;

        const int seconds = static_cast<int>(stillLeft / MsASecond);

        if (seconds <= 0)
            return;

        stillLeft -= static_cast<float>(seconds) * MsASecond;

        const bool had = written() > 0;

        if (addWritten(seconds) <= 0)
            return;

        publishWritten();

        // The button comes with the first of it
        if (not had)
            showWorkButtons();
    }

    float LifeScene::spendWritten(float ms)
    {
        auto held = save.stats.find(writtenStat);

        if (writtenStat.empty() or held == save.stats.end() or held->second <= 0)
            return 0.0f;

        // Held in seconds, spent in ms: the part of a second spent waits for the rest of it
        const float left = static_cast<float>(held->second) * MsASecond - writtenSpent;
        const float spent = std::min(ms, left);

        writtenSpent += spent;

        const int seconds = static_cast<int>(writtenSpent / MsASecond);

        held->second -= seconds;
        writtenSpent -= static_cast<float>(seconds) * MsASecond;

        // The last of it: nothing of a second is left over
        if (spent >= left or held->second <= 0)
        {
            held->second = 0;
            writtenSpent = 0.0f;
        }

        if (seconds > 0 or held->second <= 0)
            publishWritten();

        return spent;
    }

    void LifeScene::setHaste(int times)
    {
        haste = written() > 0 ? std::max(1, std::min(times, hasteMost)) : 1;

        // The running row's rule ends its month sooner or later, its ledger row and its button say how fast
        publishWritten();
        publishPace();
        showWorkButtons();
    }

    void LifeScene::pressHaste()
    {
        setHaste(haste < hasteMost ? haste + 1 : 1);
    }

    void LifeScene::publishWritten()
    {
        if (writtenStat.empty() or save.stats.count(writtenStat) == 0)
            return;

        setFact("resources." + writtenStat + ".value", holdingText(writtenStat, written()));
        setFact("resources." + writtenStat + ".rate", haste > 1 ? "\xC3\x97" + std::to_string(haste) : std::string());
        setFact("stat." + writtenStat, written());
    }

    void LifeScene::pause(bool on)
    {
        // On a walk's screen, or a story's, the calendar stays stopped: the days are his to choose
        if (not on and onWalk())
            return;

        paused = on;

        publishPace();

        // The button that goes on comes with the stop, and goes with it
        showWorkButtons();
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
                setFact("activity." + id + ".urgent", boolOf(a.fields, "urgent"));
                setFact("activity." + id + ".until", id == save.running ? std::string() : textOf(a.fields, "until"));
                setFact("activity." + id + ".closes", id == save.running ? -1 : closesOf(a));

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

        // One choice on the page: the other list lets go of its own, and the town of its place
        if (not chosen.empty())
        {
            for (const char* name : {ChoiceName, MarketName})
            {
                if (auto list = piece<ActivityList>(name); list and list->spec.id != event.list and not list->selected().empty())
                    list->select(ecsRef, "");
            }

            if (auto grid = piece<PlaceGrid>(PlacesName); grid and not grid->selected().empty())
                grid->select(ecsRef, "");
        }

        showWorkButtons();
        preview();
    }

    void LifeScene::onPlace(const PlaceSelectedEvent& event)
    {
        // A place to walk to, on the Adventure page ("walk.<zone>"): the page shows that place
        if (event.id.rfind("walk.", 0) == 0)
        {
            chooseZone(event.id.substr(5));
            return;
        }

        // Over: the ending, or a walk's screen, is over the page
        if (ended or onWalk())
            return;

        // A place of the town: a click chooses the work that raises it to its next level, as a
        // tile of the lists is chosen, and the button under "At work now" begins it, or a second
        // click soon after, as on a row. Its gloss says what it asks and what it will give
        const bool again = sincePlaceClick <= DoubleClickMs;

        sincePlaceClick = 0.0f;

        for (const auto& place : town.places)
        {
            if (textOf(place.fields, "id") != event.id)
                continue;

            const std::string name = textOf(place.fields, "name");
            const std::string work = textOf(place.fields, "nextWork");
            const RuleActivity* raise = work.empty() ? nullptr : activityOf(work);

            if (work.empty())
            {
                toast(name + " is raised as high as it goes");
                return;
            }

            // Already at it: the running row says so
            if (work == save.running)
                return;

            if (not raise or boolOf(raise->fields, "locked") or not boolOf(raise->fields, "listed"))
            {
                toast("Not yet: " + textOf(place.fields, "nextName") + " asks " + textOf(place.fields, "nextCosts"));
                return;
            }

            // What takes months waits for what he is at; what takes none may be done meanwhile
            if (not save.running.empty() and intOf(raise->fields, "months") > 0)
            {
                toast("Already at work: " + activityName(save.running));
                return;
            }

            // Clicked twice, the one chosen: begun as its button would
            if (again and work == chosen and chosenList == PlacesList)
            {
                doneNow = chosen;
                onConfirm(ActivityActivatedEvent{ActivitiesList, chosen});
                showWorkButtons();
                return;
            }

            if (work != chosen)
                ++save.picks;

            // One choice on the page: the lists let go of theirs. Theirs say so by an event that
            // may come after this: the choice is the town's by then, and stays
            for (const char* name : {ChoiceName, MarketName})
            {
                if (auto list = piece<ActivityList>(name); list and not list->selected().empty())
                    list->select(ecsRef, "");
            }

            chosen = work;
            chosenList = PlacesList;

            if (auto grid = piece<PlaceGrid>(PlacesName))
                grid->select(ecsRef, event.id);

            showWorkButtons();
            preview();
            return;
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
        // What is chosen says more than what the mouse is on, unless it can be done no more.
        // Nothing of either: no ghosts on the parts but what mends, and what he holds as it is
        const std::string id = chosen.empty() or spentChoice() ? hovered : chosen;

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
        // Over: the keyboard may still reach a row under the ending, or under a walk's screen
        if (not fromLists(event.list) or ended or onWalk())
            return;

        // A row confirmed by a double click is what its button did: chosen still once it can be
        // done no more, its button stays greyed
        doneNow = event.id;

        beginWork(event.id);
    }

    void LifeScene::beginWork(const std::string& id)
    {
        // A row that only goes to a page: no term, no month, no line
        if (const RuleActivity* asked = activityOf(id); asked and not textOf(asked->fields, "goto").empty())
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

        if (rules.forecast(save.age, save.character(), id, 0, forecast) and forecast.months <= 0)
        {
            doAtOnce(id, forecast);
            return;
        }

        if (not save.running.empty())
        {
            if (not takenAtWork(activityOf(id)))
            {
                appendLog({save.age, "Already at work: " + activityName(save.running), LogKind::Note, "", ""});
                return;
            }

            // An event of his life does not wait for his work: the work is set aside where it
            // stands, and goes on once the answer is done
            save.setAside = save.running;
            save.setAsideMonths = save.monthsIn;

            setFact("activity." + save.setAside + ".state", std::string("idle"));
            appendLog({save.age, activityName(save.setAside) + " is set aside for " + activityName(id) + ": it goes on once that is done", LogKind::Note, "", ""});

            save.running.clear();
            save.monthsIn = 0;
            rules.running.clear();
        }

        // The list refuses a locked row, a key or a script may not
        const RuleActivity* chosen = activityOf(id);

        if (not chosen or boolOf(chosen->fields, "locked") or not boolOf(chosen->fields, "listed"))
            return;

        // A story: its days are chosen on the walk's screen, and its months run as they add up
        if (not textOf(chosen->fields, "story").empty())
        {
            beginStory(id);
            return;
        }

        // A walk outside the town: he is there first, and its months run once he turns for home
        if (not textOf(chosen->fields, "zone").empty())
        {
            beginWalk(id);
            return;
        }

        save.running = id;
        save.monthsIn = 0;
        ++save.begun;
        rules.running = save.running;

        // What it costs is taken as it begins: the years of his life some works for the town ask,
        // then what it takes of what he holds. A work that feeds him gives the rations of its
        // months, and the log says so
        giveYears(intOf(chosen->fields, "yearMonths"));
        takeStats(forecast.atStart);
        writeEntries(forecast.begins);

        // Nothing is chosen any more, whether or not the list had its tile lit
        clearChoice();
        leadDue = false;

        setFact("activity." + id + ".state", std::string("running"));

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

    void LifeScene::leaveWork(const std::string& id, int monthsIn)
    {
        RuleForecast forecast;

        if (not rules.forecast(save.age, save.character(), id, monthsIn, forecast))
        {
            for (const auto& e : rules.errors)
                LOG_ERROR(DOM, e);

            return;
        }

        if (forecast.takenBack <= 0)
            return;

        takeStats(forecast.atLeave);
        writeEntries(forecast.leaves);
    }

    void LifeScene::catchUpRations()
    {
        // A save from before the rations were given up front: the work that feeds him under way
        // (or set aside for an answer) never gave them, and the months it has left must not starve
        // him. Given once, and the save says so from then on: written at once, so that a save opened
        // again before a month has passed is not given them twice. Written as it was read, the hour
        // it was shut with it: what was written while it stayed shut is counted after this
        if (save.boardUpFront)
            return;

        save.boardUpFront = true;

        const std::pair<std::string, int> works[] = {{save.running, save.monthsIn}, {save.setAside, save.setAsideMonths}};
        bool granted = false;

        for (const auto& [id, monthsIn] : works)
        {
            RuleForecast forecast;

            if (id.empty() or not rules.forecast(save.age, save.character(), id, monthsIn, forecast))
                continue;

            if (forecast.unworked > 0)
            {
                takeStats(forecast.atOwed);
                granted = true;
            }
        }

        // A work whose row is gone (Start Helping Out, read as the outskirts' row, which gives
        // nothing up front) is owed what it would have given: its rations, once
        for (const auto& [stat, amount] : save.owed)
        {
            save.stats[stat] += amount;
            granted = true;
        }

        save.owed.clear();

        if (granted and not opt.noSave and not save.save(opt.savePath, &settings))
            LOG_ERROR(DOM, "Could not save to " << opt.savePath);
    }

    void LifeScene::takeUpSetAside()
    {
        const std::string id = save.setAside;
        const int months = save.setAsideMonths;

        save.setAside.clear();
        save.setAsideMonths = 0;

        // What the answer closed is not taken up again: its months are lost
        if (const RuleActivity* left = activityOf(id); left and boolOf(left->fields, "closed"))
        {
            appendLog({save.age, activityName(id) + " is closed to him now, and is not taken up again", LogKind::Note, "", ""});

            // He leaves it before its term: what it gave up front for the months he never worked
            // there goes back
            leaveWork(id, months);

            return;
        }

        save.running = id;
        save.monthsIn = months;
        rules.running = id;

        setFact("activity." + id + ".state", std::string("running"));
        appendLog({save.age, "Back to " + activityName(id), LogKind::Note, "", ""});

        // The months go on as they did before the answer
        paused = false;
        sinceMonth = 0.0f;

        fillActivities();
    }

    void LifeScene::callEvents(bool stop)
    {
        // The events whose answers are in the list, by their group ("The Flood"), with the fewest
        // months left to begin one he can give. Not the one he is answering
        std::vector<std::pair<std::string, std::string>> open;
        std::unordered_map<std::string, int> left;
        const RuleActivity* at = activityOf(save.running);
        const std::string answering = at ? textOf(at->fields, "event") : std::string();

        for (const auto& a : activities)
        {
            const std::string event = textOf(a.fields, "event");

            if (event.empty() or event == answering or not boolOf(a.fields, "listed"))
                continue;

            if (left.count(event) == 0)
            {
                open.push_back({event, textOf(a.fields, "group")});
                left[event] = -1;
            }

            const int closes = closesOf(a);

            if (not boolOf(a.fields, "locked") and closes >= 0 and (left[event] < 0 or closes < left[event]))
                left[event] = closes;
        }

        auto once = [](std::vector<std::string>& seen, const std::string& event) {
            if (std::find(seen.begin(), seen.end(), event) != seen.end())
                return false;

            seen.push_back(event);
            return true;
        };

        std::string said;

        for (const auto& [event, group] : open)
        {
            const bool called = once(eventsCalled, event);
            const bool last = left[event] == 0 and once(eventsLastCalled, event);

            if (not stop)
                continue;

            if (called)
                said = group + "! Bellmoor calls on him: its answers are in red, and leaving it unanswered closes what they name";
            else if (last)
                said = group + "! This is the last month to answer it";
        }

        if (said.empty())
            return;

        toast(said);

        // At work the months stop on it, and the log says which key goes on; at nothing they
        // are stopped already
        if (not save.running.empty() and not paused)
        {
            appendLog({save.age, said + ". The months stop: answer it, or press Go on, or SPACE", LogKind::Note, "", ""});
            sinceMonth = 0.0f;
            pause(true);
        }
        else
        {
            appendLog({save.age, said, LogKind::Note, "", ""});
        }
    }

    bool LifeScene::doAtOnce(const std::string& id, const RuleForecast& forecast, bool batch)
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
                return false;

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

        if (not batch)
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
            return false;
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

        if (batch)
            return changed;

        if (changed)
            fillActivities();

        publishAll();

        autoSave();

        return changed;
    }

    namespace
    {
        // A log line's figure for a thing done `times` times: each number in it so many times
        // over ("+1 coin −4 hazelnuts", twice: "+2 coin −8 hazelnuts")
        std::string figureTimes(const std::string& figure, int times)
        {
            std::string out;
            size_t i = 0;

            while (i < figure.size())
            {
                if (std::isdigit(static_cast<unsigned char>(figure[i])))
                {
                    size_t end = i;

                    while (end < figure.size() and std::isdigit(static_cast<unsigned char>(figure[end])))
                        ++end;

                    out += std::to_string(std::stoll(figure.substr(i, end - i)) * times);
                    i = end;
                }
                else
                    out += figure[i++];
            }

            return out;
        }
    }

    void LifeScene::doAll(const std::string& id)
    {
        // As if its button were pressed while he can: then one line in the log for them all, and
        // the list, the page and the save brought up to date once
        RecordList entries;
        bool changed = false;
        int times = 0;

        for (; times < MostAtOnce; ++times)
        {
            const RuleActivity* activity = activityOf(id);
            RuleForecast forecast;

            if (not repeats(activity) or boolOf(activity->fields, "locked") or not boolOf(activity->fields, "listed"))
                break;

            if (not rules.forecast(save.age, save.character(), id, 0, forecast) or forecast.months > 0)
                break;

            // The first line is the thing done; what a time reaches past it (a step, a closing) is said each time
            for (size_t i = times == 0 ? 0 : 1; i < forecast.entries.size(); ++i)
                entries.push_back(forecast.entries[i]);

            changed = doAtOnce(id, forecast, true) or changed;

            if (ended)
                return;
        }

        if (times == 0)
            return;

        if (times > 1 and not entries.empty())
        {
            ElementMap& line = entries.front();

            line["text"] = ElementType{textOf(line, "text") + " \xC3\x97" + std::to_string(times)};
            line["figure"] = ElementType{figureTimes(textOf(line, "figure"), times)};
        }

        writeEntries(entries);

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

        // The phone page has one row of tabs for everything: a panel takes the choice's place, and
        // the Life and Town tabs give it back
        if (phone)
        {
            auto row = piece<Tabs>("tabs");
            const std::string label = row and event.index >= 0 and static_cast<size_t>(event.index) < row->spec.items.size() ? lower(row->spec.items[event.index].label) : std::string();

            phonePanel.clear();

            for (const auto& tab : PhoneTabsOf)
            {
                if (label == tab.label)
                    phonePanel = tab.panel;
            }

            showPhone();

            if (not phonePanel.empty())
                return;
        }

        // The Life page, the town's once a life has explored it, and the Adventure page once he has
        // somewhere to walk to: they share the middle column
        if (event.index == 0)
        {
            adventureShown = false;
            optionsShown = false;
            showTown(false);
            return;
        }

        if (event.index == optionsTab())
        {
            showOptions(true);
            return;
        }

        if (event.index == townTab() and save.townKnown)
        {
            showTown(true);
            return;
        }

        if (event.index == adventureTab() and adventureKnown())
        {
            showAdventure(true);
            return;
        }

        auto tabs = piece<Tabs>("tabs");

        if (not tabs)
            return;

        adventureShown = false;
        optionsShown = false;
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

        // The middle column holds one page at a time: the town's takes the Adventure page's place
        // too, and the Options page's
        if (townShown)
        {
            adventureShown = false;
            optionsShown = false;
        }

        // A page built without the Options page (the phone's) cannot show it
        if (optionsShown and named(OptionsName).empty())
            optionsShown = false;

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

        show(ChoiceName, not townShown and not adventureShown and not optionsShown);
        show(TownName, townShown);
        show(AdventureName, adventureShown);
        show(OptionsName, optionsShown);

        enableOptionsFocus(optionsShown);

        // Seen: what was new on the page now in view is new no longer
        if (townShown)
            newInTown = 0;
        else if (adventureShown)
            newInAdventure = 0;
        else if (not optionsShown)
            newInLife = 0;

        showNotices();

        setFact("life.town", townShown ? 1 : 0);

        // On the phone page a panel may stand in the choice's place: the town asked for takes it
        // back, and the choice does not take a panel's tab from it
        if (phone and (townShown or adventureShown or optionsShown) and not phonePanel.empty())
        {
            phonePanel.clear();
            showPhone();
        }

        const int tab = townShown ? townTab() : adventureShown ? adventureTab() : optionsShown ? optionsTab() : 0;

        if (auto tabs = piece<Tabs>("tabs"); tabs and tab >= 0 and tabs->active() != tab and phonePanel.empty())
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

        if (auto market = piece<ActivityList>(MarketName))
            market->setSize(ecsRef, width, 0.0f);

        if (auto label = piece<Label>(RaisedName))
            label->setWidth(ecsRef, width);

        // The Adventure page stands in the same place, at the same size, and so does the Options page
        fitAdventure(width, height);
        fitOptions(width, height);
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
            tile.ready = readyToRaise(place, save.running);
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

                // Lit while a click on it would raise it
                if (const bool ready = readyToRaise(place, save.running); tile->spec.ready != ready)
                    tile->setReady(ecsRef, ready);
            }

            // Who raised it, and when: under the grid for every place, and in its own gloss
            auto by = save.raisedBy.find(id);

            if (by != save.raisedBy.end())
                raised += (raised.empty() ? "" : "\n") + upper(textOf(place.fields, "name") + " \xC2\xB7 " + by->second);

            if (not registry)
                continue;

            // The gloss: what it is and every upgrade it has with what each gives, then its next one:
            // what it asks of him against what he has, and that a click on the place begins it
            GlossSpec gloss;
            gloss.inlineValues = true;
            gloss.title = textOf(place.fields, "name");
            gloss.aside = std::to_string(level) + "/" + std::to_string(most);
            gloss.text = textOf(place.fields, "about");

            // Every upgrade it has and what each gives: a level adds to what the ones before it give.
            // What he has raised stands in bold, what is still to raise in the faint ink
            if (not place.tiers.empty())
                gloss.rows.push_back({"UPGRADES", "", "", true});

            for (const auto& tier : place.tiers)
            {
                GlossRow row{std::to_string(intOf(tier, "level")) + " - " + textOf(tier, "built") + ": " + textOf(tier, "gives"), "", "gain"};
                row.strong = boolOf(tier, "reached");
                row.faint = not row.strong;

                gloss.rows.push_back(row);
            }

            if (by != save.raisedBy.end())
                gloss.rows.push_back({"Raised by", by->second, "muted"});

            const std::string work = textOf(place.fields, "nextName");

            if (not work.empty())
            {
                gloss.rows.push_back({"NEXT UPGRADE", "", "", true});
                // Its time, then each thing it takes once, on a line of its own: what it takes and
                // what he holds of it
                if (not textOf(place.fields, "nextTime").empty())
                    gloss.rows.push_back({"It takes", textOf(place.fields, "nextTime"), "time"});

                for (const auto& gap : place.gaps)
                    gloss.rows.push_back(costRow(textOf(gap, "label"), intOf(gap, "needed"), intOf(gap, "current")));

                if (intOf(place.fields, "fund") > 0)
                    gloss.rows.push_back({town.fundLine, std::to_string(intOf(place.fields, "fund")), "gain"});

                gloss.footnote = upper(std::string(phone ? "Tap" : "Click") + " the place to begin \"" + work + "\"");
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
        tabs->setNotice(ecsRef, 0, townShown or adventureShown or optionsShown ? newInLife : 0);

        if (townTab() >= 0)
            tabs->setNotice(ecsRef, townTab(), townShown ? 0 : newInTown);

        // A place that has opened to him since he last looked at the Adventure page
        if (adventureTab() >= 0)
            tabs->setNotice(ecsRef, adventureTab(), adventureShown or not adventureKnown() ? 0 : newInAdventure);
    }

    void LifeScene::raisePlace(const std::string& place, int level)
    {
        // What was left to it went into the level, as much as the level asked of it (town.pg's
        // `spends`, read with the place still one level below): its coin, and each thing under
        // "<place>.<thing>". What is over stays in the fund, toward the next level
        RuleTown before;

        if (rules.town(save.age, save.character(), before))
        {
            for (const auto& shown : before.places)
            {
                if (textOf(shown.fields, "id") != place or intOf(shown.fields, "level") != level - 1)
                    continue;

                for (const auto& spent : shown.spends)
                {
                    const std::string stat = textOf(spent, "stat");
                    const std::string key = stat == "coin" ? place : place + "." + stat;

                    if (auto held = save.fund.find(key); held != save.fund.end())
                    {
                        held->second -= intOf(spent, "amount");

                        if (held->second <= 0)
                            save.fund.erase(held);
                    }
                }
            }
        }

        save.town[place] = level;

        // The milestones read no character: the Inn's level, which moves the end of his prime, is theirs by hand
        if (place == "inn")
            rules.inn = level;

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

        // Only who knows the town has a town to leave anything to, and only what he holds of it.
        // The town is read all the same: what goes into the stash is its to say, town or none
        auto held = save.stats.find(giftStat());
        const int amount = save.townKnown and held != save.stats.end() ? held->second : 0;

        std::vector<const RulePlace*> open;
        const bool read = rules.town(save.age, save.character(), town);

        if (save.townKnown and read)
        {
            for (const auto& place : town.places)
            {
                // The places on his page that can still be raised, and that he has something
                // for: coin, or what a walk brought that the place still asks
                const bool raisable = boolOf(place.fields, "shown") and intOf(place.fields, "level") < intOf(place.fields, "most");

                if (raisable and (amount > 0 or not place.leaves.empty()))
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
            line->get<Label>()->setText(ecsRef, amount > 0 ? town.giftBefore + std::to_string(amount) + town.giftAfter : town.giftThings);

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

            // What it would take of the things he holds, beside his coin
            if (const std::string things = textOf(place->fields, "leaveLine"); not things.empty())
                spec.label += " \xC2\xB7 " + things;
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

        // What the place takes is not in the stash: the story says so where it stands
        if (EntityRef story = ending->get<Prefab>()->findEntity("endStory"); not story.empty() and story->has<Label>())
            story->get<Label>()->setText(ecsRef, endingStory());
    }

    std::string LifeScene::endingStory() const
    {
        std::string line = town.stashLine;

        if (not gift.empty())
        {
            for (const auto& place : town.places)
            {
                if (textOf(place.fields, "id") == gift)
                    line = textOf(place.fields, "stashLine");
            }
        }

        return line.empty() ? epitaph.text : epitaph.text + " " + line;
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

        // What he holds works or wastes, whatever he is doing: he eats at any work (one that feeds
        // him gave its rations as he began it)
        RuleMonth month;

        if (rules.month(save.age, save.character(), month))
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

                // The way into a path, done: he is one of it, and its asks stand on his parts. A
                // story's is his only by its result, which the story's end reads (endStory)
                const RuleActivity* at = activityOf(save.running);

                if (at and boolOf(at->fields, "enters") and not storyMonth)
                    save.aim = textOf(at->fields, "path");

                // A work for the town: the place it raises, and the level it raises it to
                const std::string raised = at ? textOf(at->fields, "raises") : std::string();
                const int raisedTo = at ? intOf(at->fields, "level") : 0;

                // A walk outside the town: he is home from it
                const bool walked = at and not textOf(at->fields, "zone").empty();

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

                // Home from a walk: what a record brought is his now, and a queue sets out again
                if (walked and not storyMonth)
                {
                    homeFromWalk();

                    if (not save.running.empty())
                        fillActivities();
                }

                // An answer done: the work it set aside goes on. Not under a story: he is still
                // on its screen, and the calendar stays stopped until it is over
                if (not save.setAside.empty() and save.running.empty() and not storyMonth)
                    takeUpSetAside();
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

        // An event of his life come, or about to go: said, and the months stopped for it
        callEvents(true);

        // The month took from what the life hangs on: he is in danger, and the page says so
        if (not month.hurt.empty())
            alert(month.hurt);

        endangered = not month.hurt.empty();

        // A life that ended above is not written: loaded again, its last month ends it again. Nor
        // is a month of a story's day: the day is written once all its months have run (walkDo),
        // or a death in its second month would be lost to a save written after the first
        if (storyMonth)
            tellAnalytics();
        else
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

            // One or the other: what taking it gives up, in red under what it is
            gloss.caution = textOf(a.fields, "caution");

            // The number of times alone, over how many it can be done when that is limited (0: as
            // often as he likes)
            gloss.aside = std::to_string(intOf(a.fields, "done"));

            if (intOf(a.fields, "uses") > 0)
                gloss.aside += "/" + std::to_string(intOf(a.fields, "uses"));

            // A rule under the head, then what it is in time: how long, and the rations a work that
            // feeds him gives as he begins it, with how long they last when his familiars eat them
            // before its term (the rules count it: `upFrontLasts`). When it closes is the tile's to say
            gloss.rows.push_back({"", "", "", true});
            gloss.rows.push_back({"Time", intOf(a.fields, "months") > 0 ? std::to_string(intOf(a.fields, "months")) + " mo" : std::string("At once"), "time"});

            if (intOf(a.fields, "upFront") > 0)
            {
                std::string upFront = std::to_string(intOf(a.fields, "upFront")) + " up front";

                if (intOf(a.fields, "upFrontLasts") > 0)
                    upFront += " \xC2\xB7 lasts " + std::to_string(intOf(a.fields, "upFrontLasts")) + " mo";

                gloss.rows.push_back({"Rations", upFront, "gain"});
            }

            // What he has of it against what it asks, first: met in the gain's colour, short in
            // the loss's. The room a thing he holds still has is not said while there is some (its
            // ledger line says how full it is): only once there is none, and that is what stops him
            // A cost is not one of them: its own line says what it takes and what he holds of it
            std::vector<GlossRow> asked;

            for (const auto& r : a.requires)
            {
                const bool short_ = intOf(r, "current") < intOf(r, "needed");

                if (boolOf(r, "cost") or (boolOf(r, "room") and not short_))
                    continue;

                asked.push_back({textOf(r, "label"), std::to_string(intOf(r, "current")) + " / " + std::to_string(intOf(r, "needed")), short_ ? "loss" : "gain"});
            }

            if (not asked.empty())
                gloss.rows.push_back({"REQUIREMENTS", "", "", true});

            gloss.rows.insert(gloss.rows.end(), asked.begin(), asked.end());

            // Then in sections, a rule between two, each figure in the colour of the way it goes and
            // under the stat's full name: the section says whether it is taken or brought. What it
            // takes says what he holds of it, in the loss's colour while that is too little; the
            // work he is at has paid
            const bool atWork = textOf(a.fields, "id") == save.running;

            if (not a.costs.empty())
                gloss.rows.push_back({"COST", "", "", true});

            for (const auto& c : a.costs)
                gloss.rows.push_back(costRow(textOf(c, "name"), intOf(c, "amount"), atWork ? -1 : intOf(c, "held")));

            // (a count the rules keep for themselves is not among it: `hidden`)
            std::vector<GlossRow> brought;

            for (const auto& g : a.gains)
                if (not boolOf(g, "hidden"))
                    brought.push_back({textOf(g, "name"), signedText(intOf(g, "amount")), intOf(g, "amount") < 0 ? "loss" : "gain"});

            if (not brought.empty())
                gloss.rows.push_back({"REWARD", "", "", true});

            gloss.rows.insert(gloss.rows.end(), brought.begin(), brought.end());

            // What a part or a skill of his would make of it, before he has it: in the gain's colour
            // once he has, plain until then
            if (not a.steps.empty())
                gloss.rows.push_back({"BONUS", "", "", true});

            for (const auto& s : a.steps)
                gloss.rows.push_back({textOf(s, "label"), "", boolOf(s, "reached") ? "gain" : ""});

            // A walk's place: what each level of knowing it brings, as the Adventure page last read it
            if (auto ladder = walkLadders.find(textOf(a.fields, "zone")); ladder != walkLadders.end() and not textOf(a.fields, "zone").empty())
                gloss.rows.insert(gloss.rows.end(), ladder->second.begin(), ladder->second.end());

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

        const bool ok = writeSave();

        if (ok)
            appendLog({save.age, "The chronicle is written up to here", LogKind::Note, "", ""});
        else
            LOG_ERROR(DOM, "Could not save to " << opt.savePath);

        return ok;
    }

    std::vector<std::string> LifeScene::namedWorks() const
    {
        // As the rules have them: the works of his sixth year (what opens before he is seven:
        // which of the two he took, which two of the three), the way into a class, and what
        // opens at the age the classes do or later (a class's own works, old age's). A walk is
        // not one of them: the digest counts those by place
        std::vector<std::string> named;

        for (const auto& a : activities)
        {
            const std::string id = textOf(a.fields, "id");
            const auto from = a.fields.find("fromAge");

            if (id.rfind("walk.", 0) == 0 or from == a.fields.end())
                continue;

            const float age = floatOf(from->second);

            // (what is done in town is a boy's from six, and is no work of his sixth year)
            const bool inTown = not textOf(a.fields, "place").empty() or not textOf(a.fields, "raises").empty();

            if (boolOf(a.fields, "enters") or age >= LateFromAge or (age < EarlyUnderAge and not inTown))
                named.push_back(id);
        }

        return named;
    }

    void LifeScene::tellAnalytics(const std::string& event)
    {
        auto analytics = ecsRef->getSystem<Analytics>();

        if (not analytics)
            return;

        // Where the life stands, and which of the three pages it is read on
        analytics->note(Analytics::withFields(save.digest(namedWorks()), std::string("\"page\":\"") + (phone ? "phone" : compact ? "compact" : "full") + "\""));

        if (not event.empty())
            analytics->send(event);
    }

    void LifeScene::autoSave()
    {
        // Told whether or not the life is written: what is played is played
        tellAnalytics();

        if (opt.noSave or leaving)
            return;

        if (not writeSave())
            LOG_ERROR(DOM, "Could not save to " << opt.savePath);
    }

    bool LifeScene::writeSave()
    {
        // Made again on a save just written (Options' Load) or burnt (Wipe): this one is gone
        if (leaving)
            return false;

        // The hour it is written at: what the chronicle writes while it stays shut runs from it
        save.savedAt = static_cast<size_t>(wallClock());

        // The settings beside the life: one file holds everything the game keeps
        return save.save(opt.savePath, &settings);
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

        // Never a first life: it opens as every later one does, at 6 with the works of his sixth
        // year all in the list, and no guide
        clearGuide();

        // His events are still to come
        eventsCalled.clear();
        eventsLastCalled.clear();

        save = freshLife();

        // The world did not begin again with him: its date runs on from the last life's, and he
        // is one more of its lives. The town is the world's too: what was raised stays raised.
        // Its page is not: every boy is taken to the market once, in his sixth year, and has it
        // from there (`freshLife` knows no town)
        save.world = last.world;
        save.lives = last.lives + 1;
        save.town = last.town;
        save.raisedBy = last.raisedBy;
        save.fund = last.fund;
        save.seen = last.seen;
        save.told = last.told;

        // And so is what its walks have taught. The records are the one who walked them's, and
        // so is what they brought him: a new life has neither
        save.knowledge = last.knowledge;

        // And what the stash keeps of the basics the lives before him held: his to draw
        save.stash = last.stash;

        // Written time is the player's, not his: the one thing held that a death does not take
        if (auto held = last.stats.find(writtenStat); not writtenStat.empty() and held != last.stats.end())
            save.stats[writtenStat] = held->second;

        closeWalk();
        walk = RuleWalk{};
        adventureShown = false;

        newInLife = 0;
        newInTown = 0;
        newInAdventure = 0;
        openedWalks.clear();
        stoodWalks.clear();
        adventureWas = false;

        rules.world = save.world;
        rules.lives = save.lives;
        rules.running = save.running;

        // His old age is settled again at his thirtieth year, by the town as it is then
        rules.prime = 0;

        // And what the town gives a life at its birth: a stat, a holding, a tie
        if (rules.town(save.age, save.character(), town))
        {
            for (const auto& given : town.start)
                save.stats[textOf(given, "stat")] += intOf(given, "amount");
        }

        // The page does not open on the town, nor on the Options page
        townShown = false;
        optionsShown = false;

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
        listedLast.clear();

        // What he is born holding has its row from the first frame
        refreshHoldings();
        save.holdEarned(holdings);

        rebuild();
        publish();
        registerDeeds();
    }
}
