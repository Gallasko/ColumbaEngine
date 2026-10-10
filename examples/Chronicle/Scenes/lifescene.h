#pragma once

#include <deque>
#include <string>
#include <unordered_map>
#include <vector>

#include "Scene/scenemanager.h"

#include "Core/rulevm.h"
#include "Core/factrouter.h"
#include "UI/activityrow.h"   // ActivitySelectedEvent, ActivityActivatedEvent
#include "UI/button.h"
#include "UI/label.h"
#include "UI/placetile.h"     // PlaceSelectedEvent
#include "UI/tabs.h"          // TabSelectedEvent
#include "lifesave.h"

namespace chronicle
{
    struct LifeSceneOptions
    {
        bool fresh = false;            // --fresh: a new life, nothing earned. A first life (`lives` 1), led by its guide
        int lives = 1;                 // With `fresh`: which life of the world it is. From the second, the old opening: 7 years old, a year of rations, every task listed
        int guide = 0;                 // --guide N, with `fresh`: a first life past the step N of its guide
        bool noSave = false;           // --no-save: the mockup's life, never written
        bool freshWithoutSave = false; // No save to load: a first life, not the mockup's (the web build)
        std::string savePath = "save/chronicle/life.sz";
        std::string rulesRoot = "examples/Chronicle/rules";
        std::string pageFile = "res/chronicle/ui/life.yaml";
        std::string compactFile = "res/chronicle/ui/life-compact.yaml";   // Below the three columns' size
        std::string phoneFile = "res/chronicle/ui/life-phone.yaml";       // A window narrower than a phone held sideways: one column
        std::string endingFile = "res/chronicle/ui/ending.yaml";          // Over the page, when a life ends
        float monthMs = 2000.0f;       // One month every so many ms while the loop runs (--month-ms)
    };

    // A step of the guide (achievements.pg, kind "guide") as the scene holds it
    struct GuideStep
    {
        std::string id;
        int order = 0;                 // Its place in the sequence, 0 for a step outside it
        std::string say;               // Its sentence, on the guide's leaf
        std::string point;             // What the hand points at: "button:begin", "tile:<id>", "holding:<id>", "stat:<key>", "clock", "log", ""
        std::string pointChosen;       // For a step that points at a tile: what the hand points at once that tile is chosen, "" to stay on it
        std::string sayChosen;         // And what is said then, "" for the same sentence
        pg::RecordList until;          // It ends when these hold; none: once `left` has run out
        float left = 0.0f;             // Milliseconds left of a step that ends with time
        bool world = false;            // Said once in a world, in whichever life meets it
        bool active = false;           // Being said now
    };

    // The Life screen: the page is res/chronicle/ui/life.yaml, built as it is; the scene does the
    // three things a file cannot. It runs the rules and the save in startUp, subscribes every
    // widget to a WorldFacts path in one function (wire), and turns input into game events
    // (select, confirm, a month passes). It computes nothing about the game: it calls the rule
    // scripts, writes paths, and the widgets react. The deeds (achievements.pg) are watched by
    // the engine's AchievementSys against the paths the scene publishes, and so are the steps of
    // a first life's guide and the lines of lore: the scene only does what each kind asks of the
    // page (a toast and a line, a sentence and a pointing hand, a line alone).
    struct LifeScene : public pg::Scene
    {
        explicit LifeScene(LifeSceneOptions opt = {});

        void init() override;          // The page from the file; nothing has data yet
        void startUp() override;       // Rules, save, rows, wire(), publish()
        void onLeave() override;       // Every subscription dropped; the page goes with the scene
        void execute() override;       // The deeds reached since the last frame; months tick on TickEvent

