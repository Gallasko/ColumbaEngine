#pragma once

/**
 * @pass_doc
 * @name: Constant Variable Access Pass
 * @purpose: Propagates constant values assigned to variables
 * @category: arithmetic
 * @example_before:
 *   OP_Constant 42
 *   OP_Set_Local 0
 *   OP_Get_Local 0
 * @example_after:
 *   OP_Constant 42
 *   OP_Set_Local 0
 *   OP_Constant 42
 * @benefits:
 *   - Eliminates variable loads for constants
 *   - Enables further constant folding
 * @end_pass_doc
 */

#include "../bytecode_pass.h"
#include "../chunk.h"

#include <vector>

#include "logger.h"

namespace pg
{
    class ConstantVarAccess : public BytecodePass
    {
    public:
        std::string getName() const override { return "ConstantVarAccess"; }

        bool runPass(Chunk& chunk, BytecodeRewriter* rewriter = nullptr) override
        {
            if (not rewriter)
            {
                LOG_ERROR("ConstantVarAccess", "No rewriter provided");

                return false;
            }

            std::vector<PatternElement> pattern = {
                PatternElement::match(OpCode::OP_Constant, true),
                PatternElement::match(OpCode::OP_Constant, true),
                PatternElement::match(OpCode::OP_Define_Global),
            };

            rewriter->addAdvancedRule(pattern, [](const std::vector<CapturedInstruction>& captured) -> std::vector<uint8_t> {
                // Create new bytecode sequence for optimized addition
                std::vector<uint8_t> newBytecode;
                newBytecode.push_back(static_cast<uint8_t>(OpCode::OP_Define_Constant_Global));

                newBytecode.push_back(captured[0].operands[0]);
                newBytecode.push_back(captured[1].operands[0]);

                return newBytecode;
            });

            pattern = {
                PatternElement::match(OpCode::OP_Constant, true),
                PatternElement::match(OpCode::OP_Constant, true),
                PatternElement::match(OpCode::OP_Set_Global),
            };

            rewriter->addAdvancedRule(pattern, [](const std::vector<CapturedInstruction>& captured) -> std::vector<uint8_t> {
                // Create new bytecode sequence for optimized addition
                std::vector<uint8_t> newBytecode;
                newBytecode.push_back(static_cast<uint8_t>(OpCode::OP_Set_Constant_Global));

                newBytecode.push_back(captured[0].operands[0]);
                newBytecode.push_back(captured[1].operands[0]);

                return newBytecode;
            });

            pattern = {
                PatternElement::match(OpCode::OP_Constant, true),
                PatternElement::match(OpCode::OP_Get_Global),
            };

            rewriter->addAdvancedRule(pattern, [](const std::vector<CapturedInstruction>& captured) -> std::vector<uint8_t> {
                // Create new bytecode sequence for optimized addition
                std::vector<uint8_t> newBytecode;
                newBytecode.push_back(static_cast<uint8_t>(OpCode::OP_Get_Constant_Global));

                newBytecode.push_back(captured[0].operands[0]);

                return newBytecode;
            });

            return rewriter->rewrite(chunk);
        }

        bool changesSize() const override { return true; }

        bool requiresMultiplePasses() const override { return true; }
    };
}
