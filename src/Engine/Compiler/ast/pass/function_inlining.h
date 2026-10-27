#pragma once

/**
 * @file function_inlining.h
 * @brief AST pass: a call to a function that is a single `return <expression>` becomes that
 * expression, its arguments in the place of its parameters.
 *
 *     fun statOf(character, stat) { return lookup(character, stat, 0) }
 *     ... statOf(character, req.stat) ...      ->      ... lookup(character, req.stat, 0) ...
 *
 * so the call costs no frame of its own. Inlined code is inlined again (a few levels deep), so
 * chains of wrappers fold down.
 *
 * Safety rules:
 * - The whole program is read for each outermost script compiled (the script, then every module it
 *   imports, as the compiler finds them; a module compiled meanwhile shares it): the function must be a top-level `fun` defined once in all of it, and
 *   never assigned, declared as a variable or a class of the same name anywhere. A module the
 *   pass cannot read as source (a .pgc only) disables the pass for the program.
 * - Its expression holds only operators, constants, variables, field and index reads, table
 *   literals and calls: no assignment, no write, no prefix/postfix operator, no `this`, no nested
 *   function. It does not call itself.
 * - Every argument is a constant or none, or (when the expression calls only natives, no script
 *   code that could write what it reads) a variable, or a chain of field reads on a variable whose
 *   parameter the expression uses exactly once. Nothing else: the count and the order of what the
 *   arguments evaluate cannot change.
 * - Neither the function's name nor any free name of its expression is a local or a parameter
 *   where it is called (it would bind to that instead of the global).
 * - The number of arguments is the number of parameters.
 *
 * Runs right after entity-loop lowering (which wants the loops as written), before the other AST
 * passes: what it folds away leaves fewer calls for them to refuse.
 */

#include "../ast_pass.h"

#include <map>
#include <set>
#include <vector>

namespace pg
{
    class FunctionInliningPass : public AstPass
    {
    public:
        std::string getName() const override { return "FunctionInlining"; }

        bool runPass(VM* vm, std::queue<StatementPtr>& statements) override;

        struct Inlinable
        {
            std::vector<std::string> params;
            ExprPtr body;
            std::set<std::string> freeNames;      // Names the expression reads that are no parameter
            std::map<std::string, int> uses;      // How often it reads each parameter
            bool callsScript = false;             // It may run script code that writes, not only natives
            std::shared_ptr<FunctionStatement> from;  // The function it is: the rest follows from it, but callsScript
        };

        void namesBrought(std::set<std::string>& names) const override;

        std::string factsAbout(const std::set<std::string>& names, std::vector<std::shared_ptr<const void>>& keep) const override;

    private:
        // Of the program being compiled: read anew for each outermost script
        std::map<std::string, Inlinable> inlinable;
    };
}
