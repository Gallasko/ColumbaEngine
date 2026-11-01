#pragma once

/**
 * @file effects.h
 * @brief What a program's code reads and writes, for passes that must know whether a value changes.
 *
 * Built on the SSA form of each body (analysis/ssa.h) and the summaries by access path
 * (analysis/summaries.h). A location names slots: `base[key] = v` writes `base.key`, and
 * "all that is reachable from l" is `l.*`.
 *
 * - An edge says what a location may hold. The edges of the program are those of its top-level
 *   statements and a function's own: a host may call it by name, for what is not about a parameter.
 * - Edges are resolved over objects: a table made by a literal (a site), a global, a value not
 *   known. What a path names is found through what its prefix holds, so an edge holds at every
 *   object its location may be (`t.b.x` is also `u.x` once `t.b` holds `u`), and a path that names
 *   no object (a number's field, a table's field never given a table) goes no further.
 * - A table stored in one place only is named by that place, and a list's entries are one key: the
 *   200 rows of a table are the one object `g:rows.*`. A container is not gone through entry by
 *   entry. An object still rooted at a site is stored in several places, or in none.
 * - What a slot holds is not written with it, unless the location stands for all that is
 *   reachable (what a call not followed left): then it is, all the way down.
 * - The region of a global is the objects it may reach. A write touches a global when it writes
 *   into one of them.
 * - A global the program never defines is an input: what it holds is not known and changes from
 *   run to run. `?` is any object: stored in it, a value may be anywhere.
 *
 * What a top-level statement does (StatementEffects) is what it and every call it makes do: the
 * whole program runs from its top levels.
 */

#include "program_view.h"
#include "run_once.h"

#include "Compiler/ast/analysis/summaries.h"
#include "Interpreter/statement.h"

