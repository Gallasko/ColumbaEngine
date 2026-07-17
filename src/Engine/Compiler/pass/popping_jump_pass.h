#pragma once

/**
 * @pass_doc
 * @name: Popping Jump Optimization Pass
 * @purpose: Optimizes jump-if-false instructions followed by pops at both locations into specialized popping jump instructions
 * @category: control_flow
 * @example_before:
 *   OP_Jump_If_False <offset>
 *   OP_Pop
 *   ...
 *   OP_Pop  ; at jump target
 * @example_after:
 *   OP_Jump_If_False_Popping <offset>
 *   ...
 *   ; (both pops eliminated)
 * @benefits:
 *   - Reduce bytecode size by eliminating redundant pop instructions
 *   - Improve execution performance by combining jump and pop operations
 * @end_pass_doc
 */

#include "../bytecode_pass.h"
#include "../chunk.h"
#include <vector>
#include <cstdint>

namespace pg
{
    class PoppingJumpPass : public BytecodePass
    {
        struct JumpInfo
        {
            size_t instructionOffset;
            OpCode opcode;
            uint32_t jumpDistance;
            size_t targetOffset;
            bool canOptimize;

            JumpInfo(size_t offset, OpCode op, uint32_t distance, size_t target)
                : instructionOffset(offset), opcode(op), jumpDistance(distance),
                targetOffset(target), canOptimize(false) {}
        };

    public:
        std::string getName() const override { return "PoppingJumpOptimization"; }

        bool runPass(Chunk& chunk, BytecodeRewriter* rewriter = nullptr) override;

        bool changesSize() const override { return true; }

        bool requiresMultiplePasses() const override { return true; }

    private:
        // Find all long jump candidates that can be optimized to short jumps
        std::optional<JumpInfo> findFirstOptimizableJumps(const Chunk& chunk, BytecodeRewriter* rewriter);

        // Helper methods
        uint32_t extractLongJumpOffset(const Chunk& chunk, size_t offset);
        uint16_t extractShortJumpOffset(const Chunk& chunk, size_t offset);
        uint32_t extractJumpOffset(const OpCode& code, const Chunk& chunk, size_t offset);
        OpCode getPoppingJumpReplacement(const OpCode& opcode);
        std::vector<uint8_t> createReplacement(const OpCode& originalOpcode, uint32_t jumpDistance);
    };

}