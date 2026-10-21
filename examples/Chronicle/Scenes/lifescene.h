#pragma once

#include <algorithm>
#include <deque>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "Scene/scenemanager.h"

#include "Core/rulevm.h"
#include "Core/factrouter.h"
#include "UI/activityrow.h"   // ActivitySelectedEvent, ActivityActivatedEvent
#include "UI/button.h"
#include "UI/label.h"
#include "UI/progressrule.h"   // the fight's timeline gauge
#include "UI/placetile.h"     // PlaceSelectedEvent
#include "UI/resourceledger.h" // LedgerRowSpec
#include "UI/eventlog.h"      // LogEntry
#include "UI/gloss.h"         // GlossRow
#include "UI/tabs.h"          // TabSelectedEvent
#include "UI/switch.h"        // SwitchToggledEvent
#include "UI/radiocard.h"     // RadioChosenEvent
#include "UI/wordfield.h"     // WordChangedEvent
#include "Core/settings.h"
#include "Core/bugsend.h"     // ReportFlow, SendResult, ReportLimit
#include "lifesave.h"

namespace chronicle
{
    struct Panel;

    struct LifeSceneOptions
    {
        bool fresh = false;            // --fresh: a new life at six, nothing earned. A first life (`lives` 1), led by its guide
        int lives = 1;                 // With `fresh`: which life of the world it is. From the second, no guide, and the works of his sixth year all listed
        float age = 0.0f;              // --age N, with `fresh`: opened at that age (7 or more), his sixth year behind him and not lived (`boyOfSeven`)
        int guide = 0;                 // --guide N, with `fresh`: a first life past the step N of its guide
        bool noSave = false;           // --no-save: the mockup's life, never written
        bool freshWithoutSave = false; // No save to load: a first life, not the mockup's (the web build)
        std::string savePath = "save/chronicle/life.sz";
        std::string rulesRoot = "examples/Chronicle/rules";
        std::string pageFile = "res/chronicle/ui/life.yaml";
        std::string compactFile = "res/chronicle/ui/life-compact.yaml";   // Below the three columns' size
        std::string phoneFile = "res/chronicle/ui/life-phone.yaml";       // A window narrower than a phone held sideways: one column
        std::string endingFile = "res/chronicle/ui/ending.yaml";          // Over the page, when a life ends
        std::string siteFile = "res/chronicle/ui/expedition.yaml";        // Over the page, while he is on a walk outside the town
        std::string siteNarrowFile = "res/chronicle/ui/expedition-narrow.yaml";   // The same in one column, in a window too narrow for three
        std::string fightFile = "res/chronicle/ui/fight.yaml";            // In its place, while he fights there
        std::string fightNarrowFile = "res/chronicle/ui/fight-narrow.yaml";
        int walkSeed = 0;              // The dice of the walks: 0 takes them from the clock, a test gives its own
        std::string walk;              // --walk <zone>: a later life, never written, opened on a walk to that zone with what the zone asks
        std::string build;             // --build <id>: a later life, never written, of one of testbuilds.pg's builds (its age, what it holds, every place known); "list" says them
        std::vector<std::pair<std::string, int>> sets;   // --set <stat>=<n>, on top of the build ("know.<key>" for what the world knows)
        bool saveAsked = false;        // --save given: a life of --walk or --build is written there, as any life
        float autoTurnMs = 600.0f;     // A fight handed to his stance plays a turn every so many ms
        float monthMs = 2000.0f;       // One month every so many ms while the loop runs (--month-ms); a child's months are a part of it (`LifeScene::monthMs`)
        unsigned long long now = 0;    // The wall clock, in seconds since 1970: 0 reads the system's, a test gives its own

        // The Options page (lifeoptions.cpp)
        std::string burnFile = "res/chronicle/ui/burn.yaml";       // The wipe's dialog, over the page
        std::string replaceFile = "res/chronicle/ui/replace.yaml"; // Asked before a save loaded takes the place of a game under way
        std::string reportFile = "res/chronicle/ui/report.yaml";   // The bug report's dialog, natively (a browser has its own, in the page)
        std::optional<LifeSave> opened;   // A save loaded from a file, opened in the place of the one at savePath (the scene is made again with it)
        bool clearFacts = false;       // The scene is made again in the place of another: the facts the last one wrote are the last life's, cleared before any is written
    };

