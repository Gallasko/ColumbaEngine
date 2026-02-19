#pragma once

/**
 * @pass_doc
 * @name: Remove Useless Jump Pass
 * @purpose: Removes jump instructions where the target is the immediately following instruction
 * @category: control_flow
 * @example_before:
 *   OP_Jump <offset to next instruction>
 *   <next instruction>
 * @example_after:
 *   <next instruction>
 * @benefits:
 *   - Reduce bytecode size by eliminating no-op jumps
 *   - Improve execution performance by removing unnecessary control flow
 * @end_pass_doc
 */

#include "../bytecode_pass.h"
#include "../chunk.h"
#include <vector>
#include <cstdint>

#include "logger.h"

namespace pg
{
    class RemoveUselessJumpPass : public BytecodePass
    {
    public:
        std::string getName() const override { return "RemoveUselessJump"; }

        bool runPass(Chunk& chunk, BytecodeRewriter* rewriter = nullptr) override
        {
            if (not rewriter)
            {
                LOG_ERROR("RemoveUselessJumpPass", "No rewriter provided");
                return false;
            }

            auto isUselessJump = [&chunk](const std::vector<CapturedInstruction>& captured) -> bool {
                if (captured.empty())
                    return false;

                const auto& jumpInst = captured[0];
                OpCode opcode = jumpInst.opcode;
                size_t jumpOffset = jumpInst.offset;

                // Calculate the target offset
                size_t instructionEnd;
                uint32_t distance;

                bool isLongJump = (opcode == OpCode::OP_Long_Jump or
                                   opcode == OpCode::OP_Long_Jump_If_False or
                                   opcode == OpCode::OP_Long_Jump_If_False_Popping);

                if (isLongJump)
                {
                    instructionEnd = jumpOffset + 5; // long jump is 5 bytes

                    if (jumpInst.operands.size() >= 4)
                    {
                        distance = static_cast<uint32_t>(jumpInst.operands[0]) |
                                  (static_cast<uint32_t>(jumpInst.operands[1]) << 8)  |
                                  (static_cast<uint32_t>(jumpInst.operands[2]) << 16) |
                                  (static_cast<uint32_t>(jumpInst.operands[3]) << 24);
                    }
                    else
                    {
                        return false;
                    }
                }
                else
                {
                    instructionEnd = jumpOffset + 3; // short jump is 3 bytes

                    if (jumpInst.operands.size() >= 2)
                    {
                        distance = static_cast<uint32_t>(jumpInst.operands[0]) |
                                  (static_cast<uint32_t>(jumpInst.operands[1]) << 8);
                    }
                    else
                    {
                        return false;
                    }
                }

                // Check if the jump target is the immediately following instruction
                bool isUseless = distance == 0;

                if (isUseless)
                {
                    size_t targetOffset = instructionEnd + distance;

                    LOG_MILE("RemoveUselessJumpPass", "Found useless jump at offset " << jumpOffset
                             << " (opcode=" << static_cast<int>(opcode) << ", target=" << targetOffset
                             << ", instructionEnd=" << instructionEnd << ")");
                }

                return isUseless;
            };

            // For unconditional jumps, we can just remove them entirely
            auto transformUnconditionalJump = [](const std::vector<CapturedInstruction>&) -> std::vector<uint8_t> {
                // Return empty bytecode to remove the jump
                return std::vector<uint8_t>();
            };

            // Add rule for unconditional jumps (OP_Jump, OP_Long_Jump)
            std::vector<PatternElement> unconditionalPattern = {
                PatternElement::anyOf({OpCode::OP_Jump, OpCode::OP_Long_Jump}, true),
            };

            rewriter->addAdvancedRule(unconditionalPattern, transformUnconditionalJump, isUselessJump);

            // For conditional jumps (jump-if-false), replace with OP_Pop since we need to pop the condition
            auto transformConditionalJump = [](const std::vector<CapturedInstruction>& captured) -> std::vector<uint8_t> {
                return std::vector<uint8_t>{static_cast<uint8_t>(OpCode::OP_Pop)};
            };

            std::vector<PatternElement> conditionalPattern = {
                PatternElement::anyOf({
                    OpCode::OP_Jump_If_False,
                    OpCode::OP_Long_Jump_If_False,
                    OpCode::OP_Jump_If_False_Popping,
                    OpCode::OP_Long_Jump_If_False_Popping
                }, true),
            };

            rewriter->addAdvancedRule(conditionalPattern, transformConditionalJump, isUselessJump);

            return rewriter->rewrite(chunk);
        }

        bool changesSize() const override { return true; }

        bool requiresMultiplePasses() const override { return true; }
    };
}
