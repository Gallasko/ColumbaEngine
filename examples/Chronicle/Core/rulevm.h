#pragma once

#include <cstdint>
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

        // A table of a list: its scalars, and the lists of records it holds under the names asked for
        struct Record
        {
            pg::ElementMap fields;
            std::vector<pg::RecordList> lists;
        };

        // A list of tables in one walk, each with its `lists` in their order. A table without one of
        // them is an error
        bool get(const std::string& path, const std::vector<std::string>& lists, std::vector<Record>& out);

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

    // An activity from activitytable.pg: its scalar fields (id, group, name, glyph, months, rank,
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
        float toward = 0.0f;           // The same a month on: where the month under way takes it
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
        std::string warning;           // "" or what is about to run out, in caps: "RATIONS FOR 2 MONTHS", "NO RATIONS LEFT"
        std::string advice;            // "" or what to do about it, as a sentence
    };

    // What is said of a life when it ends, from epitaph.pg.
    struct RuleEpitaph
    {
        std::string cause;             // "He died an old man, in his thirty-third year."
        std::vector<std::string> story;   // What he became, worked at, left, and what is told of him
        std::string text;              // The story as one paragraph
        std::string tally;             // "AGE 33 · WORKS 14 · COIN 31 · DEEDS 2"
    };

    // What an entry of achievements.pg is: its `kind`, "deed" when it gives none
    enum class RuleKind : uint8_t
    {
        Deed  = 0,       // A line in the log, a toast, what it gives
        Guide = 1,       // A step of a first life's guide: a sentence, and a hand on the page
        Lore  = 2        // A line of the town's story in the log, and nothing else
    };

    // An entry of achievements.pg: a deed, a step of the guide or a line of lore ({id, kind, name,
    // entry, say, point, hold, order}), what it asks, what it gives and, for a step, what ends it.
    struct RuleAchievement
    {
        RuleKind kind = RuleKind::Deed;
        int order = 0;                 // A guide step's place in the sequence, 0 outside it
        pg::ElementMap fields;
        pg::RecordList asks;           // {fact, op, value}: every one must hold
        pg::RecordList gives;          // {stat, amount}: gained when it is reached
        pg::RecordList until;          // {fact, op, value}: a guide step ends when these hold; none: after its `hold`
    };

    // The scene's scripts, loaded once from a rules root, each with its typed call. Every
    // number about the future or the rules comes from here, never from C++.
    //
    // An answer is kept while what it was asked with stands (the age, the character, `done`,
    // `world`, `lives` and the call's own arguments, compared by value): the scene asks the same thing from
    // several places in one month, and a run builds every table of its script again.
    struct Rules
    {
        bool load(pg::EntitySystem* ecs, const std::string& root = "examples/Chronicle/rules");

        // The terms completed, by activity: handed to every script that reads the activities.
        pg::ElementMap done;

        // The world's calendar: months since its Year 0, handed to every script that writes a date
        // (the page's head, when an activity closes). It runs on from life to life.
        int world = 0;

        // The lives lived in this world, this one counted: handed to every script as `world` is. A
        // first life (1) opens on one task and is shown the others one at a time (activitytable.pg).
        int lives = 1;

        // The activity he is at, "" for none: handed to every script that reads the activities (a
        // man on his way into a class is not one who missed them all).
        std::string running;

        // windows.pg, through activitytable.pg: the activity table, and what each still asks of
        // `character` at `age`. One run answers this and windows().
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

        // achievements.pg: every deed, guide step and line of lore, what it asks and what it gives.
        bool achievements(std::vector<RuleAchievement>& out);

        // epitaph.pg: what is said of `character`, whose life ended at `age`, with the names of the
        // deeds he reached.
        bool epitaph(float age, const pg::ElementMap& character, const std::vector<std::string>& deeds, RuleEpitaph& out);

        std::vector<std::string> errors;   // The last failed call's

        RuleScript milestonesScript;
        RuleScript windowsScript;
        RuleScript forecastScript;
        RuleScript resourcesScript;
        RuleScript achievementsScript;
        RuleScript epitaphScript;

        // How many times a script was run since load(): what the kept answers spare
        size_t nbRuns = 0;

    private:
        bool fail(RuleScript& script);

        // The activities and the windows of one run of windows.pg
        bool shape(float age, const pg::ElementMap& character);

        void forget();

        // What the scripts that read the activities were asked with
        struct Asked
        {
            bool known = false;
            float age = 0.0f;
            pg::ElementMap character;
            pg::ElementMap done;
            int world = 0;
            int lives = 1;
            std::string running;
        };

        bool stands(const Asked& asked, float age, const pg::ElementMap& character) const;
        void keep(Asked& asked, float age, const pg::ElementMap& character) const;

        Asked shapedFor;
        std::vector<RuleActivity> shapedActivities;
        pg::RecordList shapedWindows;

        bool milestonesKnown = false;
        float milestonesAge = 0.0f;
        int milestonesWorld = 0;
        int milestonesLives = 1;
        std::vector<RuleMilestone> keptMilestones;
        pg::ElementMap keptNext;
        pg::ElementMap keptHeadline;

        // Several at a time: the work at hand this month and the next, and the tile under the mouse
        Asked foreseenFor;
        std::map<std::pair<std::string, int>, RuleForecast> foreseen;

        bool monthKnown = false;
        float monthAge = 0.0f;
        pg::ElementMap monthCharacter;
        bool monthBoard = false;
        RuleMonth keptMonth;

        bool deedsKnown = false;
        std::vector<RuleAchievement> keptDeeds;
    };
}
