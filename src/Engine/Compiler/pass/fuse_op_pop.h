#pragma once

#include "../bytecode_pass.h"
#include "../chunk.h"

#include <vector>

#include "logger.h"

namespace pg
{
    class FuseOpPop : public BytecodePass
    {
    public:
        std::string getName() const override { return "FuseOpPop"; }

        bool runPass(Chunk& chunk, BytecodeRewriter* rewriter = nullptr) override
        {
            if (not rewriter)
            {
                LOG_ERROR("FuseOpPop", "No rewriter provided");

                return false;
            }

            std::vector<PatternElement> pattern = {
                PatternElement::match(OpCode::OP_Pop),
                PatternElement::match(OpCode::OP_Pop),
            };

            rewriter->addAdvancedRule(pattern, [](const std::vector<CapturedInstruction>&) -> std::vector<uint8_t> {
                // Create new bytecode sequence for optimized addition
                std::vector<uint8_t> newBytecode;
                newBytecode.push_back(static_cast<uint8_t>(OpCode::OP_PopN));
                newBytecode.push_back(2);

                return newBytecode;
            });

            pattern = {
                PatternElement::match(OpCode::OP_Pop),
                PatternElement::match(OpCode::OP_PopN, true),
            };

            rewriter->addAdvancedRule(pattern, [](const std::vector<CapturedInstruction>& captured) -> std::vector<uint8_t> {
                // Create new bytecode sequence for optimized addition
                std::vector<uint8_t> newBytecode;
                newBytecode.push_back(static_cast<uint8_t>(OpCode::OP_PopN));

                newBytecode.push_back(captured[0].operands[0] + 1);

                return newBytecode;
            });

            pattern = {
                PatternElement::match(OpCode::OP_PopN, true),
                PatternElement::match(OpCode::OP_Pop),
            };

            rewriter->addAdvancedRule(pattern, [](const std::vector<CapturedInstruction>& captured) -> std::vector<uint8_t> {
                // Create new bytecode sequence for optimized addition
                std::vector<uint8_t> newBytecode;
                newBytecode.push_back(static_cast<uint8_t>(OpCode::OP_PopN));

                newBytecode.push_back(captured[0].operands[0] + 1);

                return newBytecode;
            });

            pattern = {
                PatternElement::match(OpCode::OP_PopN, true),
                PatternElement::match(OpCode::OP_PopN, true),
            };

            rewriter->addAdvancedRule(pattern, [](const std::vector<CapturedInstruction>& captured) -> std::vector<uint8_t> {
                // Create new bytecode sequence for optimized addition
                std::vector<uint8_t> newBytecode;
                newBytecode.push_back(static_cast<uint8_t>(OpCode::OP_PopN));

                newBytecode.push_back(captured[0].operands[0] + captured[1].operands[0]);

                return newBytecode;
            });

            return rewriter->rewrite(chunk);
        }

        bool changesSize() const override { return true; }

        bool requiresMultiplePasses() const override { return true; }
    };
}
