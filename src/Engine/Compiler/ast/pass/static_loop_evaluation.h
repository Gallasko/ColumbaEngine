#pragma once

/**
 * @file static_loop_evaluation.h
 * @brief AST pass: execute fully-static loops at compile time.
 *
 * Rewrites
 *
 *     var total = 0
 *     for (var i = 0; i < 10; i++) total += i;
 *
 * into
 *
 *     var total = 0
 *     total = 45
 *
 * by interpreting the loop during compilation. A loop folds only when the
 * whole computation is provably static:
 *
 * - Every variable it reads is a tracked compile-time constant (int or
 *   bool) established by the preceding statements in the same scope.
 * - Its body/condition/increment use only pure constructs: literals,
 *   variables, arithmetic/comparison/logic operators, assignments,
 *   prefix/postfix inc/dec, blocks, if, nested while/for, break/continue.
 *   Calls, property access, indexing, tables, floats and strings all bail.
 * - It terminates within the step budget; otherwise it is left untouched
 *   (so an infinite or huge loop never hangs compilation).
 *
 * Evaluation matches VM semantics exactly: int64 arithmetic, C++ truncating
 * division/modulo (division by zero bails and stays a runtime error), and
 * the VM's truthiness rules. Results that don't fit in an int32 literal
 * bail, since script constants are 32-bit.
 *
 * Loop-scoped variables die with the loop; only variables that existed
 * before the loop and were modified get a final-value assignment.
 */

#include "../ast_pass.h"

namespace pg
{
    class StaticLoopEvaluationPass : public AstPass
    {
    public:
        std::string getName() const override { return "StaticLoopEvaluation"; }

        bool runPass(VM* vm, std::queue<StatementPtr>& statements) override;
    };
}
