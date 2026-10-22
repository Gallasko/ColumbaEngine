#include "lifescene.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string>
#include <tuple>
#include <variant>
#include <vector>

#include "logger.h"

#include "ECS/entitysystem.h"
#include "ECS/callable.h"
#include "2D/position.h"
#include "2D/simple2dobject.h"
#include "2D/decoratedshapes.h"
#include "UI/prefab.h"
#include "UI/sizer.h"
#include "UI/prefabloader.h"
#include "UI/prefabbuilder.h"
#include "Systems/gamefacts.h"

#include "UI/label.h"
#include "UI/panel.h"
#include "UI/button.h"
#include "UI/gloss.h"
#include "UI/tabs.h"
#include "UI/eventlog.h"
#include "UI/activityrow.h"
#include "UI/ornament.h"
#include "UI/lifeclock.h"
#include "UI/statline.h"
#include "UI/progressrule.h"
#include "UI/resourceledger.h"
#include "UI/themesystem.h"
#include "Core/motion.h"

using namespace pg;

// The walks outside the town, as the Life scene plays them: this file is LifeScene's, the part
// of it that is a walk's. A walk is a row of the Life page like any other (the Adventure group,
// rules/activitytable.pg): begun, it opens the on-site screen over the page instead of running
// its months, the days of the stay are chosen one at a time there, a fight takes that screen's
// place and gives it back, and once he turns for home the walk's months run as any work's do.
// What he did is kept as a record, and the Adventure page walks it again, once or in a queue,
// with no day chosen and no fight fought.
//
// The scene computes nothing of it: every figure and every word is rules/expedition.pg's, asked
// through Rules::walk. The walk as it stands is the save's (LifeSave::walk), handed to the
// script and taken back at every step, so a walk closed in its middle opens where it stood.
namespace chronicle
{
    namespace
    {
        constexpr const char * const DOM = "Chronicle.Walk";

        constexpr const char * const ActivitiesList = "life.activities";

        // The lists of the walk's screens and of the Adventure page, by the id their events carry
        constexpr const char * const SeeksList = "walk.seeks";
        constexpr const char * const WorksList = "walk.works";
        constexpr const char * const RecordsList = "walk.records";
        const std::string ZoneTile = "walk.";              // A place's tile on the Adventure page: followed by its zone (PlaceSelectedEvent's id)

        // Their buttons, by tag
        constexpr const char * const WalkTag = "walk.";
        constexpr const char * const SeekTag = "walk.seek";
        constexpr const char * const ObserveTag = "walk.observe";
        constexpr const char * const WorkTag = "walk.work";
        constexpr const char * const LeaveTag = "walk.leave";
        constexpr const char * const OptionTag = "walk.option.";   // Followed by the option's place on the card
        constexpr const char * const ActTag = "walk.act";
        constexpr const char * const HandTag = "walk.hand";
        constexpr const char * const MoveTag = "walk.move.";       // Followed by the move's id: a button of his turn, pressed and done
        const std::string StoryTile = "tile.";             // A story's tile in the day's works: followed by its id in storytable.pg
        constexpr const char * const AimTag = "walk.aim.";         // Followed by the beast's place: against several, the one his blows go to
        constexpr const char * const FewerTag = "walk.fewer";
        constexpr const char * const MoreTag = "walk.more";
        constexpr const char * const StopTag = "walk.stop";
        constexpr const char * const OnceTag = "walk.once";
        constexpr const char * const ManyTag = "walk.many";
        constexpr const char * const LastTag = "walk.last";

        // The Adventure page's pieces (life.yaml and its two smaller pages)
        constexpr const char * const AdventureName = "adventure";
        constexpr const char * const RecordsName = "records";
        constexpr const char * const ZonesName = "zones";
        constexpr const char * const NewTag = "walk.new";
        constexpr size_t MaxRecords = 5;                   // The walks kept of a place: a sixth takes the oldest one's place
        const std::string RunTag = "walk.run.";            // Followed by a record's id: the small button of its line, which walks it once
        constexpr float AdventureGap = 12.0f;              // Between two things on a line of the Adventure page
        constexpr float AdventureCells = 420.0f;           // Under this width a walk's line has no cell for what it brings: its line says it
        constexpr float RecordBrings = 110.0f;             // The cell of what a walk brings, on its line


        const std::string WalkActivity = "walk.";          // An activity that is a walk: followed by its zone
        const std::string RecordGloss = "record/";
        const std::string FoeGloss = "walk/foe/";          // Followed by a beast's place: what it is, and what he knows of it
        const std::string FoeMoveGloss = "walk/foemove/";  // And the move it means to make
        const std::string MoveGloss = "walk/move/";        // Followed by a move's id: what it would do, before it is done
        const std::string WorkGloss = "walk/work/";        // Followed by a field work's id: a line on it, what it costs, what it gives
        const std::string KnowledgeGloss = "walk/knowledge";   // His panel on site: what knowing the place means, and what each level brings
        constexpr float MoveGap = 8.0f;                    // Between two buttons of his turn
        constexpr int HoverFrames = 12;                    // After they are made again: the frames in which the hover is worked out again
        constexpr float HurtMs = 320.0f;                   // A rule that was just taken from stays in the loss's colour that long
        constexpr float HurtRiseMs = 900.0f;               // And what was taken lifts off it for that long,
        constexpr float HurtRise = 0.02f;                  // by so many pixels a millisecond
        constexpr float HurtZ = 150.0f;                    // Over the screen, under the tooltips
        constexpr int MaxOptions = 4;                      // The buttons an event's card has (a story's card has up to four answers)
        constexpr int MaxFoes = 4;                         // The beasts the panel of what he fights has room for: a pack is four at most
        constexpr int MostWalks = 20;                      // The longest queue the form asks for
        constexpr int SettlePasses = 3;                    // A screen just built is filled again that many passes, once its layouts hold their pieces
        constexpr int TrialAge = 15;                       // --walk: the age of the life it opens on, when the zone asks no more,
        constexpr int TrialParts = 5;                      // and what each of his parts is given over a child's

        // The screens' frame (expedition.yaml, fight.yaml): where the columns start, their fixed
        // widths, and under what width the three stand in one
        constexpr float WalkMargin = 16.0f;
        constexpr float WalkGap = 12.0f;
        constexpr float WalkScroll = 4.0f;         // A column's lane past its panels
        constexpr float SiteTop = 136.0f;
        constexpr float SiteLeft = 300.0f;         // As the Life page's: how he stands and what he holds at the left,
        constexpr float SiteRight = 360.0f;        // what has happened at the right, the choice between them
        constexpr float TakenRoom = 200.0f;        // What he has taken, over the record: the record has the column's rest
        constexpr float FightTop = 72.0f;
        constexpr float OrderRoom = 230.0f;        // The order of the turn, over the account, before the page says how tall it stands
        constexpr float FightLeft = 280.0f;
        constexpr float FightRight = 376.0f;
        constexpr float PanelPad = 32.0f;          // A panel's two sides
        constexpr float RefitMs = 120.0f;          // A window being dragged: the screen is built again at most that often
        constexpr float CastMs = 450.0f;           // A blow about to go: its rule fills that long, then the blow is told
        constexpr float BeatMs = 140.0f;           // Between two lines of a turn as it is told
        constexpr float NarrowSiteTop = 160.0f;    // In one column the head's figures stand under its title, and the strip's caption takes two lines
        constexpr float NarrowFightTop = 92.0f;
        constexpr float NarrowFigure = 180.0f;     // The days left, or the turn, at the head's right
        constexpr float NarrowTitle = 120.0f;      // The least a title keeps beside it
        constexpr float HeadGap = 16.0f;           // Between a line of the head and the figure beside it, as drawn
        constexpr float NarrowBelow = 1000.0f;
        constexpr float PanelChrome = 83.0f;       // A panel with a heading: 16 + the 51 head block + 16
        constexpr float LeastLog = 160.0f;
        constexpr float LeastColumn = 200.0f;
        constexpr float NoteRoom = 64.0f;          // What stands under a log in its panel: a line or two, and the gaps
        constexpr float HeadRoom = 300.0f;         // The figures at the head's right, and air before them

        // What the veil calls when the mouse enters or leaves it: nothing
        struct WalkNoOp {};

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

        std::string upper(std::string s)
        {
            for (char& c : s)
            {
                if (c >= 'a' and c <= 'z')
                    c = static_cast<char>(c - 'a' + 'A');
            }

            return s;
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

        LogKind kindOf(const std::string& kind)
        {
            if (kind == "gain")      return LogKind::Gain;
            if (kind == "loss")      return LogKind::Loss;
            if (kind == "coin")      return LogKind::Coin;
            if (kind == "time")      return LogKind::Coin;
            if (kind == "milestone") return LogKind::Milestone;

            return LogKind::Note;
        }

        // In a file a piece stands in its panel's body inside a wrap, and the wrap is what the
        // layout holds: to take the piece out of the stack it is the wrap that is hidden
        EntityRef wrapIn(EntityRef body, EntityRef piece)
        {
            if (body.empty() or not (body->has<VerticalLayout>() or body->has<HorizontalLayout>()))
                return piece;

            // By value: a handle is asked for its components, which a const one cannot be
            std::vector<EntityRef> held;

            if (body->has<VerticalLayout>())
                held.assign(body->get<VerticalLayout>()->entities.begin(), body->get<VerticalLayout>()->entities.end());
            else
                held.assign(body->get<HorizontalLayout>()->entities.begin(), body->get<HorizontalLayout>()->entities.end());

            for (auto& child : held)
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

        // A node of a screen's file by its name, the ones a layout holds as well
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

        void setProp(NodeSpec* node, const std::string& key, float value)
        {
            if (node)
                node->props[key] = value;
        }

        // What a column has of the window's height, under the head
        float columnHeight(bool fighting, bool narrow, float height)
        {
            const float top = narrow ? (fighting ? NarrowFightTop : NarrowSiteTop) : (fighting ? FightTop : SiteTop);

            return std::max(LeastColumn, height - top - WalkMargin);
        }

        // And a log that fills its column: the panel's chrome and the lines under the log taken off
        float logHeight(float column)
        {
            return std::max(LeastLog, column - PanelChrome - NoteRoom);
        }

        // The beasts' panels, each that wide
        void shapeFoes(NodeSpec& page, float width)
        {
            for (int i = 0; i < MaxFoes; ++i)
                setProp(childNamed(page, "foePanel" + std::to_string(i)), "width", width);
        }

        // A walk's screen at a window's size, written over what its file is drawn with. Three
        // columns: the two at the sides hold their width and the middle one takes the rest. One
        // column: every panel is as wide as the window leaves it
        void shapeWalk(NodeSpec& page, bool fighting, bool narrow, float width, float height, int foes)
        {
            const float tall = columnHeight(fighting, narrow, height);
            const float whole = std::max(LeastColumn, width - 2.0f * WalkMargin);

            setProp(childNamed(page, "veil"), "width", width);
            setProp(childNamed(page, "veil"), "height", height);
            setProp(childNamed(page, "strip"), "width", whole);

            for (const char* name : {"kicker", "zoneTitle", "zoneLine", "fightTitle"})
                setProp(childNamed(page, name), "width", narrow ? whole : std::max(LeastColumn, whole - HeadRoom));

            if (narrow)
            {
                // The big figure keeps the head's right corner: the two lines beside it end before it
                for (const char* name : {"kicker", "zoneTitle", "fightTitle"})
                    setProp(childNamed(page, name), "width", std::max(NarrowTitle, whole - NarrowFigure));

                for (const char* name : {"headNote", "turnNote"})
                    setProp(childNamed(page, name), "width", whole);

                setProp(childNamed(page, "walkLeft"), "width", whole + WalkScroll);
                setProp(childNamed(page, "walkLeft"), "height", tall);

                for (const char* name : {"statePanel", "seekPanel", "workPanel", "cardPanel", "recordPanel", "bagPanel", "takenPanel", "homePanel",
                                         "orderPanel", "heroPanel", "menuPanel", "accountPanel"})
                    setProp(childNamed(page, name), "width", whole);

                // In one column the beasts stand one under the other, each the column's width
                shapeFoes(page, whole);

                return;
            }

            const float left = fighting ? FightLeft : SiteLeft;
            const float right = fighting ? FightRight : SiteRight;
            const float middle = std::max(LeastColumn, whole - left - right - 2.0f * WalkGap - 2.0f * WalkScroll);

            // A fight: what he fights stands across the top, over him and over his turn, which
            // stand side by side under it. One column holds the three, as wide as the two were
            if (fighting)
            {
                const float span = left + WalkGap + middle;

                setProp(childNamed(page, "walkLeft"), "width", span + WalkScroll);
                setProp(childNamed(page, "foeRow"), "width", span);
                setProp(childNamed(page, "duelRow"), "width", span);
                setProp(childNamed(page, "menuPanel"), "width", middle);

                // A beast's panel is the size of his own, narrower when the ones there are would not hold
                const float beasts = static_cast<float>(std::clamp(foes, 1, MaxFoes));

                shapeFoes(page, std::min(left, std::floor((span - (beasts - 1.0f) * WalkGap) / beasts)));
            }

            for (const char* name : {"walkLeft", "walkMiddle", "walkRight"})
                setProp(childNamed(page, name), "height", tall);

            setProp(childNamed(page, "walkMiddle"), "width", middle + WalkScroll);

            for (const char* name : {"cardPanel", "seekPanel", "workPanel"})
                setProp(childNamed(page, name), "width", middle);

            for (const char* name : {"takenPanel", "recordPanel", "orderPanel", "accountPanel"})
                setProp(childNamed(page, name), "width", right);

            // A log is a panel's child: found in it, not in the page. The record stands under what
            // he has taken, the account under the order of the turn
            if (NodeSpec* holder = childNamed(page, "recordPanel"))
                setProp(childNamed(*holder, "record"), "height", logHeight(tall - TakenRoom));

            if (NodeSpec* holder = childNamed(page, "accountPanel"))
                setProp(childNamed(*holder, "account"), "height", logHeight(tall - OrderRoom));
        }

        // A line of a fight's account as its log writes it: the turn at its head (none for a line
        // between two turns: a day gone, the end), a tinted line for a day gone
        LogEntry accountLine(const ElementMap& a)
        {
            LogEntry line;
            line.age = textOf(a, "turn").empty() ? -1.0f : static_cast<float>(std::atoi(textOf(a, "turn").c_str()));
            line.text = textOf(a, "text");
            line.figure = textOf(a, "figure");
            line.kind = kindOf(textOf(a, "kind"));
            line.glyph = textOf(a, "kind") == "time" ? "time" : "";

            return line;
        }

        // What a list was filled with, to fill it again only when that changed: a list filled
        // again lets go of what is chosen in it
        std::string signatureOf(const std::vector<ActivityGroup>& groups)
        {
            std::string signature;

            for (const auto& group : groups)
            {
                signature += group.label + "{";

                for (const auto& row : group.rows)
                {
                    signature += row.id + "|" + row.name + "|" + row.rank + "|" + row.each + "|" + row.cost + "|" + row.until + "|" + std::to_string(static_cast<int>(row.state)) + (row.urgent ? "!" : "") + (row.adventure ? "*" : "") + ";";

                    for (const auto& r : row.requirements)
                        signature += r.label + ",";
                }

                signature += "}";
            }

            return signature;
        }

        // The things of a list written with commas ("2 clay, 1 fish"), each alone
        std::vector<std::string> listed(const std::string& text)
        {
            std::vector<std::string> things;
            size_t at = 0;

            while (at < text.size())
            {
                size_t comma = text.find(", ", at);

                if (comma == std::string::npos)
                    comma = text.size();

                if (comma > at)
                    things.push_back(text.substr(at, comma - at));

                at = comma + 2;
            }

            return things;
        }

        // A row that cannot be chosen, and why
        void lockRow(ActivityRowSpec& row, const std::string& asks)
        {
            row.state = ActivityState::Locked;

            Requirement req;
            req.label = asks;
            req.met = 0;

            row.requirements.push_back(req);
        }

        // The ladder of a place, as the rules wrote it into a row (expeditionlib.pg's `addLadder`):
        // a heading with how well it is known, then what each level brings and the studies it
        // asks in all, in the gain's colour once he knows the place that well
        std::vector<GlossRow> ladderRows(const ElementMap& row)
        {
            const int level = intOf(row, "level");
            std::vector<GlossRow> rows;

            rows.push_back({"ZONE KNOWLEDGE " + std::to_string(level) + " OF 4", "", "", true});

            for (int l = 2; l <= 4; ++l)
            {
                const std::string n = std::to_string(l);
                const std::string brings = textOf(row, "brings" + n);

                rows.push_back({"Level " + n + " \xC2\xB7 " + std::to_string(intOf(row, "needs" + n)) + " studies", brings.empty() ? std::string("He knows it better") : brings,
                                level >= l ? "gain" : ""});
            }

            return rows;
        }

        std::string ledgerSignature(const std::vector<LedgerRowSpec>& rows)
        {
            std::string signature;

            for (const auto& row : rows)
                signature += row.id + "|" + row.name + "|" + row.value + "|" + row.rate + (row.muted ? "~" : "") + ";";

            return signature;
        }
    }

    // ---- the script, asked with what the save holds ----------------------------------------------------

    int LifeScene::walkDice()
    {
        // A test gives its own, and every cast is the next of them; the game takes the clock's
        ++diceCast;

        if (opt.walkSeed != 0)
            return opt.walkSeed + diceCast;

        const auto now = std::chrono::steady_clock::now().time_since_epoch();

        return static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(now).count() % 60013) + diceCast;
    }

