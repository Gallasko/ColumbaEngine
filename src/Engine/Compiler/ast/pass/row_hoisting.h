#pragma once

/**
 * @file row_hoisting.h
 * @brief AST pass: `t[k]` read over and over by a loop is read once, before it.
 *
 *     for (var j = 0; j < @len0; j++)
 *     {
 *         res[i][j] = table[i][j] * 2
 *     }
 *
 * becomes
 *
 *     {
 *         var @row0 = res[i]
 *         var @row1 = table[i]
 *         for (var j = 0; j < @len0; j++)
 *         {
 *             @row0[j] = @row1[j] * 2
 *         }
 *     }
 *
 * t is a name, k a name or a number: what is deeper is left to be read from the row. A read goes to
 * the outermost loop it holds for, so it runs after LoopInvariantHoisting has named what k may be
 * made of (`table[i + x]`).
 *
 * Safety rules, in a function only (a function may reach any global). As with LengthHoisting, the
 * program is read whole for the natives.
 * - The names of the read are never assigned or declared in the loop.
 * - Nothing in the loop can put something else at `t[k]`: every call is to a native that changes no
 *   table and calls no script code, push / insert / pop / removeAt and index or field writes are on
 *   a list that cannot be t (Ownership). A write into a row of t is not one when t holds all it
 *   holds alone: only then can a row of t not be t.
 * - The loop may never read it, so the read must not fail, and only a read that cannot run code or
 *   stop the script moves. A program that names a metamethod anywhere is left alone: even a plain
 *   table runs `t.__get`, and one built while the script runs (`"__g" + "et"`) is an accepted limit.
 *   t is a list, a table or a string, where an integer reads none past the ends or for a key not
 *   there. t is known to be one when it is a local made from a literal, or a local that never
 *   changes which a `len` that has run measured (len stops the script on anything else). k is known
 *   to be an integer when it is a local only ever given integers.
 * - Or the hidden declaration right before the loop reads it anyway (`var @len0 = len(t[k])`).
 *
 * A statement that writes back what it reads, `t[a][k] += 1`, reads `t[a]` once, loop or not: the
 * same names, read one right after the other.
 */

#include "../ast_pass.h"

#include <set>

namespace pg
{
    class RowHoistingPass : public AstPass
    {
    public:
        std::string getName() const override { return "RowHoisting"; }

        bool runPass(VM* vm, std::queue<StatementPtr>& statements) override;

        std::string factsAbout(const std::set<std::string>& names, std::vector<std::shared_ptr<const void>>& keep) const override;

        // The reads hoisted by the last run
        long hoistedReads() const { return hoisted; }

    private:
        long hoisted = 0;

        // Of the program being compiled: read anew for each outermost script
        std::set<std::string> scriptNames;
        bool programComplete = false;
        bool metamethods = false;
    };
}
