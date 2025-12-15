#pragma once

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
