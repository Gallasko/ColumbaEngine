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
 * - A top-level `var` the program defines once, never rebinds and never writes into, made of what
 *   is the same on every run (ast::ProgramEffects::inferredConsts), is a const.
 * - A statement in such a run that reads only what is the same on every run and those consts,
 *   writes only into them (and what they hold) and calls nothing unknown is part of making them:
 *   it joins the last of the consts it uses (a `const` promises its value is made the same and
 *   never changed: what writes into it right after is how it is made).
 *
 * The whole program is read for each outermost script compiled; a module compiled meanwhile
 * shares what was read (as FunctionInlining does), its own parse read as the one analyzed.
 * The walk of the statements is ast::ProgramEffects::joinsOf: what makes a const once depends on it.
 */

#include "../ast_pass.h"
#include "effects.h"

#include <set>
#include <string>

namespace pg
{
    class RunOncePass : public AstPass
    {
    public:
        // The whole program is read (and consts inferred from it) only when asked: it costs a compile
        explicit RunOncePass(bool wholeProgram = false) : wholeProgram(wholeProgram) {}

        std::string getName() const override { return "RunOnce"; }

        bool runPass(VM* vm, std::queue<StatementPtr>& statements) override;

        std::string factsAbout(const std::set<std::string>& names, std::vector<std::shared_ptr<const void>>& keep) const override;

        std::string factsAboutIdentifiers(const std::set<std::string>& identifiers) const override;

    private:
        bool wholeProgram = false;

        // The functions of the program defined once and never written: kept from run to run
        std::set<std::string> keptFunctions;

        // What the program's code reads and writes (when it could be read whole)
        ast::ProgramEffects effects;
        bool analyzed = false;
    };
}