        // Public for the tests and the dev keys
        void onMonth();                // One month passes; a life with nothing left to live on ends
        bool saveNow();                // Writes the save (not with --no-save)
        void autoSave();               // The same with no line in the log: after a month, a choice, a thing done at once, a new life
        void tellAnalytics(const std::string& event = "");   // Where the life stands, kept by the analytics for its next row; with an event, a row now
        void newLife();                // A fresh life, rebuilt and republished
        void beginAgain();             // From the ending: it closes, the next life begins and says how the last one ended

        pg::EntityRef named(const std::string& name) const;   // A handle from the page, empty if none

        template <typename Piece>
        Piece* piece(const std::string& name) const
        {
            pg::EntityRef ent = named(name);

            if (ent.empty() or not ent->has<Piece>())
                return nullptr;

            return ent->get<Piece>().component;
        }

        LifeSceneOptions opt;
        LifeSave save;
        Rules rules;
        pg::EntityRef page;
        pg::EntityRef ending;          // The veil and the leaf over the page, while `ended`
        RuleEpitaph epitaph;           // What the ending says, from epitaph.pg
        std::vector<FactRouter::SubId> subs;
        bool paused = true;
        bool ended = false;            // The life is over and its ending is up: nothing passes until the next begins
        bool compact = false;          // Which file the page is: opt.compactFile or opt.pageFile
        bool phone = false;            // The page is opt.phoneFile: compact, in one column, with one row of tabs
        float phoneWidth = 0.0f;       // The window's width the phone page was built at
        std::string phonePanel;        // Phone: the panel in the choice's place ("parts", "holds", ...), "" for the choice or the town
        int sideTab = 0;               // Compact: the side panel in view (parts, holds, years, log)
        float windowWidth = 1320.0f;   // As the last fit saw it
        float windowHeight = 1020.0f;
        int rowRoom = -1;              // The room the row at work was made for
        int widthStep = 0;             // The three columns as built: which of their widths and heights (the roomiest the window held)
        int heightStep = 0;
        bool runningShown = false;     // "At work now" holds a row: the page is fitted to it
        float clockFitted = 0.0f;      // The height of the years' panel the log was fitted under
        bool buttonsDue = false;       // A page just built: its buttons are shown again once its layouts hold them

        // The guide of a first life: one step said at a time, the next waits for it to end
        GuideStep step;                // The step being said, `active` while one is
        std::deque<GuideStep> stepsDue;   // Reached, waiting for the one before to end
        int guideSteps = 0;            // How many steps its sequence has: a save past the last has no guide left
        pg::EntityRef hand;            // The pointing hand, made with the first step that points
        pg::_unique_id handOn = 0;     // What it stands beside, 0 while it points at nothing
        std::vector<pg::EntityRef> veil;   // The shade over the page around what is pointed at, a strip each
        bool veiled = false;           // The shade is up
        pg::EntityRef note;            // The leaf the guide's words stand on, under what is pointed at; made with the first step
        Label noteWords;               // The sentence being said
        Button noteSkip;               // "Skip tutorial": no step is said again
        Button noteNext;               // "Next": passes a step said for a time, and is not there for one that waits for him
        bool noteShown = false;        // The leaf is up
        bool nextShown = false;        // And its "Next" with it

        // The town: the world's, kept from life to life. Its page stands in the middle column in the
        // place of the choice, once a life has explored it
        RuleTown town;                 // The last answer of Rules::town
        bool townShown = false;        // The Town page is in view
        std::string gift;              // At an ending: the place he leaves his coin to, "" for none
        std::vector<std::pair<std::string, Button>> gifts;   // The ending's buttons: a place that can still be raised, each

        void showTown(bool shown);     // The Town page in the middle column, or the choice back in it

        int newInLife = 0;             // What has come onto the Life page while the Town page was in view: said at its tab's corner
        int newInTown = 0;             // And onto the Town page while the Life page was

        bool guiding() const;          // A first life whose guide has not said its last step
        int toastsUp() const { return toasts; }   // The toasts at the foot of the window now

