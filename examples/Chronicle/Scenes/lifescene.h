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
        float monthMs = 800.0f;        // One month every so many ms while the loop runs
    };

    // The Life screen: the page is res/chronicle/ui/life.yaml, built as it is; the scene does the
    // three things a file cannot. It runs the rules and the save in startUp, subscribes every
    // widget to a WorldFacts path in one function (wire), and turns input into game events
    // (select, confirm, a month passes). It computes nothing about the game: it calls the rule
    // scripts, writes paths, and the widgets react.
    struct LifeScene : public pg::Scene
    {
        explicit LifeScene(LifeSceneOptions opt = {});

        void init() override;          // The page from the file; nothing has data yet
        void startUp() override;       // Rules, save, rows, wire(), publish()
        void onLeave() override;       // Every subscription dropped; the page goes with the scene
        void execute() override;       // Nothing per frame: months tick on TickEvent

        // Public for the tests and the dev keys
        void onMonth();                // One month passes
        bool saveNow();                // Writes the save (not with --no-save)
        void newLife();                // A fresh life, rebuilt and republished

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
        std::vector<FactRouter::SubId> subs;
        bool paused = true;

    private:
        void wire();                   // THE one function with every subscription
        void rebuild();                // The rows only the save and the rules know: ledgers, lists, log, clock
        void publish();                // publishCharacter + publishRules
        void publishCharacter();       // save -> character.*, life.*, skills.*, resources.*, log.size, activity.running.*
        void publishRules();           // scripts -> life.next.*, life.headline.*, window.*, activity.<id>.state, thresholds
        void publishRunning();         // forecast for the activity at work -> activity.running.*

        void onSelect(const ActivitySelectedEvent& event);
        void onConfirm(const ActivityActivatedEvent& event);
        void onTab(const TabSelectedEvent& event);

        // The hover glosses: what the numbers mean, from the same outputs
        void glossParts(const RuleForecast* forecast, const std::string& activity);   // parts/<p>
        void glossActivities();                                                         // activity/<id>
        void glossWindows(const pg::RecordList& windows);                               // window/<id>

        void appendLog(const LogEntry& entry);
        std::string activityName(const std::string& id) const;

        template <typename Type>
        void setFact(const std::string& path, const Type& value);

        std::vector<RuleActivity> activities;    // The last activities.pg output
        pg::ElementMap next;                      // The last next milestone
        pg::RecordList nextAsks;                  // What it asks of the path he is headed for
        std::unordered_map<std::string, pg::EntityRef> handles;
        float sinceMonth = 0.0f;
    };
}