    // A step of the guide (achievements.pg, kind "guide") as the scene holds it
    struct GuideStep
    {
        std::string id;
        int order = 0;                 // Its place in the sequence, 0 for a step outside it
        std::string say;               // Its sentence, on the guide's leaf
        std::string point;             // What the hand points at: "button:begin", "tile:<id>", "holding:<id>", "stat:<key>", "clock", "log", ""
        std::vector<std::string> light;   // What else stays lit with it: the other tiles of a choice, his other parts
        std::string pointChosen;       // For a step that points at a tile: what the hand points at once that tile (or one lit with it) is chosen, "" to stay on it
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
        float monthMs() const;         // How long a month lasts at his age: opt.monthMs from ten, shorter before, to bring a new player in
        int written() const;           // The written time held, in seconds: what the chronicle wrote while it stayed shut
        void setHaste(int times);      // The months run so many times as fast, on written time: 1 is their own pace, and all there is with none left
        void pressHaste();             // The button's press: the next speed, and back to 1 after the last
        void creditAway();             // The time since the save was written is written time, up to what can be held: once, when a save is opened
        void runStill(float ms);       // A frame of the months standing still: a minute of it with no input from the player, and it is written time from its start
        void stirred();                // An input: the player is at the game, the count of time standing still begins again
        bool saveNow();                // Writes the save (not with --no-save)
        void autoSave();               // The same with no line in the log: after a month, a choice, a thing done at once, a new life
        void tellAnalytics(const std::string& event = "");   // Where the life stands, kept by the analytics for its next row; with an event, a row now
        std::vector<std::string> namedWorks() const;         // The works the digest names one by one: his sixth year's, the ways into a class, and what opens at their age or later
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
        int haste = 1;                 // How many times as fast the months run now, on written time: 1 is their own pace
        Rules rules;
        pg::EntityRef page;
        pg::EntityRef ending;          // The veil and the leaf over the page, while `ended`
        RuleEpitaph epitaph;           // What the ending says, from epitaph.pg
        std::vector<FactRouter::SubId> subs;
        bool paused = true;
        std::vector<std::string> eventsCalled;       // The events of his life whose answers have come into the list: the months stopped for each, once
        std::vector<std::string> eventsLastCalled;   // And those whose last month to begin an answer has come: the months stopped again
        bool ended = false;            // The life is over and its ending is up: nothing passes until the next begins
        bool compact = false;          // Which file the page is: opt.compactFile or opt.pageFile
        bool phone = false;            // The page is opt.phoneFile: compact, in one column, with one row of tabs
        bool touched = false;          // A finger has been on the screen: a press shows a gloss, whatever the page
        float phoneWidth = 0.0f;       // The window's width the phone page was built at
        float phoneFitted = 0.0f;      // And the one its panels were last given: at another they are given theirs, where they stand
        int pageSettling = 0;          // Passes before a page just built holds its pieces: it is not sized before
        bool phoneRefit = false;       // A width came meanwhile: the panels are given theirs once it does
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
        std::string workSaid;          // The work the facts say he is at ("working.<id>"), to say he has left it
        std::string gift;              // At an ending: the place he leaves his coin to, "" for none
        std::vector<std::pair<std::string, Button>> gifts;   // The ending's buttons: a place that can still be raised, each

        void showTown(bool shown);     // The Town page in the middle column, or the choice back in it

        // A walk outside the town (lifewalk.cpp): a row of the Adventure group begun opens its
        // screen over the page, the days of the stay are chosen there one at a time, and what he
        // did is kept as a record the Adventure page walks again
        RuleWalk walk;                 // The last answer of Rules::walk for the walk under way
        RuleWalk shelf;                // And for the Adventure page: the records kept, the form of a queue
        pg::EntityRef walkPage;        // The on-site screen, or the fight's in its place, over the page while he is on a walk
        std::string walkScreen;        // Which of the two is up: "site", "fight", "" for neither
        bool walkNarrow = false;       // It stands in one column: the window is too narrow for three
        float walkWidth = 0.0f;        // The window's width it was built at: at another it is built again
        bool walkFresh = false;        // It was built this pass: its layouts do not hold their pieces yet, and nothing of it moves
        int walkSettling = 0;          // Passes left in which it is filled again: what leaves a stack leaves it once the stack holds it
        std::string seekChosen;        // The beast chosen in "Seek a beast", "" for none
        std::string workChosen;        // And the work chosen in "Field work"
        bool adventureShown = false;   // The Adventure page is in view
        std::string adventureZone;     // The place it is about, "" before it is filled: the one whose tile he chose, or of the walk he last made
        float adventureWidth = 0.0f;   // The width its regions were last given
        bool recordsShown = false;     // The place in view has walks kept: their table is on the page