    RecordList LifeScene::walkRows() const
    {
        RecordList rows;

        for (const auto& a : activities)
        {
            const std::string zone = textOf(a.fields, "zone");

            if (zone.empty())
                continue;

            rows.push_back({{"zone", ElementType{zone}},
                            {"listed", ElementType{boolOf(a.fields, "listed") ? 1 : 0}},
                            {"locked", ElementType{boolOf(a.fields, "locked") ? 1 : 0}},
                            {"months", ElementType{intOf(a.fields, "months")}},
                            {"until", ElementType{textOf(a.fields, "until")}}});
        }

        return rows;
    }

    bool LifeScene::askWalk(const std::string& ask, const WalkAct& act, const std::string& replay, RuleWalk& out)
    {
        if (rules.walk(save.age, save.character(), save.name, ask, walkDice(), save.walk, act, save.recipes, walkRows(), save.queue, replay, out))
            return true;

        for (const auto& e : rules.errors)
            LOG_ERROR(DOM, e);

        return false;
    }

    // ---- a walk by hand --------------------------------------------------------------------------------

    void LifeScene::beginWalk(const std::string& activity)
    {
        const RuleActivity* at = activityOf(activity);

        if (not at or ended or onWalk() or not save.running.empty())
            return;

        const std::string zone = textOf(at->fields, "zone");

        if (zone.empty() or boolOf(at->fields, "locked") or not boolOf(at->fields, "listed"))
            return;

        RuleWalk answer;

        if (not askWalk("enter", WalkAct{"", zone, 0}, "", answer) or not answer.state.under())
            return;

        // He is there: the months do not run while the days of the stay are chosen
        paused = true;
        sinceMonth = 0.0f;

        walk = answer;
        save.walk = answer.state;

        takeStats(answer.after);
        writeEntries(answer.entries);

        clearChoice();
        leadDue = false;
        seekChosen.clear();
        workChosen.clear();

        showWalk();

        publish();
        autoSave();
    }

    void LifeScene::setOutAt(const std::string& zone)
    {
        const std::string activity = WalkActivity + zone;
        const RuleActivity* at = activityOf(activity);

        if (not at)
        {
            LOG_ERROR(DOM, "No walk to a zone '" << zone << "'");
            return;
        }

        // A life to try a zone with: of the age the walk asks, a grown youth's parts, and whatever
        // else its row asks of him, as its row asks it. A walk that eats and mends finds both in the
        // bag. A build (--build) is taken as it is: only what the row asks and he lacks is added
        if (not built)
        {
            save.age = static_cast<float>(std::max(intOf(at->fields, "fromAge"), TrialAge));

            // (a part with a most it can be is left where it is: the most is the rules' to move)
            for (const auto& part : save.parts)
            {
                if (caps.count(part) == 0)
                    save.stats[part] += TrialParts;
            }
        }

        for (const auto& r : at->requires)
        {
            const std::string stat = textOf(r, "stat");

            if (stat.empty() or stat == "age" or LifeSave::ofTheTown(stat))
                continue;

            save.stats[stat] = std::max(save.stats[stat], intOf(r, "needed"));
        }

        // What the world must know for it is known: the zone it asks studied, studied
        for (const auto& r : at->requires)
        {
            const std::string stat = textOf(r, "stat");
            const std::string known = "know.";

            if (stat.rfind(known, 0) == 0)
                save.knowledge[stat.substr(known.size())] = std::max(save.knowledge[stat.substr(known.size())], intOf(r, "needed"));
        }

        refreshHoldings();
        fillActivities();
        publish();

        beginWalk(activity);
    }

    void LifeScene::applyBuild()
    {
        RuleWalk answer;

        if (not askWalk("builds", WalkAct{}, "", answer))
            return;

        const ElementMap* chosen = nullptr;

        for (const auto& b : answer.builds)
        {
            if (textOf(b, "id") == opt.build)
                chosen = &b;
        }

        // "list", or one the rules do not know: what there is, said where the game was launched
        if (not chosen)
        {
            if (opt.build != "list")
                LOG_ERROR(DOM, "No build '" << opt.build << "'");

            std::fprintf(stderr, "Builds (--build <id>, with --walk <zone> to open on its site, --set <stat>=<n> on top):\n");

            for (const auto& b : answer.builds)
                std::fprintf(stderr, "  %-10s %s, %d: %s\n", textOf(b, "id").c_str(), textOf(b, "name").c_str(), intOf(b, "age"), textOf(b, "about").c_str());

            return;
        }

        // He is that man: his age, what he is and holds, and every place known through and through
        save.age = static_cast<float>(intOf(*chosen, "age"));

        const std::string stat = "stat.";
        const std::string know = "know.";

        for (const auto& [key, value] : *chosen)
        {
            if (key.rfind(stat, 0) == 0)
                save.stats[key.substr(stat.size())] = intOf(*chosen, key);
            else if (key.rfind(know, 0) == 0)
                save.knowledge[key.substr(know.size())] = intOf(*chosen, key);
        }

        // And what --set changes on top of it
        for (const auto& [key, value] : opt.sets)
        {
            if (key.rfind(know, 0) == 0)
                save.knowledge[key.substr(know.size())] = value;
            else
                save.stats[key] = value;
        }

        built = true;

        LOG_INFO(DOM, "Opened as the build '" << opt.build << "': " << textOf(*chosen, "name"));

        refreshHoldings();
        rules.done = save.terms();
        fillActivities();
        publish();
    }

    void LifeScene::walkDo(const WalkAct& act)
    {
        if (not onWalk() or ended)
            return;

        // A turn of a fight is told in its time, where things move: the lines the account has now
        // are the ones already told
        const bool turn = walkScreen == "fight" and (act.kind == "do" or act.kind == "move" or act.kind == "step") and not Motion::reduced() and not walkFresh;
        size_t told = 0;

        if (auto log = walkPiece<EventLog>("account"); turn and log)
            told = log->size();

        // Where the move pressed puts his next turn, as the gauge shows it now: the fill goes there
        // as the turn is told (tellTurn)
        timeSpan = timelineSpanOf(act.kind == "move" ? act.target : act.kind == "do" ? textOf(save.walk.fight, "choice") : std::string());

        RuleWalk answer;

        if (not askWalk("act", act, "", answer))
            return;

        // For the analytics: a fight he came to, and how one ended (in his hands or his stance's)
        const bool fought = not textOf(save.walk.fight, "mob").empty();
        const bool wasOver = not textOf(save.walk.fight, "over").empty();
        const bool fighting = not textOf(answer.state.fight, "mob").empty();
        const std::string outcome = textOf(answer.state.fight, "over");

        if (fighting and not fought)
            ++save.fights;

        if (fighting and not outcome.empty() and not (fought and wasOver))
        {
            // (a story's fight whose time was called, "held", is neither won nor lost)
            if (outcome == "won")
                ++save.fightsWon;
            else if (outcome == "fled")
                ++save.fightsFled;
            else if (outcome != "held")
                ++save.fightsLost;

            if (intOf(answer.state.fight, "auto") > 0)
                ++save.fightsAuto;
        }

        walk = answer;
        save.walk = answer.state;

        takeStats(answer.after);
        writeEntries(answer.entries);

        // A story's day that ended: the months it adds up to are his life's, run now. The rules
        // count them (`monthsDue` is this act's alone): nothing is written back
        if (walk.state.story())
        {
            const int months = intOf(walk.state.run, "monthsDue");
            const int lives = save.lives;

            runStoryMonths(months);

            // A month that left him nothing to live on: the life is over, and the story with it.
            // Over, or already another's: a life with no ending to show begins the next at once
            // (and is written there)
            if (ended or save.lives != lives)
            {
                save.walk = WalkState{};
                walk = RuleWalk{};
                seekChosen.clear();
                workChosen.clear();

                closeWalk();
                return;
            }

            // The page as the months left him (his Vitality, what he holds)
            if (months > 0 and textOf(walk.site, "screen") != "done")
            {
                RuleWalk looked;

                if (askWalk("look", WalkAct{}, "", looked))
                {
                    walk = looked;
                    save.walk = looked.state;
                }
            }
        }

        if (textOf(walk.site, "screen") == "done")
        {
            endWalk();
            return;
        }

        // What he holds for the first time gets its row, under the screen
        refreshHoldings();

        for (const auto& r : save.holdEarned(holdings))
            addToLedger(r);

        // A turn that wrote something in the account: told line by line, the screen following
        if (turn and walkScreen == "fight" and textOf(walk.site, "screen") == "fight" and walk.state.account.size() > told)
            tellTurn(told);
        else
            showWalk();

        // What was asked and could not be done says why
        if (const std::string refused = textOf(walk.state.run, "refused"); not refused.empty())
            toast(refused);

        publishAll();
        autoSave();
    }

    void LifeScene::endWalk()
    {
        // A story is never kept, nor counted as a walk: its months ran on its days
        if (walk.state.story())
        {
            endStory();
            return;
        }

        const std::string zone = textOf(walk.recipe, "zone");
        const std::string activity = WalkActivity + zone;
        const std::string name = textOf(walk.recipe, "name");
        const bool carried = intOf(walk.recipe, "downed") > 0;

        // Fallen to a boss: no record, no way home, and the life ends where he fell
        refreshHoldings();

        if (intOf(walk.recipe, "died") > 0 or not death.empty())
        {
            ++save.walksByHand;

            save.walk = WalkState{};
            walk = RuleWalk{};
            seekChosen.clear();
            workChosen.clear();

            closeWalk();

            tellAnalytics("life_end");
            endLife();
            return;
        }

        // The record of a walk made to its end is kept with the others of the place, the last
        // few of them: a walk he was carried home from is not one to walk again
        if (not carried and not zone.empty())
        {
            WalkRecipe recipe;
            recipe.id = "r" + std::to_string(++save.recorded);
            recipe.fields = walk.recipe;
            recipe.steps = walk.state.record;
            recipe.marks = walk.state.marks;

            // The oldest of the place goes when there are too many: never the one a queue walks
            size_t kept = 0;

            for (const auto& r : save.recipes)
            {
                if (textOf(r.fields, "zone") == zone)
                    ++kept;
            }

            for (auto it = save.recipes.begin(); it != save.recipes.end() and kept >= MaxRecords;)
            {
                if (textOf(it->fields, "zone") == zone and it->id != save.walking and not (save.queue.left > 0 and it->id == save.queue.recipe))
                {
                    it = save.recipes.erase(it);
                    --kept;
                }
                else
                    ++it;
            }

            // The Adventure page opens on the place he has just walked, and its form on that walk
            adventureZone = zone;

            // (its form on the best of its walks, which the page picks: none is held here)
            if (save.queue.left <= 0)
                save.queue = WalkQueue{};

            save.recipes.push_back(std::move(recipe));
        }

        ++save.walksByHand;

        if (not zone.empty())
            ++save.walkedAt[zone];

        if (carried)
            ++save.carried;

        save.walk = WalkState{};
        walk = RuleWalk{};
        seekChosen.clear();
        workChosen.clear();

        closeWalk();

        // The walk is on the calendar from here: the way there, the stay and the way back run as
        // the months of any work do
        save.running = activity;
        save.monthsIn = 0;
        save.walking.clear();
        ++save.begun;
        rules.running = save.running;

        if (auto facts = ecsRef->getSystem<WorldFacts>())
            facts->setFact("activity." + activity + ".state", std::string("running"));

        paused = false;
        sinceMonth = 0.0f;

        refreshHoldings();

        for (const auto& r : save.holdEarned(holdings))
            addToLedger(r);

        fillActivities();
        publish();
        fillAdventure();

        if (carried)
            toast("Carried home from " + name);
        else
            toast("The record of " + name + " is kept: the Adventure page walks it again");

        autoSave();
    }

    // ---- a story ------------------------------------------------------------------------------------------

    void LifeScene::beginStory(const std::string& activity)
    {
        const RuleActivity* at = activityOf(activity);

        if (not at or ended or onWalk() or not save.running.empty())
            return;

        const std::string story = textOf(at->fields, "story");

        if (story.empty() or boolOf(at->fields, "locked") or not boolOf(at->fields, "listed"))
            return;

        // What beginning its row takes and gives, as any work's: a board's rations up front
        RuleForecast forecast;

        if (not rules.forecast(save.age, save.character(), activity, 0, forecast))
        {
            for (const auto& e : rules.errors)
                LOG_ERROR(DOM, e);

            return;
        }

        // Taken before he is there: the story is entered with what its row gave him, and its first
        // page counts it. (`atStart` is every stat as it was asked: taken after the enter's answer,
        // it would undo what entering did.) Given back if the story cannot be entered
        const auto held = save.stats;

        takeStats(forecast.atStart);

        RuleWalk answer;

        if (not askWalk("enter", WalkAct{"story", story, 0}, "", answer) or not answer.state.story())
        {
            save.stats = held;
            return;
        }

        // He is there: the calendar does not run, the days he chooses move it
        paused = true;
        sinceMonth = 0.0f;

        walk = answer;
        save.walk = answer.state;

        writeEntries(forecast.begins);
        takeStats(answer.after);
        writeEntries(answer.entries);

        // Its row runs from here, its months the story's, each run as its days add up to it
        save.running = activity;
        save.monthsIn = 0;
        ++save.begun;
        rules.running = save.running;

        if (auto facts = ecsRef->getSystem<WorldFacts>())
            facts->setFact("activity." + activity + ".state", std::string("running"));

        clearChoice();
        leadDue = false;
        seekChosen.clear();
        workChosen.clear();

        showWalk();

        publish();

        // A step of the guide that waited for a work to begin ends with it, as at any work's
        if (step.active and not step.until.empty() and holds(step.until))
            endStep();

        autoSave();
    }

    void LifeScene::runStoryMonths(int months)
    {
        // Each the life's own month, as the calendar runs it: eaten, made, mended, aged, paid. One
        // that leaves him nothing to live on ends the life there (and one that ends it where no
        // ending can be shown begins the next life at once: the months left are not the new
        // boy's). They are written once they have all run, by what asked them
        const int lives = save.lives;

        storyMonth = true;

        for (int i = 0; i < months and not ended and save.lives == lives; ++i)
            onMonth();

        storyMonth = false;
    }

    void LifeScene::endStory()
    {
        const std::string story = textOf(walk.state.run, "story");
        const std::string result = textOf(walk.state.run, "result");
        const std::string name = textOf(walk.site, "title");
        const bool plural = boolOf(walk.site, "plural");

        // The row that opened it: the story's own (`story`), done by now if its term came
        std::string row;

        for (const auto& a : activities)
        {
            if (textOf(a.fields, "story") == story)
            {
                row = textOf(a.fields, "id");
                break;
            }
        }

        save.walk = WalkState{};
        walk = RuleWalk{};
        seekChosen.clear();
        workChosen.clear();

        closeWalk();

        // What the guide still had to say on its screens is not said: he is off them for good
        passStorySteps();

        // Left, or sent away, before its term: the row stops where its days left it, and what its
        // board gave ahead for the months he will not stay goes back
        if (not row.empty() and save.running == row)
        {
            leaveWork(row, save.monthsIn);

            if (auto facts = ecsRef->getSystem<WorldFacts>())
                facts->setFact("activity." + row + ".state", std::string("idle"));

            save.running.clear();
            save.monthsIn = 0;
            rules.running.clear();
        }

        // Passed: the way into a path that the story is makes him one of it
        const bool passed = result == "pass" or result == "merit" or result == "distinction";

        if (const RuleActivity* at = activityOf(row); passed and at and boolOf(at->fields, "enters"))
            save.aim = textOf(at->fields, "path");

        // The months wait for his next choice
        paused = true;
        sinceMonth = 0.0f;

        if (not save.setAside.empty() and save.running.empty())
            takeUpSetAside();

        refreshHoldings();

        for (const auto& r : save.holdEarned(holdings))
            addToLedger(r);

        rules.done = save.terms();

        fillActivities();
        fillWindows();
        publish();

        // (a name that is a plural, the Outskirts, are behind him)
        const std::string behind = name + (plural ? " are behind him" : " is behind him");

        if (result == "pass")
            toast(behind + ": passed");
        else if (result == "merit")
            toast(behind + ": passed with merit");
        else if (result == "distinction")
            toast(behind + ": passed with distinction");
        else if (result == "fail")
            toast(behind + ": he failed it");
        else if (result == "dropout")
            toast(behind + ": he was sent away");
        else if (result == "left")
            toast(behind + (plural ? ": he left before their end" : ": he left before its end"));
        else
            toast(behind);

        autoSave();
    }

    // ---- the screens of a walk ------------------------------------------------------------------------

    EntityRef LifeScene::inWalk(const std::string& name) const
    {
        EntityRef root = walkPage;

        if (root.empty() or not root->has<Prefab>())
            return EntityRef{};

        return root->get<Prefab>()->findEntity(name);
    }

    // ---- what the guide points at there (achievements.pg's walk kinds) ------------------------------

    EntityRef LifeScene::walkPointed(const std::string& kind, const std::string& name) const
    {
        if (walkPage.empty())
            return EntityRef{};

        // A work of the day, by its id or by a story's tile's without its "tile."
        if (kind == "work")
        {
            auto list = walkPiece<ActivityList>("works");

            if (not list)
                return EntityRef{};

            if (auto row = list->find(ecsRef, StoryTile + name))
                return row->root;

            if (auto row = list->find(ecsRef, name))
                return row->root;

            return EntityRef{};
        }

        // An answer of the card waiting for him, from 0
        if (kind == "option")
        {
            auto option = walkPiece<Button>("option" + name);

            return option ? option->root : EntityRef{};
        }

        // A move of his turn: the buttons are made again with what he can do, found by their tag
        if (kind == "move")
        {
            for (const auto& [tag, button] : walkMoves)
            {
                if (tag == MoveTag + name)
                    return button.root;
            }

            return EntityRef{};
        }

        // A beast's panel, from 0: its next move stands on it
        if (kind == "foe")
            return inWalk("foePanel" + name);

        if (kind == "order")
            return inWalk("orderPanel");

        // A button of the screen: "do" is the one that does the work chosen
        if (kind == "walkbutton")
        {
            auto button = walkPiece<Button>(name == "do" ? "work" : name);

            return button ? button->root : EntityRef{};
        }

        return EntityRef{};
    }

