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
 *   half filled. Function definitions and other consts made of constant data may stand between;
 *   any other statement ends the run of fills.
 *
 * The whole program is read for each outermost script compiled; a module compiled meanwhile
 * shares what was read (as FunctionInlining does). The walk of the consts is ast::walkConsts:
 * ProgramConstsPass walks them the same way.
 */

#include "../ast_pass.h"

#include <functional>
#include <set>
#include <string>
#include <vector>

namespace pg
{
    namespace ast
    {
        // What the walk of a file's top level found for one statement
        struct Join
        {
            bool joined = false;
            size_t target = 0;           // Joined: the index of the const it joins
            bool fill = false;           // A constant entry of it, or else found to make it
            std::set<std::string> open;  // Joined: the consts open then
        };

        // The walk of a file's top level, one Join per statement. The consts declared in a row stay
        // open: a statement setting a constant entry of one joins it, one that `makes` says makes them
        // joins the const of the index it gives (negative: none), anything else closes them. A const
        // that already holds a statement joined to it reads what that reads: it closes those before it.
        // `isConst`: a declaration that counts as a const.
        // `inOrder`: joined, statements run in the order of their consts, so a statement whose const
        // comes before the last one joined stays where it is and closes them: it would run before
        // what it follows
        std::vector<Join> walkConsts(const std::vector<StatementPtr>& statements,
                                     const std::function<bool(const VariableStatement&)>& isConst,
                                     const std::function<int(size_t statement, const std::vector<size_t>& open)>& makes,
                                     bool inOrder);
    }

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
