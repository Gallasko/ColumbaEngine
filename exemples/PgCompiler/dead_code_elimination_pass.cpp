#include "dead_code_elimination_pass.h"
#include "logger.h"
#include <algorithm>
#include <queue>

namespace pg {

    bool DeadCodeEliminationPass::runPass(Chunk& chunk, BytecodeRewriter* rewriter) {
        if (chunk.code.empty()) {
            return false;
        }

        if (!rewriter) {
            LOG_ERROR("DeadCodeEliminationPass", "No rewriter provided");
            return false;
        }

        LOG_INFO("DeadCodeEliminationPass", "Analyzing bytecode for dead code");

        // Step 1: Build basic blocks
        auto blocks = buildBasicBlocks(chunk);
        
        if (blocks.empty()) {
            return false;
        }

        // Step 2: Mark reachable blocks
        markReachableBlocks(blocks);

        // Step 3: Find dead code ranges
        auto deadRanges = findDeadCodeRanges(chunk, blocks);

        if (deadRanges.empty()) {
            LOG_INFO("DeadCodeEliminationPass", "No dead code found");
            return false;
        }

        // Step 4: Remove dead code
        LOG_INFO("DeadCodeEliminationPass", "Found " << deadRanges.size() << " dead code ranges");
        return removeDeadCode(chunk, deadRanges, rewriter);
    }

    std::vector<BasicBlock> DeadCodeEliminationPass::buildBasicBlocks(const Chunk& chunk) {
        // Find all block leaders (instructions that start basic blocks)
        auto leaders = findBlockLeaders(chunk);
        
        if (leaders.empty()) {
            return {};
        }

        // Convert leaders to sorted vector
        std::vector<size_t> sortedLeaders(leaders.begin(), leaders.end());
        std::sort(sortedLeaders.begin(), sortedLeaders.end());

        std::vector<BasicBlock> blocks;
        
        // Create basic blocks
        for (size_t i = 0; i < sortedLeaders.size(); ++i) {
            size_t start = sortedLeaders[i];
            size_t end = (i + 1 < sortedLeaders.size()) ? sortedLeaders[i + 1] - 1 : chunk.code.size() - 1;
            
            blocks.emplace_back(start, end);
        }

        // Build successor/predecessor relationships
        for (auto& block : blocks) {
            size_t offset = block.startOffset;
            
            // Scan through the block to find jumps
            while (offset <= block.endOffset && offset < chunk.code.size()) {
                OpCode opcode = static_cast<OpCode>(chunk.code[offset]);
                
                if (isJumpInstruction(opcode)) {
                    size_t target = getJumpTarget(chunk, offset);
                    
                    // Find which block contains this target
                    for (size_t i = 0; i < blocks.size(); ++i) {
                        if (target >= blocks[i].startOffset && target <= blocks[i].endOffset) {
                            block.successors.push_back(i);
                            blocks[i].predecessors.push_back(&block - &blocks[0]);
                            break;
                        }
                    }
                    
                    // For conditional jumps, also add fall-through successor
                    if (isConditionalJump(opcode)) {
                        size_t nextOffset = offset + getInstructionSize(opcode);
                        for (size_t i = 0; i < blocks.size(); ++i) {
                            if (nextOffset >= blocks[i].startOffset && nextOffset <= blocks[i].endOffset) {
                                block.successors.push_back(i);
                                blocks[i].predecessors.push_back(&block - &blocks[0]);
                                break;
                            }
                        }
                    }
                    
                    break; // Only process first jump in block
                }
                
                if (isTerminatorInstruction(opcode)) {
                    break; // No successors after terminator
                }
                
                offset += getInstructionSize(opcode);
            }
            
            // If no jump found and not a terminator, add fall-through successor
            if (block.successors.empty() && !isTerminatorInstruction(static_cast<OpCode>(chunk.code[block.endOffset]))) {
                size_t nextOffset = block.endOffset + 1;
                for (size_t i = 0; i < blocks.size(); ++i) {
                    if (nextOffset >= blocks[i].startOffset && nextOffset <= blocks[i].endOffset) {
                        block.successors.push_back(i);
                        blocks[i].predecessors.push_back(&block - &blocks[0]);
                        break;
                    }
                }
            }
        }

        return blocks;
    }

    void DeadCodeEliminationPass::markReachableBlocks(std::vector<BasicBlock>& blocks) {
        if (blocks.empty()) {
            return;
        }

        // Start from entry block (first block)
        std::queue<size_t> worklist;
        worklist.push(0);
        blocks[0].isReachable = true;

        while (!worklist.empty()) {
            size_t current = worklist.front();
            worklist.pop();

            // Mark all successors as reachable
            for (size_t successor : blocks[current].successors) {
                if (!blocks[successor].isReachable) {
                    blocks[successor].isReachable = true;
                    worklist.push(successor);
                }
            }
        }
    }

