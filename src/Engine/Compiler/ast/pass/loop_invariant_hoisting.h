#pragma once

/**
 * @file loop_invariant_hoisting.h
 * @brief AST pass: hoist loop-invariant pure expressions out of loops.
 *
 * Rewrites
 *
 *     while (i < n) { result = result + (a * b + c) }
 *
 * into
 *
 *     {
 *         var @hoist0 = a * b + c
 *         while (i < n) { result = result + @hoist0 }
 *     }
 *
 * so the invariant is evaluated once instead of every iteration. The hidden
 * variable also becomes a LOCAL slot, so at script top level this replaces
 * per-iteration global hash lookups with a stack read.
 *
 * Conservative safety rules (v1):
 * - A loop containing ANY call is left untouched: a call can mutate captured
 *   locals and globals, so nothing is provably invariant across it.
 * - Hoistable expressions are pure by construction: literals and variables
 *   (never assigned/declared inside the loop) combined with unary, binary
 *   and logic operators. No property access, no indexing, no calls - those
 *   can hit metamethods/natives with side effects or aliasing.
 * - Nested function/class bodies inside the loop are ignored (they don't
 *   execute per iteration) and their own loops are transformed separately.
 *
 * Known (accepted) observability limit: operator metamethods on class
 * instances (__add/__mul/...) run once instead of per iteration for hoisted
 * expressions; an impure operator metamethod could detect this.
 */

#include "../ast_pass.h"

namespace pg
{
    class LoopInvariantHoistingPass : public AstPass
    {
    public:
        std::string getName() const override { return "LoopInvariantHoisting"; }

        bool runPass(VM* vm, std::queue<StatementPtr>& statements) override;
    };
}
