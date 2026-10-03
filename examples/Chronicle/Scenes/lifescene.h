#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "Scene/scenemanager.h"

#include "Core/rulevm.h"
#include "Core/factrouter.h"
#include "UI/activityrow.h"   // ActivitySelectedEvent, ActivityActivatedEvent
#include "UI/tabs.h"          // TabSelectedEvent
#include "lifesave.h"

namespace chronicle
{
    struct LifeSceneOptions
    {
        bool fresh = false;            // --fresh: age 7, nothing earned
        bool noSave = false;           // --no-save: the mockup's life, never written
        std::string savePath = "save/chronicle/life.sz";
        std::string rulesRoot = "examples/Chronicle/rules";
        std::string pageFile = "res/chronicle/ui/life.yaml";
        std::string compactFile = "res/chronicle/ui/life-compact.yaml";   // Below the three columns' size
        std::string endingFile = "res/chronicle/ui/ending.yaml";          // Over the page, when a life ends
        float monthMs = 2000.0f;       // One month every so many ms while the loop runs (--month-ms)
    };

    // The Life screen: the page is res/chronicle/ui/life.yaml, built as it is; the scene does the
    // three things a file cannot. It runs the rules and the save in startUp, subscribes every
    // widget to a WorldFacts path in one function (wire), and turns input into game events
    // (select, confirm, a month passes). It computes nothing about the game: it calls the rule
    // scripts, writes paths, and the widgets react. The deeds (achievements.pg) are watched by
    // the engine's AchievementSys against the paths the scene publishes.
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
        int sideTab = 0;               // Compact: the side panel in view (parts, holds, years, log)
        float windowWidth = 1320.0f;   // As the last fit saw it
        float windowHeight = 1020.0f;
        bool runningShown = false;     // "At work now" holds a row: the page is fitted to it

    private:
        bool buildPage(bool compact);  // The page from its file, in place of the one there was
        void fit(float width, float height);   // The page to the window, swapping it at the breakpoint
        void fitFull(float width, float height);      // The middle column's width, the choice's and the log's heights
        void fitCompact(float width, float height);   // The main column's width, the choice's and the log's heights
        float workingHeight() const;   // "At work now": its chrome alone, or with a running row
        void showSide(int index);      // Compact: one side panel in view, the others hidden
        void wire();                   // THE one function with every subscription
        void rebuild();                // The rows only the save and the rules know: ledgers, lists, log, clock
        void addToLedger(const LifeResource& resource);   // One row of what he holds
        void fillSkills();             // The skills he has any of; one at 0 is not shown
        void fillActivities();         // The rows of what he may do; an activity the rules do not list has none
        void fillWindows();            // The doors on the clock, as the rules list them for his path
        void refreshHoldings();        // resources.pg's rows, rates, glosses and verdict for the character as he is
        bool boarded() const;          // The activity at work feeds him
        const RuleActivity* activityOf(const std::string& id) const;   // From the last activities.pg output, nullptr if none
        void endLife();                // The life is lost: the months stop and its ending comes up
        bool showEnding();             // The ending from its file, over the page, saying what epitaph.pg says
        void closeEnding();            // The ending leaves
        void alert(const std::vector<std::string>& stats);   // The parts a month took from, in red for a moment; the months stop at the first
        void clearAlert();             // Back to ink
        void registerDeeds();          // The deeds not reached yet, handed to the AchievementSys
        void reachDeed(const std::string& id);   // What a deed gives, its line in the log
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

        // The hover glosses: what the numbers mean, from the same outputs
        void glossParts(const RuleForecast* forecast, const std::string& activity);   // parts/<p>
        void glossActivities();                                                         // activity/<id>
        void glossHoldings();                                                           // resource/<id>
        void glossWindows(const pg::RecordList& windows);                               // window/<id>

        void appendLog(const LogEntry& entry);
        std::string activityName(const std::string& id) const;

        template <typename Type>
        void setFact(const std::string& path, const Type& value);

        std::vector<RuleActivity> activities;    // The last activities.pg output
        std::string listedRows;                   // What the list was built from: each listed row and how much it asks
        pg::RecordList holdings;                  // The last resources.pg rows
        std::vector<RuleGloss> holdingGlosses;    // And what hovering each says
        std::string death;                        // "" or why the character as he is cannot go on
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