    bool LifeScene::walkChosen(const std::string& point) const
    {
        const std::string work = "work:";

        if (workChosen.empty() or point.rfind(work, 0) != 0)
            return false;

        const std::string name = point.substr(work.size());

        return workChosen == name or workChosen == StoryTile + name;
    }

    void LifeScene::walkFacts(ElementMap& facts) const
    {
        // Off the walk's screens: no screen, no story, no fight. A fight is the one under way, or
        // over and not left yet
        const bool on = onWalk();
        const bool fighting = on and not textOf(save.walk.fight, "mob").empty();

        facts["walk.screen"] = ElementType{on ? textOf(walk.site, "screen") : std::string()};
        facts["story.id"] = ElementType{on ? textOf(save.walk.run, "story") : std::string()};
        facts["story.day"] = ElementType{on and save.walk.story() ? intOf(save.walk.run, "day") : 0};
        const bool over = fighting and not textOf(save.walk.fight, "over").empty();

        facts["fight.turn"] = ElementType{fighting ? intOf(save.walk.fight, "turn") : 0};
        facts["fight.over"] = ElementType{over ? 1 : 0};

        // He has done something in it: a turn is behind him, or it is over (a flight ends it before
        // its turn is counted)
        facts["fight.acted"] = ElementType{fighting and (over or intOf(save.walk.fight, "turn") >= 2) ? 1 : 0};
    }

    void LifeScene::walkSay(const std::string& name, const std::string& text)
    {
        if (auto label = walkPiece<Label>(name); label and label->spec.text != text)
            label->setText(ecsRef, text);
    }

    void LifeScene::walkShow(const std::string& name, bool shown, const std::string& panel)
    {
        EntityRef node = inWalk(name);

        if (node.empty())
            return;

        // Out of its stack, or back in it: it is the wrap its layout holds that says so
        if (not panel.empty())
        {
            if (auto holder = walkPiece<Panel>(panel))
                wrapIn(holder->body, node)->get<PositionComponent>()->setVisibility(shown);
        }
        else
        {
            for (const char* column : {"walkLeft", "walkMiddle", "walkRight"})
            {
                if (EntityRef layout = inWalk(column); not layout.empty())
                    wrapIn(layout, node)->get<PositionComponent>()->setVisibility(shown);
            }
        }

        node->get<PositionComponent>()->setVisibility(shown);
    }

    void LifeScene::walkList(const std::string& name, const std::vector<ActivityGroup>& groups, const std::string& chosen)
    {
        auto list = walkPiece<ActivityList>(name);

        if (not list)
            return;

        const std::string signature = signatureOf(groups);

        if (walkFilled[name] != signature)
        {
            walkFilled[name] = signature;
            list->setRows(ecsRef, groups);
        }

        // Lit once the list holds its rows (execute): a row is not in its list until the next frame
        walkPicks[name] = chosen;
    }

    void LifeScene::walkLedger(const std::string& name, const std::vector<LedgerRowSpec>& rows)
    {
        auto ledger = walkPiece<ResourceLedger>(name);

        if (not ledger)
            return;

        const std::string signature = ledgerSignature(rows);

        if (walkFilled[name] == signature)
            return;

        walkFilled[name] = signature;

        ledger->clear(ecsRef);
        ledger->addGroup(ecsRef, name, "");   // Unlabelled: no heading

        for (const auto& row : rows)
            ledger->addRow(ecsRef, name, row, "");
    }

    void LifeScene::walkLog(const std::string& name, const std::vector<LogEntry>& lines)
    {
        auto log = walkPiece<EventLog>(name);

        if (not log)
            return;

        // What is there is kept while it still begins what is to be shown: the lines after it are
        // written under it. Else the whole of it again
        bool grown = log->size() <= lines.size();
        size_t at = 0;

        for (const auto& item : log->items)
        {
            if (not std::holds_alternative<EventLog::Row>(item))
                continue;

            const auto& row = std::get<EventLog::Row>(item);

            grown = grown and at < lines.size() and row.entry.text == lines[at].text and row.entry.figure == lines[at].figure;
            ++at;
        }

        if (not grown)
        {
            log->clear(ecsRef);
            at = 0;
        }

        for (; at < lines.size(); ++at)
            log->append(ecsRef, lines[at]);

        log->scrollToEnd(ecsRef);
    }

    bool LifeScene::buildWalk(const std::string& screen)
    {
        float width, height;
        windowSize(ecsRef, width, height);

        const bool narrow = width < NarrowBelow;
        const bool fighting = screen == "fight";
        const std::string& file = fighting ? (narrow ? opt.fightNarrowFile : opt.fightFile) : (narrow ? opt.siteNarrowFile : opt.siteFile);

        std::vector<std::string> errors;
        PrefabLoadOptions options;
        options.errors = &errors;

        auto spec = loadNodeSpec(ecsRef, file, options);

        if (not spec)
        {
            LOG_ERROR(DOM, "Could not load the walk's screen " << file);
            return false;
        }

        for (const auto& e : errors)
            LOG_ERROR(DOM, file << ": " << e);

        // The screen at the window's width, written over the file's before it is built: a panel
        // hands its width to what stands in it, so nothing else has to be told
        shapeWalk(*spec, fighting, narrow, width, height, static_cast<int>(walk.targets.size()));

        EntityRef built = buildTree(ecsRef, *spec);

        if (built.empty() or not built->has<Prefab>())
        {
            LOG_ERROR(DOM, "Could not build the walk's screen " << file);
            return false;
        }

        closeWalk();

        walkPage = built;
        walkScreen = screen;
        walkNarrow = narrow;
        walkWidth = width;
        walkFresh = true;
        walkSettling = SettlePasses;

        // It leaves with the scene, like the page
        ecsRef->attach<SceneElement>(walkPage);

        // The veil takes the mouse: what is under it is neither hovered nor clicked
        if (EntityRef veil = inWalk("veil"); not veil.empty())
        {
            if (not veil->has<MouseEnterComponent>())
                ecsRef->attach<MouseEnterComponent>(veil, makeCallable<WalkNoOp>(WalkNoOp{}));

            if (not veil->has<MouseLeaveComponent>())
                ecsRef->attach<MouseLeaveComponent>(veil, makeCallable<WalkNoOp>(WalkNoOp{}));
        }

        // Its columns scroll under a finger, or a mouse held down, as the lists do
        for (const char* name : {"walkLeft", "walkMiddle", "walkRight"})
        {
            if (EntityRef column = inWalk(name); not column.empty() and column->has<VerticalLayout>())
                column->get<VerticalLayout>()->dragToScroll = true;
        }

        return true;
    }

    void LifeScene::closeWalk()
    {
        if (not walkPage.empty())
            ecsRef->removeEntity(walkPage);

        walkPage = EntityRef{};
        walkScreen.clear();
        walkFilled.clear();
        walkPicks.clear();
        autoLeft = 0.0f;
        walkTelling = false;
        beatCasting = false;
        walkBeats.clear();
        castDone.clear();
        walkMoves.clear();
        moveRows.clear();
        moveFlow.clear();
        moveHoverStale = 0;
        timeChips.clear();
        timeBar.reset();
        timeDay = EntityRef{};
        timeDayLabel.reset();
        timelineMove = "-";
        walkGlossed = false;
        accountFitted = 0.0f;
    }

    void LifeScene::showWalk()
    {
        if (not onWalk())
        {
            closeWalk();
            return;
        }

        // A walk opened from a save has no answer yet: the pages of the walk as it stands
        if (textOf(walk.site, "screen").empty())
        {
            RuleWalk answer;

            if (not askWalk("look", WalkAct{}, "", answer))
                return;

            walk = answer;
        }

        // A fight has the screen to itself; an event is answered on site, before its fight
        const std::string screen = textOf(walk.site, "screen") == "fight" ? "fight" : "site";

        if (walkScreen != screen and not buildWalk(screen))
            return;

        if (screen == "fight")
            fillFight();
        else
            fillSite();
    }

    void LifeScene::fitWalk(float width, float height)
    {
        if (walkPage.empty())
            return;

        // The screen is built again only when the window crosses from three columns to one or
        // back: that is another file. A window dragged by its edge may cross it back and forth:
        // built for the first crossing, then no more often than every RefitMs, and once more
        // when the dragging lets up (runWalk)
        if ((width < NarrowBelow) != walkNarrow)
        {
            if (walkCooling <= 0.0f)
            {
                const std::string screen = walkScreen;

                walkCooling = RefitMs;
                walkRefitDue = false;

                if (buildWalk(screen))
                {
                    if (screen == "fight")
                        fillFight();
                    else
                        fillSite();
                }

                return;
            }

            // Meanwhile the screen it has is stretched under the window: its veil and its columns
            walkRefitDue = true;
        }
        else if (std::abs(width - walkWidth) > 0.5f and walkSettling <= 0)
        {
            // At another width what stands is given its new width, panel by panel. A screen just
            // built does not hold its pieces yet: it is sized once it does (runWalk)
            sizeWalk(width, height);
        }

        // The veil is the window's size, from its corner: nothing of the page shows past it
        if (EntityRef veil = inWalk("veil"); not veil.empty())
        {
            auto pos = veil->get<PositionComponent>();

            pos->setX(0.0f);
            pos->setY(0.0f);
            pos->setWidth(width);
            pos->setHeight(height);
        }

        // The columns run to the bottom, and scroll what does not fit
        const bool fighting = walkScreen == "fight";
        const float tall = columnHeight(fighting, walkNarrow, height);

        for (const char* name : {"walkLeft", "walkMiddle", "walkRight"})
        {
            if (EntityRef column = inWalk(name); not column.empty())
                column->get<PositionComponent>()->setHeight(tall);
        }

        // In three columns a log fills its own to the bottom; in one it keeps its height
        if (not walkNarrow)
        {
            if (auto log = walkPiece<EventLog>("record"))
                log->setHeight(ecsRef, logHeight(tall - TakenRoom));
        }

        fitAccount();
    }

    void LifeScene::widenPanel(const std::string& name, float width)
    {
        widen(walkPiece<Panel>(name), width);
    }

    void LifeScene::widen(Panel* panel, float width)
    {
        if (not panel or std::abs(panel->spec.width - width) < 0.5f)
            return;

        EntitySystem* ecs = ecsRef;

        panel->setWidth(ecs, width);

        if (panel->body.empty() or not panel->body->has<VerticalLayout>())
            return;

        // What stands in a panel is as wide as the panel leaves it, each in its own way. A button
        // keeps the width of what it says, and so does a label that grows with its text
        const float inner = panel->innerWidth();

        std::vector<EntityRef> held(panel->body->get<VerticalLayout>()->entities.begin(), panel->body->get<VerticalLayout>()->entities.end());

        for (auto& wrap : held)
        {
            EntityRef piece = wrap;

            // In a file a piece stands in its panel inside a wrap: the piece is the wrap's main entity
            if (wrap->has<Prefab>() and wrap->get<Prefab>()->namedChildrenIds.count("MainEntity") > 0)
                piece = wrap->get<Prefab>()->getEntity("MainEntity");

            if (piece.empty())
                continue;

            if (piece->has<Label>())
            {
                if (auto label = piece->get<Label>(); label->spec.overflow != Overflow::Grow)
                    label->setWidth(ecs, inner);
            }
            else if (piece->has<ProgressRule>())
                piece->get<ProgressRule>()->setWidth(ecs, inner);
            else if (piece->has<ResourceLedger>())
                piece->get<ResourceLedger>()->setWidth(ecs, inner);
            else if (piece->has<EventLog>())
                piece->get<EventLog>()->setWidth(ecs, inner);
            else if (piece->has<ActivityList>())
                piece->get<ActivityList>()->setSize(ecs, inner, 0.0f);
            else if (piece->has<PlaceGrid>())
                piece->get<PlaceGrid>()->setWidth(ecs, inner);
            else if (piece->has<StatLine>())
                piece->get<StatLine>()->setWidth(ecs, inner);
            else if (piece->has<LifeClock>())
                piece->get<LifeClock>()->setWidth(ecs, inner);
            else if (piece->has<Ornament>() or piece->has<VerticalLayout>() or piece->has<HorizontalLayout>())
                piece->get<PositionComponent>()->setWidth(inner);
        }
    }

    void LifeScene::sizeWalk(float width, float height)
    {
        if (walkPage.empty())
            return;

        EntitySystem* ecs = ecsRef;

        // The same figures as the file was written over with before it was built (shapeWalk)
        const bool fighting = walkScreen == "fight";
        const bool narrow = walkNarrow;
        const float whole = std::max(LeastColumn, width - 2.0f * WalkMargin);

        auto wide = [&](const char* name, float to) {
            if (EntityRef piece = inWalk(name); not piece.empty())
                piece->get<PositionComponent>()->setWidth(to);
        };

        auto says = [&](const char* name, float to) {
            if (auto label = walkPiece<Label>(name))
                label->setWidth(ecs, to);
        };

        if (auto strip = walkPiece<ProgressRule>("strip"))
            strip->setWidth(ecs, whole);

        says("zoneLine", narrow ? whole : std::max(LeastColumn, whole - HeadRoom));

        // The big figure keeps the head's right corner: the lines beside it end before it
        for (const char* name : {"kicker", "zoneTitle", "fightTitle"})
            says(name, narrow ? std::max(NarrowTitle, whole - NarrowFigure) : std::max(LeastColumn, whole - HeadRoom));

        if (narrow)
        {
            for (const char* name : {"headNote", "turnNote"})
                says(name, whole);

            wide("walkLeft", whole + WalkScroll);

            for (const char* name : {"statePanel", "seekPanel", "workPanel", "cardPanel", "recordPanel", "bagPanel", "takenPanel", "homePanel",
                                     "orderPanel", "heroPanel", "menuPanel", "accountPanel", "foePanel0", "foePanel1", "foePanel2", "foePanel3"})
                widenPanel(name, whole);
        }
        else
        {
            const float left = fighting ? FightLeft : SiteLeft;
            const float right = fighting ? FightRight : SiteRight;
            const float middle = std::max(LeastColumn, whole - left - right - 2.0f * WalkGap - 2.0f * WalkScroll);

            // The columns at the sides keep their width: the one between them takes the rest
            if (fighting)
            {
                const float span = left + WalkGap + middle;
                const float beasts = static_cast<float>(std::clamp(static_cast<int>(walk.targets.size()), 1, MaxFoes));
                const float foe = std::min(left, std::floor((span - (beasts - 1.0f) * WalkGap) / beasts));

                wide("walkLeft", span + WalkScroll);
                wide("foeRow", span);
                wide("duelRow", span);

                widenPanel("menuPanel", middle);

                for (int i = 0; i < MaxFoes; ++i)
                    widenPanel("foePanel" + std::to_string(i), foe);
            }
            else
            {
                wide("walkMiddle", middle + WalkScroll);

                for (const char* name : {"cardPanel", "seekPanel", "workPanel"})
                    widenPanel(name, middle);
            }
        }

        walkWidth = width;

        // The timeline is drawn again at its panel's new width (the mouse check finds it not drawn)
        if (fighting)
            timelineMove = "-";

        // The buttons of his turn stand as many to a line as the panel holds: the lines they
        // stand on follow it, and they are made again only when another number would hold
        bool flows = false;

        for (const char* name : {"aims", "moves"})
        {
            EntityRef rows = inWalk(name);

            if (rows.empty())
                continue;

            const float room = rows->get<PositionComponent>()->width;

            for (auto id : moveRows[name])
            {
                if (auto row = ecs->getEntity(id))
                    row->get<PositionComponent>()->setWidth(room);
            }

            if (auto it = moveFlow.find(name); it != moveFlow.end() and (room < it->second.first or room >= it->second.second))
                flows = true;
        }

        if (flows)
        {
            walkFilled.erase("moves");
            fillFight();
        }

        (void) height;
    }

    void LifeScene::fitNarrowHead()
    {
        // In one column the big figure at the head's right (the days left, or the turn) stands
        // beside the kicker and the title: they end a gap before it as it is drawn, not before the
        // widest it could be ("THE COLLEGIUM · SIXTE…" beside a TURN 1 of half that room). On a
        // site the title ends before the line under the figure too
        EntitySystem* ecs = ecsRef;
        const bool fighting = walkScreen == "fight";
        const float whole = std::max(LeastColumn, walkWidth - 2.0f * WalkMargin);

        auto drawn = [this](const char* name) {
            auto label = walkPiece<Label>(name);

            if (not label or label->entity.empty() or not label->entity->has<TTFText>() or not label->entity->get<PositionComponent>()->visible)
                return 0.0f;

            return label->entity->get<TTFText>()->textWidth;
        };

        const float figure = drawn(fighting ? "turn" : "daysLeft");
        const float under = fighting ? 0.0f : drawn("daysNote");

        auto fit = [&](const char* name, float to) {
            to = std::max(NarrowTitle, to);

            if (auto label = walkPiece<Label>(name); label and std::abs(label->spec.width - to) > 0.5f)
                label->setWidth(ecs, to);
        };

        // The bug report's button stands between them and the figure (lifereport.cpp), over the
        // line under it
        const float report = reportRoom();

        fit("kicker", whole - figure - report - HeadGap);
        fit(fighting ? "fightTitle" : "zoneTitle", whole - std::max(figure + report, under) - HeadGap);
    }

