#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "Memory/elementtype.h"
#include "UI/prefabspec.h"   // RecordList

namespace pg
{
    class EntitySystem;
    class ScriptHandle;
    struct VM;
    struct ObjFunction;
}

namespace chronicle
{
    // One rule script, compiled once and run as often as needed. The seam is the prefab loader's:
    // inputs are globals the C++ side defines before each run, outputs are globals the script
    // leaves behind, read back by path ("forecast.atTerm", "activities.3.gains").
    //
    // A run executes the script's whole top level again from its prepared bytecode (the VM's
    // prepareCachedFunction / runPreparedFunction fast path): there is no `main()` to call, and a
    // script keeps no state between runs because every run redefines what it leaves.
    class RuleScript
    {
    public:
        RuleScript();
        ~RuleScript();

        RuleScript(const RuleScript&) = delete;
        RuleScript& operator=(const RuleScript&) = delete;

        // Compiles through the ECS's script registry (a .pgc beside the .pg) and prepares it.
        bool load(pg::EntitySystem* ecs, const std::string& path);

        // Inputs: kept, and (re)defined before every run.
        void set(const std::string& name, const pg::ElementType& value);
        void set(const std::string& name, const pg::ElementMap& table);
        void set(const std::string& name, const pg::RecordList& tables);
        void set(const std::string& name, const std::vector<pg::ElementType>& values);

        // False on a runtime error or a non-empty `errors` list global; errors() says why.
        bool run();

        // Outputs, read by a dotted path after run(): a global, then fields of tables and indices
        // of lists. A missing path is an error, never a default.
        bool get(const std::string& path, pg::ElementType& out);
        bool get(const std::string& path, pg::ElementMap& out);
        bool get(const std::string& path, pg::RecordList& out);
        bool get(const std::string& path, std::vector<pg::ElementType>& out);
        bool size(const std::string& path, size_t& out);        // The length of a list

        const std::vector<std::string>& errors() const { return errorList; }
        const std::string& path() const { return scriptPath; }
        bool loaded() const { return fn != nullptr; }

        void clearErrors() { errorList.clear(); }
        void error(const std::string& message);                  // Logged, with the script's path

    private:
        struct Input;

        bool resolve(const std::string& path, uint64_t& out);   // A pg::Value
        uint64_t makeValue(const Input& input);

        std::unique_ptr<pg::VM> vm;                              // Its stack is large: on the heap
        pg::ObjFunction* fn = nullptr;
        std::shared_ptr<pg::ScriptHandle> handle;
        std::shared_ptr<const std::vector<char>> bound;          // The bytecode `fn` was prepared from
        std::map<std::string, std::shared_ptr<Input>> inputs;
        std::vector<std::string> errorList;
        std::string scriptPath;
    };

    // An activity from activities.pg: its scalar fields (id, group, name, glyph, months, rank,
    // each, path, enters, board, fromAge, finishBy, locked, done, uses, left, spent, closed,
    // pathOpen, listed, tally) and its three lists.
    struct RuleActivity
    {
        pg::ElementMap fields;
        pg::RecordList gains;          // {stat, amount, label}: what the activity brings at term
        pg::RecordList costs;          // {stat, amount, label}: what it takes when it begins
        pg::RecordList requires;       // {stat, label, current, needed}: what it asks before it can start
    };

    // A milestone from milestones.pg: {age, id, label, passed} and what it asks, per path.
    struct RuleMilestone
    {
        pg::ElementMap fields;
        pg::RecordList asks;           // {path, stat, label, needed}
    };

    // What an activity does to the character, from forecast.pg.
    struct RuleForecast
    {
        pg::ElementMap atStart;        // Every stat of the character, once the activity's costs are taken
        pg::ElementMap atTerm;         // Every stat of the character, after the activity's gains
        float percent = 0.0f;          // monthsIn / months x 100
        int months = 0;                // The activity's term
        std::string caption;           // "MONTH 3 OF 6 · STRENGTH 14 → 16 AT TERM"
        pg::RecordList gaps;           // {stat, label, current, needed} against the next milestone
        pg::RecordList entries;        // The log lines the term writes: {text, kind, figure, glyph}
        std::string error;             // "" or "unknown activity"
    };

