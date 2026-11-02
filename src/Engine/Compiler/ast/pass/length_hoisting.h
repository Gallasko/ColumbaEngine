#pragma once

/**
 * @file length_hoisting.h
 * @brief AST pass: `len(x)` read by a loop's condition is read once, before the loop.
 *
 *     for (var i = 0; i < len(rows); i++) { ... }
 *
 * becomes
 *
 *     {
 *         var @len0 = len(rows)
 *         for (var i = 0; i < @len0; i++) { ... }
 *     }
 *
 * (and the body's own `len(rows)` reads the hidden local too).
 *
 * x may be a row too: `len(table[i])`, a chain of indexing by names and numbers.
 *
 * Safety rules: nothing in the loop (initializer, condition, increment, body; not the bodies of
 * functions it only defines) can change how long x is.
 * - The names of x are never assigned or declared in the loop.
 * - The condition reads `len(x)` on its first evaluation (not only on the right of an `and` / `or`,
 *   where it may never have been evaluated: `x != none and i < len(x)`).
 * - Every call in the loop is to a native known not to change any table and not to call back into
 *   script (len, lookup, toFloat, the string and math functions, ...): no script function, no
 *   method, nothing else. The name must be no function, variable or class of the program (read
 *   whole, as FunctionInlining reads it) and no local where it is called.
 * - push / insert / pop / removeAt, index and field writes only on a local list other than x's that is
 *   made from a literal in the function and never leaves it (only pushed to, indexed, measured,
 *   iterated, compared or returned; never passed elsewhere, stored or captured), so that x cannot
 *   be the same list. A row of that list counts when the list holds all it holds alone: only given
 *   new tables and what is no table, no row read anywhere it could leave (Ownership::ownedDeep).
 *   A local only ever given rows of it counts as that list (Ownership::rowOfOwned).
 * - Or x is beyond them, and any list other than x's may be written: x is such a list itself,
 *   which nothing else can name, or a local that only ever holds what is no table (a string, which
 *   no write changes).
 * - A local only ever given `[...]` is no table, so no field write matters to it: a new field only
 *   lengthens a table, and one written on a list stops the script (FunctionScope::isList). A row
 *   may be a table, whatever its list is.
 *
 * Known (accepted) limits. As for LoopInvariantHoisting: field reads and writes on class instances
 * run __get / __set metamethods, which could change a list, and an operator metamethod could
 * return a table where a number is taken for granted. As ever here: a name read before the function
 * declares it is taken for the local.
 */

#include "../ast_pass.h"

#include <set>

namespace pg
{
    class LengthHoistingPass : public AstPass
    {
    public:
        std::string getName() const override { return "LengthHoisting"; }

        bool runPass(VM* vm, std::queue<StatementPtr>& statements) override;

        std::string factsAbout(const std::set<std::string>& names, std::vector<std::shared_ptr<const void>>& keep) const override;

        // The reads hoisted by the last run
        long hoistedReads() const { return hoisted; }

    private:
        long hoisted = 0;

        // Of the program being compiled: read anew for each outermost script
        std::set<std::string> scriptNames;
        bool programComplete = false;
    };
}
