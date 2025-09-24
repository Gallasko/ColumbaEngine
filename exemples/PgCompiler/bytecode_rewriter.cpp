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
            size_t instSize = pg::getInstructionSize(opcode);
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

    bool BytecodeRewriter::rewriteAtRaw(Chunk& chunk, size_t index, size_t size, const std::vector<uint8_t>& replacement) {
        if (index >= chunk.code.size()) {
            LOG_WARNING("BytecodeRewriter", "Raw rewrite index " << index << " is out of bounds (chunk size: " << chunk.code.size() << ")");
            return false;
        }
        
        if (index + size > chunk.code.size()) {
            LOG_WARNING("BytecodeRewriter", "Raw rewrite range [" << index << ", " << (index + size) << ") exceeds chunk bounds");
            return false;
        }
        
        if (size == 0) {
            LOG_WARNING("BytecodeRewriter", "Cannot rewrite zero-sized region");
            return false;
        }
        
        LOG_INFO("BytecodeRewriter", "Raw rewrite at index " << index << " (size " << size << " -> " << replacement.size() << " bytes)");
        
        int sizeDelta = static_cast<int>(replacement.size()) - static_cast<int>(size);
        
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
        
        // Insert replacement bytes directly
        chunk.code.insert(chunk.code.begin() + index, replacement.begin(), replacement.end());
        
        // Insert corresponding line numbers
        int line = originalLines.empty() ? 0 : originalLines[0];
        for (size_t i = 0; i < replacement.size(); ++i) {
            chunk.lines.insert(chunk.lines.begin() + index + i, line);
        }
        
        // Adjust jump offsets if size changed
        if (sizeDelta != 0) {
            LOG_INFO("BytecodeRewriter", "Size changed by " << sizeDelta << " bytes, adjusting affected jump offsets");
            adjustJumpOffsetsAfterRewrite(chunk, index, sizeDelta);
        }
        
        LOG_INFO("BytecodeRewriter", "Raw rewrite completed successfully");
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
                i += pg::getInstructionSize(static_cast<OpCode>(chunk.code[i]));
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

            currentOffset += pg::getInstructionSize(expectedOpcode);
        }

        return true;
    }

    void BytecodeRewriter::applyRewrite(Chunk& chunk, size_t offset, const RewriteRule& rule) {
        size_t patternByteSize = getPatternByteSize(rule.pattern);
        // size_t replacementByteSize = getReplacementByteSize(rule.replacement);

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
            size_t instSize = pg::getInstructionSize(opcode);
            for (size_t i = 1; i < instSize; ++i) {
                chunk.code.insert(chunk.code.begin() + insertPos, 0); // Placeholder for operand
                chunk.lines.insert(chunk.lines.begin() + insertPos, line);
                insertPos++;
            }
        }
    }




    size_t BytecodeRewriter::getPatternByteSize(const std::vector<OpCode>& pattern) const {
        size_t totalSize = 0;
        for (OpCode opcode : pattern) {
            totalSize += pg::getInstructionSize(opcode);
        }
        return totalSize;
    }

    size_t BytecodeRewriter::getReplacementByteSize(const std::vector<OpCode>& replacement) const {
        size_t totalSize = 0;
        for (OpCode opcode : replacement) {
            totalSize += pg::getInstructionSize(opcode);
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
                    // Backward jump: adjust if the jump instruction is after the rewrite point
                    // AND the target is before the rewrite point (target didn't move, but jump moved)
                    needsAdjustment = (i > rewriteIndex && currentTarget < rewriteIndex);
                } else {
                    // Forward jump: adjust if the target is after the rewrite point
                    // AND the jump instruction is before or at the rewrite point (jump didn't move, but target moved)
                    needsAdjustment = (currentTarget > rewriteIndex && i <= rewriteIndex);
                }

                if (needsAdjustment) {
                    // Get current index and adjust it
                    uint32_t currentIndex;
                    if (isLongJumpInstruction(opcode)) {
                        currentIndex = extractLongJumpOffset(chunk, i);
                    } else {
                        currentIndex = extractShortJumpOffset(chunk, i);
                    }

                    LOG_INFO("BytecodeRewriter", "Adjusting jump at " << i << " (opcode=" << static_cast<int>(opcode) 
                             << ", currentTarget=" << currentTarget << ", currentIndex=" << currentIndex 
                             << ", rewriteIndex=" << rewriteIndex << ", sizeDelta=" << sizeDelta << ")");

                    // Calculate new index based on direction
                    int newIndex;
                    if (opcode == OpCode::OP_Loop || opcode == OpCode::OP_Long_Loop) {
                        // Backward jump: jump instruction moved by sizeDelta, target stayed same
                        // If sizeDelta = -2 (chunk shrank), jump moved 2 bytes closer to target, so index decreases by 2
                        newIndex = static_cast<int>(currentIndex) + sizeDelta;
                    } else {
                        // Forward jump: target moved by sizeDelta, jump instruction stayed same  
                        // If sizeDelta = -2 (chunk shrank), target moved 2 bytes closer, so index decreases by 2
                        newIndex = static_cast<int>(currentIndex) + sizeDelta;
                    }

                    LOG_INFO("BytecodeRewriter", "Index adjusted from " << currentIndex << " to " << newIndex);

                    if (newIndex < 0) {
                        LOG_WARNING("BytecodeRewriter", "Jump index became negative, setting to 0");
                        newIndex = 0;
                    }

                    // Write adjusted index - PRESERVE INSTRUCTION TYPE
                    if (isLongJumpInstruction(opcode)) {
                        writeLongJumpOffset(chunk, i, static_cast<uint32_t>(newIndex));
                        LOG_INFO("BytecodeRewriter", "Adjusted long jump at " << i << " from " << currentIndex << " to " << newIndex);
                    } else {
                        if (newIndex <= 65535) {
                            writeShortJumpOffset(chunk, i, static_cast<uint16_t>(newIndex));
                            LOG_INFO("BytecodeRewriter", "Adjusted short jump at " << i << " from " << currentIndex << " to " << newIndex);
                        } else {
                            LOG_WARNING("BytecodeRewriter", "Short jump index overflow: " << newIndex << " at position " << i << " - keeping original index");
                            // Keep original index rather than converting to long jump
                        }
                    }
                }
            }

            i += pg::getInstructionSize(opcode);
        }

        LOG_INFO("BytecodeRewriter", "Jump offset adjustment completed");
    }

}