#pragma once

/**
 * @file ast_analysis.h
 * @brief Shared conservative analysis helpers for AST passes.
 */

#include "Interpreter/expression.h"
#include "Interpreter/statement.h"

#include <functional>
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

        /**
         * Conservative upper bound of the local slots a function body can create: every
         * declaration counts, sibling blocks are summed even though their slots are reused, for-in
         * adds its three hidden iteration locals plus the key variable. Nested function/class
         * bodies have their own slot space and are excluded.
         */
        int countLocalSlots(const StatementPtr& s);

        using ExprVisit = std::function<void(const ExprPtr&)>;

        // An expression or a statement and every expression below it, each before those it holds; in
        // the bodies of the functions and classes it defines only with `intoFunctions`
        void eachExpr(const ExprPtr& e, const ExprVisit& visit, bool intoFunctions = false);
        void eachExpr(const StatementPtr& s, const ExprVisit& visit, bool intoFunctions = false);

        // Local slots are addressed with a single byte; passes adding hidden locals stay under this
        constexpr int kMaxLocalSlots = 240;
    }
}