    std::vector<std::pair<size_t, size_t>> DeadCodeEliminationPass::findDeadCodeRanges(
        const Chunk& chunk, const std::vector<BasicBlock>& blocks) {
        
        std::vector<std::pair<size_t, size_t>> deadRanges;

        for (const auto& block : blocks) {
            if (!block.isReachable) {
                deadRanges.emplace_back(block.startOffset, block.endOffset);
            }
        }

        return deadRanges;
    }

    bool DeadCodeEliminationPass::removeDeadCode(Chunk& chunk,
                                                const std::vector<std::pair<size_t, size_t>>& deadRanges,
                                                BytecodeRewriter* rewriter) {
        if (deadRanges.empty()) {
            return false;
        }

        // Remove ranges in reverse order to maintain correct offsets
        auto sortedRanges = deadRanges;
        std::sort(sortedRanges.begin(), sortedRanges.end(), 
                 [](const auto& a, const auto& b) { return a.first > b.first; });

        bool modified = false;
        for (const auto& range : sortedRanges) {
            size_t count = range.second - range.first + 1;
            if (rewriter->removeInstructions(chunk, range.first, count)) {
                modified = true;
                LOG_INFO("DeadCodeEliminationPass", 
                        "Removed dead code range [" << range.first << "-" << range.second << "]");
            }
        }

        return modified;
    }

    std::unordered_set<size_t> DeadCodeEliminationPass::findBlockLeaders(const Chunk& chunk) {
        std::unordered_set<size_t> leaders;
        
        // First instruction is always a leader
        leaders.insert(0);

        size_t offset = 0;
        while (offset < chunk.code.size()) {
            OpCode opcode = static_cast<OpCode>(chunk.code[offset]);
            
            if (isJumpInstruction(opcode)) {
                // Target of jump is a leader
                size_t target = getJumpTarget(chunk, offset);
                if (target < chunk.code.size()) {
                    leaders.insert(target);
                }
                
                // Instruction after jump is a leader (for conditional jumps)
                if (isConditionalJump(opcode)) {
                    size_t nextOffset = offset + getInstructionSize(opcode);
                    if (nextOffset < chunk.code.size()) {
                        leaders.insert(nextOffset);
                    }
                }
            }
            
            offset += getInstructionSize(opcode);
        }

        return leaders;
    }

    bool DeadCodeEliminationPass::isJumpInstruction(OpCode opcode) const {
        return opcode == OpCode::OP_Jump || opcode == OpCode::OP_Long_Jump ||
               opcode == OpCode::OP_Jump_If_False || opcode == OpCode::OP_Long_Jump_If_False ||
               opcode == OpCode::OP_Loop || opcode == OpCode::OP_Long_Loop;
    }

    bool DeadCodeEliminationPass::isConditionalJump(OpCode opcode) const {
        return opcode == OpCode::OP_Jump_If_False || opcode == OpCode::OP_Long_Jump_If_False;
    }

    bool DeadCodeEliminationPass::isUnconditionalJump(OpCode opcode) const {
        return opcode == OpCode::OP_Jump || opcode == OpCode::OP_Long_Jump ||
               opcode == OpCode::OP_Loop || opcode == OpCode::OP_Long_Loop;
    }

    bool DeadCodeEliminationPass::isTerminatorInstruction(OpCode opcode) const {
        return opcode == OpCode::OP_Return || isUnconditionalJump(opcode);
    }

    size_t DeadCodeEliminationPass::getInstructionSize(const Chunk& chunk, size_t offset) const {
        if (offset >= chunk.code.size()) {
            return 0;
        }
        return pg::getInstructionSize(static_cast<OpCode>(chunk.code[offset]));
    }

    size_t DeadCodeEliminationPass::getJumpTarget(const Chunk& chunk, size_t offset) const {
        if (offset >= chunk.code.size()) {
            return 0;
        }

        OpCode opcode = static_cast<OpCode>(chunk.code[offset]);
        
        switch (opcode) {
            case OpCode::OP_Jump_If_False:
            case OpCode::OP_Jump:
            case OpCode::OP_Loop: {
                if (offset + 2 < chunk.code.size()) {
                    uint16_t jumpOffset = static_cast<uint16_t>(chunk.code[offset + 1]) |
                                        (static_cast<uint16_t>(chunk.code[offset + 2]) << 8);
                    return offset + 3 + jumpOffset;
                }
                break;
            }
            
            case OpCode::OP_Long_Jump_If_False:
            case OpCode::OP_Long_Jump:
            case OpCode::OP_Long_Loop: {
                if (offset + 4 < chunk.code.size()) {
                    uint32_t jumpOffset = static_cast<uint32_t>(chunk.code[offset + 1]) |
                                        (static_cast<uint32_t>(chunk.code[offset + 2]) << 8) |
                                        (static_cast<uint32_t>(chunk.code[offset + 3]) << 16) |
                                        (static_cast<uint32_t>(chunk.code[offset + 4]) << 24);
                    return offset + 5 + jumpOffset;
                }
                break;
            }
            
            default:
                break;
        }
        
        return 0;
    }

}