    void LifeScene::fitAccount()
    {
        // The account has what the order of the turn leaves of the column, and the order stands
        // as tall as those it names: taller against a pack, shorter once one of them is down
        if (walkPage.empty() or walkScreen != "fight" or walkNarrow)
            return;

        EntityRef order = inWalk("orderPanel");
        EntityRef column = inWalk("walkRight");
        auto log = walkPiece<EventLog>("account");

        if (order.empty() or column.empty() or not log)
            return;

        // Until the page has said how tall the panel stands, the room it was drawn with
        const float stands = order->get<PositionComponent>()->height;
        const float room = stands > 1.0f ? stands + WalkGap : OrderRoom;
        const float wanted = logHeight(column->get<PositionComponent>()->height - room);

        if (std::abs(wanted - accountFitted) > 0.5f)
        {
            accountFitted = wanted;
            log->setHeight(ecsRef, wanted);
        }
    }

    void LifeScene::fillSite()
    {
        EntitySystem* ecs = ecsRef;
        const ElementMap& site = walk.site;
        const bool asked = textOf(site, "screen") == "event";
        const bool story = walk.state.story();

        // A story's head: what its days are worth over its name, the stage's text under it on the
        // stage's first day only, and the stage's title over the end of its gauge
        if (story)
        {
            const int days = intOf(site, "days");
            const int months = intOf(site, "monthsTotal");
            const std::string unit = upper(textOf(site, "unit"));

            // (a phone's head has the room for the figures only, beside its big one)
            walkSay("kicker", std::to_string(days) + " " + unit + (days == 1 ? "" : "S") + " \xC2\xB7 " + std::to_string(months) + (months == 1 ? " MONTH" : " MONTHS") + (walkNarrow ? "" : " OF HIS LIFE"));
            // (a phone's head has one line for it: there the stage is its title, and its text is
            // the record's line of that day)
            walkSay("zoneLine", walkNarrow ? std::string() : textOf(site, "stageText"));
            walkSay("headNote", upper(textOf(site, "stage")));
        }
        else
        {
            walkSay("kicker", textOf(site, "kicker"));
            walkSay("zoneLine", textOf(site, "line"));
            walkSay("headNote", textOf(site, "home"));
        }

        walkSay("zoneTitle", textOf(site, "title"));
        walkSay("daysLeft", textOf(site, "daysLeft"));
        walkSay("daysNote", textOf(site, "daysNote"));

        // How he stands, at the head of the left column: his Vitality in the field and how well
        // the place is known, each over its rule, and what knowing it better would bring
        if (auto panel = walkPiece<Panel>("statePanel"); panel and panel->title and panel->title->spec.text != save.name)
            panel->setHeading(ecs, save.name);

        // A story is not a walk: no way home to turn to, nothing met on a way
        if (story)
        {
            for (auto [name, heading] : {std::pair<const char*, const char*>{"workPanel", "The day's work"}, {"cardPanel", "On this day"}, {"homePanel", "If he left now"}})
            {
                if (auto panel = walkPiece<Panel>(name); panel and panel->title and panel->title->spec.text != heading)
                    panel->setHeading(ecs, heading);
            }
        }

        walkSay("vitality", textOf(site, "vitality"));
        walkSay("knowledge", textOf(site, "knowledge"));
        walkSay("knowledgeNote", textOf(site, "knowledgeNote"));

        // In a story, how he stands before its masters in the place of a place's knowledge: in
        // words, as the rules say it (a story without it says nothing there)
        const std::string standing = story ? textOf(site, "standing") : std::string();

        walkSay("standing", standing);
        walkShow("standing", not standing.empty(), "statePanel");

        for (const char* part : {"knowledge", "knowledgeBar", "knowledgeNote"})
            walkShow(part, not story, "statePanel");

        // The knowledge's rule fills with the studies toward the next level, not with the level
        // he has: what is left to learn is what he can do something about here
        for (auto [name, key] : {std::pair<const char*, const char*>{"vitalityBar", "vitalityPercent"}, {"knowledgeBar", "studyPercent"}})
        {
            if (auto bar = walkPiece<ProgressRule>(name))
                bar->setPercent(ecs, static_cast<float>(intOf(site, key)), not walkFresh);
        }

        // His panel's gloss: what knowing a place is, what each level of it brings, and how far
        // he is from the next
        if (auto registry = ecs->getSystem<GlossRegistry>(); registry and story)
        {
            // A story is no place to know: its gloss says what the days are worth
            GlossSpec gloss;
            gloss.inlineValues = true;
            gloss.title = textOf(site, "title");
            gloss.text = "His days here are months of his life: each month they add up to is lived as it comes, "
                         "what he eats, mends and pays with it. Leaving keeps what the days have given him.";

            registry->set(KnowledgeGloss, gloss);
        }
        else if (registry)
        {
            const int level = intOf(site, "level");

            GlossSpec gloss;
            gloss.inlineValues = true;
            gloss.title = "Zone knowledge";
            gloss.aside = std::to_string(level) + "/4";
            gloss.text = "How well " + textOf(site, "title") + " is known. The first visit makes it known 1 of 4, and every study of the place adds to it. "
                         "It is the world's: what he learns here, every life after his knows.";
            gloss.rows = ladderRows(site);

            if (level >= 4)
                gloss.footnote = "KNOWN 4 OF 4";
            else if (level > 0)
                gloss.footnote = "STUDY " + std::to_string(intOf(site, "studies")) + " OF " + std::to_string(intOf(site, "needs" + std::to_string(level + 1))) + " TO KNOWLEDGE " + std::to_string(level + 1);

            registry->set(KnowledgeGloss, gloss);
        }

        // Hung once a screen on his panel: its ground and heading, and how well the place is known
        if (not walkGlossed)
        {
            walkGlossed = true;

            if (auto panel = walkPiece<Panel>("statePanel"))
            {
                for (EntityRef part : {panel->ground, panel->title ? panel->title->entity : EntityRef{}})
                {
                    if (not part.empty())
                        attachGloss(ecs, part, KnowledgeGloss);
                }
            }

            for (const char* part : {"knowledge", "knowledgeBar", "knowledgeNote"})
            {
                if (EntityRef piece = inWalk(part); not piece.empty())
                    attachGloss(ecs, piece, KnowledgeGloss);
            }
        }

        // The strip of days: what is spent solid, what is left of the stay hatched after it. In a
        // story, the gauge of its days, filled to the day he is on, and the months gone with them
        if (auto strip = walkPiece<ProgressRule>("strip"))
        {
            float spent = static_cast<float>(intOf(site, "percent"));
            std::string caption = textOf(site, "caption");

            if (story)
            {
                const int days = intOf(site, "days");
                const int gone = intOf(site, "months");

                spent = days > 0 ? 100.0f * static_cast<float>(intOf(site, "day")) / static_cast<float>(days) : 0.0f;
                caption = textOf(site, "dayText");

                if (gone > 0)
                    caption += " \xC2\xB7 " + std::to_string(gone) + (gone == 1 ? " month gone" : " months gone");
            }

            // A screen built this pass is filled as it stands: what moves, moves from the next
            strip->setPercent(ecs, spent, not walkFresh);
            strip->setForecast(ecs, 100.0f);   // The hatch runs from the fill's head to where it is told: the end of the stay
            strip->setCaption(ecs, caption);
        }

        // The beasts he may seek: one that he does not know yet, or has not the days for, stands
        // there with what it asks and cannot be chosen
        std::vector<ActivityRowSpec> seeks;
        bool seekStands = false;
        std::string seekSaid;
        std::string seekCost;

        for (const auto& s : walk.seeks)
        {
            ActivityRowSpec row;
            row.id = textOf(s, "id");
            row.name = textOf(s, "name");
            row.glyph = "adventure";
            row.cost = textOf(s, "costText");

            // One line under the name: how rare it is and how well he knows it. A row that cannot
            // be chosen says why there
            row.until = textOf(s, "rank");

            // A beast he does not know has no cost to say yet
            if (row.cost.empty())
                row.cost = "?";

            if (intOf(s, "locked") > 0)
            {
                lockRow(row, textOf(s, "asks"));
                row.until = textOf(s, "rank") + " \xC2\xB7 " + textOf(s, "asks");
            }
            else if (intOf(s, "short") > 0 or asked)
            {
                lockRow(row, asked ? "SOMETHING WAITS FOR HIS ANSWER" : textOf(s, "asks"));

                if (not asked)
                {
                    row.until = textOf(s, "asks");
                    row.urgent = true;
                }
            }
            else if (row.id == seekChosen)
            {
                seekStands = true;
                seekSaid = textOf(s, "note");
                seekCost = row.cost;
            }

            seeks.push_back(row);
        }

        if (not seekStands)
            seekChosen.clear();

        walkList("seeks", seeks.empty() ? std::vector<ActivityGroup>{} : std::vector<ActivityGroup>{{"", seeks}}, seekChosen);

        // No beast lives there: the panel has nothing to say, and leaves the column
        walkShow("seekPanel", not walk.seeks.empty());

        if (auto seek = walkPiece<Button>("seek"))
        {
            seek->setCost(ecs, seekStands ? seekCost : std::string());
            seek->setDisabled(ecs, not seekStands);
        }

        if (auto observe = walkPiece<Button>("observe"))
        {
            observe->setCost(ecs, textOf(site, "watchCost"));
            observe->setDisabled(ecs, not seekStands);
        }

        // Under the list, in a room that is always there (a line that came and went would move
        // the buttons under the mouse): what is known of the beast chosen, or what to do
        walkSay("seekNote", seekStands ? seekSaid : "Choose a beast, then seek it or watch it from cover.");

        // The field work: gathering, the study of the place, a rest, a remedy
        std::vector<ActivityRowSpec> works;
        bool workStands = false;
        std::string workName;
        std::string workCost;

        for (const auto& w : walk.works)
        {
            ActivityRowSpec row;
            row.id = textOf(w, "id");
            row.name = textOf(w, "name");
            row.glyph = textOf(w, "tone") == "know" ? "study" : row.id == "rest" or row.id == "remedy" ? "vitality" : "work";
            row.cost = textOf(w, "costText");
            row.glossKey = WorkGloss + row.id;

            // The tile is its name and its days. The rest is its gloss's to say: a line on the
            // work, what it costs, what it gives, and what it waits for when it cannot be done
            GlossSpec gloss;
            gloss.inlineValues = true;
            gloss.title = row.name;
            gloss.aside = row.cost;
            gloss.text = textOf(w, "about");

            if (const std::string costs = textOf(w, "costs"); not costs.empty())
                gloss.rows.push_back({"Costs", costs, "time"});

            if (const std::string gives = textOf(w, "gives"); not gives.empty())
                gloss.rows.push_back({"Gives", gives, "gain"});

            // What a part of his would make of a gathering, a line a step, as a work's gloss on
            // the Life page says them: in the gain's colour once he has it, plain until then
            if (const std::string bonus = textOf(w, "bonus"); not bonus.empty())
            {
                gloss.rows.push_back({"BONUS", "", "", true});

                const int reached = intOf(w, "bonusReached");
                size_t start = 0;

                for (int i = 0; start <= bonus.size(); ++i)
                {
                    size_t end = bonus.find('\n', start);

                    if (end == std::string::npos)
                        end = bonus.size();

                    gloss.rows.push_back({bonus.substr(start, end - start), "", i < reached ? "gain" : ""});
                    start = end + 1;
                }
            }

            if (intOf(w, "locked") > 0)
            {
                lockRow(row, textOf(w, "asks"));
                gloss.footnote = textOf(w, "asks");
            }
            else if (asked)
            {
                lockRow(row, "SOMETHING WAITS FOR HIS ANSWER");
                gloss.footnote = "SOMETHING WAITS FOR HIS ANSWER";
            }
            else if (row.id == workChosen)
            {
                workStands = true;
                workName = row.name;
                workCost = row.cost;
            }

            // A story's tile offered for the first time today wears the adventure's look, that
            // day only. Not on the first day: everything is new then
            if (story and intOf(w, "fresh") > 0 and intOf(site, "day") > 1)
            {
                row.adventure = true;
                gloss.footnote = gloss.footnote.empty() ? std::string("NEW TODAY") : "NEW TODAY \xC2\xB7 " + gloss.footnote;
            }

            if (auto registry = ecs->getSystem<GlossRegistry>())
                registry->set(row.glossKey, gloss);

            works.push_back(row);
        }

        if (not workStands)
            workChosen.clear();

        walkList("works", {{"", works}}, workChosen);

        // A story's day that offers nothing (an examination's: sitting it is the day's work) has
        // no day's work to choose: the panel leaves the column, with what it says, till a day
        // offers some again
        if (story)
            walkShow("workPanel", not walk.works.empty());

        if (auto work = walkPiece<Button>("work"))
        {
            work->setLabel(ecs, workStands ? workName : "Do it");
            work->setCost(ecs, workStands ? workCost : std::string());
            work->setDisabled(ecs, not workStands);
        }

        if (story)
            walkSay("workNote", "Choose the day's work, then do it. Hover one to read what it costs and what it gives.");

        // What a story stopped offering, said under the tiles the day after, once
        std::string gone;

        for (const auto& g : walk.gone)
            gone += (gone.empty() ? "" : ", ") + textOf(g, "name");

        // (not on a day that offers nothing: the panel is not there)
        if (story and walk.works.empty())
            gone.clear();

        walkSay("goneNote", gone.empty() ? std::string() : "No longer offered: " + gone);
        walkShow("goneNote", not gone.empty(), "workPanel");

        walkSay("leaveNote", textOf(site, "leaveNote"));

        if (auto leave = walkPiece<Button>("leave"))
        {
            leave->setLabel(ecs, textOf(site, "leave"));
            leave->setDisabled(ecs, asked);
        }

        // An event waiting for his answer: its card stands over the record until he gives it
        walkShow("cardPanel", asked);

        if (asked)
        {
            walkSay("cardTitle", textOf(walk.card, "title"));
            walkSay("cardText", textOf(walk.card, "text"));
            walkSay("cardFoot", textOf(walk.card, "foot"));
        }

        for (int i = 0; i < MaxOptions; ++i)
        {
            const std::string button = "option" + std::to_string(i);
            const std::string line = "optionLine" + std::to_string(i);
            const bool there = asked and static_cast<size_t>(i) < walk.options.size();

            walkShow(button, there, "cardPanel");
            walkShow(line, there, "cardPanel");

            if (not there)
            {
                if (auto option = walkPiece<Button>(button))
                    option->setDisabled(ecs, true);

                continue;
            }

            const ElementMap& o = walk.options[i];
            const bool locked = intOf(o, "locked") > 0;

            if (auto option = walkPiece<Button>(button))
            {
                option->setLabel(ecs, textOf(o, "label"));
                option->setDisabled(ecs, locked);
            }

            walkSay(line, locked ? textOf(o, "line") + " \xC2\xB7 " + textOf(o, "asks") : textOf(o, "line"));
        }

        // The record so far: a line a step, numbered as the record numbers them
        std::vector<LogEntry> lines;

        for (const auto& step : walk.state.record)
        {
            LogEntry line;
            line.age = static_cast<float>(intOf(step, "n"));
            line.text = textOf(step, "text");
            line.figure = textOf(step, "figure");
            line.kind = textOf(step, "tone") == "loss" ? LogKind::Loss : LogKind::Coin;
            line.glyph = textOf(step, "glyph");

            lines.push_back(line);
        }

        walkLog("record", lines);

        if (auto panel = walkPiece<Panel>("recordPanel"); panel and panel->aside and panel->aside->spec.text != textOf(site, "written"))
            panel->setAside(ecs, textOf(site, "written"));

        walkSay("recordNote", textOf(site, "summary"));

        // What he carries, what he has taken, and what the record would cost to walk again
        for (auto [name, rows] : {std::pair<const char*, const RecordList*>{"bag", &walk.bag}, {"taken", &walk.taken}, {"home", &walk.home}})
        {
            std::vector<LedgerRowSpec> ledger;

            for (const auto& r : *rows)
            {
                LedgerRowSpec row;
                row.id = textOf(r, "id");
                row.name = textOf(r, "name");
                row.glyph = textOf(r, "glyph");
                row.value = textOf(r, "value");

                ledger.push_back(row);
            }

            walkLedger(name, ledger);
        }

        walkShow("takenPanel", not walk.taken.empty());

        walkSay("scribe", textOf(site, "scribe"));
    }

