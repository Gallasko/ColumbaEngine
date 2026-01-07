#pragma once

/**
 * @pass_doc
 * @name: Constant Folding Pass
 * @purpose: Evaluates constant expressions at compile time to eliminate runtime computation
 * @category: arithmetic
 * @example_before:
 *   OP_Constant 5
 *   OP_Constant 3
 *   OP_Add
 * @example_after:
 *   OP_Constant 8
 * @benefits:
 *   - Eliminates runtime arithmetic operations
 *   - Reduces instruction count by 66% for constant expressions (3 → 1 instruction)
 *   - Smaller constant table through deduplication
 *   - Better instruction cache utilization
 *   - Cascading effect enables other optimizations
 * @additional_notes:
 *   Requires multiple passes because folding can expose new opportunities.
 *   Example: (5 + 3) + (2 + 4) needs two passes to fully fold to 14.
 *
 *   Supported operations:
 *   - Arithmetic: +, -, *, /, % (binary), - (unary negation)
 *   - Comparison: ==, !=, <, >, <=, >=
 *   - Logical: and, or, not
 *   - String: concatenation
 *
 *   Current limitation: Only folds to OP_Constant (not OP_LongConstant).
 *   This limits constant table to 254 entries during optimization.
 * @end_pass_doc
 */

#include "../bytecode_pass.h"
#include "../chunk.h"

#include <vector>

#include "logger.h"



namespace pg
{
    class ConstantFoldingPass : public BytecodePass
    {
    public:
        std::string getName() const override { return "ConstantFoldingPass"; }

        bool runPass(Chunk& chunk, BytecodeRewriter* rewriter = nullptr) override;

        bool changesSize() const override { return true; }

        bool requiresMultiplePasses() const override { return true; }
    };
}
