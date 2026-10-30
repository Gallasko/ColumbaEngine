#pragma once

/**
 * @file effects.h
 * @brief What a program's code reads and writes, for passes that must know whether a value changes.
 *
 * A points-to analysis, by allocation site and by field:
 *
 * - An object is where it is made: a table or list literal of the source (and the table a copy or
 *   an indexBy makes) is one object for all it makes there. A value handed in (an input: a global
 *   the program never defines) is one object of its own, whose insides are not known.
 * - A value is the objects it may be; a global holds what it is ever given. An object's field
 *   (`t.name`, `t["name"]`) holds what is ever put in it; a key not known when compiled and a
 *   list's entries are its field "*", which every read sees.
 * - A function is summed up once (FunctionEffects), its parameters standing for whatever its
 *   calls hand it (#0, #1, ... meaning "any object reachable from that argument"), and applied at
 *   each call with what that call hands it.
 * - The natives whose effects are known (algorithm, string, math) are followed; anything else
 *   called (another native, a value called, a method, a closure) is taken to write all it is
 *   handed and to return any of it.
 *
 * What a top-level statement does (StatementEffects) is what it and every call it makes do: the
 * whole program runs from its top levels.
 */

#include "Interpreter/statement.h"

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace pg
{
    namespace ast
    {
        // Objects: "@<site>" (made by the source there), "in:<name>" (an input), "#<i>" (a
        // function's parameter i, inside its summary)
        struct Roots
        {
            std::set<std::string> names;
            std::set<std::string> deep;     // These and anything reachable from them (kept unexpanded:
                                            // a table of a hundred rows is one name, not its rows)
            bool any = false;               // Anything at all

            bool add(const Roots& other);
            bool add(const std::string& name);

            bool empty() const { return names.empty() and deep.empty() and not any; }
        };

        // A function summed up, its parameters by their index
        struct FunctionEffects
        {
            size_t arity = 0;
            std::set<size_t> writesParams;                          // Writes into what is reachable from them
            Roots result;                                           // What it returns
            std::map<size_t, Roots> paramStores;                    // Put into what is reachable from a parameter
            std::map<std::pair<std::string, std::string>, Roots> pending;   // An object's field given a value holding parameters
            std::set<std::string> reads;                            // Globals read (functions, natives too)
            std::set<std::string> writes;                           // Objects written, not through a parameter
            std::set<std::string> writesDeep;                       // ... and all reachable from these
            bool writesAny = false;
            std::set<std::string> rebinds;                          // Globals assigned
            bool unknown = false;                                   // Calls something that may do anything

            bool operator==(const FunctionEffects& other) const;

            // A text of it, the same for the same summary
            std::string digest() const;
        };

        // What one top-level statement does
        struct StatementEffects
        {
            std::set<std::string> reads;
            std::set<std::string> writes;       // Objects
            std::set<std::string> writesDeep;   // ... and all reachable from these
            bool writesAny = false;
            std::set<std::string> rebinds;
            bool unknown = false;
        };

        class ProgramEffects
        {
        public:
            // The program: its files' top-level statements, the script's first. False when it was
            // given up (more work than the budget allows): then nothing it says may be used
            bool analyze(const std::vector<std::vector<StatementPtr>>& files);

            // The work an analysis may do (the elements of the sets it goes through) before it is
            // given up: a little more than the Chronicle rules need where they are followed whole
            static constexpr uint64_t Budget = 3000000;

            // A statement of the program's top level (or one like it), against what the program does
            StatementEffects effectsOf(const StatementPtr& statement) const;

            // A name whose value is the same on every run: a const, a function defined once and
            // never rebound, a known native, or a var found to be a const (inferredConsts)
            bool invariant(const std::string& name) const;

            // Whether a statement only makes what these globals hold: it reads only what is the same
            // on every run and them, calls nothing unknown, rebinds nothing, and writes only into
            // objects reachable from them or made by itself (reachable from no global)
            bool onlyMakes(const StatementEffects& effects, const std::set<std::string>& targets) const;

            // The top-level vars the program defines once and never changes, made of what is the
            // same on every run: as good as consts
            const std::set<std::string>& inferredConsts() const { return inferred; }

            const std::map<std::string, FunctionEffects>& functions() const { return summaries; }

            // What the program's choices about a global rest on, as text (a module cache's key)
            std::string factsOf(const std::string& name) const;

            int definitions(const std::string& name) const;

        private:
            friend class EffectsWalker;

            std::map<std::string, int> defined;                 // Top-level definitions by name (var, const, fun, class)
            std::set<std::string> consts;
            std::set<std::string> vars;
            std::set<std::string> rebinds;                      // Globals assigned besides their definition

            std::map<std::string, Roots> globalValue;           // What each global of the program may hold
            std::map<std::string, std::map<std::string, Roots>> heap;   // Object -> field -> what it may hold

            std::map<std::string, FunctionEffects> summaries;   // The top-level functions
            std::map<std::string, std::shared_ptr<FunctionStatement>> bodies;
            std::map<std::string, std::shared_ptr<VariableStatement>> definitionOf;   // A top-level var's statement
            std::map<const Statement*, StatementEffects> topLevel;

            // After the analysis: the objects reachable from each global, and which globals reach each object
            std::map<std::string, std::set<std::string>> reachOf;
            std::map<std::string, std::set<std::string>> reachedBy;

            std::set<std::string> inferred;

            // Grows each time an object's field is given more: what was expanded at a version holds
            // while it lasts
            uint64_t heapVersion = 0;

            uint64_t work = 0;
            uint64_t workFrom = 0;
            mutable std::map<std::set<std::string>, std::pair<uint64_t, std::set<std::string>>> expandedAt;

            std::set<std::string> reachable(const Roots& roots) const;

            // Every object a statement writes
            std::set<std::string> written(const StatementEffects& effects) const;
        };
    }
}
