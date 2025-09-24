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
        int totalBytesRemoved = 0;
        
        // Separate forward and backward jumps
        std::vector<JumpInfo> forwardJumps;
        std::vector<JumpInfo> backwardJumps;
        
        for (const auto& jump : jumps) {
            if (jump.canOptimize) {
                if (jump.opcode == OpCode::OP_Long_Loop) {
                    backwardJumps.push_back(jump);
                } else {
                    forwardJumps.push_back(jump);
                }
            }
        }
        
        // Pass 1: Optimize forward jumps (in reverse order to maintain offsets)
        LOG_INFO("LongJumpOptimization", "Pass 1: Optimizing " << forwardJumps.size() << " forward jumps");
        for (auto it = forwardJumps.rbegin(); it != forwardJumps.rend(); ++it) {
            if (optimizeSingleJump(chunk, *it)) {
                totalBytesRemoved += 2; // Each optimization saves 2 bytes (5->3)
                changed = true;
            }
        }
        
        // Pass 2: Optimize backward jumps with position adjustment  
        LOG_INFO("LongJumpOptimization", "Pass 2: Optimizing " << backwardJumps.size() << 
                 " backward jumps (total offset: -" << totalBytesRemoved << " bytes)");
        
        int pass2BytesRemoved = 0; // Track bytes removed in pass 2
        
        for (auto it = backwardJumps.rbegin(); it != backwardJumps.rend(); ++it) {
            const auto& originalJump = *it;
            
            // Adjust the jump position based on bytes removed in pass 1
            JumpInfo adjustedJump = originalJump;
            if (adjustedJump.instructionOffset >= totalBytesRemoved) {
                adjustedJump.instructionOffset -= totalBytesRemoved;
            }
            
            // Calculate new distance to original target
            size_t originalEndPos = originalJump.instructionOffset + 5; // Original long jump end
            size_t originalTarget = originalEndPos - originalJump.jumpDistance;
            size_t newEndPos = adjustedJump.instructionOffset + 3; // New short jump end
            
            if (newEndPos > originalTarget) {
                uint32_t newDistance = newEndPos - originalTarget;
                if (newDistance <= 65535) { // Still fits in short jump
                    adjustedJump.jumpDistance = newDistance;
                    
                    if (optimizeSingleJump(chunk, adjustedJump)) {
                        pass2BytesRemoved += 2;
                        totalBytesRemoved += 2;
                        changed = true;
                    }
                }
            }
        }
        
        // Pass 3: Update forward jump distances affected by pass 2 optimizations
        if (pass2BytesRemoved > 0) {
            LOG_INFO("LongJumpOptimization", "Pass 3: Updating forward jump distances affected by " << 
                     pass2BytesRemoved << " bytes removed in pass 2");
            updateForwardJumpDistances(chunk, pass2BytesRemoved);
        }
        
        LOG_INFO("LongJumpOptimization", "Total bytes saved: " << totalBytesRemoved);
        return changed;
    }
    
    bool LongJumpOptimizationPass::optimizeSingleJump(Chunk& chunk, const JumpInfo& jump) {
        // Convert long jump to short jump
        OpCode shortOpcode = getShortJumpEquivalent(jump.opcode);
        if (shortOpcode == jump.opcode) {
            LOG_ERROR("LongJumpOptimization", "Failed to find short jump equivalent for opcode");
            return false;
        }
        
        size_t oldSize = getInstructionSize(jump.opcode); // 5 bytes
        size_t newSize = getInstructionSize(shortOpcode);  // 3 bytes
        
        // Replace the instruction
        chunk.code[jump.instructionOffset] = static_cast<uint8_t>(shortOpcode);
        
        // Recalculate distance based on current chunk state and target position
        uint32_t newDistance;
        size_t newInstructionEnd = jump.instructionOffset + newSize; // 3 bytes for short jump
        
        if (jump.opcode == OpCode::OP_Long_Loop) {
            // For backward jumps: target is jump.targetOffset, distance is how far back to jump
            if (newInstructionEnd > jump.targetOffset) {
                newDistance = newInstructionEnd - jump.targetOffset;
            } else {
                LOG_ERROR("LongJumpOptimization", "Invalid backward jump target");
                // Revert the opcode change
                chunk.code[jump.instructionOffset] = static_cast<uint8_t>(jump.opcode);
                return false;
            }
        } else {
            // For forward jumps: target is jump.targetOffset, distance is how far forward to jump
            if (jump.targetOffset >= newInstructionEnd) {
                newDistance = jump.targetOffset - newInstructionEnd;
            } else {
                LOG_ERROR("LongJumpOptimization", "Invalid forward jump target");
                // Revert the opcode change  
                chunk.code[jump.instructionOffset] = static_cast<uint8_t>(jump.opcode);
                return false;
            }
        }
        
        if (newDistance > 65535) {
            LOG_WARNING("LongJumpOptimization", "Jump distance too large for short jump, reverting");
            // Revert the opcode change
            chunk.code[jump.instructionOffset] = static_cast<uint8_t>(jump.opcode);
            return false;
        }
        
        uint16_t shortDistance = static_cast<uint16_t>(newDistance);
        writeShortJumpOffset(chunk, jump.instructionOffset, shortDistance);
        
        // Remove the extra 2 bytes
        chunk.code.erase(chunk.code.begin() + jump.instructionOffset + newSize,
                        chunk.code.begin() + jump.instructionOffset + oldSize);
        
        // Also remove corresponding line numbers
        if (jump.instructionOffset + oldSize <= chunk.lines.size()) {
            chunk.lines.erase(chunk.lines.begin() + jump.instructionOffset + newSize,
                             chunk.lines.begin() + jump.instructionOffset + oldSize);
        }
        
        LOG_INFO("LongJumpOptimization", "Optimized " << 
                (jump.opcode == OpCode::OP_Long_Loop ? "backward" : "forward") <<
                " jump at offset " << jump.instructionOffset << 
                " (distance " << newDistance << " -> " << shortDistance << ")");
        
        return true;
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

    void LongJumpOptimizationPass::updateForwardJumpDistances(Chunk& chunk, int bytesRemovedFromBackwardOptimizations) {
        // After optimizations, ALL forward jumps may have incorrect targets
        // because optimizations have shifted bytecode positions
        
        for (size_t i = 0; i < chunk.code.size();) {
            OpCode opcode = static_cast<OpCode>(chunk.code[i]);
            
            // Handle both short and long forward jumps (but not backward loops)
            if ((isShortJumpInstruction(opcode) && opcode != OpCode::OP_Loop) || 
                (isLongJumpInstruction(opcode) && opcode != OpCode::OP_Long_Loop)) {
                
                uint32_t currentDistance;
                size_t instructionEnd;
                
                if (isLongJumpInstruction(opcode)) {
                    currentDistance = extractLongJumpOffset(chunk, i);
                    instructionEnd = i + 5;
                } else {
                    currentDistance = extractShortJumpOffset(chunk, i);
                    instructionEnd = i + 3;
                }
                
                size_t currentTarget = instructionEnd + currentDistance;
                
                // Check if target is valid after all optimizations
                if (currentTarget >= chunk.code.size()) {
                    LOG_WARNING("LongJumpOptimization", "Forward jump at offset " << i << 
                               " has invalid target " << currentTarget << " (chunk size: " << chunk.code.size() << ")");
                    
                    // For jumps that go beyond the chunk, point them to the end (for exit behavior)
                    size_t newTarget = chunk.code.size();
                    if (newTarget > instructionEnd) {
                        uint32_t newDistance = newTarget - instructionEnd;
                        
                        if (isLongJumpInstruction(opcode)) {
                            writeLongJumpOffset(chunk, i, newDistance);
                            LOG_INFO("LongJumpOptimization", "Adjusted long jump distance from " << 
                                    currentDistance << " to " << newDistance);
                        } else {
                            if (newDistance <= 65535) {
                                uint16_t shortDistance = static_cast<uint16_t>(newDistance);
                                writeShortJumpOffset(chunk, i, shortDistance);
                                LOG_INFO("LongJumpOptimization", "Adjusted short jump distance from " << 
                                        currentDistance << " to " << shortDistance);
                            } else {
                                LOG_ERROR("LongJumpOptimization", "Cannot fix short jump - new distance too large: " << newDistance);
                            }
                        }
                    } else {
                        LOG_ERROR("LongJumpOptimization", "Cannot fix jump - target would be negative");
                    }
                } else {
                    LOG_INFO("LongJumpOptimization", "Forward jump at offset " << i << 
                             " verified (distance=" << currentDistance << ", target=" << currentTarget << ")");
                }
            }
            
            i += getInstructionSize(opcode);
        }
        
        LOG_INFO("LongJumpOptimization", "Forward jump distance update completed");
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

    void LongJumpOptimizationPass::writeLongJumpOffset(Chunk& chunk, size_t offset, uint32_t jumpOffset) {
        if (offset + 4 < chunk.code.size()) {
            chunk.code[offset + 1] = (jumpOffset >> 24) & 0xFF;
            chunk.code[offset + 2] = (jumpOffset >> 16) & 0xFF;
            chunk.code[offset + 3] = (jumpOffset >> 8) & 0xFF;
            chunk.code[offset + 4] = jumpOffset & 0xFF;
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