    private:
        bool buildPage(bool compact, bool phone = false, float width = 0.0f);   // The page from its file, in place of the one there was. A phone's is built at the window's width
        void fit(float width, float height);   // The page to the window, swapping it at the breakpoint
        void fitFull(float width, float height);      // The middle column's width, the choice's and the log's heights
        void fitCompact(float width, float height);   // The main column's width, the choice's and the log's heights
        void fitPhone(float width, float height);     // The one column's width, and the height of what is under the tabs
        void fitEnding(float width, float height);    // The ending's veil to the window
        void fitTown(float width, float height);      // The Town page to the middle column
        int townTab() const;           // Which tab is the town's, -1 when the page has none
        void fillTown();               // The places' tiles, as the rules say the town stands
        void layTown();                // The tiles of the places on his page: the town grows with what he does
        void showNotices();            // The two counts above, at the corner of the tab of the page that is not in view
        bool told(const std::string& id) const;   // A step the world's guide has said already
        void seeTown();                // A place on the page for the first time is kept as seen, and said when it comes later than the town itself
        void publishTown();            // Their levels and lines, their glosses, and who raised what
        void openTown();               // A life has come to know the town: its tab, its page, a line and a toast
        void raisePlace(const std::string& place, int level);   // A work for the town is done: the place is the world's at that level
        void giveYears(int months);    // Years of his life given at once: the clock jumps, the world's with it
        void fillGifts();              // The ending's row of places his coin can be left to
        std::string giftStat() const;  // What of his is left to a place at his death: the rules say (coin)
        void chooseGift(const std::string& place);   // One of them chosen, or let go of
        bool fromLists(const std::string& list) const;      // An event of the choice's list or of the market's
        ActivityList* listHolding(const std::string& id) const;   // The one of them that has that row, nullptr for neither
        void clearChoice();            // Nothing chosen in either list
        float workingHeight() const;   // "At work now": its chrome alone, or with a running row
        void showWorkButtons();        // At nothing: the button that begins what is chosen, or the one that passes a month
        bool guideSaid() const;        // The guide line is up: a life led to its first work, with no guide to say so
        float guideRoom() const;       // What the guide line takes of "At work now", 0 when it is not up
        bool led() const;              // A life that has done nothing yet and is at nothing: the page leads it to its first work
        std::string firstWork() const; // The first activity of the list he may begin that takes months, "" for none
        void lead();                   // That work chosen for him; not in a first life, whose guide has him choose it
        int workRoom();                // What its row has the room to say beside its name: 0 its time, 1 the line under it too, 2 the tally as well
        void showRunning(const std::string& id);   // The row of the work at hand, made for the room there is
        void showSide(int index);      // Compact: one side panel in view, the others hidden
        void showPhone();              // Phone: the choice under the tabs, or the panel that has its place
        void wire();                   // THE one function with every subscription
        void rebuild();                // The rows only the save and the rules know: ledgers, lists, log, clock
        void addToLedger(const LifeResource& resource);   // One row of what he holds
        void fillSkills();             // The skills he has any of; one at 0 is not shown
        void fillActivities();         // The rows of what he may do; an activity the rules do not list has none
        void fillWindows();            // The doors on the clock, as the rules list them for his path
        void refreshHoldings();        // resources.pg's rows, rates, glosses and verdict for the character as he is
        bool boarded() const;          // The activity at work feeds him
        bool titled(const std::string& id) const;   // A holding he has or has not (a standing, a tie): the rules say so, and its row shows no figure
        const RuleActivity* activityOf(const std::string& id) const;   // From the last answer of Rules::activities, nullptr if none
        std::string tileNote(const RuleActivity& activity, bool& urgent) const;   // What a tile says under its name and time: when it closes, once that is near. What it asks is its gloss's
        void onHover(const ActivityHoveredEvent& event);   // The mouse came onto a tile or left them
        void preview();                // What the chosen activity, or the one the mouse is on, would make of his parts and of what he holds
        void previewHoldings(const RuleForecast* forecast);   // "46 -> 52" ("6/60 -> 12/60") on the rows it would add to; nullptr: the figures as they are
        std::string holdingText(const std::string& id, int amount) const;   // "12", or "12/60" for what has a most he can hold
        void followLog();              // The log taken to its end in a moment, for a reader who was there
        void runPassing(float ms);     // What lasts a moment, a tick further
        void endPassing();             // All of it ended at once: the scene leaves
        int roseOf(const std::string& key, const std::string& text);   // By how much a figure rose since it was last written, 0 if it did not
        void showGain(pg::EntityRef over, int amount);   // "+2" lifting off the figure that rose
        void toast(const std::string& text, float ms = 0.0f);   // A slip at the foot of the window, for a few seconds (its own time when given one)
        void endLife();                // The life is lost: the months stop and its ending comes up
        bool showEnding();             // The ending from its file, over the page, saying what epitaph.pg says
        void closeEnding();            // The ending leaves
        void alert(const std::vector<std::string>& stats);   // The parts a month took from, in red for a moment; the months stop at the first
        void clearAlert();             // Back to ink
        void publishThreat();          // The coming month's warning under his life, and in red what it would take from
        void readDeeds();              // achievements.pg's entries, and how long the guide is
        void takeAsRead();             // A life that opens past its first frame: the lore that already holds is not written now
        void watchDeed(const RuleAchievement& deed);   // One of them handed to the AchievementSys: it is reached when what it asks holds
        void registerDeeds();          // The deeds, the lore and the guide's steps not reached yet, handed to the AchievementSys
        void reachDeed(const std::string& id);   // What a deed gives, its line in the log; a line of lore; a step of the guide
        void reachStep(const RuleAchievement& entry);   // A step of the guide reached: said once the one before has ended
        bool stepChosen() const;       // The step being said points at a tile, and he has chosen it: its second half, the button that begins it
        pg::ElementMap watched() const;                  // Where the life stands, by the paths the deeds, the lore and the guide ask after
        bool holds(const pg::RecordList& asks) const;    // Every one of them, as the life stands now
        void runGuide();               // The step being said ends or goes on, the next one begins, the hand follows what it points at
        void endStep();                // The step being said has ended: the guide is one step further
        void nextStep();               // "Next": the step being said for a time ends now
        void skipGuide();              // "Skip tutorial": past its last step for good
        void makeNote();               // The leaf, its words and its two buttons
        void sayNote(pg::EntityRef target);   // The leaf with the step's sentence under what it points at, or gone with no step
        void clearGuide();             // No step, no hand, no shade: the life is over, or another begins
        pg::EntityRef pointed(const std::string& point) const;   // What a step points at, empty when it is not on the page
        void pointAt(pg::EntityRef target);     // The hand beside it and the shade around it, or neither for nothing
        void shadeAround(const std::vector<pg::EntityRef>& lit);   // The page under a shade, but what is given
        void takeStats(const pg::ElementMap& stats);     // A script's numbers become the character's
        void writeEntries(const pg::RecordList& entries); // A script's lines become the log's
        void publish();                // refreshHoldings + publishAll
        void publishAll();             // glossHoldings + publishCharacter + publishRules + the ghosts of what is selected
        void publishProjected(const RuleForecast* forecast);   // character.parts.<p>.projected: what the activity brings it to, else what it mends back to
        void publishCharacter();       // save -> character.*, life.*, skills.*, resources.*, stat.*, log.size, activity.running.*
        void publishRules();           // scripts -> life.next.*, life.headline.*, window.*, activity.<id>.state, .count and .requirement.<n>, done.<id>, thresholds
        void publishRunning();         // forecast for the activity at work -> activity.running.*
        void publishPace();            // -> activity.running.pace (idle, running, paused) and .glideMs: the rule follows the month's clock
        void pause(bool on);           // The months stop or run, and the page says which