        bool onWalk() const { return save.walk.under(); }   // He is on site: the walk's screen is the page
        void beginWalk(const std::string& activity);     // He sets out by hand: the on-site screen opens
        void setOutAt(const std::string& zone);           // A life made fit for a zone and put on its way there: what --walk opens on
        void applyBuild();                                // --build: the life made one of testbuilds.pg's builds, with --set on top; "list" or an unknown one says them
        bool built = false;                               // A build was applied: --walk takes him as he is
        void walkDo(const WalkAct& act);                  // One thing he does there, or in a fight
        void beginStory(const std::string& activity);    // A row with a `story`: the story opens on the walk's screen, its row runs, its board given up front
        void runStoryMonths(int months);                  // The months a story's days have added up to, each the life's own month, there and then; stops at a life's end
        void endStory();                                  // The story is over: no record, no walk counted; a row left before its term gives back what it fed him ahead
        bool storyMonth = false;                          // A month run by a story's days: its term neither enters a path nor brings him home, and the calendar stays stopped
        void showAdventure(bool shown);                   // The Adventure page in the middle column, or the choice back in it
        void chooseRecord(const std::string& id);         // The record the form of a queue is filled for
        void chooseZone(const std::string& zone);         // The place the Adventure page is about: its tile lit, its walks listed
        void setOutAnew();                                // A new walk to that place, by hand
        void layAdventure();                              // What stands beside a button of the Adventure page has the room the button leaves
        bool adventureRoomy() const;                      // The Adventure page is wide enough for everything a line says at its right
        void queueWalks(int count);                       // The chosen record walked so many times, one walk after the other
        bool onWalkButton(const std::string& tag);        // A button of the walk's screens or of the Adventure page; false for another's
        bool telling() const { return walkTelling; }      // A turn of a fight is being told on the page: nothing is pressed meanwhile
        const std::vector<std::pair<std::string, Button>>& moves() const { return walkMoves; }   // The buttons of his turn in a fight, by tag
        std::vector<std::pair<std::string, float>> timelineShown() const;   // The timeline's words in view (his turns, the beasts', the end of the day), each with its x
        std::vector<pg::EntityRef> timelinePieces() const;                    // Everything the timeline draws over its box: the gauge, the day, its tags

        int newInLife = 0;             // What has come onto the Life page while the Town page was in view: said at its tab's corner
        int newInTown = 0;             // And onto the Town page while the Life page was
        int newInAdventure = 0;        // What is new on the Adventure page while it is not in view: a place's tile come onto it, a place that opened to him; and its tiles the day the page itself comes
        std::vector<std::string> openedWalks;   // The places seen open to him, so that one is new once
        std::vector<std::string> stoodWalks;    // And the places whose tile has stood on a page, open or not
        bool adventureWas = false;     // The Adventure page was there the last time the list was filled: the day it comes is news

        bool guiding() const;          // A first life whose guide has not said its last step
        // A slip at the foot of the window, for a few seconds (its own time when given one). A few at
        // most are up, one over another; the same line again keeps the one up instead
        struct Toast
        {
            std::string text;
            pg::_unique_id textId = 0;
            pg::_unique_id groundId = 0;
            float height = 0.0f;       // Its ground's: a line, or more when its text wraps
            float left = 0.0f;         // Milliseconds until it goes
        };

        void toast(const std::string& text, float ms = 0.0f);
        std::vector<Toast> shownToasts;   // The toasts up: the lowest, the oldest, first

        int toastsUp() const { return static_cast<int>(shownToasts.size()); }   // The toasts at the foot of the window now
        bool toastUp(const std::string& text) const { return std::any_of(shownToasts.begin(), shownToasts.end(), [&text](const Toast& t) { return t.text == text; }); }   // And whether one says that

        void markAdventure();          // The Adventure page's tab on the page or not, as he has been on a walk or never