#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace pg
{
    namespace ast
    {
        // What one top-level statement does
        struct StatementEffects
        {
            std::set<std::string> reads;
            std::vector<analysis::LocId> writes;
            std::vector<analysis::Edge> edges;
            std::set<std::string> rebinds;
            bool unknown = false;
        };

        class ProgramEffects
        {
        public:
            // The program: its files' top-level statements, the script's first, and what each imports
            // where (`imports`: what is not imported runs after, in order). False when code of the
            // program runs behind what the bodies show (a class, a function named as a metamethod) or a
            // `return` in a top-level statement decides whether what follows runs at all: then nothing
            // it says may be used
            bool analyze(const std::vector<std::vector<StatementPtr>>& files, const FileImports& imports = {});

            // A top-level statement of the program; any other is unknown: it may do anything, and
            // onlyMakes refuses it
            StatementEffects effectsOf(const StatementPtr& statement) const;

            // The file of the program a module's own parse is (the same source): the one with the same
            // statements, by kind and name, at every index. NoFile when none is; TwinFiles when two could
            // be: nothing was decided for those
            static constexpr int NoFile = -1;
            static constexpr int TwinFiles = -2;

            int fileOf(const std::vector<StatementPtr>& statements) const;

            // A name whose value is the same on every run: a const all of whose writers join it, a
            // function defined once and never rebound, a known native, or a var found to be a const
            // (inferredConsts)
            bool invariant(const std::string& name) const;

            // Whether a statement only makes what these globals hold: it reads only what is the same
            // on every run (a var in the making counts, a const must be one that does), calls nothing
            // unknown, rebinds nothing, and writes only into what they reach or what it made itself
            // (reached by no global)
            bool onlyMakes(const StatementEffects& effects, const std::set<std::string>& targets) const;

            // The top-level vars the program defines once and never changes, made of what is the
            // same on every run: as good as consts
            const std::set<std::string>& inferredConsts() const { return inferred; }

            // What the program decided for a file: one Join per statement of it (ast::walkConsts, the
            // parser's empty statements left aside). Nothing joins in a file another could be taken for
            const std::vector<Join>& joinsInFile(size_t file) const { return fileJoins[file]; }

            // Whether anything was decided for a file: nothing is for one another file of the program
            // could be taken for (its joins are all empty), the plain walk is what it gets
            bool decided(size_t file) const { return undecidedFiles.count(file) == 0; }

            // Every write of the statement stores what it stored last run, by the values stored only
            // (analysis::Idempotence says what that leaves unchecked: nothing is decided on it)
            bool idempotent(const StatementPtr& statement) const;

            // A function followed; nullptr for any other name
            const analysis::Summary* summaryOf(const std::string& function) const;

            // The locations a statement writes, as written and as slots of the objects they name, as
            // sorted texts
            std::vector<std::string> writtenTexts(const StatementPtr& statement) const;

            // What a global may reach, as sorted texts
            std::vector<std::string> regionsOf(const std::string& global) const;

            // Values that hit their guard, now the unknown: sound, reported only
            size_t collapsed() const { return collapsedValues; }

            // What the program's choices about a global rest on, as text (a module cache's key)
            std::string factsOf(const std::string& name) const;

            int definitions(const std::string& name) const;

        private:
            using Objects = std::set<analysis::LocId>;  // Each named by a location: a root, or a path of holders

            bool usable = false;

            std::map<std::string, int> defined;  // Top-level definitions by name (var, const, fun, class)
            std::set<std::string> consts;
            std::set<std::string> vars;
            std::set<std::string> rebinds;       // Globals assigned besides their definition
            std::map<std::string, const Statement*> definitionOf;  // A top-level var's statement

            std::set<std::string> invariantConsts;  // The consts all of whose writers join them
            std::unordered_set<analysis::LocId> definedRoots;

            std::vector<std::vector<StatementPtr>> programFiles;  // What the sites and `topLevel` are keyed by, kept alive
            std::vector<std::vector<Join>> fileJoins;             // By file, by statement
            std::set<size_t> undecidedFiles;                      // Files another could be taken for
            std::vector<bool> runTwice;                           // By file: reached by the run order more than once

            // The order the program runs in, an import where it stands
            std::unordered_map<const Statement*, size_t> ranAt;
            std::unordered_map<std::string, size_t> definedAt;    // The last definition of a name
            std::map<std::string, std::vector<size_t>> joinedTo;  // Const -> the statements joined to it

            std::set<std::string> followed;      // The functions summed up
            std::map<std::string, analysis::Summary> summaries;
            std::unordered_map<const Statement*, analysis::Summary> topLevel;

            // What the edges come to: object -> field -> objects it may hold
            std::unordered_map<analysis::LocId, std::unordered_map<analysis::KeyId, Objects>> heap;
            std::unordered_map<analysis::LocId, Objects> holds;  // Global -> objects it may hold
            std::unordered_map<analysis::LocId, analysis::LocId> holderOf;  // A site stored in one place: that place
            Objects anywhere;                                    // Stored in `?`: any field may hold them
            std::unordered_set<analysis::LocId> reachingAll;     // Locations that stand for all that is reachable

            std::map<std::string, Objects> regions;              // Global -> objects it may reach
            std::unordered_map<analysis::LocId, std::set<std::string>> reachedBy;

            std::set<std::string> inferred;
            size_t collapsedValues = 0;

            // Every location is made where it is first needed, questions included
            mutable analysis::Locations locs;
            mutable std::unordered_map<analysis::LocId, Objects> objectsMemo;
            mutable std::unordered_map<analysis::LocId, analysis::LocId> namedObjects;

            analysis::Summary evaluate(const analysis::BodyIR& body, size_t& collapsed) const;

            // What a statement does: from the analysis when it is of the program, else read now
            analysis::Summary summaryOfStatement(const StatementPtr& statement) const;
            StatementEffects effectsFrom(const analysis::Summary& summary) const;

            bool isInput(analysis::LocId l) const;

            void findHolders(const std::vector<analysis::Edge>& edges);
            analysis::LocId objectNamed(analysis::LocId site) const;

            bool reachesAll(analysis::LocId l) const;

            void addFieldsOf(analysis::LocId object, analysis::KeyId key, Objects& out) const;
            Objects reachableFrom(Objects objects) const;

            // The objects a location may hold
            const Objects& objectsAt(analysis::LocId l) const;

            // The objects a write writes into, and those of every write
            Objects objectsOf(analysis::LocId write) const;
            Objects written(const std::vector<analysis::LocId>& writes) const;

            void resolveEdges(const std::vector<analysis::Edge>& edges);
            void findRegions();

            void findRunOrder(const FileImports& imports);

            // The walk of a file with what the program knows now
            std::vector<Join> walkFile(size_t file) const;

            // A statement standing among open consts that is part of making them: the const it joins
            int joinTarget(const std::vector<StatementPtr>& file, size_t statement, const std::vector<size_t>& open) const;

            // `writers`: the objects each statement or function writes into. The vars found to be consts
            // and the consts found not to be rest on each other: both found again until none is lost
            void settleConsts(const std::vector<Objects>& writers, const std::unordered_map<const Statement*, size_t>& writerOf);
        };
    }
}