        void onSelect(const ActivitySelectedEvent& event);
        void onConfirm(const ActivityActivatedEvent& event);
        void doAtOnce(const std::string& id, const RuleForecast& forecast);   // An activity that takes no time
        void onTab(const TabSelectedEvent& event);
        void onPlace(const PlaceSelectedEvent& event);   // A place's tile clicked: lit, and the work that raises it chosen with it

        // The hover glosses: what the numbers mean, from the same outputs
        void glossParts(const RuleForecast* forecast, const std::string& activity);   // parts/<p>
        void glossActivities();                                                         // activity/<id>
        void glossHoldings();                                                           // resource/<id>
        void glossWindows(const pg::RecordList& windows);                               // window/<id>

        void appendLog(const LogEntry& entry);
        std::string activityName(const std::string& id) const;

        template <typename Type>
        void setFact(const std::string& path, const Type& value);

        std::vector<RuleActivity> activities;    // The last answer of Rules::activities
        std::string listedRows;                   // What the list was built from: each listed row and how much it asks
        pg::RecordList holdings;                  // The last resources.pg rows
        std::vector<RuleGloss> holdingGlosses;    // And what hovering each says
        std::string death;                        // "" or why the character as he is cannot go on
        std::vector<std::string> threat;          // The parts the coming month would take from, as things stand
        std::vector<std::string> threatShown;     // The ones of them in red for it
        std::string warning;                      // What the rules say of the coming month ("RATIONS FOR 2 MONTHS"), "" for nothing
        std::string advice;                       // And what to do about it, as last said by a toast
        std::string chosen;                       // The activity chosen in the list, "" for none: what the Begin button begins
        std::string chosenList;                   // And the list it was chosen in: the choice's, or the market's
        bool leadDue = false;                     // The page chose it for him: its tile is lit once the list holds its row
        pg::EntityRef noteEdge;                   // The leaf's edge, and its ground a pixel inside it
        pg::EntityRef noteGround;
        float guideFitted = 0.0f;                 // The room the guide line had when the page was last fitted
        float handMs = 0.0f;                      // How long the hand has pointed where it does: it moves a little, to be seen
        std::string shaded;                       // What the shade was last laid around: laid again only when that moves
        std::string hovered;                      // The one the mouse is on, "" for none: previewed when nothing is chosen
        std::vector<std::string> known;           // The activities he could do when the list was last filled
        std::vector<std::string> stood;           // Every activity that has had a row in this life: one that has not is new, and its tab says so

