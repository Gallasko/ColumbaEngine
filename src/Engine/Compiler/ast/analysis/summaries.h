#pragma once

/**
 * @file summaries.h
 * @brief What a body does to the world, by access path: the evaluation of the SSA IR and the summary of each function.
 *
 * Each value of a body is given the locations it may name (its points-to set). From them come
 * the locations the body writes (a slot: `base[key] = v` writes `base.key`), the objects stored at
 * a location (edges), the globals it reads and rebinds, and what it returns. A function is summed
 * up once, its parameters as roots (`#0.[#1].*.amount`); a call of a function followed puts the
 * call's arguments in place of the parameters. A call of anything else is unknown: it may write
 * all that is reachable from what it is handed, written as `l.*`, and return any of it or a value
 * of its own (`in:@n`, an input made at that call). When that call runs code of the program (a
 * value called, a function not followed), it may also write anything (`?`) and return anything.
 *
 * A write is idempotent (`shape`) when every value it may store is the field's own old value or
 * a value made only of what is the same on every run: constants, and the arguments (`needsParams`)
 * and globals (`needsGlobals`) it names, provided those are invariant where the summary is used.
 */

#include "locations.h"
#include "ssa.h"

#include <cstdint>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace pg
{
    namespace ast
    {
        namespace analysis
        {
            constexpr size_t MaxLocsPerValue = 256;

            // Of the values stored only, so it says nothing alone: nothing may be decided on it until both of
            // these are checked as well.
            // - What guards a store is not looked at: `if (t.on) { t.on = false } else { t.on = true }` stores
            //   constants and reads as idempotent, and it is not.
            // - Whether a store's key or index is invariant is not looked at. In `var i = t.n; t.y = l[i];
            //   t.n = i + 1` the write of `t.y` reads as idempotent, though each run reads the next entry. In
            //   `markAt(t.list, t.n); t.n = t.n + 1` with `fun markAt(l, i) { l[i].done = true }`, the write of
            //   `done` reads as idempotent, though each run marks the next entry
            struct Idempotence
            {
                bool shape = false;                  // every stored value is the field's own old value or invariant-made
                std::set<uint32_t> needsParams;      // ... provided these arguments are invariant at the call
                std::set<std::string> needsGlobals;  // ... and these globals are invariant in the program
            };

            struct Write
            {
                LocId at;  // the slot written
                Idempotence idempotence;
            };

            struct Edge
            {
                LocId at;     // a location
                LocId value;  // an object that may be stored there (param locations allowed in a summary)
            };

            struct Summary
            {
                uint32_t arity = 0;
                std::vector<LocId> returns;         // sorted, unique
                std::vector<Write> writes;          // one per location, sorted by location
                std::vector<Edge> edges;            // sorted, unique
                std::set<std::string> reads;        // globals read and functions called, transitively through calls
                std::set<std::string> rebinds;      // globals assigned, transitively through calls
                bool unknown = false;               // calls what is not followed, or holds a closure

                // A text of the whole summary, the same from one compile to the next
                std::string digest(const Locations& locs) const;
            };

            class Evaluator
            {
            public:
                // `followed(name)`: a program function defined once, never rebound; its summary is in `summaries`.
                // `defined(name)`: the program defines that name, as a function, a class or a global (a global
                // may hold a function); a name it does not define is an engine native
                Evaluator(Locations& locs, const std::map<std::string, Summary>& summaries,
                          std::function<bool(const std::string&)> followed, std::function<bool(const std::string&)> defined);

                Summary evaluate(const BodyIR& body);
                size_t collapsed() const;  // values that hit MaxLocsPerValue

            private:
                Locations& locs;
                const std::map<std::string, Summary>& summaries;
                std::function<bool(const std::string&)> followed;
                std::function<bool(const std::string&)> defined;
                size_t collapsedCount = 0;
            };

            // Every followed function summed up: reverse topological order of the call graph, a fixpoint inside each cycle
            std::map<std::string, Summary> summarize(const std::map<std::string, BodyIR>& bodies, Locations& locs,
                                                     const std::function<bool(const std::string&)>& followed,
                                                     const std::function<bool(const std::string&)>& defined, size_t& collapsed);
        }
    }
}
