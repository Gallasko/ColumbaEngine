#pragma once

#include "../bytecode_pass.h"
#include "../chunk.h"

#include <vector>

#include "logger.h"

namespace pg
{
    class RemoveDefGetGlobalRedunduncy : public BytecodePass
    {
    public:
        std::string getName() const override { return "RemoveDefGetGlobalRedunduncy"; }

        bool runPass(Chunk& chunk, BytecodeRewriter* rewriter = nullptr)
        {
            if (not rewriter)
            {
                LOG_ERROR("RemoveDefGetGlobalRedunduncy", "No rewriter provided");

                return false;
            }

            std::vector<PatternElement> pattern = {
                PatternElement::constant(true),
                PatternElement::match(OpCode::OP_Define_Global),
                PatternElement::constant(true),
                PatternElement::match(OpCode::OP_Get_Global),
            };

            rewriter->addAdvancedRule(pattern, [](const std::vector<CapturedInstruction>& captured) -> std::vector<uint8_t> {
                // Create new bytecode sequence for optimized addition
                std::vector<uint8_t> newBytecode;

                if (captured[0].opcode == OpCode::OP_Constant)
                {
                    newBytecode.push_back(static_cast<uint8_t>(OpCode::OP_Constant));
                    newBytecode.push_back(captured[0].operands[0]);
                    newBytecode.push_back(static_cast<uint8_t>(OpCode::OP_Define_Global_Non_Popping));
                }
                else // Long constant case
                {
                    newBytecode.push_back(static_cast<uint8_t>(OpCode::OP_LongConstant));
                    newBytecode.push_back(captured[0].operands[0]);
                    newBytecode.push_back(captured[0].operands[1]);
                    newBytecode.push_back(captured[0].operands[2]);
                    newBytecode.push_back(static_cast<uint8_t>(OpCode::OP_Define_Global_Non_Popping));
                }

                return newBytecode;
            }, [](const std::vector<CapturedInstruction>& captured) -> bool {
                LOG_INFO("Def", "Found a possible match");

                if (captured[0].opcode != captured[1].opcode)
                    return false;

                if (captured[0].opcode == OpCode::OP_Constant)
                {
                    if (captured[0].operands[0] == captured[1].operands[0])
                        return true;
                    else
                        return false;
                }
                else if (captured[0].opcode == OpCode::OP_LongConstant)
                {
                    if (captured[0].operands[0] == captured[1].operands[0] and
                        captured[0].operands[1] == captured[1].operands[1] and
                        captured[0].operands[2] == captured[1].operands[2])
                        return true;
                    else
                        return false;
                }
                else
                {
                    return false;
                }

                return false;
            });

            return rewriter->rewrite(chunk);
        }

        bool changesSize() const override { return true; }

        bool requiresMultiplePasses() const override { return true; }
    };
}
