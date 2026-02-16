#pragma once

/**
 * @pass_doc
 * @name: Long Jump Optimization Pass
 * @purpose: Converts between short and long jump instructions based on offset requirements
 * @category: control_flow
 * @example_before:
 *   OP_Long_Jump <small offset>
 * @example_after:
 *   OP_Jump <small offset>
 * @benefits:
 *   - Reduce bytecode size by using short jumps when possible
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
        std::optional<JumpInfo> findFirstOptimizableJumps(const Chunk& chunk);

        // Helper methods
        uint32_t extractLongJumpOffset(const Chunk& chunk, size_t offset);
        uint16_t extractShortJumpOffset(const Chunk& chunk, size_t offset);
        uint32_t extractJumpOffset(const OpCode& code, const Chunk& chunk, size_t offset);
        OpCode getPoppingJumpReplacement(const OpCode& opcode);
        std::vector<uint8_t> createReplacement(const OpCode& originalOpcode, uint32_t jumpDistance);
    };

}