    private:
        bool buildPage(bool compact, bool phone = false, float width = 0.0f);   // The page from its file, in place of the one there was. A phone's is built at the window's width
        void fit(float width, float height);   // The page to the window, swapping it at the breakpoint
        void fitFull(float width, float height);      // The middle column's width, the choice's and the log's heights
        void fitCompact(float width, float height);   // The main column's width, the choice's and the log's heights
        void fitPhone(float width, float height);     // The one column's width, and the height of what is under the tabs
        void fitPhoneHead();                          // The title ends a gap before his age as it is drawn
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
        std::string endingStory() const;   // The ending's story: the epitaph's, then what goes into the stash with the gift as chosen
        std::string giftStat() const;  // What of his is left to a place at his death: the rules say (coin)
        void chooseGift(const std::string& place);   // One of them chosen, or let go of
        bool fromLists(const std::string& list) const;      // An event of the choice's list or of the market's
        ActivityList* listHolding(const std::string& id) const;   // The one of them that has that row, nullptr for neither
        void clearChoice();            // Nothing chosen in either list, nor in the town's places
        bool spentChoice() const;      // What its button did is chosen still, and can be done no more (the last lot sold): the button greyed in its place
        bool buttonAtWork() const;     // At work, a button's line under the running row: Go on, what is chosen that may be taken while at it, or that spent
        bool repeats(const RuleActivity* activity) const;   // Done at once from what he holds, as often as he holds it (a sale), or bought as often as he can pay (a buy): it has the All button
        bool pressedOn(const char* name, pg::Point2D at) const;   // That button is shown, and the press is on it
        float workingHeight() const;   // "At work now": its chrome alone, or with a running row
        void showWorkButtons();        // At nothing: the button that begins what is chosen (Buy, Sell or Make for a trade of the market), or the one that passes a month
        bool guideSaid() const;        // The guide line is up: a life led to its first work, with no guide to say so
        float guideRoom() const;       // What the guide line takes of "At work now", 0 when it is not up
        bool resumes() const;          // His work is stopped: "At work now" has a button that goes on
        bool answersEvent(const RuleActivity* activity) const;   // An answer to an event of his life, listed and his to give
        bool takenAtWork(const RuleActivity* activity) const;    // Begun while he is at another work: done at once, or an answer that sets that work aside
        void callEvents(bool stop);    // An event whose answers came into the list, or whose last month to answer has come: said, and the months stopped for it; false: only noted
        void takeUpSetAside();         // The work an answer set aside goes on where it stopped
        void leaveWork(const std::string& id, int monthsIn);   // A work left before its term, so many months in: what it gave up front for the rest goes back
        void catchUpRations();         // A save from before the rations were given up front: the months left of a work that feeds him are given once
        float resumeRoom() const;      // What that button takes of the panel, 0 when it is not up
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
        bool titled(const std::string& id) const;   // A holding he has or has not (a standing, a tie): the rules say so, and its row shows no figure
        const RuleActivity* activityOf(const std::string& id) const;   // From the last answer of Rules::activities, nullptr if none
        int closesOf(const RuleActivity& activity) const;   // The months he still has to begin it, for its tile to count down: -1 when it does not close, or not soon
        void onHover(const ActivityHoveredEvent& event);   // The mouse came onto a tile or left them
        void readRow(const std::string& id);               // Looked at: its seal of a new row goes
        void preview();                // What the chosen activity, or the one the mouse is on, would make of his parts and of what he holds
        void previewHoldings(const RuleForecast* forecast);   // "46 -> 52" ("6/60 -> 12/60") on the rows it would add to; nullptr: the figures as they are
        std::string holdingText(const std::string& id, int amount) const;   // "12", or "12/60" for what has a most he can hold
        void followLog();              // The log taken to its end in a moment, for a reader who was there
        void runPassing(float ms);     // What lasts a moment, a tick further
        void endPassing();             // All of it ended at once: the scene leaves
        int roseOf(const std::string& key, const std::string& text);   // By how much a figure rose since it was last written, 0 if it did not
        void showGain(pg::EntityRef over, int amount);   // "+2" lifting off the figure that rose
        void runToasts(float ms);      // The toasts a tick older: the ones whose time is up go, and the rest come down
        void dropToast(size_t index);  // One toast gone at once
        void stackToasts();            // Each toast over the ones under it
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
        void passOutgrownSteps();      // The steps of the sequence he is too old for (`untilAge`) are passed over unsaid
        void passStorySteps();         // A story over: the steps of the sequence still waiting for a walk's screens are passed over unsaid
        void forgetPassedSteps();      // The step being said and the ones waiting, once passed over, are gone
        void nextStep();               // "Next": the step being said for a time ends now
        void skipGuide();              // "Skip tutorial": past its last step for good
        void makeNote();               // The leaf, its words and its two buttons
        void sayNote(pg::EntityRef target, bool said = true, const std::vector<pg::EntityRef>& also = {});   // The leaf with the step's sentence under what it points at and what is lit with it, or gone with no step (or one not `said` on this screen)
        void clearGuide();             // No step, no hand, no shade: the life is over, or another begins
        void renumberGuide();          // A save from before the outskirts' steps: the step it is past, in this guide's numbers
        bool onScreen(const std::string& point) const;   // What a step points at is of the screen up: a walk's while he is on one, the page's else
        void liftGuide(bool over);     // The shade, the leaf and the hand over a walk's screen, or back over the page
        bool guideLifted = false;      // They stand over a walk's screen
        pg::EntityRef pointed(const std::string& point) const;   // What a step points at, empty when it is not on the page
        pg::EntityRef walkPointed(const std::string& kind, const std::string& name) const;   // And on a walk's screen (lifewalk.cpp): a work of the day, an answer, a move, a beast, the order, a button
        bool walkChosen(const std::string& point) const;   // A work of the walk's day a step points at (or lights) is the one chosen there
        void walkFacts(pg::ElementMap& facts) const;     // Where he stands on a walk's screens, for the guide: walk.screen, story.id, story.day, fight.turn, fight.over, fight.acted
        bool behindPage(const std::string& point) const;   // A tile of the Life page while the Town page, the Adventure page or (on a phone) another tab's panel is the one in view
        void pointAt(pg::EntityRef target, const std::vector<pg::EntityRef>& also = {});     // The hand beside it and the shade around it and what is lit with it, or neither for nothing
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
        unsigned long long wallClock() const;   // The hour it is, in seconds since 1970 (opt.now for a test)
        bool writeSave();              // The save to its file, with the hour it is written at
        float spendWritten(float ms);  // So many ms of written time are spent, or what is left of it: what was, in ms
        int addWritten(int seconds);   // So many seconds written, up to what can be held: how many were
        void publishWritten();         // -> resources.<written time>.value and .rate, stat.<written time>
        bool hastens() const;          // Whether "At work now" has the button that spends it: while any is held, in a life past its guide
        float hasteRoom() const;       // And the room it takes there: a line under a running row, none where a button's line stands

