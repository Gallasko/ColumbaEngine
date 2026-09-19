#pragma once

/**
 * @file ast_analysis.h
 * @brief Shared conservative analysis helpers for AST passes.
 */

#include "Interpreter/expression.h"
#include "Interpreter/statement.h"

#include <set>
#include <string>

namespace pg
{
    namespace ast
    {
        /**
         * What a statement/expression subtree may mutate.
         *
         * - 'mutated' contains every name assigned, declared, or touched by
         *   prefix/postfix operators anywhere in the analyzed region.
         * - 'hasCalls' is set when ANY call appears: a call can mutate
         *   captured locals and globals, so callers must treat everything as
         *   potentially changed.
         *
         * Nested function/class bodies are skipped (they only run when
         * called, and a call sets hasCalls).
         */
        struct LoopFacts
        {
            std::set<std::string> mutated;
            bool hasCalls = false;
        };

        void analyzeExpr(const ExprPtr& e, LoopFacts& f);
        void analyzeStmt(const StatementPtr& s, LoopFacts& f);
    }
}
