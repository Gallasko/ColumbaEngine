#pragma once

/**
 * @file list_methods.h
 * @brief AST pass: a list's method call is the call of the function it runs, when nothing else can run.
 *
 *     rows.push(x)      becomes   __push(rows, x)
 *     rows.insert(i, x)           __insert(rows, i, x)
 *     rows.pop()                  __pop(rows)
 *     rows.removeAt(i)            __removeAt(rows, i)
 *     rows.len()                  __len(rows)
 *
 * A list has these methods of itself (`len` is a table's and a string's too: VM's invoke), and the
 * VM gives the same functions as globals under these names, which no import is needed for. The
 * passes after it know them as they know `push` and `len`: what `push(rows, x)` is hoisted or owned
 * through, `rows.push(x)` is too. Left alone, a method call is the call of a value unknown to them.
 *
 * Safety rules: under a name the call can run nothing but the list's method.
 * - No class of the program (any of its files, at any depth) has a method of that name: an
 *   instance's method is called first.
 * - No field of the program may have that name: no `.push = ...`, no `{push: ...}` key and no
 *   string "push" anywhere (a key `t["push"]`): a table's field is called first.
 * - The program is read whole (ProgramFiles): a module read as bytecode only could hold either.
 * - The call gives the method what it takes (`push` one argument, `pop` none, ...): one that does
 *   not stops the script either way, and stays as it is.
 *
 * Known (accepted) limits, as for the classes the effects analysis follows: a key built while the
 * script runs (`"pu" + "sh"`), or a table a native makes, is not seen. The `__` names are the
 * engine's: a script defining `__push` is not looked for.
 */

#include "../ast_pass.h"

#include <set>
#include <string>

namespace pg
{
    class ListMethodsPass : public AstPass
    {
    public:
        std::string getName() const override { return "ListMethods"; }

        bool runPass(VM* vm, std::queue<StatementPtr>& statements) override;

        std::string factsAbout(const std::set<std::string>& names, std::vector<std::shared_ptr<const void>>& keep) const override;

        // The calls rewritten by the last run
        long rewrittenCalls() const { return rewritten; }

        // The methods a call can run nothing but, in the program being compiled
        const std::set<std::string>& onlyListMethods() const { return callable; }

    private:
        long rewritten = 0;

        // Of the program being compiled: read anew for each outermost script
        std::set<std::string> callable;
    };
}
