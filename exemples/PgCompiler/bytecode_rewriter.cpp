#include "bytecode_rewriter.h"
#include "logger.h"
#include <algorithm>

namespace pg {

    void BytecodeRewriter::addRule(const std::vector<OpCode>& pattern, const std::vector<OpCode>& replacement) {
        if (pattern.empty()) {
            LOG_WARNING("BytecodeRewriter", "Cannot add rule with empty pattern");
            return;
        }
        
        rules.emplace_back(pattern, replacement);
        LOG_INFO("BytecodeRewriter", "Added rewrite rule: " << pattern.size() << " -> " << replacement.size() << " opcodes");
    }
    
    void BytecodeRewriter::addRule(OpCode pattern, OpCode replacement) {
        addRule(std::vector<OpCode>{pattern}, std::vector<OpCode>{replacement});
    }
    
    bool BytecodeRewriter::rewrite(Chunk& chunk) {
        if (rules.empty()) {
            LOG_INFO("BytecodeRewriter", "No rewrite rules defined");
            return false;
        }
        
        if (chunk.code.empty()) {
            LOG_INFO("BytecodeRewriter", "Empty chunk, nothing to rewrite");
            return false;
        }
        
        LOG_INFO("BytecodeRewriter", "Starting bytecode rewrite with " << rules.size() << " rules");
        return findAndApplyRewrites(chunk);
    }
    
    bool BytecodeRewriter::rewriteAt(Chunk& chunk, size_t index, size_t size, const std::vector<OpCode>& replacement) {
        if (index >= chunk.code.size()) {
            LOG_WARNING("BytecodeRewriter", "Rewrite index " << index << " is out of bounds (chunk size: " << chunk.code.size() << ")");
            return false;
        }
        
        if (index + size > chunk.code.size()) {
            LOG_WARNING("BytecodeRewriter", "Rewrite range [" << index << ", " << (index + size) << ") exceeds chunk bounds");
            return false;
        }
        
        if (size == 0) {
            LOG_WARNING("BytecodeRewriter", "Cannot rewrite zero-sized region");
            return false;
        }
        
        LOG_INFO("BytecodeRewriter", "Direct rewrite at index " << index << " (size " << size << " -> " << replacement.size() << " opcodes)");
        
        // Calculate replacement byte size
        size_t replacementByteSize = getReplacementByteSize(replacement);
        int sizeDelta = static_cast<int>(replacementByteSize) - static_cast<int>(size);
        
        // Store original lines for replacement
        std::vector<int> originalLines;
        if (index < chunk.lines.size()) {
            size_t linesToCopy = std::min(size, chunk.lines.size() - index);
            originalLines.assign(chunk.lines.begin() + index, chunk.lines.begin() + index + linesToCopy);
        }
        
        // Remove original bytes and lines
        chunk.code.erase(chunk.code.begin() + index, chunk.code.begin() + index + size);
        if (index < chunk.lines.size()) {
            size_t linesToRemove = std::min(size, chunk.lines.size() - index);
            chunk.lines.erase(chunk.lines.begin() + index, chunk.lines.begin() + index + linesToRemove);
        }
        
        // Insert replacement opcodes
        size_t insertPos = index;
        size_t lineIndex = 0;
        for (OpCode opcode : replacement) {
            int line = (lineIndex < originalLines.size()) ? originalLines[lineIndex % originalLines.size()] : 0;
            
            chunk.code.insert(chunk.code.begin() + insertPos, static_cast<uint8_t>(opcode));
            chunk.lines.insert(chunk.lines.begin() + insertPos, line);
            insertPos++;
            
            // Add operand bytes for multi-byte instructions
            size_t instSize = getInstructionSize(opcode);
            for (size_t i = 1; i < instSize; ++i) {
                chunk.code.insert(chunk.code.begin() + insertPos, 0); // Placeholder for operand
                chunk.lines.insert(chunk.lines.begin() + insertPos, line);
                insertPos++;
            }
            
            lineIndex++;
        }
        
        // Adjust jump offsets if size changed
        if (sizeDelta != 0) {
            LOG_INFO("BytecodeRewriter", "Size changed by " << sizeDelta << " bytes, adjusting affected jump offsets");
            adjustJumpOffsetsAfterRewrite(chunk, index, sizeDelta);
        }
        
        LOG_INFO("BytecodeRewriter", "Direct rewrite completed successfully");
        return true;
    }
    
    void BytecodeRewriter::clearRules() {
        rules.clear();
        LOG_INFO("BytecodeRewriter", "Cleared all rewrite rules");
    }
    
