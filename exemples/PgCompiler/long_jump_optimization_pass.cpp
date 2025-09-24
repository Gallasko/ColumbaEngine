#include "long_jump_optimization_pass.h"
#include "bytecode_rewriter.h"
#include "logger.h"

namespace pg {

    bool LongJumpOptimizationPass::runPass(Chunk& chunk, BytecodeRewriter* rewriter) {
        if (chunk.code.empty()) {
            return false;
        }
        
        if (!rewriter) {
            LOG_ERROR("LongJumpOptimization", "No rewriter provided");
            return false;
        }
        
        LOG_INFO("LongJumpOptimization", "Starting long jump optimization pass");
        
        // Find all long jumps that can be optimized to short jumps
        auto candidates = findOptimizableJumps(chunk);
        
        if (candidates.empty()) {
            LOG_INFO("LongJumpOptimization", "No long jumps can be optimized to short jumps");
            return false;
        }
        
        LOG_INFO("LongJumpOptimization", "Found " << candidates.size() << " long jumps that can be optimized");
        
        bool changed = false;
        
        // Process candidates in reverse order to maintain correct indices
        for (auto it = candidates.rbegin(); it != candidates.rend(); ++it) {
            const auto& jump = *it;
            
            // Get the short jump equivalent
            OpCode shortOpcode = getShortJumpEquivalent(jump.opcode);
            if (shortOpcode == jump.opcode) {
                LOG_WARNING("LongJumpOptimization", "No short equivalent found for opcode at offset " << jump.instructionOffset);
                continue;
            }
            
            // Use rewriter to replace long jump (5 bytes) with short jump (3 bytes)
            bool success = rewriter->rewriteAt(chunk, jump.instructionOffset, 5, {shortOpcode});
            
            if (success) {
                LOG_INFO("LongJumpOptimization", "Optimized long jump at offset " << jump.instructionOffset);
                changed = true;
            } else {
                LOG_WARNING("LongJumpOptimization", "Failed to optimize jump at offset " << jump.instructionOffset);
            }
        }
        
        if (changed) {
            LOG_INFO("LongJumpOptimization", "Successfully optimized " << candidates.size() << " long jumps");
        }
        
        return changed;
    }
    
    std::vector<JumpInfo> LongJumpOptimizationPass::findOptimizableJumps(const Chunk& chunk) {
        std::vector<JumpInfo> candidates;
        
        for (size_t i = 0; i < chunk.code.size();) {
            OpCode opcode = static_cast<OpCode>(chunk.code[i]);
            
            if (isLongJumpInstruction(opcode)) {
                uint32_t jumpDistance = extractLongJumpOffset(chunk, i);
                
                // Check if the jump distance fits in a 16-bit short jump
                if (jumpDistance <= MAX_SHORT_JUMP_DISTANCE) {
                    size_t targetOffset = 0;
                    
                    if (opcode == OpCode::OP_Long_Loop) {
                        // Loop jumps backward
                        if (jumpDistance <= i + 5) {
                            targetOffset = (i + 5) - jumpDistance;
                        } else {
                            targetOffset = 0; // Invalid backward jump
                        }
                    } else {
                        // Forward jump
                        targetOffset = i + 5 + jumpDistance;
                    }
                    
                    JumpInfo jumpInfo(i, opcode, jumpDistance, targetOffset);
                    jumpInfo.canOptimize = true;
                    candidates.push_back(jumpInfo);
                    
                    LOG_INFO("LongJumpOptimization", "Long jump at offset " << i << 
                             " with distance " << jumpDistance << " can be optimized");
                }
            }
            
            i += getInstructionSize(opcode);
        }
        
        return candidates;
    }
    
    uint32_t LongJumpOptimizationPass::extractLongJumpOffset(const Chunk& chunk, size_t offset) {
        if (offset + 4 >= chunk.code.size()) {
            return 0;
        }
        
        return (static_cast<uint32_t>(chunk.code[offset + 1]) << 24) |
               (static_cast<uint32_t>(chunk.code[offset + 2]) << 16) |
               (static_cast<uint32_t>(chunk.code[offset + 3]) << 8) |
               static_cast<uint32_t>(chunk.code[offset + 4]);
    }
    
    OpCode LongJumpOptimizationPass::getShortJumpEquivalent(OpCode longJump) {
        switch (longJump) {
            case OpCode::OP_Long_Jump:
                return OpCode::OP_Jump;
            case OpCode::OP_Long_Jump_If_False:
                return OpCode::OP_Jump_If_False;
            case OpCode::OP_Long_Loop:
                return OpCode::OP_Loop;
            default:
                return longJump; // No equivalent found
        }
    }
    
    bool LongJumpOptimizationPass::isLongJumpInstruction(OpCode opcode) {
        return opcode == OpCode::OP_Long_Jump ||
               opcode == OpCode::OP_Long_Jump_If_False ||
               opcode == OpCode::OP_Long_Loop;
    }
    
    size_t LongJumpOptimizationPass::getInstructionSize(OpCode opcode) {
        switch (opcode) {
            // Long instructions: opcode + 4 bytes operand
            case OpCode::OP_Long_Jump:
            case OpCode::OP_Long_Jump_If_False:
            case OpCode::OP_Long_Loop:
            case OpCode::OP_LongConstant:
                return 5;
                
            // Short instructions: opcode + 2 bytes operand
            case OpCode::OP_Jump:
            case OpCode::OP_Jump_If_False:
            case OpCode::OP_Loop:
            case OpCode::OP_Constant:
                return 3;
                
            // Single byte instructions
            default:
                return 1;
        }
    }

}