    void LifeScene::fillMoves(bool over, bool handed)
    {
        EntitySystem* ecs = ecsRef;
        auto registry = ecs->getSystem<GlossRegistry>();

        // What the buttons are: the beasts to aim at (against several), then every move of the
        // menu. Made again only when what stands on them changes (a move learned, another cost):
        // which beast is aimed at is a colour, given to the button that stands
        struct Wanted
        {
            std::string layout;        // "aims" or "moves"
            std::string tag;
            std::string label;
            std::string cost;          // "" or "2 BREATH"
            ButtonVariant variant = ButtonVariant::Quiet;
            bool locked = false;
            bool newLine = false;      // What he has learned stands on a line of its own, under what anyone can do
        };

        std::vector<Wanted> wanted;
        std::string signature;

        if (walk.targets.size() > 1)
        {
            for (const auto& t : walk.targets)
            {
                Wanted aim;
                aim.layout = "aims";
                aim.tag = AimTag + std::to_string(intOf(t, "n"));
                aim.label = textOf(t, "name");
                aim.variant = intOf(t, "chosen") > 0 and intOf(t, "dead") <= 0 ? ButtonVariant::Study : ButtonVariant::Quiet;
                aim.locked = intOf(t, "dead") > 0 or over or handed;

                wanted.push_back(aim);
            }
        }

        std::string lastGroup;

        for (const auto& m : walk.menu)
        {
            Wanted move;
            move.layout = "moves";
            move.tag = MoveTag + textOf(m, "id");
            move.label = textOf(m, "name");
            move.cost = textOf(m, "group").empty() ? std::string() : textOf(m, "cost");
            move.variant = textOf(m, "kind") == "spell" ? ButtonVariant::Study : ButtonVariant::Quiet;
            move.locked = intOf(m, "locked") > 0 or over or handed;

            // What anyone can do has no group: "-" stands for it
            const std::string group = textOf(m, "group").empty() ? std::string("-") : textOf(m, "group");

            move.newLine = not lastGroup.empty() and group != lastGroup;
            lastGroup = group;

            wanted.push_back(move);

            if (not registry)
                continue;

            // What it would do, said before it is done: the gloss of its button
            GlossSpec gloss;
            gloss.inlineValues = true;
            gloss.title = move.label;
            gloss.aside = move.cost;
            gloss.text = textOf(m, "text");

            for (auto [key, label, tone] : {std::tuple<const char*, const char*, const char*>{"against", "Against", ""}, {"hits", "Hits", ""}, {"deals", "Deals", "gain"},
                                             {"costs", "Costs", "time"}, {"then", "Then", "muted"}, {"bites", "Comes back", "loss"}, {"foeAfter", "It is left", "muted"}})
            {
                if (const std::string value = textOf(m, key); not value.empty())
                    gloss.rows.push_back({label, value, tone});
            }

            // What is said in a sentence is its text's, not a row's
            if (const std::string odds = textOf(m, "odds"); not odds.empty())
                gloss.text += (gloss.text.empty() ? "" : " ") + odds;

            if (intOf(m, "locked") > 0)
                gloss.footnote = textOf(m, "asks");

            registry->set(MoveGloss + textOf(m, "id"), gloss);
        }

        for (const auto& w : wanted)
            signature += w.layout + "|" + w.tag + "|" + w.label + "|" + w.cost + (w.newLine ? "/" : "") + ";";

        const bool again = signature != walkFilled["moves"];

        if (again)
        {
            walkFilled["moves"] = signature;

            // The rows of buttons there were go: the layout destroys each one as it lets it go,
            // and the screen forgets it
            for (const char* name : {"aims", "moves"})
            {
                EntityRef rows = inWalk(name);

                if (rows.empty() or not rows->has<VerticalLayout>())
                    continue;

                for (auto id : moveRows[name])
                {
                    rows->get<VerticalLayout>()->removeEntity(id);

                    if (not walkPage.empty())
                        walkPage->get<Prefab>()->childrenIds.erase(id);
                }

                moveRows[name].clear();
            }

            // Other buttons than a frame ago, and the mouse may not have moved: the one now under
            // it is hovered, and takes a press, only once the hover is worked out again (runWalk)
            moveHoverStale = HoverFrames;

            walkMoves.clear();
            moveFlow.clear();

            EntityRef row;
            std::string rowOf;
            float used = 0.0f;

            for (const auto& w : wanted)
            {
                EntityRef rows = inWalk(w.layout);

                if (rows.empty() or not rows->has<VerticalLayout>() or walkPage.empty())
                    continue;

                const float width = rows->get<PositionComponent>()->width;
                const int z = static_cast<int>(rows->get<PositionComponent>()->z);

                ButtonSpec spec;
                spec.variant = w.variant;
                spec.label = w.label;
                spec.tag = w.tag;
                spec.z = z;
                spec.months = w.cost.empty() ? -1 : 0;   // Made with the cost pair, then told what it reads

                Button button = makeButton(ecs, spec);

                if (not w.cost.empty())
                    button.setCost(ecs, w.cost);

                const float face = button.root->get<PositionComponent>()->width;

                // Between which widths these buttons stand as they do: the widest line there is,
                // and the least width at which one more would hold on a line that is full
                auto [flow, fresh] = moveFlow.try_emplace(w.layout, 0.0f, std::numeric_limits<float>::max());

                (void) fresh;

                if (not row.empty() and rowOf == w.layout and not w.newLine and used + face > width)
                    flow->second.second = std::min(flow->second.second, used + face);

                // As many to a line as the panel holds
                if (row.empty() or rowOf != w.layout or w.newLine or used + face > width)
                {
                    auto made = makeAnchoredPrefab(ecs, 0.0f, 0.0f, static_cast<float>(z));

                    made.get<PositionComponent>()->setWidth(width);
                    made.get<PositionComponent>()->setHeight(button.root->get<PositionComponent>()->height);

                    row = made.entity;
                    rowOf = w.layout;
                    used = 0.0f;

                    rows->get<VerticalLayout>()->addEntity(row);
                    moveRows[w.layout].push_back(row.id);

                    // It leaves with the screen, as everything on it does. Under no name: a name
                    // the screen kept would outlive the row, and be looked through with the others
                    walkPage->get<Prefab>()->addToPrefab(row);
                }

                auto anchor = button.root->get<UiAnchor>();
                anchor->setTopAnchor(PosAnchor{row.id, AnchorType::Top});
                anchor->setLeftAnchor(PosAnchor{row.id, AnchorType::Left});
                anchor->setLeftMargin(used);

                row->get<Prefab>()->addToPrefab(button.root, w.tag);

                // A move's button says what the move would do when the mouse is on it
                if (w.layout == "moves")
                    attachGloss(ecs, button.face, MoveGloss + w.tag.substr(std::string(MoveTag).size()));

                used += face + MoveGap;
                flow->second.first = std::max(flow->second.first, used - MoveGap);

                walkMoves.push_back({w.tag, button});
            }
        }

        // What cannot be done now waits, greyed: its gloss says why. And the beast his blows go to
        // is the lit one of its line
        for (size_t i = 0; i < walkMoves.size() and i < wanted.size(); ++i)
        {
            if (walkMoves[i].second.spec.variant != wanted[i].variant)
                walkMoves[i].second.setVariant(ecs, wanted[i].variant);

            if (walkMoves[i].second.spec.disabled != wanted[i].locked)
            {
                walkMoves[i].second.setDisabled(ecs, wanted[i].locked);
                walkMoves[i].second.spec.disabled = wanted[i].locked;
            }
        }
    }

    void LifeScene::fillFight()
    {
        EntitySystem* ecs = ecsRef;
        const ElementMap& duel = walk.duel;
        const bool over = not textOf(duel, "over").empty();
        const bool handed = intOf(duel, "auto") > 0;

        walkSay("kicker", textOf(duel, "kicker"));
        walkSay("fightTitle", textOf(duel, "title"));
        walkSay("turn", textOf(duel, "turn"));
        walkSay("turnNote", textOf(duel, "cost") + " \xC2\xB7 " + textOf(duel, "daysLeft"));
        // Under the timeline, what is coming at him and why he is first or not. Once it is over
        // nothing is coming: the timeline is out of its panel's stack, and one line says so
        const std::string warn = over ? std::string() : textOf(duel, "warn");

        walkSay("quick", over ? std::string("THE FIGHT IS OVER") : textOf(duel, "quick"));
        walkSay("warn", warn.empty() ? std::string(" ") : warn);
        walkShow("timeline", not over, "orderPanel");
        walkShow("warn", not over, "orderPanel");   // (the timeline says what is coming before his next turn)

        auto registry = ecs->getSystem<GlossRegistry>();

        // The beasts, across the top, a panel each: its name, how it stands over its rule, the
        // move it means to make by its name alone. What it is and what he knows of it is its
        // gloss's to say, and so is what the move does: only once he knows
        const bool pack = walk.targets.size() > 1;
        EntityRef foeRow = inWalk("foeRow");

        for (int i = 0; i < MaxFoes; ++i)
        {
            const std::string n = std::to_string(i);
            const bool there = static_cast<size_t>(i) < walk.targets.size();

            // A beast there is none of has no panel: the row holds the ones there are, or the
            // column does where there is no row (one column, fight-narrow.yaml)
            if (EntityRef panel = inWalk("foePanel" + n); not panel.empty())
            {
                if (foeRow.empty())
                    walkShow("foePanel" + n, there);
                else
                {
                    wrapIn(foeRow, panel)->get<PositionComponent>()->setVisibility(there);
                    panel->get<PositionComponent>()->setVisibility(there);
                }
            }

            if (not there)
                continue;

            const ElementMap& t = walk.targets[i];
            const bool down = intOf(t, "dead") > 0;
            const bool aimed = pack and not down and intOf(t, "chosen") > 0;

            if (auto panel = walkPiece<Panel>("foePanel" + n))
            {
                if (panel->title and panel->title->spec.text != textOf(t, "name"))
                    panel->setHeading(ecs, textOf(t, "name"));

                // Of several, the one his blows go to says so
                const std::string aside = aimed ? "AIMED AT" : "";

                if (panel->aside and panel->aside->spec.text != aside)
                    panel->setAside(ecs, aside.empty() ? " " : aside);
            }

            walkSay("foeLife" + n, textOf(t, "life"));
            // A beast that is down means nothing more, and neither does one left standing once it is over
            walkSay("foeIntent" + n, over or down ? std::string(" ") : textOf(t, "intent"));

            if (auto label = walkPiece<Label>("foeLife" + n))
            {
                const std::string color = down ? "ink-muted" : "ink";

                if (label->spec.color != color)
                    label->setColor(ecs, color);
            }

            if (auto label = walkPiece<Label>("foeIntent" + n))
            {
                const std::string color = textOf(t, "tone") == "loss" ? "vermilion" : "ink-muted";

                if (label->spec.color != color)
                    label->setColor(ecs, color);
            }

            if (auto rule = walkPiece<ProgressRule>("foeBar" + n))
                rule->setPercent(ecs, static_cast<float>(intOf(t, "percent")), not walkFresh);

            if (not registry)
                continue;

            // What it is, and what he knows of it
            GlossSpec beast;
            beast.inlineValues = true;
            beast.title = textOf(t, "name");
            beast.text = textOf(duel, "about");

            for (const auto& r : walk.foe)
                beast.rows.push_back({textOf(r, "name"), textOf(r, "value"), intOf(r, "muted") > 0 ? "muted" : ""});

            registry->set(FoeGloss + n, beast);

            // The move it means to make: its name, a line on it, and what it does to him, said
            // only once it has been used on him or once he has watched the beast
            GlossSpec move;
            move.inlineValues = true;
            move.title = textOf(t, "moveName");
            move.text = textOf(t, "moveLine");

            if (down)
            {
                move.title = "Down";
                move.text = "It does nothing more.";
            }
            else if (intOf(t, "moveKnown") > 0)
                move.rows.push_back({"It does", textOf(t, "moveEffect"), textOf(t, "tone") == "loss" ? "loss" : "muted"});
            else
                move.footnote = "WHAT IT DOES IS NOT KNOWN YET: SEEN ONCE IT IS USED ON HIM, OR ONCE THE BEAST IS WATCHED";

            registry->set(FoeMoveGloss + n, move);
        }

        // The glosses hang on what they speak of, once: how it stands for the beast, the move's
        // name and its rule for the move
        if (not walkGlossed)
        {
            walkGlossed = true;

            for (int i = 0; i < MaxFoes; ++i)
            {
                const std::string n = std::to_string(i);

                for (const auto& [part, key] : {std::pair<std::string, std::string>{"foeLife" + n, FoeGloss + n}, {"foeBar" + n, FoeGloss + n},
                                                {"foeIntent" + n, FoeMoveGloss + n}, {"foeCast" + n, FoeMoveGloss + n}})
                {
                    if (EntityRef piece = inWalk(part); not piece.empty())
                        attachGloss(ecs, piece, key);
                }
            }
        }

        // He, under them: his Vitality, his breath when he has something to spend it on, the
        // rule of his blow, what he strikes and guards with
        if (auto panel = walkPiece<Panel>("heroPanel"); panel and panel->title and panel->title->spec.text != save.name)
            panel->setHeading(ecs, save.name);

        const int vitality = intOf(duel, "vitality");
        const int most = std::max(1, intOf(duel, "vitalityMost"));

        walkSay("heroVit", "VITALITY " + std::to_string(vitality) + " OF " + std::to_string(most));

        if (auto bar = walkPiece<ProgressRule>("heroBar"))
            bar->setPercent(ecs, 100.0f * static_cast<float>(vitality) / static_cast<float>(most), not walkFresh);

        walkSay("low", textOf(duel, "low"));
        walkShow("low", not textOf(duel, "low").empty(), "heroPanel");

        const bool breathes = not textOf(duel, "breath").empty();

        walkSay("breath", textOf(duel, "breath"));
        walkShow("breath", breathes, "heroPanel");
        walkShow("breathBar", breathes, "heroPanel");

        if (auto bar = walkPiece<ProgressRule>("breathBar"))
            bar->setPercent(ecs, static_cast<float>(intOf(duel, "breathPercent")), not walkFresh);

        std::vector<LedgerRowSpec> hero;

        for (const auto& r : walk.hero)
        {
            LedgerRowSpec row;
            row.id = textOf(r, "id");
            row.name = textOf(r, "name");
            row.glyph = textOf(r, "glyph");
            row.value = textOf(r, "value");
            row.rate = textOf(r, "rate");

            hero.push_back(row);
        }

        walkLedger("hero", hero);

        // His turn: a button a thing he may do, pressed and done; against several, a button a
        // beast to aim at first
        fillMoves(over, handed);

        // The timeline: as it stands, or as the move under the mouse would leave it; its corner
        // says how far into his day he is
        fillTimeline(hoveredMove());

        if (auto panel = walkPiece<Panel>("orderPanel"); panel and panel->aside)
        {
            const std::string aside = over ? std::string(" ") : textOf(duel, "dayText");

            if (panel->aside->spec.text != aside)
                panel->setAside(ecs, aside);
        }

        walkShow("aimNote", pack and not over, "menuPanel");
        walkShow("aims", pack and not over, "menuPanel");
        walkShow("moves", not over, "menuPanel");
        walkShow("moveNote", not over, "menuPanel");

        if (auto hand = walkPiece<Button>("hand"))
        {
            hand->setLabel(ecs, textOf(duel, "hand"));
            hand->setDisabled(ecs, over);
        }

        walkShow("hand", not over, "menuPanel");

        // Once it is over: what it came to, and the one way on
        walkSay("spoils", over ? textOf(duel, "title") + ". " + textOf(duel, "spoils") : std::string());
        walkShow("spoils", over, "menuPanel");

        if (auto act = walkPiece<Button>("act"))
        {
            act->setLabel(ecs, textOf(duel, "back"));
            act->setDisabled(ecs, not over);
        }

        walkShow("act", over, "menuPanel");

        // The account of the fight: a line a blow, the turn at its head, a tinted line a day gone
        std::vector<LogEntry> lines;

        for (const auto& a : walk.state.account)
            lines.push_back(accountLine(a));

        walkLog("account", lines);

        walkSay("footnote", textOf(duel, "footnote"));

        // A fight handed to his stance plays on by itself
        if (handed and not over and autoLeft <= 0.0f)
            autoLeft = opt.autoTurnMs;

        if (not handed or over)
            autoLeft = 0.0f;
    }

    // ---- the timeline: who acts when, in time ------------------------------------------------------

    std::string LifeScene::hoveredMove() const
    {
        const std::string prefix = MoveTag;

        for (const auto& [tag, button] : walkMoves)
        {
            EntityRef face = button.face;

            if (tag.rfind(prefix, 0) != 0 or face.empty() or not face->has<ButtonState>())
                continue;

            if (face->get<ButtonState>()->hovered)
                return tag.substr(prefix.size());
        }

        return std::string();
    }

    std::vector<std::pair<std::string, float>> LifeScene::timelineShown() const
    {
        std::vector<std::pair<std::string, float>> shown;

        auto add = [&shown](const Label& label) {
            EntityRef entity = label.entity;

            if (entity.empty())
                return;

            auto pos = entity->get<PositionComponent>();

            if (pos->visible)
                shown.emplace_back(label.spec.text, pos->x);
        };

        for (const auto& chip : timeChips)
            add(chip.label);

        if (timeDayLabel)
            add(*timeDayLabel);

        return shown;
    }

    std::vector<EntityRef> LifeScene::timelinePieces() const
    {
        std::vector<EntityRef> pieces;

        if (timeBar)
            pieces.push_back(timeBar->root);

        if (timeDayLabel)
            pieces.push_back(timeDayLabel->entity);

        pieces.push_back(timeDay);

        for (const auto& chip : timeChips)
        {
            for (EntityRef piece : {chip.ground, chip.frame, chip.tick, chip.label.entity})
                pieces.push_back(piece);
        }

        return pieces;
    }

    float LifeScene::timelineSpanOf(const std::string& move) const
    {
        for (const auto& t : walk.timeline)
        {
            if (textOf(t, "move") == move and textOf(t, "kind") == "next")
                return static_cast<float>(intOf(t, "at")) / 10.0f;
        }

        return move.empty() ? 0.0f : timelineSpanOf("");
    }