        void onSelect(const ActivitySelectedEvent& event);
        void onConfirm(const ActivityActivatedEvent& event);
        void beginWork(const std::string& id);   // An activity begun: the choice's Begin, a row confirmed, a place's tile clicked
        bool doAtOnce(const std::string& id, const RuleForecast& forecast, bool batch = false);   // An activity that takes no time; batched, the log, the list, the page and the save are the caller's. True when the list changed
        void doAll(const std::string& id);       // That one done as many times as he can: one line in the log for them all
        void onTab(const TabSelectedEvent& event);
        void onPlace(const PlaceSelectedEvent& event);   // A place's tile clicked: the work that raises it chosen, or said why not

        // The walks (lifewalk.cpp)
        bool askWalk(const std::string& ask, const WalkAct& act, const std::string& replay, RuleWalk& out);   // expedition.pg, asked with what the save holds
        int walkDice();                // The dice of the next question: the clock's, or a test's own
        pg::RecordList walkRows() const;   // The Adventure rows of the Life page, as expedition.pg reads them
        void endWalk();                // The stay is over: its record is kept, and the months of the walk run
        void showWalk();               // The screen the walk stands on, built when it is another and filled
        bool buildWalk(const std::string& screen);   // "site" or "fight", from its file, at the window's width
        void closeWalk();              // The walk's screen leaves
        void fitWalk(float width, float height);   // The veil and the columns to the window; built again at another width
        void fitAccount();   // A fight's account takes what the order of the turn leaves of its column
        void sizeWalk(float width, float height);   // The walk's screen at another width, in place: what shapeWalk writes in the file, given to what stands
        void fitNarrowHead();                        // In one column, the head's lines end beside the big figure as wide as it is drawn
        void widenPanel(const std::string& name, float width);   // A panel of the walk's screen, and what stands in it
        void widen(Panel* panel, float width);   // A panel, and what stands in it: each piece as wide as the panel leaves it, where it stands
        void fillSite();               // The on-site screen says what expedition.pg says
        void fillFight();              // And the fight's
        void fillMoves(bool over, bool handed);   // The buttons of his turn: a move each, and against several a beast each to aim at
        void showHurt(const std::string& rule, const std::string& taken);   // A rule just taken from: in the loss's colour for a moment, what was taken lifting off it
        void showTag(const std::string& rule, const std::string& text, const std::string& style, const std::string& color);   // A word or a figure lifting off a rule of the fight: what was taken, or a miss
        void runWalk(float ms);        // A fight handed to his stance plays its turns; a row chosen is lit once its list holds it
        void tellTurn(size_t shown);   // The lines the account has past the ones shown are told in their time
        void runTelling(float ms);     // The telling, a tick further
        void endTelling();             // Told: the screen says where the fight stands
        void onWalkSelect(const ActivitySelectedEvent& event);
        void onWalkConfirm(const ActivityActivatedEvent& event);
        int adventureTab() const;      // Which tab is the Adventure page's, -1 when the page has none
        bool adventureKnown() const;   // He has somewhere to walk to, or a record of a walk: the page has something to show
        void fitAdventure(float width, float height);   // The Adventure page to the middle column
        void fillAdventure(bool again = false);   // The records kept and the form of a queue; `again` for a page just built
        bool beginReplay();            // The queue's next walk sets out by the record; false when it cannot, and the queue has stopped
        void homeFromWalk();           // The months of a walk are over: a record walked is folded into him, and the queue goes on
        void stopQueue(const std::string& why, bool loss);   // No walk is left to begin; said on the Life page when there is a why
        pg::EntityRef inWalk(const std::string& name) const;   // A piece of the walk's screen by its name, empty if none
        void walkSay(const std::string& name, const std::string& text);   // A label of it, written when it says something else
        void walkShow(const std::string& name, bool shown, const std::string& panel = "");   // A piece taken out of its stack or put back: a column's panel, or what stands in a `panel`
        void walkList(const std::string& name, const std::vector<ActivityGroup>& groups, const std::string& chosen);   // A list of it filled, when what it holds changed
        void walkLedger(const std::string& name, const std::vector<LedgerRowSpec>& rows);
        void walkLog(const std::string& name, const std::vector<LogEntry>& lines);

