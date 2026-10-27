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
 * Safety rules: nothing in the loop (initializer, condition, increment, body; not the bodies of
 * functions it only defines) can change how long x is.
 * - x is a plain name, never assigned or declared in the loop.
 * - The condition reads `len(x)` on its first evaluation (not only on the right of an `and` / `or`,
 *   where it may never have been evaluated: `x != none and i < len(x)`).
 * - Every call in the loop is to a native known not to change any table and not to call back into
 *   script (len, lookup, toFloat, the string and math functions, ...): no script function, no
 *   method, nothing else. The name must be no function, variable or class of the program (read
 *   whole, as FunctionInlining reads it) and no local where it is called.
 * - push / insert / pop / removeAt and index writes only on a local list other than x that is
 *   made from a literal in the function and never leaves it (only pushed to, indexed, measured,
 *   iterated, compared or returned; never passed elsewhere, stored or captured), so that x cannot
 *   be the same list.
 *
 * Known (accepted) limit, as for LoopInvariantHoisting: field reads and writes on class instances
 * run __get / __set metamethods, which could change a list.
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

    private:
        // Of the program being compiled: read anew for each outermost script
        std::set<std::string> scriptNames;
        bool programComplete = false;
    };
}