    void LifeScene::fillTimeline(const std::string& move)
    {
        EntitySystem* ecs = ecsRef;
        EntityRef box = inWalk("timeline");

        timelineMove = move;

        // A screen built this pass does not hold all its pieces yet (its box among them): the
        // timeline is drawn the next one, when the mouse check finds it not drawn (runWalk)
        if (box.empty() or walkPage.empty() or walkFresh)
        {
            timelineMove = "-";
            return;
        }

        constexpr size_t Pool = 10;                    // Tags the gauge has room for: now, his next turn, the beasts before it, the one after
        constexpr float BarY = 44.0f;                  // The gauge, from the box's top
        constexpr float HeroY = 14.0f;                 // His tags over it
        constexpr float FoeY = 66.0f;                  // The beasts' under it, and a row lower when two would touch
        constexpr float FoeRow = 22.0f;
        constexpr float ChipH = 18.0f;
        constexpr float ChipPad = 5.0f;

        // The pieces stand in the timeline's panel, not on the page: in one column the panel
        // scrolls, and what is in it is clipped to the column (on the page they were drawn over
        // its head once scrolled up to it)
        Panel* panel = walkPiece<Panel>("orderPanel");
        EntityRef holder = panel ? panel->root : walkPage;

        // Made on another screen (one built anew without being closed, as a save opened on a
        // fight): those pieces went with it
        if (timePage != holder.id)
        {
            timeChips.clear();
            timeBar.reset();
            timeDay = EntityRef{};
            timeDayLabel.reset();
            timePage = holder.id;
        }

        const auto boxPos = box->get<PositionComponent>();

        // The box is as wide as its panel leaves it (the file draws it for the widest), and so
        // is the gauge across it
        if (panel and std::abs(boxPos->width - panel->innerWidth()) > 0.5f)
            boxPos->setWidth(panel->innerWidth());

        const float width = boxPos->width;
        const int z = static_cast<int>(boxPos->z) + 2;

        if (timeBar and std::abs(timeBar->spec.width - width) > 0.5f)
            timeBar->setWidth(ecs, width);

        auto anchorIn = [&box](EntityRef piece, float x, float y) {
            auto anchor = piece->get<UiAnchor>();

            anchor->setLeftAnchor(PosAnchor{box.id, AnchorType::Left});
            anchor->setLeftMargin(x);
            anchor->setTopAnchor(PosAnchor{box.id, AnchorType::Top});
            anchor->setTopMargin(y);
        };

        // Made once a screen: the gauge, the day across it, and a pool of tags to move
        if (not timeBar)
        {
            ProgressRuleSpec gauge;
            gauge.width = width;
            gauge.nib = false;
            gauge.z = z;

            ProgressRule bar = makeProgressRule(ecs, gauge);
            bar.forecast->get<ThemeComponent>()->setElement("timeline.hatch");
            bar.fill->get<ThemeComponent>()->setElement("timeline.fill");
            anchorIn(bar.root, 0.0f, BarY);
            holder->get<Prefab>()->addToPrefab(bar.root);
            timeBar = bar;

            auto day = makeUiSimple2DShape(ecs, Shape2D::Square, 2.0f, 40.0f);
            day.get<PositionComponent>()->setZ(static_cast<float>(z + 3));
            ecs->attach<ThemeComponent>(day.entity, "timeline.day");
            anchorIn(day.entity, 0.0f, BarY - 16.0f);
            holder->get<Prefab>()->addToPrefab(day.entity);
            timeDay = day.entity;

            Label dayLabel = makeLabel(ecs, {"label", "THE DAY ENDS", "status-time", Align::Left, Overflow::Grow, 0.0f, 0, z + 3});
            anchorIn(dayLabel.entity, 0.0f, 0.0f);
            holder->get<Prefab>()->addToPrefab(dayLabel.entity);
            timeDayLabel = dayLabel;

            for (size_t i = 0; i < Pool; ++i)
            {
                TimeChip chip;

                auto ground = makeUiSimple2DShape(ecs, Shape2D::Square, 40.0f, ChipH);
                ground.get<PositionComponent>()->setZ(static_cast<float>(z + 4));
                ecs->attach<ThemeComponent>(ground.entity, "timeline.chip.ground");
                anchorIn(ground.entity, 0.0f, 0.0f);
                holder->get<Prefab>()->addToPrefab(ground.entity);
                chip.ground = ground.entity;

                auto frame = makeStrokeRect2DShape(ecs, 40.0f, ChipH, {255.0f, 255.0f, 255.0f, 255.0f}, 1.0f);
                frame.get<PositionComponent>()->setZ(static_cast<float>(z + 5));
                ecs->attach<ThemeComponent>(frame.entity, "timeline.chip.hero");
                anchorIn(frame.entity, 0.0f, 0.0f);
                holder->get<Prefab>()->addToPrefab(frame.entity);
                chip.frame = frame.entity;

                auto tick = makeUiSimple2DShape(ecs, Shape2D::Square, 2.0f, 10.0f);
                tick.get<PositionComponent>()->setZ(static_cast<float>(z + 3));
                ecs->attach<ThemeComponent>(tick.entity, "timeline.tick.hero");
                anchorIn(tick.entity, 0.0f, 0.0f);
                holder->get<Prefab>()->addToPrefab(tick.entity);
                chip.tick = tick.entity;

                chip.label = makeLabel(ecs, {"label", " ", "ink", Align::Left, Overflow::Grow, 0.0f, 0, z + 6});
                anchorIn(chip.label.entity, 0.0f, 0.0f);
                holder->get<Prefab>()->addToPrefab(chip.label.entity);

                timeChips.push_back(chip);
            }
        }

        // The marks of the move asked for, or as it stands when it has none (fleeing has none)
        std::vector<const ElementMap*> marks;

        for (const auto& t : walk.timeline)
        {
            if (textOf(t, "move") == move)
                marks.push_back(&t);
        }

        if (marks.empty() and not move.empty())
        {
            for (const auto& t : walk.timeline)
            {
                if (textOf(t, "move").empty())
                    marks.push_back(&t);
            }
        }

        const bool over = not textOf(walk.duel, "over").empty();

        // The gauge: hatched from now to his next turn, empty until a turn is told
        float span = 0.0f;

        for (const ElementMap* m : marks)
        {
            if (textOf(*m, "kind") == "next")
                span = static_cast<float>(intOf(*m, "at")) / 10.0f;
        }

        timeBar->setPercent(ecs, 0.0f, false);
        timeBar->setForecast(ecs, over ? 0.0f : span);
        timeBar->root->get<PositionComponent>()->setVisibility(not over);

        // A tag: its word in its frame, over or under the gauge, its tick on it
        std::vector<std::vector<std::pair<float, float>>> placed(3);   // The beasts' tags placed, row by row
        size_t used = 0;

        auto tag = [&](const std::string& text, const std::string& tone, float x, bool above, bool tick) {
            if (used >= timeChips.size())
                return;

            TimeChip& chip = timeChips[used++];

            const std::string color = tone == "hero" ? "lapis" : tone == "bite" ? "vermilion" : "ink-muted";
            const std::string frame = "timeline.chip." + (tone == "held" ? std::string("calm") : tone);
            const std::string mark = tone == "hero" ? "timeline.tick.hero" : tone == "bite" ? "timeline.tick.foe" : "timeline.tick.muted";

            if (chip.label.spec.text != text)
                chip.label.setText(ecs, text);

            if (chip.label.spec.color != color)
                chip.label.setColor(ecs, color);

            const float w = chip.label.entity->get<PositionComponent>()->width + 2.0f * ChipPad;
            const float cx = std::clamp(x - w / 2.0f, 0.0f, std::max(0.0f, width - w));

            float y = HeroY;

            if (not above)
            {
                // The first row where it touches no tag already there (the last one if none is free)
                size_t row = 0;

                for (; row + 1 < placed.size(); ++row)
                {
                    bool touches = false;

                    for (const auto& [a, b] : placed[row])
                        touches = touches or (cx < b + 3.0f and cx + w > a - 3.0f);

                    if (not touches)
                        break;
                }

                placed[row].emplace_back(cx, cx + w);
                y = FoeY + FoeRow * static_cast<float>(row);
            }

            chip.ground->get<PositionComponent>()->setWidth(w);
            chip.frame->get<PositionComponent>()->setWidth(w);
            chip.frame->get<ThemeComponent>()->setElement(frame);

            for (EntityRef piece : {chip.ground, chip.frame})
            {
                piece->get<UiAnchor>()->setLeftMargin(cx);
                piece->get<UiAnchor>()->setTopMargin(y);
            }

            chip.label.entity->get<UiAnchor>()->setLeftMargin(cx + ChipPad);
            chip.label.entity->get<UiAnchor>()->setTopMargin(y + 2.0f);

            chip.tick->get<ThemeComponent>()->setElement(mark);
            chip.tick->get<UiAnchor>()->setLeftMargin(x - 1.0f);
            chip.tick->get<UiAnchor>()->setTopMargin(above ? HeroY + ChipH : BarY + 6.0f);

            for (EntityRef piece : {chip.ground, chip.frame, chip.label.entity})
                piece->get<PositionComponent>()->setVisibility(not over);

            chip.tick->get<PositionComponent>()->setVisibility(tick and not over);
        };

        tag("NOW", "hero", 0.0f, true, false);

        bool day = false;
        std::string warn;

        for (const ElementMap* m : marks)
        {
            const std::string kind = textOf(*m, "kind");
            const float x = width * static_cast<float>(intOf(*m, "at")) / 1000.0f;

            if (kind == "next")
                tag(textOf(*m, "text"), "hero", x, true, true);
            else if (kind == "foe")
                tag(textOf(*m, "text"), textOf(*m, "tone"), x, false, true);
            else if (kind == "warn")
                warn = textOf(*m, "text");
            else if (kind == "day")
            {
                day = true;
                timeDay->get<UiAnchor>()->setLeftMargin(x - 1.0f);

                const float w = timeDayLabel->entity->get<PositionComponent>()->width;

                timeDayLabel->entity->get<UiAnchor>()->setLeftMargin(std::clamp(x - w - 4.0f, 0.0f, std::max(0.0f, width - w)));
            }
        }

        timeDay->get<PositionComponent>()->setVisibility(day and not over);
        timeDayLabel->entity->get<PositionComponent>()->setVisibility(day and not over);

        for (size_t i = used; i < timeChips.size(); ++i)
        {
            for (EntityRef piece : {timeChips[i].ground, timeChips[i].frame, timeChips[i].tick, timeChips[i].label.entity})
                piece->get<PositionComponent>()->setVisibility(false);
        }

        // What is coming before his next turn, as the move under the mouse leaves it
        if (not over and not warn.empty())
            walkSay("warn", warn);
    }

    void LifeScene::tellTurn(size_t shown)
    {
        walkBeats.clear();
        castDone.clear();

        for (size_t i = shown; i < walk.state.account.size(); ++i)
        {
            const ElementMap& a = walk.state.account[i];

            WalkBeat beat;
            beat.by = textOf(a, "by");
            beat.n = intOf(a, "n");
            beat.hp = a.count("hp") > 0 ? intOf(a, "hp") : -1;
            beat.left = a.count("left") > 0 ? intOf(a, "left") : -1;
            beat.missed = intOf(a, "missed") > 0;
            beat.line = accountLine(a);

            walkBeats.push_back(beat);
        }

        walkTelling = true;
        beatCasting = false;
        beatLeft = 0.0f;

        // How long the telling takes: a blow's cast and its line, a beat for any other line. The
        // timeline's gauge fills over that time, to where the move put his next turn
        timeTellMs = 0.0f;

        for (const auto& beat : walkBeats)
            timeTellMs += (beat.by.empty() ? 0.0f : CastMs) + BeatMs;

        if (timeBar)
            timeBar->setGlide(ecsRef, timeSpan, std::max(BeatMs, timeTellMs));

        // The turn is played: what the fight handed over waits for it to be told
        autoLeft = 0.0f;
    }

    void LifeScene::runTelling(float ms)
    {
        if (not walkTelling)
            return;

        EntitySystem* ecs = ecsRef;

        // Gone from under the telling (the screen was built again, or closed): nothing left to tell
        if (walkPage.empty() or walkScreen != "fight")
        {
            walkTelling = false;
            walkBeats.clear();

            return;
        }

        beatLeft -= ms;

        if (beatLeft > 0.0f)
            return;

        if (walkBeats.empty())
        {
            endTelling();
            return;
        }

        const WalkBeat& beat = walkBeats.front();

        // Whose blow it is: his rule, or the beast's under what it meant to do
        const std::string who = beat.by == "hero" ? std::string("hero") : beat.by == "foe" ? "foe" + std::to_string(beat.n) : std::string();
        const std::string rule = beat.by == "hero" ? std::string("heroCast") : "foeCast" + std::to_string(beat.n);

        // A blow is seen coming: its rule fills first, once a turn for each who strikes
        if (not who.empty() and not beatCasting and std::find(castDone.begin(), castDone.end(), who) == castDone.end())
        {
            if (auto cast = walkPiece<ProgressRule>(rule))
            {
                cast->setPercent(ecs, 0.0f, false);
                cast->setGlide(ecs, 100.0f, CastMs);
            }

            castDone.push_back(who);
            beatCasting = true;
            beatLeft = CastMs;

            return;
        }

        beatCasting = false;

        // Then it is told, and what it did is seen: his Vitality after a beast's blow, what is left
        // of the beast after his
        if (auto log = walkPiece<EventLog>("account"))
        {
            log->append(ecs, beat.line);
            log->scrollToEnd(ecs);
        }

        if (beat.by == "foe" and beat.hp >= 0)
        {
            const int most = std::max(1, intOf(walk.duel, "vitalityMost"));

            walkSay("heroVit", "VITALITY " + std::to_string(beat.hp) + " OF " + std::to_string(most));

            if (auto bar = walkPiece<ProgressRule>("heroBar"))
            {
                bar->setPercent(ecs, 100.0f * static_cast<float>(beat.hp) / static_cast<float>(most));

                if (beat.line.kind == LogKind::Loss)
                    showHurt("heroBar", beat.line.figure);
            }
        }

        if (beat.by == "hero" and beat.left >= 0)
        {
            const std::string name = "foeBar" + std::to_string(beat.n);

            if (auto bar = walkPiece<ProgressRule>(name))
            {
                bar->setPercent(ecs, static_cast<float>(beat.left));
                showHurt(name, "\xE2\x88\x92" + beat.line.figure);
            }
        }

        // A blow that found nothing says so over the one it was meant for
        if (beat.missed)
            showTag(beat.by == "hero" ? "foeBar" + std::to_string(beat.n) : std::string("heroBar"), "MISS", "label", "ink-muted");

        if (not who.empty())
        {
            if (auto cast = walkPiece<ProgressRule>(rule))
                cast->setPercent(ecs, 0.0f, false);
        }

        walkBeats.pop_front();
        beatLeft = BeatMs;
    }

    void LifeScene::showHurt(const std::string& rule, const std::string& taken)
    {
        auto bar = walkPiece<ProgressRule>(rule);

        if (not bar or Motion::reduced() or bar->fill.empty() or not bar->fill->has<ThemeComponent>())
            return;

        // The rule is in the loss's colour for a moment, while it falls
        const _unique_id fill = bar->fill.id;

        bar->fill->get<ThemeComponent>()->setElement("progress.fill.hurt");

        passing.push_back({HurtMs, nullptr, [this, fill]() {
            if (auto entity = ecsRef->getEntity(fill); entity and entity->has<ThemeComponent>())
                entity->get<ThemeComponent>()->setElement("progress.fill");
        }});

        // And what was taken lifts off it
        if (not taken.empty())
            showTag(rule, taken, "figure", "status-loss");
    }

    void LifeScene::showTag(const std::string& rule, const std::string& text, const std::string& style, const std::string& color)
    {
        auto bar = walkPiece<ProgressRule>(rule);

        if (not bar or Motion::reduced() or bar->root.empty())
            return;

        // A word or a figure over the rule's right end: it lifts, and goes
        LabelSpec spec;
        spec.style = style;
        spec.color = color;
        spec.text = text;
        spec.z = static_cast<int>(HurtZ);

        Label figure = makeLabel(ecsRef, spec);

        auto anchor = figure.entity.get<UiAnchor>();
        anchor->setBottomAnchor(PosAnchor{bar->root.id, AnchorType::Top});
        anchor->setRightAnchor(PosAnchor{bar->root.id, AnchorType::Right});

        const _unique_id id = figure.entity.id;

        passing.push_back({HurtRiseMs,
            [this, id, risen = 0.0f](float ms) mutable {
                if (auto entity = ecsRef->getEntity(id))
                {
                    risen += ms * HurtRise;

                    entity->get<UiAnchor>()->setBottomMargin(risen);
                }
            },
            [this, id]() {
                if (ecsRef->getEntity(id))
                    ecsRef->removeEntity(id);
            }});
    }

    void LifeScene::endTelling()
    {
        walkTelling = false;
        beatCasting = false;
        walkBeats.clear();
        castDone.clear();

        // Every rule of a blow is empty again, and the screen says where the fight stands: the
        // next turn's menu, what each beast means to do now, or its end
        for (int i = 0; i < MaxFoes; ++i)
        {
            if (auto cast = walkPiece<ProgressRule>("foeCast" + std::to_string(i)))
                cast->setPercent(ecsRef, 0.0f, false);
        }

        if (auto cast = walkPiece<ProgressRule>("heroCast"))
            cast->setPercent(ecsRef, 0.0f, false);

        showWalk();
    }

