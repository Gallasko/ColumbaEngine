#pragma once

/**
 * @file run_once.h
 * @brief AST pass: top-level code a script would run the same on every run of a VM is run once.
 *
 * A rule script runs whole at every call, and so do the modules it imports: its top-level `fun`
 * definitions made a new closure each time, and the fills of its `const` tables wrote the same
 * entries again. Like a `const`'s initializer (VariableStatement::isConst), they are made on the
 * script's first run on a VM and kept by the later ones (OP_Global_Missing; a script reloaded on
 * the VM makes them anew, VM::forgetKeptConsts):
 *
 * - A top-level `fun` the whole program defines once and never writes (no assignment, variable
 *   or class of its name anywhere: ast::ProgramView) is marked FunctionStatement::keptOnce.
 * - The statements right after a top-level `const t = ...` that set an entry of it to a
 *   constant (`t["key"] = "value"`, `t.key = 3`) join its initializer
 *   (VariableStatement::keptFills): nothing runs between them, so nothing could see the table
 *   half filled. Any other statement ends the run of fills.
 *
 * The whole program is read for each outermost script compiled; a module compiled meanwhile
 * shares what was read (as FunctionInlining does).
 */

#include "../ast_pass.h"

#include <set>
#include <string>

namespace pg
{
    class RunOncePass : public AstPass
    {
    public:
        std::string getName() const override { return "RunOnce"; }

        bool runPass(VM* vm, std::queue<StatementPtr>& statements) override;

        std::string factsAbout(const std::set<std::string>& names, std::vector<std::shared_ptr<const void>>& keep) const override;

    private:
        // The functions of the program defined once and never written: kept from run to run
        std::set<std::string> keptFunctions;
    };
}