    bool BytecodeRewriter::findAndApplyRewrites(Chunk& chunk) {
        bool anyChanges = false;
        
        for (size_t i = 0; i < chunk.code.size();) {
            bool foundMatch = false;
            
            for (const auto& rule : rules) {
                if (matchesPattern(chunk, i, rule.pattern)) {
                    size_t patternSize = getPatternByteSize(rule.pattern);
                    size_t replacementSize = getReplacementByteSize(rule.replacement);
                    int sizeDelta = static_cast<int>(replacementSize) - static_cast<int>(patternSize);
                    
                    applyRewrite(chunk, i, rule);
                    
                    LOG_INFO("BytecodeRewriter", "Applied rewrite at offset " << i << 
                             " (size change: " << sizeDelta << ")");
                    
                    // Adjust jump offsets immediately if size changed
                    if (sizeDelta != 0) {
                        LOG_INFO("BytecodeRewriter", "Adjusting jump offsets after size change");
                        adjustJumpOffsetsAfterRewrite(chunk, i, sizeDelta);
                    }
                    
                    foundMatch = true;
                    anyChanges = true;
                    i += replacementSize;
                    break;
                }
            }
            
            if (!foundMatch) {
                i += getInstructionSize(static_cast<OpCode>(chunk.code[i]));
            }
        }
        
        if (anyChanges) {
            LOG_INFO("BytecodeRewriter", "Pattern-based rewrite completed with changes");
        }
        
        return anyChanges;
    }
    
    bool BytecodeRewriter::matchesPattern(const Chunk& chunk, size_t offset, const std::vector<OpCode>& pattern) {
        if (offset >= chunk.code.size()) {
            return false;
        }
        
        size_t currentOffset = offset;
        
        for (OpCode expectedOpcode : pattern) {
            if (currentOffset >= chunk.code.size()) {
                return false;
            }
            
            if (static_cast<OpCode>(chunk.code[currentOffset]) != expectedOpcode) {
                return false;
            }
            
            currentOffset += getInstructionSize(expectedOpcode);
        }
        
        return true;
    }
    
    void BytecodeRewriter::applyRewrite(Chunk& chunk, size_t offset, const RewriteRule& rule) {
        size_t patternByteSize = getPatternByteSize(rule.pattern);
        size_t replacementByteSize = getReplacementByteSize(rule.replacement);
        
        // Remove original pattern bytes
        chunk.code.erase(chunk.code.begin() + offset, chunk.code.begin() + offset + patternByteSize);
        chunk.lines.erase(chunk.lines.begin() + offset, chunk.lines.begin() + offset + patternByteSize);
        
        // Insert replacement bytes
        size_t insertPos = offset;
        for (OpCode opcode : rule.replacement) {
            int line = (insertPos < chunk.lines.size()) ? chunk.lines[insertPos] : 0;
            
            chunk.code.insert(chunk.code.begin() + insertPos, static_cast<uint8_t>(opcode));
            chunk.lines.insert(chunk.lines.begin() + insertPos, line);
            insertPos++;
            
            // Add operand bytes for multi-byte instructions
            size_t instSize = getInstructionSize(opcode);
            for (size_t i = 1; i < instSize; ++i) {
                chunk.code.insert(chunk.code.begin() + insertPos, 0); // Placeholder for operand
                chunk.lines.insert(chunk.lines.begin() + insertPos, line);
                insertPos++;
            }
        }
    }
    
    void BytecodeRewriter::adjustJumpOffsets(Chunk& chunk, const std::vector<RewriteContext>& contexts) {
        if (contexts.empty()) {
            return;
        }
        
        LOG_INFO("BytecodeRewriter", "Adjusting jump offsets after " << contexts.size() << " rewrites");
        
        // Calculate cumulative adjustments at each position
        std::vector<std::pair<size_t, int>> adjustments;
        for (const auto& ctx : contexts) {
            adjustments.emplace_back(ctx.matchOffset, ctx.sizeDelta());
        }
        
        std::sort(adjustments.begin(), adjustments.end());
        
        // Scan through bytecode and adjust all jump instructions
        for (size_t i = 0; i < chunk.code.size();) {
            OpCode opcode = static_cast<OpCode>(chunk.code[i]);
            
            if (isJumpInstruction(opcode)) {
                // Calculate total adjustment affecting this jump
                int totalAdjustment = 0;
                for (const auto& adj : adjustments) {
                    if (adj.first < i) {
                        totalAdjustment += adj.second;
                    }
                }
                
                adjustSingleJump(chunk, i, opcode, totalAdjustment);
            }
            
            i += getInstructionSize(opcode);
        }
    }
    
