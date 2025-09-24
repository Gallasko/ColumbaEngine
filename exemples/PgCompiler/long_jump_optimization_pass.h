#pragma once

#include "bytecode_pass.h"
#include "chunk.h"
#include <vector>
#include <cstdint>

namespace pg {

    struct JumpInfo {
        size_t instructionOffset;
        OpCode opcode;
        uint32_t jumpDistance;
        size_t targetOffset;
        bool canOptimize;
        
        JumpInfo(size_t offset, OpCode op, uint32_t distance, size_t target)
            : instructionOffset(offset), opcode(op), jumpDistance(distance), 
              targetOffset(target), canOptimize(false) {}
    };

    class LongJumpOptimizationPass : public BytecodePass {
    public:
        std::string getName() const override { return "LongJumpOptimization"; }
        
        bool runPass(Chunk& chunk, BytecodeRewriter* rewriter = nullptr) override;
        
        bool changesSize() const override { return true; }
        
        bool requiresMultiplePasses() const override { return true; }
        
    private:
        // Find all long jump candidates that can be optimized to short jumps
        std::vector<JumpInfo> findOptimizableJumps(const Chunk& chunk);
        
        // Helper methods
        uint32_t extractLongJumpOffset(const Chunk& chunk, size_t offset);
        OpCode getShortJumpEquivalent(OpCode longJump);
        bool isLongJumpInstruction(OpCode opcode);
        size_t getInstructionSize(OpCode opcode);
        
        // Constants
        static constexpr uint32_t MAX_SHORT_JUMP_DISTANCE = 65535; // 16-bit max
    };

}