    void LifeScene::runWalk(float ms)
    {
        // A width that came while the screen was just built: it is built for it now
        if (walkCooling > 0.0f)
        {
            walkCooling -= ms;

            if (walkCooling <= 0.0f and walkRefitDue)
            {
                walkRefitDue = false;
                fitWalk(windowWidth, windowHeight);
            }
        }

        // A width that came while the screen did not hold its pieces yet: it is sized for it now
        if (walkSettling <= 0 and not walkPage.empty() and (windowWidth < NarrowBelow) == walkNarrow and std::abs(windowWidth - walkWidth) > 0.5f)
            sizeWalk(windowWidth, windowHeight);

        if (walkSettling <= 0 and not walkPage.empty() and walkNarrow)
            fitNarrowHead();

        // The mouse on another move of his turn: the timeline shows where that move would put him
        if (walkScreen == "fight" and not walkPage.empty() and not walkTelling)
        {
            const std::string move = hoveredMove();

            if (move != timelineMove)
                fillTimeline(move);
        }

        // The buttons of his turn were made again: the hover is asked for while they are placed
        if (moveHoverStale > 0)
        {
            --moveHoverStale;

            ecsRef->sendEvent(RefreshHoverEvent{});
        }

        // The account follows the order of the turn over it, whenever that one changes its height
        fitAccount();

        // A screen built a pass ago: its layouts hold what stands in them now. What was taken out
        // of a stack is taken out again (it is the wrap its layout holds that leaves, and the
        // layout did not hold it yet), and from here its rules may move
        if (walkSettling > 0 and not walkPage.empty())
        {
            // Not before its columns hold their panels: the layouts take what they are given when
            // their turn comes, which is not always the pass after
            bool held = true;

            for (const char* name : {"walkLeft", "walkMiddle", "walkRight"})
            {
                if (EntityRef column = inWalk(name); not column.empty() and column->has<VerticalLayout>())
                    held = held and not column->get<VerticalLayout>()->entities.empty();
            }

            if (held)
            {
                // And a pass or two more, for the stacks inside the panels
                --walkSettling;
                walkFresh = false;

                if (walkScreen == "fight")
                    fillFight();
                else
                    fillSite();
            }
        }

        // The rows chosen in a list filled a moment ago are lit once the list holds them: a list
        // of the walk's screen, or the records' on the page
        {
            for (auto it = walkPicks.begin(); it != walkPicks.end();)
            {
                auto list = walkPiece<ActivityList>(it->first);

                if (not list)
                    list = piece<ActivityList>(it->first);

                if (not list or list->selected() == it->second)
                    it = walkPicks.erase(it);
                else if (it->second.empty() or list->find(ecsRef, it->second))
                {
                    const std::string id = it->second;

                    it = walkPicks.erase(it);
                    list->select(ecsRef, id);
                }
                else
                    ++it;
            }
        }

        // Under the bug report the fight waits, the turn being told as the one his stance would play
        if (reportAsked or reportOpen)
            return;

        // A turn being told goes on; nothing else of the fight moves until it is
        if (walkTelling)
        {
            runTelling(ms);
            return;
        }

        if (autoLeft <= 0.0f or not onWalk() or walkScreen != "fight")
            return;

        autoLeft -= ms;

        if (autoLeft > 0.0f)
            return;

        autoLeft = 0.0f;

        walkDo(WalkAct{"step", "", 0});
    }

    // ---- what he presses there ------------------------------------------------------------------------

    void LifeScene::onWalkSelect(const ActivitySelectedEvent& event)
    {
        if (event.list == RecordsList)
        {
            if (event.id != save.queue.recipe)
                chooseRecord(event.id);

            return;
        }

        if (not onWalk() or ended or walkTelling)
            return;

        if (event.list == SeeksList and event.id != seekChosen)
        {
            seekChosen = event.id;
            fillSite();
        }
        else if (event.list == WorksList and event.id != workChosen)
        {
            workChosen = event.id;
            fillSite();
        }
    }

    void LifeScene::onWalkConfirm(const ActivityActivatedEvent& event)
    {
        if (event.list == RecordsList)
        {
            if (event.id != save.queue.recipe)
                chooseRecord(event.id);

            queueWalks(1);

            return;
        }

        if (not onWalk() or ended or walkTelling)
            return;

        // A second click on a row does what its button does
        if (event.list == SeeksList)
        {
            seekChosen = event.id;
            onWalkButton(SeekTag);
        }
        else if (event.list == WorksList)
        {
            workChosen = event.id;
            onWalkButton(WorkTag);
        }
    }

    bool LifeScene::onWalkButton(const std::string& tag)
    {
        if (tag.rfind(WalkTag, 0) != 0)
            return false;

        // The Adventure page's own: the form of a queue
        if (tag == FewerTag or tag == MoreTag)
        {
            save.queue.count = std::clamp(save.queue.count + (tag == MoreTag ? 1 : -1), 1, MostWalks);
            fillAdventure();
            autoSave();
        }
        else if (tag == StopTag)
        {
            // The next of the ways it may stop, round and round
            const RecordList& stops = shelf.stops;

            for (size_t i = 0; i < stops.size(); ++i)
            {
                if (textOf(stops[i], "id") != textOf(shelf.form, "stop"))
                    continue;

                const ElementMap& next = stops[(i + 1) % stops.size()];

                save.queue.stop = textOf(next, "id");
                save.queue.value = intOf(next, "value");
                save.queue.item = textOf(next, "item");

                break;
            }

            fillAdventure();
            autoSave();
        }
        else if (tag == NewTag)
            setOutAnew();
        else if (tag.rfind(RunTag, 0) == 0)
        {
            // A line's own button: that walk, once, now
            chooseRecord(tag.substr(RunTag.size()));

            if (save.queue.recipe == tag.substr(RunTag.size()))
                queueWalks(1);
        }
        else if (tag == OnceTag)
            queueWalks(1);
        else if (tag == ManyTag)
            queueWalks(std::max(1, save.queue.count));
        else if (tag == LastTag)
        {
            save.lastWalk = not save.lastWalk;
            fillAdventure();
            autoSave();
        }

        // (nor while a turn of a fight is being told: the press would be of a screen a turn late)
        if (not onWalk() or ended or walkTelling)
            return true;

        if (tag == SeekTag and not seekChosen.empty())
        {
            for (const auto& s : walk.seeks)
            {
                if (textOf(s, "id") == seekChosen)
                {
                    const WalkAct act{"seek", textOf(s, "mob"), intOf(s, "count")};

                    seekChosen.clear();
                    walkDo(act);

                    break;
                }
            }
        }
        else if (tag == ObserveTag and not seekChosen.empty())
        {
            for (const auto& s : walk.seeks)
            {
                if (textOf(s, "id") == seekChosen)
                {
                    walkDo(WalkAct{"observe", textOf(s, "mob"), 0});

                    break;
                }
            }
        }
        else if (tag == WorkTag and not workChosen.empty())
        {
            // "gather.berries" is a gathering of berries; the others are their own name
            const std::string gather = "gather.";

            if (workChosen.rfind(gather, 0) == 0)
                walkDo(WalkAct{"gather", workChosen.substr(gather.size()), 0});
            else
                walkDo(WalkAct{workChosen, "", 0});
        }
        else if (tag == LeaveTag)
            walkDo(WalkAct{"leave", "", 0});
        else if (tag.rfind(OptionTag, 0) == 0)
        {
            const size_t index = static_cast<size_t>(std::atoi(tag.substr(std::string(OptionTag).size()).c_str()));

            if (index < walk.options.size())
                walkDo(WalkAct{"option", textOf(walk.options[index], "id"), 0});
        }
        else if (tag.rfind(MoveTag, 0) == 0)
            walkDo(WalkAct{"move", tag.substr(std::string(MoveTag).size()), 0});
        else if (tag.rfind(AimTag, 0) == 0)
            walkDo(WalkAct{"target", "", std::atoi(tag.substr(std::string(AimTag).size()).c_str())});
        else if (tag == ActTag)
            walkDo(WalkAct{textOf(walk.duel, "over").empty() ? "do" : "back", "", 0});
        else if (tag == HandTag)
            walkDo(WalkAct{"auto", "", 0});

        return true;
    }

    // ---- the Adventure page: the records kept, and a queue of walks --------------------------------

    int LifeScene::adventureTab() const
    {
        auto tabs = piece<Tabs>("tabs");

        if (not tabs)
            return -1;

        for (size_t i = 0; i < tabs->spec.items.size(); ++i)
        {
            const std::string label = lower(tabs->spec.items[i].label);

            if (label == AdventureName or label == "walks")
                return static_cast<int>(i);
        }

        return -1;
    }

    bool LifeScene::adventureKnown() const
    {
        // The page is there once he has been on a walk, his first: before, it has nothing to
        // show. A record kept by the world, or a walk under way, says he has
        return not save.recipes.empty() or save.walksByHand > 0 or save.walksByRecord > 0 or onWalk();
    }

    void LifeScene::showAdventure(bool shown)
    {
        adventureShown = shown and adventureKnown();

        // The pages of the middle column take each other's place: showTown lays them
        if (adventureShown)
            optionsShown = false;

        showTown(adventureShown ? false : townShown);

        if (adventureShown)
            fillAdventure();
    }

    void LifeScene::fitAdventure(float width, float height)
    {
        EntitySystem* ecs = ecsRef;

        if (EntityRef body = named(AdventureName); not body.empty())
        {
            body->get<PositionComponent>()->setWidth(width);
            body->get<PositionComponent>()->setHeight(height);
        }

        // Each region is a panel as wide as the column: what stands in each follows (widen)
        for (const char* name : {"zonePanel", "recordsPanel", "queuePanel", "workPanel"})
            widen(piece<Panel>(name), width);

        if (auto foot = piece<Label>("footLine"))
            foot->setWidth(ecs, width);

        // Across the width where a line loses its cells, the lines are other lines: filled again
        // (in view or not: a page out of view is filled all the same, to be right when it is opened)
        const bool had = adventureRoomy();

        adventureWidth = width;

        if (had != adventureRoomy())
            fillAdventure();

        layAdventure();
    }

    bool LifeScene::adventureRoomy() const
    {
        // Before the page has been given a width, as the files draw it: with room
        return adventureWidth <= 0.0f or adventureWidth >= AdventureCells;
    }

    void LifeScene::layAdventure()
    {
        EntitySystem* ecs = ecsRef;

        auto panel = piece<Panel>("zonePanel");

        if (not panel)
            return;

        const float inner = panel->innerWidth();

        auto widthOf = [this](const char* name) {
            EntityRef piece = named(name);

            return piece.empty() ? 0.0f : piece->get<PositionComponent>()->width;
        };

        auto wide = [this, ecs](const char* name, float to) {
            if (auto label = piece<Label>(name); label and std::abs(label->spec.width - to) > 0.5f)
                label->setWidth(ecs, std::max(1.0f, to));
        };

        // The places' tiles share the panel's width out, as the town's do
        if (auto grid = piece<PlaceGrid>(ZonesName); grid and std::abs(grid->spec.width - inner) > 0.5f)
            grid->setWidth(ecs, inner);

        // What a walk by hand is at the left, the button that sets out at the right: the line
        // has what the button leaves
        if (EntityRef row = named("zoneGo"); not row.empty() and std::abs(row->get<PositionComponent>()->width - inner) > 0.5f)
            row->get<PositionComponent>()->setWidth(inner);

        const float noteRoom = inner - widthOf("walkNew") - AdventureGap;

        wide("zoneNote", noteRoom);

        if (auto note = piece<Label>("zoneNote"))
        {
            const std::string said = noteRoom >= 160.0f ? "A walk he chooses day by day. The record is kept." : " ";

            if (note->spec.text != said)
                note->setText(ecs, said);
        }

        // The four cells of the forecast share the row
        const float within = piece<Panel>("queuePanel") ? piece<Panel>("queuePanel")->innerWidth() : inner;
        const float cell = std::floor((within - 3.0f - 6.0f * AdventureGap) / 4.0f);

        for (const char* name : {"cellTake", "cellUse", "cellBring", "cellTeach"})
        {
            if (EntityRef column = named(name); not column.empty())
                column->get<PositionComponent>()->setWidth(cell);
        }

        for (const char* name : {"takeHead", "takeFigure", "takeNote", "useHead", "useFigure", "useNote", "bringHead", "bringFigure", "bringNote", "teachHead", "teachFigure", "teachNote"})
            wide(name, cell);

        // What the count comes to, after the stepper; and the two buttons at the form's right end
        wide("queueTotal", within - widthOf("timesHead") - widthOf("queueFewer") - widthOf("queueTimes") - widthOf("queueMore") - 4.0f * 8.0f);
        wide("queueSpacer", within - widthOf("walkOnce") - widthOf("walkMany") - 2.0f * 8.0f);
    }

    void LifeScene::chooseZone(const std::string& zone)
    {
        // A queue under way keeps its place: the page is its own until it is done
        if (save.queue.left > 0 or not save.walking.empty())
            return;

        if (zone.empty() or zone == adventureZone)
            return;

        adventureZone = zone;

        fillAdventure();
    }

    void LifeScene::setOutAnew()
    {
        if (ended or onWalk() or adventureZone.empty())
            return;

        if (not save.running.empty())
        {
            appendLog({save.age, "Already at work: " + activityName(save.running), LogKind::Note, "", ""});
            return;
        }

        // A walk made by hand, as its tile of the Life page begins the first one
        showAdventure(false);
        onConfirm(ActivityActivatedEvent{ActivitiesList, WalkActivity + adventureZone});
    }

    void LifeScene::chooseRecord(const std::string& id)
    {
        // A queue under way keeps its record: the form is its own until it is done
        if (save.queue.left > 0 or not save.walking.empty())
            return;

        if (save.queue.recipe == id)
            return;

        save.queue = WalkQueue{};
        save.queue.recipe = id;

        fillAdventure();
    }

    void LifeScene::markAdventure()
    {
        // Its tab is on the page for who has been on a walk, and for no one else
        if (auto tabs = piece<Tabs>("tabs"); tabs and adventureTab() >= 0 and tabs->tabs[adventureTab()].shown != adventureKnown())
            tabs->setShown(ecsRef, adventureTab(), adventureKnown());
    }