    void BytecodeRewriter::adjustSingleJump(Chunk& chunk, size_t jumpOffset, OpCode jumpOpcode, int totalAdjustment) {
        size_t originalTarget = calculateJumpTarget(chunk, jumpOffset, jumpOpcode);
        
        if (isLongJumpInstruction(jumpOpcode)) {
            uint32_t currentOffset = extractLongJumpOffset(chunk, jumpOffset);
            size_t instructionEnd = jumpOffset + 5;
            
            if (jumpOpcode == OpCode::OP_Long_Loop) {
                // Backward jump - target should remain the same
                if (originalTarget < instructionEnd) {
                    uint32_t newDistance = instructionEnd - originalTarget;
                    writeLongJumpOffset(chunk, jumpOffset, newDistance);
                }
            } else {
                // Forward jump - adjust for size changes
                size_t adjustedTarget = originalTarget;
                uint32_t newDistance = (adjustedTarget > instructionEnd) ? 
                                     (adjustedTarget - instructionEnd) : 0;
                writeLongJumpOffset(chunk, jumpOffset, newDistance);
            }
        } else if (isShortJumpInstruction(jumpOpcode)) {
            uint16_t currentOffset = extractShortJumpOffset(chunk, jumpOffset);
            size_t instructionEnd = jumpOffset + 3;
            
            if (jumpOpcode == OpCode::OP_Loop) {
                // Backward jump
                if (originalTarget < instructionEnd) {
                    uint32_t newDistance = instructionEnd - originalTarget;
                    if (newDistance <= 65535) {
                        writeShortJumpOffset(chunk, jumpOffset, static_cast<uint16_t>(newDistance));
                    } else {
                        LOG_WARNING("BytecodeRewriter", "Short jump distance overflow at offset " << jumpOffset);
                    }
                }
            } else {
                // Forward jump
                size_t adjustedTarget = originalTarget;
                if (adjustedTarget >= instructionEnd) {
                    uint32_t newDistance = adjustedTarget - instructionEnd;
                    if (newDistance <= 65535) {
                        writeShortJumpOffset(chunk, jumpOffset, static_cast<uint16_t>(newDistance));
                    } else {
                        LOG_WARNING("BytecodeRewriter", "Short jump distance overflow at offset " << jumpOffset);
                    }
                }
            }
        }
    }
    
