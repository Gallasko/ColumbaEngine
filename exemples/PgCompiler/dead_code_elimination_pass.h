#pragma once

#include "bytecode_pass.h"
#include "chunk.h"
#include <vector>
#include <unordered_set>
#include <cstdint>

namespace pg {

    struct BasicBlock {
        size_t startOffset;
        size_t endOffset;
        std::vector<size_t> successors;  // Offsets of blocks this can jump to
        std::vector<size_t> predecessors; // Offsets of blocks that can jump here
        bool isReachable;

        BasicBlock(size_t start, size_t end) 
            : startOffset(start), endOffset(end), isReachable(false) {}
    };

    class DeadCodeEliminationPass : public BytecodePass {
    public:
        std::string getName() const override { return "DeadCodeElimination"; }
        
        bool runPass(Chunk& chunk, BytecodeRewriter* rewriter = nullptr) override;
        
        bool changesSize() const override { return true; }
        
        bool requiresMultiplePasses() const override { return false; }
        
    private:
        // Build basic blocks from bytecode
        std::vector<BasicBlock> buildBasicBlocks(const Chunk& chunk);
        
        // Mark reachable blocks starting from entry point
        void markReachableBlocks(std::vector<BasicBlock>& blocks);
        
        // Find all unreachable code segments
        std::vector<std::pair<size_t, size_t>> findDeadCodeRanges(
            const Chunk& chunk, const std::vector<BasicBlock>& blocks);
        
        // Remove dead code ranges from chunk
        bool removeDeadCode(Chunk& chunk, 
                           const std::vector<std::pair<size_t, size_t>>& deadRanges,
                           BytecodeRewriter* rewriter);
        
        // Helper methods
        bool isJumpInstruction(OpCode opcode) const;
        bool isConditionalJump(OpCode opcode) const;
        bool isUnconditionalJump(OpCode opcode) const;
        bool isTerminatorInstruction(OpCode opcode) const;
        
        size_t getInstructionSize(const Chunk& chunk, size_t offset) const;
        size_t getJumpTarget(const Chunk& chunk, size_t offset) const;
        
        // Find block leaders (instructions that start basic blocks)
        std::unordered_set<size_t> findBlockLeaders(const Chunk& chunk);
    };

}