        template <typename Piece>
        Piece* walkPiece(const std::string& name) const
        {
            pg::EntityRef ent = inWalk(name);

            if (ent.empty() or not ent->has<Piece>())
                return nullptr;

            return ent->get<Piece>().component;
        }

        // The hover glosses: what the numbers mean, from the same outputs
        void glossParts(const RuleForecast* forecast, const std::string& activity);   // parts/<p>
        void glossActivities();                                                         // activity/<id>
        void glossHoldings();                                                           // resource/<id>
        void glossWindows(const pg::RecordList& windows);                               // window/<id>

        void appendLog(const LogEntry& entry);
        std::string activityName(const std::string& id) const;

        template <typename Type>
        void setFact(const std::string& path, const Type& value);

        std::unordered_map<std::string, std::string> walkFilled;   // What each list of the walk's screens was last filled with
        std::unordered_map<std::string, std::string> walkPicks;    // The row to light in each, once the list holds it
        float autoLeft = 0.0f;         // Milliseconds before his stance plays the next turn of a fight handed to it
        std::vector<std::pair<std::string, Button>> walkMoves;   // The buttons of his turn, by tag, in the order they were made
        // A tag of the timeline: a word in a frame, and its tick on the gauge
        struct TimeChip
        {
            pg::EntityRef ground, frame, tick;
            Label label;
        };

        std::vector<TimeChip> timeChips;                       // Made once a fight screen and moved: now, his next turn, the beasts before it, the one after
        std::optional<ProgressRule> timeBar;                   // The gauge: hatched as long as the move under the mouse takes, filled as a turn is told
        pg::EntityRef timeDay;                                 // The end of his day, across it
        std::optional<Label> timeDayLabel;
        float timeTellMs = 0.0f;                               // How long the turn being told takes: the fill sweeps that long
        float timeSpan = 0.0f;                                 // Where the move pressed put his next turn, percent of the gauge
        float timelineSpanOf(const std::string& move) const;  // That percent for a move ("" as it stands)
        pg::_unique_id timePage = 0;                           // The fight screen they were made on: another one makes them anew
        std::string timelineMove = "-";                        // The move the timeline shows ("" as it stands; "-" none yet)
        void fillTimeline(const std::string& move);           // The timeline as it stands, or as the move under the mouse would leave it
        std::string hoveredMove() const;                       // The move of his turn under the mouse, "" for none
        std::unordered_map<std::string, std::vector<pg::_unique_id>> moveRows;   // The rows they stand on, by the layout that holds them
        int moveHoverStale = 0;        // Frames left in which the hover is asked for again: buttons made again under a mouse that stays still
        std::unordered_map<std::string, std::pair<float, float>> moveFlow;   // By layout: the widths between which its buttons stand on their lines as they do (at least, under)
        bool walkGlossed = false;      // The glosses of the screen hang on what they speak of: the beasts' panels, his own on site
        float accountFitted = 0.0f;    // The height the account of a fight was last given, under the order of the turn
        float walkCooling = 0.0f;      // Milliseconds before the walk's screen may be built again for another width
        bool walkRefitDue = false;     // A width came meanwhile: the screen is built for the window as it stands once that has passed

        // A turn of a fight is told in its time: the lines the turn wrote in the account come one
        // after the other, and a blow's rule fills before the line that tells it. The walk itself
        // is already where the turn left it: only the page lags, and nothing is pressed meanwhile
        struct WalkBeat
        {
            std::string by;            // Whose blow the line tells: "hero", "foe", "" for what is no blow
            int n = 0;                 // The beast: the one that struck, or the one he struck
            int hp = -1;               // His Vitality in the field once the line has happened
            int left = -1;             // What is left of the beast his blow landed on, in percent; -1 for none
            bool missed = false;       // The blow found nothing
            LogEntry line;
        };

        std::deque<WalkBeat> walkBeats;   // The lines still to tell, in their order
        bool walkTelling = false;      // A turn is being told
        bool beatCasting = false;      // The rule of the blow at their head is filling
        float beatLeft = 0.0f;         // Milliseconds before the next thing of the telling
        std::vector<std::string> castDone;   // Whose rule has filled this turn ("hero", "foe0"): once each
        int diceCast = 0;              // The questions asked of expedition.pg: no two with the same dice