    size_t BytecodeRewriter::getInstructionSize(OpCode opcode) const {
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
    
    size_t BytecodeRewriter::getPatternByteSize(const std::vector<OpCode>& pattern) const {
        size_t totalSize = 0;
        for (OpCode opcode : pattern) {
            totalSize += getInstructionSize(opcode);
        }
        return totalSize;
    }
    
    size_t BytecodeRewriter::getReplacementByteSize(const std::vector<OpCode>& replacement) const {
        size_t totalSize = 0;
        for (OpCode opcode : replacement) {
            totalSize += getInstructionSize(opcode);
        }
        return totalSize;
    }
    
    bool BytecodeRewriter::isJumpInstruction(OpCode opcode) const {
        return isLongJumpInstruction(opcode) || isShortJumpInstruction(opcode);
    }
    
    bool BytecodeRewriter::isLongJumpInstruction(OpCode opcode) const {
        return opcode == OpCode::OP_Long_Jump ||
               opcode == OpCode::OP_Long_Jump_If_False ||
               opcode == OpCode::OP_Long_Loop;
    }
    
    bool BytecodeRewriter::isShortJumpInstruction(OpCode opcode) const {
        return opcode == OpCode::OP_Jump ||
               opcode == OpCode::OP_Jump_If_False ||
               opcode == OpCode::OP_Loop;
    }
    
    uint32_t BytecodeRewriter::extractLongJumpOffset(const Chunk& chunk, size_t offset) const {
        if (offset + 4 >= chunk.code.size()) {
            return 0;
        }
        
        return (static_cast<uint32_t>(chunk.code[offset + 1]) << 24) |
               (static_cast<uint32_t>(chunk.code[offset + 2]) << 16) |
               (static_cast<uint32_t>(chunk.code[offset + 3]) << 8) |
               static_cast<uint32_t>(chunk.code[offset + 4]);
    }
    
    uint16_t BytecodeRewriter::extractShortJumpOffset(const Chunk& chunk, size_t offset) const {
        if (offset + 2 >= chunk.code.size()) {
            return 0;
        }
        
        return (static_cast<uint16_t>(chunk.code[offset + 1]) << 8) |
               static_cast<uint16_t>(chunk.code[offset + 2]);
    }
    
    void BytecodeRewriter::writeLongJumpOffset(Chunk& chunk, size_t offset, uint32_t jumpOffset) const {
        if (offset + 4 < chunk.code.size()) {
            chunk.code[offset + 1] = (jumpOffset >> 24) & 0xFF;
            chunk.code[offset + 2] = (jumpOffset >> 16) & 0xFF;
            chunk.code[offset + 3] = (jumpOffset >> 8) & 0xFF;
            chunk.code[offset + 4] = jumpOffset & 0xFF;
        }
    }
    
    void BytecodeRewriter::writeShortJumpOffset(Chunk& chunk, size_t offset, uint16_t jumpOffset) const {
        if (offset + 2 < chunk.code.size()) {
            chunk.code[offset + 1] = (jumpOffset >> 8) & 0xFF;
            chunk.code[offset + 2] = jumpOffset & 0xFF;
        }
    }
    
    size_t BytecodeRewriter::calculateJumpTarget(const Chunk& chunk, size_t jumpOffset, OpCode jumpOpcode) const {
        size_t instructionEnd;
        uint32_t distance;
        
        if (isLongJumpInstruction(jumpOpcode)) {
            instructionEnd = jumpOffset + 5;
            distance = extractLongJumpOffset(chunk, jumpOffset);
        } else {
            instructionEnd = jumpOffset + 3;
            distance = extractShortJumpOffset(chunk, jumpOffset);
        }
        
        if (jumpOpcode == OpCode::OP_Loop || jumpOpcode == OpCode::OP_Long_Loop) {
            // Backward jump
            return (distance <= instructionEnd) ? (instructionEnd - distance) : 0;
        } else {
            // Forward jump
            return instructionEnd + distance;
        }
    }
    
    void BytecodeRewriter::adjustJumpOffsetsAfterRewrite(Chunk& chunk, size_t rewriteIndex, int sizeDelta) {
        LOG_INFO("BytecodeRewriter", "Adjusting jump offsets: rewrite at " << rewriteIndex << ", delta=" << sizeDelta);
        
        for (size_t i = 0; i < chunk.code.size();) {
            OpCode opcode = static_cast<OpCode>(chunk.code[i]);
            
            if (isJumpInstruction(opcode)) {
                size_t currentTarget = calculateJumpTarget(chunk, i, opcode);
                bool needsAdjustment = false;
                
                if (opcode == OpCode::OP_Loop || opcode == OpCode::OP_Long_Loop) {
                    // Backward jump: if target is after rewrite point, adjust
                    needsAdjustment = (currentTarget > rewriteIndex);
                } else {
                    // Forward jump: if target is after rewrite point, adjust
                    needsAdjustment = (currentTarget > rewriteIndex);
                }
                
                if (needsAdjustment) {
                    // Get current distance and adjust it
                    uint32_t currentDistance;
                    if (isLongJumpInstruction(opcode)) {
                        currentDistance = extractLongJumpOffset(chunk, i);
                    } else {
                        currentDistance = extractShortJumpOffset(chunk, i);
                    }
                    
                    // Calculate new distance based on direction
                    int newDistance;
                    if (opcode == OpCode::OP_Loop || opcode == OpCode::OP_Long_Loop) {
                        // Backward jump: target moved, so distance changes by -sizeDelta
                        newDistance = static_cast<int>(currentDistance) - sizeDelta;
                    } else {
                        // Forward jump: target moved, so distance changes by +sizeDelta  
                        newDistance = static_cast<int>(currentDistance) + sizeDelta;
                    }
                    
                    if (newDistance < 0) {
                        LOG_WARNING("BytecodeRewriter", "Jump distance became negative, setting to 0");
                        newDistance = 0;
                    }
                    
                    // Write adjusted distance - PRESERVE INSTRUCTION TYPE
                    if (isLongJumpInstruction(opcode)) {
                        writeLongJumpOffset(chunk, i, static_cast<uint32_t>(newDistance));
                        LOG_INFO("BytecodeRewriter", "Adjusted long jump at " << i << " from " << currentDistance << " to " << newDistance);
                    } else {
                        if (newDistance <= 65535) {
                            writeShortJumpOffset(chunk, i, static_cast<uint16_t>(newDistance));
                            LOG_INFO("BytecodeRewriter", "Adjusted short jump at " << i << " from " << currentDistance << " to " << newDistance);
                        } else {
                            LOG_WARNING("BytecodeRewriter", "Short jump distance overflow: " << newDistance << " at position " << i << " - keeping original distance");
                            // Keep original distance rather than converting to long jump
                        }
                    }
                }
            }
            
            i += getInstructionSize(opcode);
        }
        
        LOG_INFO("BytecodeRewriter", "Jump offset adjustment completed");
    }

}