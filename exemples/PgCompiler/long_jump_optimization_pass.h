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
        
        bool runPass(Chunk& chunk) override;
        
        bool changesSize() const override { return true; }
        
        bool requiresMultiplePasses() const override { return true; }
        
    private:
        // Phase 1: Analyze all jump instructions
        std::vector<JumpInfo> analyzeJumps(const Chunk& chunk);
        
        // Phase 2: Determine which long jumps can be converted to short jumps
        void identifyOptimizableJumps(std::vector<JumpInfo>& jumps);
        
        // Phase 3: Apply optimizations (convert long jumps to short jumps)
        bool applyOptimizations(Chunk& chunk, const std::vector<JumpInfo>& jumps);
        
        // Phase 4: Recalculate all jump offsets after size changes
        void recalculateJumpOffsets(Chunk& chunk);
        
        // Helper methods
        uint32_t extractLongJumpOffset(const Chunk& chunk, size_t offset);
        uint16_t extractShortJumpOffset(const Chunk& chunk, size_t offset);
        
        void writeLongJumpOffset(Chunk& chunk, size_t offset, uint32_t jumpOffset);
        void writeShortJumpOffset(Chunk& chunk, size_t offset, uint16_t jumpOffset);
        
        OpCode getLongJumpEquivalent(OpCode shortJump);
        OpCode getShortJumpEquivalent(OpCode longJump);
        
        bool isJumpInstruction(OpCode opcode);
        bool isLongJumpInstruction(OpCode opcode);
        bool isShortJumpInstruction(OpCode opcode);
        
        size_t getInstructionSize(OpCode opcode);
        
        // Constants
        static constexpr uint32_t MAX_SHORT_JUMP_DISTANCE = 65535; // 16-bit max
    };

}