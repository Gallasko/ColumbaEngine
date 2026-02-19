#include "stdafx.h"

#include "popping_jump_pass.h"
#include "../bytecode_rewriter.h"
#include "logger.h"

#include "../compiler_debug.h"

namespace pg
{
    bool PoppingJumpPass::runPass(Chunk& chunk, BytecodeRewriter* rewriter)
    {
        if (chunk.code.empty())
        {
            return false;
        }

        if (not rewriter)
        {
            LOG_ERROR("LongJumpOptimization", "No rewriter provided");
            return false;
        }

        LOG_MILE("PoppingJumpPass", "Starting popping jump optimization pass");

        // Find optimizable jumps in the current state of the chunk
        auto candidate = findFirstOptimizableJumps(chunk);

        if (not candidate.has_value())
        {
            LOG_MILE("PoppingJumpPass", "No more popping jumps can be optimized");
            return false;
        }

        // Optimize the first candidate (this maintains simpler logic)
        const auto& jump = candidate.value();

        // Get the short jump equivalent
        OpCode poppingOpcode = getPoppingJumpReplacement(jump.opcode);

        if (poppingOpcode == jump.opcode)
        {
            LOG_WARNING("PoppingJumpPass", "No popping equivalent found for opcode at offset " << jump.instructionOffset);
            return false;
        }

        // Strategy: We need to do two separate rewrites because the pops are not adjacent
        // 1. Replace the jump + following pop with a popping jump
        // 2. Remove the target pop
        //
        // The rewriter will automatically adjust jump offsets after each rewrite,
        // but we need to ensure the jump distance accounts for BOTH removed pops.

        // After removing the first pop, the target will shift back by 1 byte
        // After removing the target pop, we want to land to the instruction after the pop
        // So we need to increase the jump distance by 1
        uint32_t adjustedJumpDistance = jump.jumpDistance + 1;

        // First, replace the jump instruction with the popping jump and remove the following pop
        bool success = rewriter->rewriteAtRaw(chunk, jump.instructionOffset, pg::getInstructionSize(jump.opcode) + 1, createReplacement(jump.opcode, adjustedJumpDistance));

        // Then, remove the target pop instruction
        success &= rewriter->removeInstructions(chunk, jump.targetOffset - 1, 1);

        if (success)
        {
            LOG_MILE("PoppingJumpPass", "Optimized popping jump at offset " << jump.instructionOffset
                        << " (distance: " << jump.jumpDistance << ")");
            return true;
        }
        else
        {
            LOG_WARNING("PoppingJumpPass", "Failed to optimize jump at offset " << jump.instructionOffset);
        }

        return false;
    }

    std::optional<PoppingJumpPass::JumpInfo> PoppingJumpPass::findFirstOptimizableJumps(const Chunk& chunk)
    {
        for (size_t i = 0; i < chunk.code.size();)
        {
            OpCode opcode = static_cast<OpCode>(chunk.code[i]);

            if (opcode == OpCode::OP_Long_Jump_If_False or opcode == OpCode::OP_Jump_If_False)
            {
                uint32_t jumpDistance = extractJumpOffset(opcode, chunk, i);

                auto nextInstroffset = i + pg::getInstructionSize(opcode);

                auto nextInstr = static_cast<OpCode>(chunk.code[nextInstroffset]);
                auto jumpInstr = static_cast<OpCode>(chunk.code[nextInstroffset + jumpDistance]);

                LOG_MILE("PoppingJumpPass", "Found jump at offset " << i << " with distance " << jumpDistance <<
                         ", next instruction: " << opcodeToString(nextInstr) << " at " << nextInstroffset <<
                         ", jump target instruction: " << opcodeToString(jumpInstr) << " at " << nextInstroffset + jumpDistance);

                if (nextInstr == OpCode::OP_Pop and jumpInstr == OpCode::OP_Pop)
                {
                    JumpInfo jumpInfo(i, opcode, jumpDistance, nextInstroffset + jumpDistance);
                    jumpInfo.canOptimize = true;

                    LOG_MILE("PoppingJumpPass", "Popping jump at offset " << i <<
                             " with distance " << jumpDistance << " can be optimized");

                    return jumpInfo;
                }
            }

            i += pg::getInstructionSize(opcode);
        }

        return std::nullopt;
    }

    uint32_t PoppingJumpPass::extractLongJumpOffset(const Chunk& chunk, size_t offset)
    {
        if (offset + 4 >= chunk.code.size())
        {
            return 0;
        }

        return (static_cast<uint32_t>(chunk.code[offset + 1]) << 24) |
               (static_cast<uint32_t>(chunk.code[offset + 2]) << 16) |
               (static_cast<uint32_t>(chunk.code[offset + 3]) << 8)  |
               static_cast<uint32_t>(chunk.code[offset + 4]);
    }

    uint16_t PoppingJumpPass::extractShortJumpOffset(const Chunk& chunk, size_t offset)
    {
        if (offset + 2 >= chunk.code.size())
        {
            return 0;
        }

        return (static_cast<uint16_t>(chunk.code[offset + 1]) << 8) |
               (static_cast<uint16_t>(chunk.code[offset + 2]));
    }

    uint32_t PoppingJumpPass::extractJumpOffset(const OpCode& opcode, const Chunk& chunk, size_t offset)
    {
        return opcode == OpCode::OP_Long_Jump_If_False ? extractLongJumpOffset(chunk, offset) : extractShortJumpOffset(chunk, offset);
    }

    OpCode PoppingJumpPass::getPoppingJumpReplacement(const OpCode& opcode)
    {
        switch (opcode)
        {
            case OpCode::OP_Long_Jump_If_False:
                return OpCode::OP_Long_Jump_If_False_Popping;
            case OpCode::OP_Jump_If_False:
                return OpCode::OP_Jump_If_False_Popping;
            default:
                return opcode; // No replacement
        }
    }

    std::vector<uint8_t> PoppingJumpPass::createReplacement(const OpCode& originalOpcode, uint32_t jump)
    {
        OpCode poppingOpcode = getPoppingJumpReplacement(originalOpcode);

        jump--; // Adjust jump distance to account for the removed pop instruction

        if (originalOpcode == OpCode::OP_Long_Jump_If_False)
        {
            // Long jump with 4 byte offset
            return {static_cast<uint8_t>(poppingOpcode),
                    static_cast<uint8_t>((jump >> 24) & 0xFF),
                    static_cast<uint8_t>((jump >> 16) & 0xFF),
                    static_cast<uint8_t>((jump >> 8)  & 0xFF),
                    static_cast<uint8_t>( jump        & 0xFF)};
        }
        else
        {
            // Short jump with 2 byte offset
            return {static_cast<uint8_t>(poppingOpcode),
                    static_cast<uint8_t>((jump >> 8)  & 0xFF),
                    static_cast<uint8_t>( jump        & 0xFF)};
        }

    }
}