    // What hovering a holding says, from resources.pg: {id, title, text, footnote} and its figures.
    struct RuleGloss
    {
        pg::ElementMap fields;
        pg::RecordList rows;           // {label, value}
    };

    // One month of what the character holds, from resources.pg.
    struct RuleMonth
    {
        pg::ElementMap after;          // Every stat of the character once the month has passed
        pg::RecordList entries;        // The log lines the month writes: {text, kind, figure, glyph}
        std::vector<std::string> hurt; // The stats a life needs that the month took from
        pg::RecordList caps;           // The most each capped stat of the character can be: {stat, most}
        pg::RecordList rows;           // The holdings the ledger can show: {id, group, groupLabel, glyph, name, tone, rate, limit}
        std::vector<RuleGloss> glosses;   // One per holding, in the rows' order
        std::string death;             // "" or the line the next life opens with: the character as given cannot go on
    };

    // What is said of a life when it ends, from epitaph.pg.
    struct RuleEpitaph
    {
        std::string cause;             // "He died an old man, in his thirty-third year."
        std::vector<std::string> story;   // What he became, worked at, left, and what is told of him
        std::string text;              // The story as one paragraph
        std::string tally;             // "AGE 33 · WORKS 14 · COIN 31 · DEEDS 2"
    };

    // A deed from achievements.pg: {id, name, entry}, what it asks and what it gives.
    struct RuleAchievement
    {
        pg::ElementMap fields;
        pg::RecordList asks;           // {fact, op, value}: every one must hold
        pg::RecordList gives;          // {stat, amount}: gained when it is reached
    };

    // The scene's scripts, loaded once from a rules root, each with its typed call. Every
    // number about the future or the rules comes from here, never from C++.
    struct Rules
    {
        bool load(pg::EntitySystem* ecs, const std::string& root = "examples/Chronicle/rules");

        // The terms completed, by activity: handed to every script that reads the activities.
        pg::ElementMap done;

        // The world's calendar: months since its Year 0, handed to every script that writes a date
        // (the page's head, when an activity closes). It runs on from life to life.
        int world = 0;

        // activities.pg: the activity table, and what each still asks of `character` at `age`.
        bool activities(float age, const pg::ElementMap& character, std::vector<RuleActivity>& out);

        // milestones.pg: every milestone (with `passed`), and the next one after `age`
        // ({id, label, age, in}: `in` is the months to it; "", "", -1, -1 past the last), and the
        // page's head at that age ({ageText, subtitle, ageNote}) when asked for.
        bool milestones(float age, std::vector<RuleMilestone>& out, pg::ElementMap& next, pg::ElementMap* headline = nullptr);

        // windows.pg: each window's {id, name, from, to, state, note, attempts} at `age`.
        bool windows(float age, const pg::ElementMap& character, pg::RecordList& out);

        // forecast.pg: `activityId` for `character` at `age`, `monthsIn` months already spent.
        bool forecast(float age, const pg::ElementMap& character, const std::string& activityId, int monthsIn, RuleForecast& out);

        // resources.pg: what the month that ends at `age` does to what `character` holds (fed by
        // his work when `board`; old age takes from him past the last milestone), the ledger's
        // rows and their glosses, and whether the character as he is can go on.
        bool month(float age, const pg::ElementMap& character, bool board, RuleMonth& out);

        // achievements.pg: every deed, what it asks and what it gives.
        bool achievements(std::vector<RuleAchievement>& out);

        // epitaph.pg: what is said of `character`, whose life ended at `age`, with the names of the
        // deeds he reached.
        bool epitaph(float age, const pg::ElementMap& character, const std::vector<std::string>& deeds, RuleEpitaph& out);

        std::vector<std::string> errors;   // The last failed call's

        RuleScript activitiesScript;
        RuleScript milestonesScript;
        RuleScript windowsScript;
        RuleScript forecastScript;
        RuleScript resourcesScript;
        RuleScript achievementsScript;
        RuleScript epitaphScript;

    private:
        bool fail(RuleScript& script);
    };
}
