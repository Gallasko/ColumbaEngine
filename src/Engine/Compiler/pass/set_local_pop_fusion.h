#pragma once

/**
 * @pass_doc
 * @name: Set Local + Pop Fusion Pass
 * @purpose: Fuses Set_Local followed by Pop into a single Set_Local_Pop op
 * @category: stack
 * @example_before:
 *   OP_Set_Local 1
 *   OP_Pop
 * @example_after:
 *   OP_Set_Local_Pop 1
 * @benefits:
 *   - 1 dispatch instead of 2 per assignment statement
 *   - Set_Local_Pop pops once instead of peek-then-pop, saving one stack write
 *   - Especially impactful inside loops (every `i = i + 1` and `sum = sum + i`)
 * @end_pass_doc
 */

#include "../bytecode_pass.h"
#include "../chunk.h"

#include "logger.h"

namespace pg
{
    class SetLocalPopFusionPass : public BytecodePass
    {
    public:
        std::string getName() const override { return "SetLocalPopFusion"; }

        bool runPass(Chunk& chunk, BytecodeRewriter* rewriter = nullptr) override
        {
            if (not rewriter)
            {
                LOG_ERROR("SetLocalPopFusion", "No rewriter provided");
                return false;
            }

            std::vector<PatternElement> pattern = {
                PatternElement::match(OpCode::OP_Set_Local, true),  // capture slot operand
                PatternElement::match(OpCode::OP_Pop),
            };

            rewriter->addAdvancedRule(pattern, [](const std::vector<CapturedInstruction>& captured) -> std::vector<uint8_t> {
                uint8_t slot = captured[0].operands[0];

                std::vector<uint8_t> out;
                out.push_back(static_cast<uint8_t>(OpCode::OP_Set_Local_Pop));
                out.push_back(slot);
                return out;
            });

            return rewriter->rewrite(chunk);
        }

        bool changesSize() const override { return true; }
        bool requiresMultiplePasses() const override { return false; }
    };
}
