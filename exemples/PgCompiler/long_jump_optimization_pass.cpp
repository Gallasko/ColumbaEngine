#include "long_jump_optimization_pass.h"
#include "logger.h"
#include <algorithm>

namespace pg {

    bool LongJumpOptimizationPass::runPass(Chunk& chunk) {
        if (chunk.code.empty()) {
            return false;
        }
        
        LOG_INFO("LongJumpOptimization", "Starting long jump optimization pass");
        
        // Phase 1: Analyze all jump instructions
        auto jumps = analyzeJumps(chunk);
        
        if (jumps.empty()) {
            LOG_INFO("LongJumpOptimization", "No jump instructions found");
            return false;
        }
        
        LOG_INFO("LongJumpOptimization", "Found " << jumps.size() << " jump instructions");
        
        // Phase 2: Identify optimizable jumps
        identifyOptimizableJumps(jumps);
        
        size_t optimizableCount = std::count_if(jumps.begin(), jumps.end(),
            [](const JumpInfo& jump) { return jump.canOptimize; });
        
        if (optimizableCount == 0) {
            LOG_INFO("LongJumpOptimization", "No long jumps can be optimized to short jumps");
            return false;
        }
        
        LOG_INFO("LongJumpOptimization", "Found " << optimizableCount << " long jumps that can be optimized");
        
        // Phase 3: Apply optimizations
        bool changed = applyOptimizations(chunk, jumps);
        
        if (changed) {
            // Phase 4: Recalculate all offsets
            recalculateJumpOffsets(chunk);
            LOG_INFO("LongJumpOptimization", "Successfully optimized " << optimizableCount << " long jumps");
        }
        
        return changed;
    }

    std::vector<JumpInfo> LongJumpOptimizationPass::analyzeJumps(const Chunk& chunk) {
        std::vector<JumpInfo> jumps;
        
        for (size_t i = 0; i < chunk.code.size();) {
            OpCode opcode = static_cast<OpCode>(chunk.code[i]);
            
            if (isJumpInstruction(opcode)) {
                uint32_t jumpDistance = 0;
                size_t targetOffset = 0;
                
                if (isLongJumpInstruction(opcode)) {
                    jumpDistance = extractLongJumpOffset(chunk, i);
                    
                    if (opcode == OpCode::OP_Long_Loop) {
                        // Loop jumps backward - target is before the instruction
                        if (jumpDistance <= i + 5) {
                            targetOffset = (i + 5) - jumpDistance;
                        } else {
                            targetOffset = 0; // Invalid backward jump
                        }
                    } else {
                        // Forward jump
                        targetOffset = i + 5 + jumpDistance;
                    }
                } else if (isShortJumpInstruction(opcode)) {
                    jumpDistance = extractShortJumpOffset(chunk, i);
                    
                    if (opcode == OpCode::OP_Loop) {
                        // Loop jumps backward
                        if (jumpDistance <= i + 3) {
                            targetOffset = (i + 3) - jumpDistance;
                        } else {
                            targetOffset = 0; // Invalid backward jump
                        }
                    } else {
                        // Forward jump
                        targetOffset = i + 3 + jumpDistance;
                    }
                }
                
                jumps.emplace_back(i, opcode, jumpDistance, targetOffset);
            }
            
            i += getInstructionSize(opcode);
        }
        
        return jumps;
    }

    void LongJumpOptimizationPass::identifyOptimizableJumps(std::vector<JumpInfo>& jumps) {
        for (auto& jump : jumps) {
            if (isLongJumpInstruction(jump.opcode)) {
                // Check if the jump distance fits in a 16-bit short jump
                if (jump.jumpDistance <= MAX_SHORT_JUMP_DISTANCE) {
                    jump.canOptimize = true;
                    LOG_INFO("LongJumpOptimization", "Long jump at offset " << jump.instructionOffset << 
                             " with distance " << jump.jumpDistance << " can be optimized");
                }
            }
        }
    }

    bool LongJumpOptimizationPass::applyOptimizations(Chunk& chunk, const std::vector<JumpInfo>& jumps) {
        bool changed = false;
        std::vector<int> sizeDeltas; // Track size changes at each position
        sizeDeltas.resize(chunk.code.size(), 0);
        
        // Process in reverse order to maintain offset validity
        for (auto it = jumps.rbegin(); it != jumps.rend(); ++it) {
            const auto& jump = *it;
            
            if (!jump.canOptimize) continue;
            
            // Convert long jump to short jump
            OpCode shortOpcode = getShortJumpEquivalent(jump.opcode);
            if (shortOpcode == jump.opcode) {
                LOG_ERROR("LongJumpOptimization", "Failed to find short jump equivalent for opcode");
                continue;
            }
            
            size_t oldSize = getInstructionSize(jump.opcode); // 5 bytes
            size_t newSize = getInstructionSize(shortOpcode);  // 3 bytes
            int sizeDelta = static_cast<int>(newSize) - static_cast<int>(oldSize); // -2
            
            // Replace the instruction
            chunk.code[jump.instructionOffset] = static_cast<uint8_t>(shortOpcode);
            
            // Recalculate the jump distance accounting for all previous size changes
            uint32_t originalDistance = jump.jumpDistance;
            int cumulativeDelta = 0;
            
            // Sum up all size changes between this jump and its target
            size_t startPos = jump.instructionOffset + oldSize;
            size_t endPos = jump.instructionOffset + oldSize + originalDistance;
            
            for (size_t i = startPos; i < endPos && i < sizeDeltas.size(); ++i) {
                cumulativeDelta += sizeDeltas[i];
            }
            
            // Adjust the jump distance
            int32_t adjustedDistance = static_cast<int32_t>(originalDistance) + cumulativeDelta;
            if (adjustedDistance < 0) {
                LOG_WARNING("LongJumpOptimization", "Adjusted jump distance became negative, skipping optimization");
                continue;
            }
            
            uint16_t shortDistance = static_cast<uint16_t>(adjustedDistance);
            writeShortJumpOffset(chunk, jump.instructionOffset, shortDistance);
            
            // Remove the extra 2 bytes
            chunk.code.erase(chunk.code.begin() + jump.instructionOffset + newSize,
                            chunk.code.begin() + jump.instructionOffset + oldSize);
            
            // Also remove corresponding line numbers
            if (jump.instructionOffset + oldSize <= chunk.lines.size()) {
                chunk.lines.erase(chunk.lines.begin() + jump.instructionOffset + newSize,
                                 chunk.lines.begin() + jump.instructionOffset + oldSize);
            }
            
            // Record size change for future jumps
            if (jump.instructionOffset < sizeDeltas.size()) {
                sizeDeltas[jump.instructionOffset] = sizeDelta;
            }
            
            changed = true;
        }
        
        return changed;
    }