        std::vector<RuleActivity> activities;    // The last answer of Rules::activities
        std::string listedRows;                   // What the list was built from: each listed row and how much it asks
        std::unordered_map<std::string, std::vector<GlossRow>> walkLadders;   // A place he may walk to: what each level of knowing it brings, under its activity's gloss
        std::string laddersSaid;                  // What they were made from, to make the glosses again when it changes
        pg::RecordList holdings;                  // The last resources.pg rows
        std::vector<RuleGloss> holdingGlosses;    // And what hovering each says
        std::string death;                        // "" or why the character as he is cannot go on
        std::vector<std::string> threat;          // The parts the coming month would take from, as things stand
        std::vector<std::string> threatShown;     // The ones of them in red for it
        std::string warning;                      // What the rules say of the coming month ("RATIONS FOR 2 MONTHS"), "" for nothing
        std::string advice;                       // And what to do about it, as last said by a toast
    public:   // Read by the tests
        std::string chosen;                       // The activity chosen in the list, "" for none: what the Begin button begins
    private:
        std::string chosenList;                   // And the list it was chosen in: the choice's, the market's, or the town's places
        float sincePlaceClick = 1.0e9f;           // Ms since a place's tile was last clicked: a second click on the chosen one soon after begins it, as on a row
        std::string doneNow;                      // The last chosen thing its button did at once: chosen still once it can be done no more, its button stays greyed
        bool leadDue = false;                     // The page chose it for him: its tile is lit once the list holds its row
        pg::EntityRef noteEdge;                   // The leaf's edge, and its ground a pixel inside it
        pg::EntityRef noteGround;
        float guideFitted = 0.0f;                 // The room the guide line had when the page was last fitted
        float handMs = 0.0f;                      // How long the hand has pointed where it does: it moves a little, to be seen
        std::string shaded;                       // What the shade was last laid around: laid again only when that moves
        std::string hovered;                      // The one the mouse is on, "" for none: previewed when nothing is chosen
        std::vector<std::string> known;           // The activities he could do when the list was last filled
        std::vector<std::string> listedLast;      // The activities that had a row when the list was last filled, open to him or not
        std::vector<std::string> stood;           // Every activity that has had a row in this life: one that has not is new, and its tab says so
        std::vector<std::string> unread;          // The rows new to their page that he has not looked at yet: a gold seal on each, until the mouse goes over it or it is chosen
        std::vector<std::string> arriving;        // The rows that come onto the page in view as the list is filled: they fade in

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
        std::string endedLine;                    // The line the next life opens with: how this one ended
        std::string endedAge;                     // And the age it ended at, as the head wrote it
        std::unordered_map<std::string, int> caps;   // The most each capped stat can be
        std::vector<RuleAchievement> deeds;       // The achievements.pg output
        std::vector<std::string> reached;         // Deeds unlocked, waiting for execute()
        std::vector<RuleMilestone> milestones;    // The last milestones.pg output
        pg::ElementMap next;                      // The last next milestone
        pg::RecordList nextAsks;                  // What it asks of the path he is headed for
        std::unordered_map<std::string, pg::EntityRef> handles;
        float sinceMonth = 0.0f;                  // How far the month under way has run, in ms of its own length (monthMs()): written time runs it faster
        std::string writtenStat;                  // The stat written time is held in, as the rules name it (their holding with a `haste`); "" when they have none
        int writtenMost = 0;                      // The most of it that can be held, in seconds
        int hasteMost = 1;                        // How many times as fast it can run the months
        int writtenAMonth = 0;                    // What a month run faster takes of it, in seconds, whatever the speed; 0: a second played at ×N takes N − 1
        float writtenSpent = 0.0f;                // The part of its next second already spent, in ms
        std::string awayLine;                     // What was written while the chronicle stayed shut, to be said once the page has arrived
        float stillMs = 0.0f;                     // How long the months have stood still with no input from the player, in ms
        float stillLeft = 0.0f;                   // Of that, once it is time away, the part of a second not written yet
        std::vector<std::string> alerted;         // The parts in red
        float alertLeft = 0.0f;                   // Milliseconds of red left
        bool endangered = false;                  // The last month took from what the life hangs on

    public:   // The Options tab: the tests look at it
        // The Options page (lifeoptions.cpp): how the game is played and lit, kept on this machine,
        // and the save itself: exported, loaded, burnt. It takes the choice's place in the middle
        // column, as the Town page does
        Settings settings;             // As read at the scene's start, and written with the life at every change
        bool optionsShown = false;     // The Options page is in view
        pg::EntityRef dialog;          // The wipe's or the load's question over the page, empty for none
        std::string dialogKind;        // "burn" or "replace"
        std::optional<LifeSave> toReplace;   // A save read from a file, waiting for the player's word
        float exportedLeft = 0.0f;     // Milliseconds left of "Exported to <file>" in the place of the last-written line
        std::string exportedTo;        // The file it names
        std::string exportFailed;      // A line in vermilion under Export, "" for none
        std::string loadFailed;        // And under Load
        bool uploading = false;        // In a browser: the file picker is open, or the file chosen on its way in
        bool skipTick = false;         // An OS dialog held the game still: the frame after it is no time played
        bool leaving = false;          // The scene is being made again (a save loaded, the chronicle burnt): no month runs and nothing is written meanwhile
        std::vector<pg::_unique_id> dialogFaces;   // What the keyboard goes round while a dialog is up