        // Something that lasts a moment (a gain lifting off its figure, a lit line, a toast): the
        // milliseconds left, what each tick does with the ones it is given, what its end does
        struct Passing
        {
            float left = 0.0f;
            std::function<void(float)> each;
            std::function<void()> done;
        };

        std::vector<Passing> passing;
        float quiet = 0.0f;                       // Milliseconds left of a page arriving: nothing is shown as a gain meanwhile
        std::unordered_map<std::string, int> figures;   // The figure each skill and holding last had on the page, to see it rise
        int toasts = 0;                           // The toasts up, one over another
        std::string endedLine;                    // The line the next life opens with: how this one ended
        std::string endedAge;                     // And the age it ended at, as the head wrote it
        std::unordered_map<std::string, int> caps;   // The most each capped stat can be
        std::vector<RuleAchievement> deeds;       // The achievements.pg output
        std::vector<std::string> reached;         // Deeds unlocked, waiting for execute()
        std::vector<RuleMilestone> milestones;    // The last milestones.pg output
        pg::ElementMap next;                      // The last next milestone
        pg::RecordList nextAsks;                  // What it asks of the path he is headed for
        std::unordered_map<std::string, pg::EntityRef> handles;
        float sinceMonth = 0.0f;
        std::vector<std::string> alerted;         // The parts in red
        float alertLeft = 0.0f;                   // Milliseconds of red left
        bool endangered = false;                  // The last month took from what the life hangs on
    };
}
