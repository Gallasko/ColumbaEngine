#pragma once

/**
 * @pass_doc
 * @name: Increment Optimization Pass
 * @purpose: Converts x = x + 1 patterns into specialized increment instructions
 * @category: arithmetic
 * @example_before:
 *   OP_Get_Local 0
 *   OP_Constant 1
 *   OP_Add
 *   OP_Set_Local 0
 *   OP_Pop
 * @example_after:
 *   OP_Short_Int 0
 *   OP_Post_Incr_Local
 * @benefits:
 *   - Reduces instruction count from 5 to 2 (60% reduction)
 *   - Eliminates constant table lookup
 *   - Specialized opcode executes faster than generic arithmetic
 *   - Critical for loop performance (for loops with i++)
 *   - Better instruction cache utilization in tight loops
 * @additional_notes:
 *   Recognizes the specific pattern of:
 *   1. Loading a local variable
 *   2. Adding constant value 1
 *   3. Storing back to the same local variable
 *   4. Popping the result (statement context)
 *
 *   The pass validates that the GET and SET target the same local variable
 *   to avoid incorrect transformations. This pattern is extremely common in
 *   for loops: for (var i = 0; i < n; i++)
 *
 *   Could be extended to support:
 *   - Pre-increment (++i) pattern detection
 *   - Decrement patterns (i--)
 *   - Global variable increments
 * @end_pass_doc
 */

#include "../bytecode_pass.h"
#include "../chunk.h"

#include <vector>

#include "logger.h"

namespace pg
{
    class IncrementOptimizationPass : public BytecodePass
    {
    public:
        std::string getName() const override { return "IncrementOptimizationPass"; }

        bool runPass(Chunk& chunk, BytecodeRewriter* rewriter = nullptr) override
        {
            if (not rewriter)
            {
                LOG_ERROR("IncrementOptimizationPass", "No rewriter provided");

                return false;
            }

            auto sameLocalAndConstantOne = [&chunk](const std::vector<CapturedInstruction>& captured) {
                auto value = chunk.constants[captured[1].getConstantIndex()];

                if (not IS_INT(value))
                    return false;

                return (captured[0].operands[0] == captured[2].operands[0]) and (AS_INT(value) == 1);
            };

            std::vector<PatternElement> pattern = {
                PatternElement::match(OpCode::OP_Get_Local, true),
                PatternElement::match(OpCode::OP_Constant, true),
                PatternElement::match(OpCode::OP_Add),
                PatternElement::match(OpCode::OP_Set_Local, true),
                PatternElement::match(OpCode::OP_Pop),
            };

            rewriter->addAdvancedRule(pattern, [](const std::vector<CapturedInstruction>& captured) -> std::vector<uint8_t> {
                // Create new bytecode sequence for optimized addition
                std::vector<uint8_t> newBytecode;

                newBytecode.push_back(static_cast<uint8_t>(OpCode::OP_Short_Int));
                newBytecode.push_back(captured[0].operands[0]);
                newBytecode.push_back(static_cast<uint8_t>(OpCode::OP_Post_Incr_Local));


                return newBytecode;
            }, sameLocalAndConstantOne);

            return rewriter->rewrite(chunk);
        }

        bool changesSize() const override { return true; }

        bool requiresMultiplePasses() const override { return false; }
    };
}
