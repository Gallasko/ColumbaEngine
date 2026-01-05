#pragma once

/**
 * @pass_doc
 * @name: Basic Operator Local Indexing Pass
 * @purpose: Optimizes binary operations on local variables into specialized instructions
 * @category: arithmetic
 * @example_before:
 *   OP_Get_Local 0
 *   OP_Get_Local 1
 *   OP_Add
 * @example_after:
 *   OP_AddLL 0 1
 * @benefits:
 *   - Reduces 3 instructions to 1 (66%% reduction)
 *   - Eliminates stack manipulation
 *   - Direct register-style operations
 *   - Significant performance gain in tight loops
 * @additional_notes:
 *   Optimizes Add and Subtract operations on local variables.
 *   Also handles mixed local/constant patterns.
 * @end_pass_doc
 */

#include "../bytecode_pass.h"
#include "../chunk.h"

namespace pg
{
    class BasicOperatorLocalIndexingPass : public BytecodePass
    {
    public:
        std::string getName() const override { return "BasicOperatorLocalIndexing"; }

        bool runPass(Chunk& chunk, BytecodeRewriter* rewriter = nullptr) override;

        bool changesSize() const override { return true; }

        bool requiresMultiplePasses() const override { return false; }
    };
}