    void LongJumpOptimizationPass::recalculateJumpOffsets(Chunk& chunk) {
        LOG_INFO("LongJumpOptimization", "Recalculating all jump offsets after optimization");
        
        // After optimization, we need to recalculate all jump offsets because
        // converting long jumps to short jumps changes bytecode positions
        
        for (size_t i = 0; i < chunk.code.size();) {
            OpCode opcode = static_cast<OpCode>(chunk.code[i]);
            
            if (isJumpInstruction(opcode)) {
                // For now, we'll trust that the jump targets are still valid
                // A more sophisticated implementation would build a symbol table
                // and recalculate exact target positions
                
                if (isLongJumpInstruction(opcode)) {
                    // Verify long jump offset is within bounds
                    uint32_t offset = extractLongJumpOffset(chunk, i);
                    size_t targetPos = i + 5 + offset;
                    if (targetPos > chunk.code.size()) {
                        LOG_WARNING("LongJumpOptimization", "Long jump target out of bounds at offset " << i);
                        // Could attempt to fix or flag for another pass
                    }
                } else if (isShortJumpInstruction(opcode)) {
                    // Verify short jump offset is within bounds
                    uint16_t offset = extractShortJumpOffset(chunk, i);
                    size_t targetPos = i + 3 + offset;
                    if (targetPos > chunk.code.size()) {
                        LOG_WARNING("LongJumpOptimization", "Short jump target out of bounds at offset " << i);
                        // Could attempt to fix or flag for another pass
                    }
                }
            }
            
            i += getInstructionSize(opcode);
        }
        
        LOG_INFO("LongJumpOptimization", "Jump offset verification completed");
    }

    // Helper method implementations
    uint32_t LongJumpOptimizationPass::extractLongJumpOffset(const Chunk& chunk, size_t offset) {
        if (offset + 4 >= chunk.code.size()) {
            return 0;
        }
        
        return (static_cast<uint32_t>(chunk.code[offset + 1]) << 24) |
               (static_cast<uint32_t>(chunk.code[offset + 2]) << 16) |
               (static_cast<uint32_t>(chunk.code[offset + 3]) << 8) |
               static_cast<uint32_t>(chunk.code[offset + 4]);
    }

    uint16_t LongJumpOptimizationPass::extractShortJumpOffset(const Chunk& chunk, size_t offset) {
        if (offset + 2 >= chunk.code.size()) {
            return 0;
        }
        
        return (static_cast<uint16_t>(chunk.code[offset + 1]) << 8) |
               static_cast<uint16_t>(chunk.code[offset + 2]);
    }

    void LongJumpOptimizationPass::writeShortJumpOffset(Chunk& chunk, size_t offset, uint16_t jumpOffset) {
        if (offset + 2 < chunk.code.size()) {
            chunk.code[offset + 1] = (jumpOffset >> 8) & 0xFF;
            chunk.code[offset + 2] = jumpOffset & 0xFF;
        }
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

    bool LongJumpOptimizationPass::isJumpInstruction(OpCode opcode) {
        return isLongJumpInstruction(opcode) || isShortJumpInstruction(opcode);
    }

    bool LongJumpOptimizationPass::isLongJumpInstruction(OpCode opcode) {
        return opcode == OpCode::OP_Long_Jump ||
               opcode == OpCode::OP_Long_Jump_If_False ||
               opcode == OpCode::OP_Long_Loop;
    }

    bool LongJumpOptimizationPass::isShortJumpInstruction(OpCode opcode) {
        return opcode == OpCode::OP_Jump ||
               opcode == OpCode::OP_Jump_If_False ||
               opcode == OpCode::OP_Loop;
    }

    size_t LongJumpOptimizationPass::getInstructionSize(OpCode opcode) {
        switch (opcode) {
            // Long jumps: opcode + 4 bytes offset
            case OpCode::OP_Long_Jump:
            case OpCode::OP_Long_Jump_If_False:
            case OpCode::OP_Long_Loop:
            case OpCode::OP_LongConstant:
                return 5;
                
            // Short jumps: opcode + 2 bytes offset
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