    void LifeScene::fillAdventure(bool again)
    {
        EntitySystem* ecs = ecsRef;

        // A page just built has a list with nothing in it, whatever the last one held
        if (again)
        {
            walkFilled.erase(RecordsName);
            walkFilled.erase(ZonesName);
        }

        markAdventure();

        if (named(AdventureName).empty())
            return;

        const bool busy = save.queue.left > 0 or not save.walking.empty();
        const bool free = save.running.empty() and not onWalk() and not ended;

        auto zoneOfRecord = [this](const std::string& id) -> std::string {
            for (const auto& r : save.recipes)
            {
                if (r.id == id)
                    return textOf(r.fields, "zone");
            }

            return {};
        };

        // The places he may walk to, open or not yet: a tile each, in the table's order
        std::vector<const RuleActivity*> places;

        for (const auto& a : activities)
        {
            if (not textOf(a.fields, "zone").empty() and boolOf(a.fields, "listed"))
                places.push_back(&a);
        }

        auto placeOf = [&places](const std::string& zone) -> const RuleActivity* {
            for (const RuleActivity* a : places)
            {
                if (textOf(a->fields, "zone") == zone)
                    return a;
            }

            return nullptr;
        };

        // The one in view: a queue under way keeps its own; else the one chosen while it has its
        // tile, the one of the walk the form holds, the first there is
        if (busy and not zoneOfRecord(save.walking.empty() ? save.queue.recipe : save.walking).empty())
            adventureZone = zoneOfRecord(save.walking.empty() ? save.queue.recipe : save.walking);
        else if (not placeOf(adventureZone))
        {
            adventureZone = zoneOfRecord(save.queue.recipe);

            if (not placeOf(adventureZone))
                adventureZone = places.empty() ? std::string() : textOf(places.front()->fields, "zone");
        }

        // And the walk the form is filled for is one of that place: another's is let go of here,
        // and the best of this one's is taken below, once the rules have said what each is worth
        if (not busy and zoneOfRecord(save.queue.recipe) != adventureZone)
            save.queue = WalkQueue{};

        if (not askWalk("records", WalkAct{}, "", shelf))
            return;

        auto registry = ecs->getSystem<GlossRegistry>();

        auto say = [this, ecs](const char* name, const std::string& text) {
            if (auto label = piece<Label>(name); label and label->spec.text != text)
                label->setText(ecs, text);
        };

        auto shown = [this](const char* name, bool there) {
            EntityRef node = named(name);
            EntityRef body = named(AdventureName);

            if (node.empty() or body.empty())
                return;

            wrapIn(body, node)->get<PositionComponent>()->setVisibility(there);
            node->get<PositionComponent>()->setVisibility(there);
        };

        auto placeRow = [this](const std::string& zone) -> const ElementMap* {
            for (const auto& row : shelf.places)
            {
                if (textOf(row, "id") == zone)
                    return &row;
            }

            return nullptr;
        };

        // A narrow column (the phone's) has no room for everything a line says at its right:
        // what kind of walk it is and what he is short of are left to the line under the name
        const bool cells = adventureRoomy();

        // ---- Where to walk: a tile a place, as the town's places are shown, the open ones first
        // and the one in view lit. Its mark and its name, a seal for each level he knows it to,
        // and a line: what kind of walk it is and what it costs, or what it asks of him ----
        std::vector<PlaceTileSpec> open;
        std::vector<PlaceTileSpec> closed;
        std::string placeSignature;
        std::string ladders;

        for (const RuleActivity* a : places)
        {
            const std::string zone = textOf(a->fields, "zone");
            const ElementMap* said = placeRow(zone);
            const bool locked = boolOf(a->fields, "locked");

            PlaceTileSpec tile;
            tile.id = ZoneTile + zone;
            tile.name = textOf(a->fields, "name");
            tile.glyph = textOf(a->fields, "glyph").empty() ? std::string("adventure") : textOf(a->fields, "glyph");
            tile.level = said ? intOf(*said, "level") : 0;
            tile.of = said and intOf(*said, "levels") > 0 ? intOf(*said, "levels") : 4;
            tile.glossKey = "activity/" + textOf(a->fields, "id");

            // Its gloss, the activity's, says under the rest what each level of knowing it brings
            if (said)
            {
                walkLadders[zone] = ladderRows(*said);

                for (const auto& r : walkLadders[zone])
                    ladders += zone + "|" + r.label + "|" + r.value + "|" + r.tone + ";";
            }

            if (locked)
            {
                // Shut to him: what it asks, in a line
                std::string asks;

                for (const auto& r : a->requires)
                {
                    const int needed = intOf(r, "needed");
                    const std::string label = textOf(r, "label");

                    asks += (asks.empty() ? "Asks " : ", ") + (needed > 1 ? label + " " + std::to_string(needed) : lower(label));
                }

                tile.line = upper(asks.empty() ? std::string("Not open to him yet") : asks);
            }
            else
            {
                const std::string kind = said ? textOf(*said, "kind") : std::string();
                const std::string cost = said ? textOf(*said, "cost") : std::to_string(intOf(a->fields, "months")) + " mo";

                tile.line = upper(kind.empty() ? cost : kind + " \xC2\xB7 " + cost);
            }

            placeSignature += tile.id + "|" + tile.name + "|" + tile.line + "|" + std::to_string(tile.level) + "/" + std::to_string(tile.of) + ";";

            (locked ? closed : open).push_back(tile);
        }

        if (ladders != laddersSaid)
        {
            laddersSaid = ladders;
            glossActivities();
        }

        const size_t opened = open.size();
        const size_t shut = closed.size();

        open.insert(open.end(), closed.begin(), closed.end());

        if (auto grid = piece<PlaceGrid>(ZonesName))
        {
            // Made again only when what a tile says has changed; the one lit follows the place in view
            if (walkFilled[ZonesName] != placeSignature)
            {
                walkFilled[ZonesName] = placeSignature;
                grid->setPlaces(ecs, open);
            }

            const std::string lit = placeOf(adventureZone) ? ZoneTile + adventureZone : std::string();

            if (grid->selected() != lit)
                grid->select(ecs, lit);
        }

        if (auto panel = piece<Panel>("zonePanel"))
        {
            const std::string count = std::to_string(opened) + " OPEN" + (shut > 0 ? " \xC2\xB7 " + std::to_string(shut) + " CLOSED" : std::string());

            if (panel->aside and panel->aside->spec.text != count)
                panel->setAside(ecs, count);
        }

        // The place in view, and the button that sets out for it: named with the place and its
        // months, or saying why not
        const RuleActivity* place = placeOf(adventureZone);
        const bool shutNow = not place or boolOf(place->fields, "locked");
        // (inside a sentence its article is a small one: "Set out for the Hedgerows")
        std::string placeName = place ? textOf(place->fields, "name") : std::string();

        if (placeName.rfind("The ", 0) == 0)
            placeName[0] = 't';

        if (auto anew = piece<Button>("walkNew"))
        {
            std::string label = place ? "Set out for " + placeName : std::string("Set out");

            if (place and shutNow)
            {
                label = "Set out";

                for (const auto& r : place->requires)
                {
                    if (intOf(r, "current") < intOf(r, "needed"))
                    {
                        label += " \xC2\xB7 asks " + (intOf(r, "needed") > 1 ? textOf(r, "label") + " " + std::to_string(intOf(r, "needed")) : lower(textOf(r, "label")));
                        break;
                    }
                }
            }
            else if (place and not free)
                label = busy ? "Set out \xC2\xB7 a queue is walking" : "Set out \xC2\xB7 he is at work";

            if (place and not shutNow and free)
                label += " \xC2\xB7 " + std::to_string(intOf(place->fields, "months")) + " mo";

            if (anew->label.spec.text != label)
                anew->setLabel(ecs, label);

            anew->setDisabled(ecs, shutNow or busy or not free);
        }

        // ---- The walks kept of that place: a table, the best first (the most brought a month) ----
        std::vector<const ElementMap*> kept;

        for (const auto& r : shelf.shelf)
        {
            if (textOf(r, "state") == "kept" and textOf(r, "zone") == adventureZone)
                kept.push_back(&r);
        }

        // (the later of two as good as each other first: the list is in the order they were made)
        std::stable_sort(kept.rbegin(), kept.rend(), [](const ElementMap* a, const ElementMap* b) { return intOf(*a, "worth") < intOf(*b, "worth"); });

        // The walk the form is filled for is one of them: the best, when none of them was held
        if (not busy and not kept.empty()
            and std::none_of(kept.begin(), kept.end(), [this](const ElementMap* r) { return textOf(*r, "id") == save.queue.recipe; }))
        {
            save.queue = WalkQueue{};
            save.queue.recipe = textOf(*kept.front(), "id");

            if (not askWalk("records", WalkAct{}, "", shelf))
                return;

            // (asked again, the rows are other tables: found again by their id)
            kept.clear();

            for (const auto& r : shelf.shelf)
            {
                if (textOf(r, "state") == "kept" and textOf(r, "zone") == adventureZone)
                    kept.push_back(&r);
            }

            std::stable_sort(kept.rbegin(), kept.rend(), [](const ElementMap* a, const ElementMap* b) { return intOf(*a, "worth") < intOf(*b, "worth"); });
        }

        std::vector<ActivityRowSpec> rows;
        std::string lit;
        std::string signature = cells ? "c;" : "n;";

        for (const ElementMap* kept_ : kept)
        {
            const ElementMap& r = *kept_;

            ActivityRowSpec row;
            row.id = textOf(r, "id");
            row.name = textOf(r, "label").empty() ? textOf(r, "title") : textOf(r, "label");
            row.glyph = "";
            row.cost = textOf(r, "costTop");
            row.each = textOf(r, "costBottom");
            row.eachTone = boolOf(r, "hurt") ? "loss" : "muted";
            row.line = textOf(r, "used");
            row.glossKey = RecordGloss + row.id;
            row.action = "Walk";
            row.actionTag = RunTag + row.id;

            // What it brings has a cell of its own where the column has room for it
            if (cells)
            {
                row.asideTop = textOf(r, "bringsShort").empty() ? std::string("\xE2\x80\x94") : textOf(r, "bringsShort");
                row.asideBottom = textOf(r, "teachShort");
                row.asideWidth = RecordBrings;
            }
            else if (not textOf(r, "bringsShort").empty())
                row.line += " \xC2\xB7 " + textOf(r, "bringsShort");

            // The way there shut to him, or a queue walking: it is not to be walked now
            if (intOf(r, "locked") > 0)
            {
                row.state = ActivityState::Locked;
                row.line = lower(textOf(r, "asks"));
            }

            if (row.state != ActivityState::Locked and row.id == save.queue.recipe)
                lit = row.id;

            signature += row.id + "|" + row.line + "|" + row.cost + "|" + row.each + "|" + row.asideTop + "|" + row.asideBottom + "|" + std::to_string(static_cast<int>(row.state)) + ";";

            rows.push_back(row);

            if (not registry)
                continue;

            // The gloss: what the walk takes and what it brings home, as the record saw it
            GlossSpec gloss;
            gloss.inlineValues = true;
            gloss.title = row.name;
            gloss.aside = textOf(r, "kind");
            gloss.text = textOf(r, "title") + " \xC2\xB7 " + lower(textOf(r, "note"));

            gloss.rows.push_back({"COST", "", "", true});
            gloss.rows.push_back({"Time", textOf(r, "time"), "time"});
            gloss.rows.push_back({"Vitality", textOf(r, "vitality"), intOf(r, "locked") > 0 ? "muted" : "loss"});
            gloss.rows.push_back({"From the bag", textOf(r, "takes"), "loss"});
            gloss.rows.push_back({"REWARD", "", "", true});

            // A thing a line: a list of them in one row would wrap into a block
            for (const auto& [text, tone] : {std::pair<std::string, const char*>{textOf(r, "brings"), "gain"}, {textOf(r, "learns"), "muted"}})
            {
                for (const auto& thing : listed(text))
                    gloss.rows.push_back({thing, "", tone});
            }

            gloss.footnote = "WALKED AGAIN WITH NO FIGHT FOUGHT";

            registry->set(RecordGloss + row.id, gloss);
        }

        if (auto list = piece<ActivityList>(RecordsName))
        {
            if (walkFilled[RecordsName] != signature or list->spec.groups.empty() != rows.empty())
            {
                // (the head's last column is there with the lines' cell)
                list->spec.headName = "WALK";
                list->spec.headCost = "COST";
                list->spec.headAside = cells ? "BRINGS" : "";

                walkFilled[RecordsName] = signature;
                list->setRows(ecs, rows.empty() ? std::vector<ActivityGroup>{} : std::vector<ActivityGroup>{{"", rows}});
            }

            walkPicks[RecordsName] = lit;
        }

        if (auto panel = piece<Panel>("recordsPanel"))
        {
            const std::string heading = place ? "Records of " + placeName : std::string("Records");
            const std::string count = rows.empty() ? std::string(" ") : std::to_string(rows.size()) + " KEPT";

            if (panel->title and panel->title->spec.text != heading)
                panel->setHeading(ecs, heading);

            if (panel->aside and panel->aside->spec.text != count)
                panel->setAside(ecs, count);

            // No walk kept of it: one faint line, and no table
            recordsShown = not rows.empty();

            if (EntityRef node = named(RecordsName); not node.empty())
            {
                wrapIn(panel->body, node)->get<PositionComponent>()->setVisibility(recordsShown);
                node->get<PositionComponent>()->setVisibility(recordsShown);
            }

            if (EntityRef note = named("recordsNote"); not note.empty())
            {
                wrapIn(panel->body, note)->get<PositionComponent>()->setVisibility(not recordsShown);
                note->get<PositionComponent>()->setVisibility(not recordsShown);
            }
        }

        // ---- Walk it again: the walk chosen, so many times, and what that comes to ----
        const ElementMap& form = shelf.form;
        const bool chosen = not textOf(form, "recipe").empty() and recordsShown;
        const std::string blocked = textOf(form, "blocked");

        shown("queuePanel", chosen and not busy);
        shown("workPanel", busy);

        if (auto panel = piece<Panel>("queuePanel"); panel and chosen)
        {
            // (the panel's title keeps its room: the standing fact is said short, and not at all on a narrow column)
            const std::string subject = upper(textOf(form, "subject")) + (cells ? " \xC2\xB7 NO FIGHT FOUGHT" : "");

            if (panel->aside and panel->aside->spec.text != subject)
                panel->setAside(ecs, subject);
        }

        say("queueTimes", std::to_string(std::max(1, intOf(form, "count"))));
        say("queueTotal", chosen ? "\xC2\xB7 " + textOf(form, "takeFigure") + " \xC2\xB7 " + textOf(form, "homeAt") : std::string());

        for (const char* cell : {"take", "use", "bring", "teach"})
        {
            say((std::string(cell) + "Figure").c_str(), textOf(form, std::string(cell) + "Figure"));
            say((std::string(cell) + "Note").c_str(), textOf(form, std::string(cell) + "Note"));
        }

        // Short of what the walks use: the figure says so, and a line under the row says when it stops
        if (auto figure = piece<Label>("useFigure"))
        {
            const std::string color = intOf(form, "useShort") > 0 ? "vermilion" : "ink";

            if (figure->spec.color != color)
                figure->setColor(ecs, color);
        }

        const std::string warning = chosen ? (blocked.empty() ? textOf(form, "warning") : blocked) : std::string();

        say("queueWarn", warning.empty() ? std::string(" ") : warning);

        if (auto panel = piece<Panel>("queuePanel"))
        {
            if (EntityRef warn = named("queueWarn"); not warn.empty())
            {
                wrapIn(panel->body, warn)->get<PositionComponent>()->setVisibility(not warning.empty());
                warn->get<PositionComponent>()->setVisibility(not warning.empty() and chosen and not busy);
            }
        }

        if (auto stop = piece<Button>("queueStop"))
        {
            stop->setLabel(ecs, chosen ? textOf(form, "stopLabel") : std::string("Never"));
            stop->setDisabled(ecs, not chosen or busy);
        }

        if (auto fewer = piece<Button>("queueFewer"))
            fewer->setDisabled(ecs, not chosen or busy or intOf(form, "count") <= 1);

        if (auto more = piece<Button>("queueMore"))
            more->setDisabled(ecs, not chosen or busy or intOf(form, "count") >= MostWalks);

        const int each = std::max(1, intOf(form, "months")) / std::max(1, intOf(form, "count"));

        if (auto once = piece<Button>("walkOnce"))
        {
            const std::string label = chosen ? "Walk once \xC2\xB7 " + std::to_string(each) + " mo" : std::string("Walk once");

            if (once->label.spec.text != label)
                once->setLabel(ecs, label);

            once->setDisabled(ecs, busy or not free or not chosen or not blocked.empty());
        }

        if (auto many = piece<Button>("walkMany"))
        {
            const std::string label = chosen ? textOf(form, "many") + " \xC2\xB7 " + std::to_string(intOf(form, "months")) + " mo" : std::string("Queue");

            if (many->label.spec.text != label)
                many->setLabel(ecs, label);

            many->setDisabled(ecs, busy or not free or not chosen or not blocked.empty());
        }

        // ---- A queue under way, in the form's place: which walk of how many, how far along ----
        if (busy)
        {
            const int total = std::max(1, save.queue.total);
            const int at = std::clamp(total - save.queue.left, 1, total);
            const RuleActivity* walking = activityOf(save.running);
            const int months = walking ? std::max(1, intOf(walking->fields, "months")) : 1;

            std::string line = "WALK " + std::to_string(at) + " OF " + std::to_string(total) + " \xC2\xB7 " + upper(placeName) + " \xC2\xB7 NO FIGHT IS FOUGHT AGAIN";

            if (save.lastWalk)
                line += " \xC2\xB7 THE LAST: THE QUEUE STOPS WITH IT";

            say("atWalk", line);

            // The walks done solid, the one under way as far as its months have run, the rest hatched
            if (auto bar = piece<ProgressRule>("queueBar"))
            {
                const float done = static_cast<float>(at - 1) + std::clamp(static_cast<float>(save.monthsIn) / static_cast<float>(months), 0.0f, 1.0f);

                bar->setPercent(ecs, 100.0f * done / static_cast<float>(total), false);
                bar->setForecast(ecs, 100.0f);
            }
        }

        if (auto last = piece<Button>("queueLast"))
        {
            last->setLabel(ecs, save.lastWalk ? "Go on with the queue" : "Finish this walk, then stop");
            last->setDisabled(ecs, not busy or save.queue.left <= 0);
        }

        // ---- The foot: the scribe's line of the place, at his age ----
        const ElementMap* said = placeRow(adventureZone);

        say("footLine", said ? textOf(*said, "foot") : std::string());

        // The buttons are other widths now: what stands beside them has what they leave
        layAdventure();
    }

    void LifeScene::queueWalks(int count)
    {
        if (ended or onWalk())
            return;

        if (not save.running.empty())
        {
            appendLog({save.age, "Already at work: " + activityName(save.running), LogKind::Note, "", ""});
            return;
        }

        if (save.queue.recipe.empty())
            return;

        // The stop as the form shows it: what a queue counts is the rules' to say
        save.queue.stop = textOf(shelf.form, "stop").empty() ? std::string("never") : textOf(shelf.form, "stop");
        save.queue.value = intOf(shelf.form, "stopValue");
        save.queue.item = textOf(shelf.form, "stopItem");
        save.queue.left = std::max(1, count);
        save.queue.total = save.queue.left;
        save.lastWalk = false;

        if (beginReplay())
        {
            ++save.begun;
            save.queued = std::max(save.queued, std::max(1, count));
        }

        publish();
        fillAdventure();
        autoSave();
    }

    void LifeScene::stopQueue(const std::string& why, bool loss)
    {
        save.queue.left = 0;
        save.queue.total = 0;
        save.lastWalk = false;
        save.walking.clear();

        if (why.empty())
            return;

        // Said on the Life page: a queue that stops is a work that has to be chosen again
        appendLog({save.age, why, loss ? LogKind::Loss : LogKind::Note, "", "adventure"});
        toast(why);
    }

    bool LifeScene::beginReplay()
    {
        const WalkRecipe* recipe = nullptr;

        for (const auto& r : save.recipes)
        {
            if (r.id == save.queue.recipe)
                recipe = &r;
        }

        if (not recipe)
        {
            stopQueue("", false);
            return false;
        }

        const std::string activity = WalkActivity + textOf(recipe->fields, "zone");
        const RuleActivity* at = activityOf(activity);

        // The way there may have shut since the record was made: an age, a thing it asks
        if (not at or boolOf(at->fields, "locked") or not boolOf(at->fields, "listed"))
        {
            stopQueue("The queue stopped: the way to " + textOf(recipe->fields, "name") + " is shut to him now", true);
            return false;
        }

        RuleWalk answer;

        if (not askWalk("begin", WalkAct{}, recipe->id, answer))
        {
            stopQueue("", false);
            return false;
        }

        if (not answer.stopped.empty())
        {
            stopQueue(answer.stopped, false);
            return false;
        }

        if (not answer.blocked.empty())
        {
            // A queue told to end when the bag runs short ends there; another says what it lacks
            const bool asked = save.queue.stop == "bag";

            stopQueue(asked ? "The bag is short of " + answer.blocked + ": the queue is done" : "The queue stopped at the door: the bag lacks " + answer.blocked, not asked);
            return false;
        }

        // What the record takes from the bag is taken as he sets out
        takeStats(answer.after);

        --save.queue.left;
        save.walking = recipe->id;

        save.running = activity;
        save.monthsIn = 0;
        rules.running = save.running;

        if (auto facts = ecsRef->getSystem<WorldFacts>())
            facts->setFact("activity." + activity + ".state", std::string("running"));

        paused = false;
        sinceMonth = 0.0f;

        return true;
    }

    void LifeScene::homeFromWalk()
    {
        // A walk made by hand gave what it gave on site: its months were the way and the stay
        if (save.walking.empty())
        {
            fillAdventure();
            return;
        }

        // By the record: its days are folded into him now that he is home
        RuleWalk answer;

        if (askWalk("term", WalkAct{}, save.walking, answer))
        {
            takeStats(answer.after);
            writeEntries(answer.entries);
            ++save.walksByRecord;

            for (const auto& r : save.recipes)
            {
                if (r.id == save.walking)
                    ++save.replayedAt[textOf(r.fields, "zone")];
            }
        }

        const bool more = save.queue.left > 0 and not save.lastWalk;

        save.walking.clear();

        refreshHoldings();

        // What the walk left him too hurt to go on with ends the queue with the life
        if (not death.empty())
            return;

        if (more)
        {
            // The list as he stands now: the next walk asks what the first did
            rules.done = save.terms();
            rules.activities(save.age, save.character(), activities);

            beginReplay();
        }
        else
        {
            const bool queued = save.queue.total > 1;

            save.queue.left = 0;
            save.queue.total = 0;
            save.lastWalk = false;

            if (queued)
                toast("The queue is done");
        }

        fillAdventure();
    }
}
