#pragma once

/**
 * @file program_consts.h
 * @brief AST pass: what the whole program shows to be made once, read before any pass rewrites it.
 *
 * Runs first, at O3 only (VmOptimizationLevel), so that the analysis and what it decides see the
 * same statements. For the script compiled and every module it imports (ast::ProgramEffects):
 *
 * - A top-level `var` the program defines once, never rebinds and never writes into, made of what
 *   is the same on every run, is a const (VariableStatement::inferredConst).
 * - A statement in a run of consts (ast::walkConsts) that sets a constant entry of one, or reads
 *   only what is the same on every run and writes only into them, joins it
 *   (VariableStatement::keptFills): it is part of making it. The passes after this one do not see
 *   it: it runs once.
 *
 * A module is compiled from a parse of its own, of the same source as the one analyzed: what was
 * decided is applied by index. A file that does not line up has nothing applied.
 */

#include "../ast_pass.h"
#include "effects.h"

#include <set>
#include <string>

namespace pg
{
    class ProgramConstsPass : public AstPass
    {
    public:
        std::string getName() const override { return "ProgramConsts"; }

        bool runPass(VM* vm, std::queue<StatementPtr>& statements) override;

        std::string factsAboutIdentifiers(const std::set<std::string>& identifiers) const override;

    private:
        // What the program's code reads and writes (when it could be read whole)
        ast::ProgramEffects effects;
        bool analyzed = false;
    };
}