        int optionsTab() const;
        void showOptions(bool shown);  // The Options page in the middle column, or the choice back in it
        void fitOptions(float width, float height);
        void fillOptions();            // Its figures and lines from the save and the settings, then laid out
        void layOptions();             // Each row as tall as its text, each line as wide as its panel leaves it
        void readSettings();           // From the save's file (opt.savePath), and applied: the way a click begins a work
        void writeSettings();
        void applyClickMode();         // The Life page's list begins a work on its first click, or chooses it
        void chooseTheme(const std::string& id);   // The page lit so, at once, and the settings written
        bool onOptionsButton(const std::string& tag);   // One of the page's or of its dialogs' buttons: true when it was
        void exportSave();             // The save written now, then a copy where the player says (a download in a browser)
        void askLoad();                // The file to load: the OS's dialog, or the browser's picker
        void runUpload();              // A browser's file on its way in: taken once it has come
        void takeSave(const std::string& path);   // A file offered as a save: refused with a line, asked about, or taken
        void replaceWith(LifeSave loaded);         // It is the chronicle now: written, the old one kept as .bak, the scene made again on it
        void wipe();                   // Every life, the town and all knowledge gone; a first life begins. The settings stay
        void remake(LifeSceneOptions next);        // The scene made again in its own place, with these options
        bool untouched() const;        // The chronicle has not begun: nothing would be lost by replacing it
        std::string exportName() const;           // chronicle-<first name>-life<N>.sz
        bool openDialog(const std::string& kind); // "burn" or "replace", over the page
        void closeDialog();
        void fitDialog(float width, float height);
        void burnWordChanged(const std::string& word);   // The burn's button opens once the word is BURN
        void enableOptionsFocus(bool on);         // The page's controls in the Tab order only while it is in view

        // The bug report (lifereport.cpp): a button in the window's top right corner over every
        // screen, made with the scene. Pressed, the screen is caught first, so the picture is what
        // he saw; then the dialog opens over the game: a page of the browser's own on the web (a
        // box of several lines, a phone's keyboard), the kit's dialog natively. What he writes goes
        // to the author with the picture, the save as last written, the recent messages and where
        // he stood; natively it is written to a folder beside the save
        Button reportButton;           // Its corner's button, the `bug` mark on it
        bool reportOpen = false;       // The dialog is up: what is typed and clicked is not the game's
        bool reportAsked = false;      // Pressed: the screen is being caught, the dialog comes with it
        float reportGrabLeft = 0.0f;   // Milliseconds the frame may still take; then the dialog opens without it
        std::string reportScreen;      // The screen as it was caught, a JPEG data URL; "" for none
        std::string reportLine;        // What the dialog says under the words: when he may send again, or why it did not go
        bool reportSending = false;    // Sent, its answer not come yet
        bool reportKeysShut = false;   // The dialog closed by a key this pass: the keys of the same pass are not played either
        ReportFlow reportFlow;         // His words, kept until they have gone
        float reportRight = -1.0f;     // The margin from the window's right the button was last put at

        void makeReportButton();       // Once, with the page
        void placeReportButton();      // Left of the head's figure in that corner, as it is drawn now: on every screen, at every width
        bool onReportButton(const std::string& tag);   // The corner's button, or one of the native dialog's: true when it was
        void openReport();             // The frame asked for; the dialog opens when it comes
        void showReport();             // The dialog itself, once the screen is caught (or not, in time)
        void closeReport();            // Shut, his words kept
        void fillReport();             // The native dialog's line and Send as the limit and the last answer leave them
        void sendFromReport(const std::string& message);   // The report made and sent; the answer comes to reportAnswered
        void reportAnswered(SendResult result, int waitS, const std::string& line);
        void runReport(float ms);      // A tick: the frame's wait runs out
        void pollReport();             // Each pass: the sends' answers, and the browser's dialog pressed
        void dropReport();             // The scene leaves: nothing asked for answers into it
        std::string reportDir() const;           // The folder beside the save: the player's id, the limit's file, the reports written
        ReportLimit reportLimit() const;         // The last reports' times, from their file
        std::string lifeLogTail(size_t lines) const;   // The life's own log, its last lines, one a line
        std::string reportPlace() const;         // Where he stood: page, screen, age, the work he is at
        float headFigureLeft() const;            // Where the big figure of the head's right begins, as drawn; the window's right with none
        float reportRoom() const;                // What the button takes of a head beside its figure: its width and its gap, 0 with no button
    };
}
