#include "stdafx.h"

#include <algorithm>

#include "bytecode_rewriter.h"
#include "compiler_debug.h"
#include "vm.h"

#include "logger.h"

namespace pg
{

    void BytecodeRewriter::addRule(const std::vector<OpCode>& pattern, const std::vector<OpCode>& replacement)
    {
        if (pattern.empty())
        {
            LOG_WARNING("BytecodeRewriter", "Cannot add rule with empty pattern");
            return;
        }

        rules.emplace_back(pattern, replacement);
    }

    void BytecodeRewriter::addRule(OpCode pattern, OpCode replacement)
    {
        addRule(std::vector<OpCode>{pattern}, std::vector<OpCode>{replacement});
    }

    void BytecodeRewriter::addAdvancedRule(const std::vector<PatternElement>& pattern,
                                          std::function<std::vector<uint8_t>(const std::vector<CapturedInstruction>&)> transform,
                                          std::function<bool(const std::vector<CapturedInstruction>&)> condition)
    {
        if (pattern.empty())
        {
            LOG_WARNING("BytecodeRewriter", "Cannot add advanced rule with empty pattern");
            return;
        }

        advancedRules.emplace_back(pattern, transform, condition);
    }

    bool BytecodeRewriter::rewrite(Chunk& chunk)
    {
        if (rules.empty() and advancedRules.empty())
        {
            LOG_MILE("BytecodeRewriter", "No rewrite rules defined");
            return false;
        }

        if (chunk.code.empty())
        {
            LOG_MILE("BytecodeRewriter", "Empty chunk, nothing to rewrite");
            return false;
        }

        LOG_MILE("BytecodeRewriter", "Starting bytecode rewrite with " << rules.size() << " simple rules and " << advancedRules.size() << " advanced rules");

        bool anyChanges = false;

        // Apply advanced rules first (more specific)
        if (not advancedRules.empty())
        {
            anyChanges |= findAndApplyAdvancedRewrites(chunk);
        }

        // Then apply simple rules
        if (not rules.empty())
        {
            anyChanges |= findAndApplyRewrites(chunk);
        }

        return anyChanges;
    }

    bool BytecodeRewriter::rewriteAtRaw(Chunk& chunk, size_t index, size_t size, const std::vector<uint8_t>& replacement)
    {
        if (index >= chunk.code.size())
        {
            LOG_WARNING("BytecodeRewriter", "Raw rewrite index " << index << " is out of bounds (chunk size: " << chunk.code.size() << ")");
            return false;
        }

        if (index + size > chunk.code.size())
        {
            LOG_WARNING("BytecodeRewriter", "Raw rewrite range [" << index << ", " << (index + size) << ") exceeds chunk bounds");
            return false;
        }

        if (size == 0)
        {
            LOG_WARNING("BytecodeRewriter", "Cannot rewrite zero-sized region");
            return false;
        }

        LOG_MILE("BytecodeRewriter", "Raw rewrite at index " << index << " (size " << size << " -> " << replacement.size() << " bytes)");

        int sizeDelta = static_cast<int>(replacement.size()) - static_cast<int>(size);

        // The jumps as they are now: a raw edit can come from any pass, on any chunk
        trackJumps(chunk);

        // Adjust jump offsets BEFORE modifying bytecode (with stable offsets)
        if (sizeDelta != 0)
        {
            adjustJumpOffsetsBeforeRewrite(chunk, index, size, sizeDelta);
        }

        forgetJumpsIn(chunk, index, size);

        spliceCode(chunk, index, size, replacement);

        followRewrite(chunk, index, size, replacement.size());

        // Jump offsets were already adjusted BEFORE the rewrite
        LOG_MILE("BytecodeRewriter", "Raw rewrite completed successfully");
        return true;
    }

    bool BytecodeRewriter::removeInstructions(Chunk& chunk, size_t index, size_t count)
    {
        // Use rewriteAtRaw with empty replacement (removal)
        std::vector<uint8_t> empty;
        return rewriteAtRaw(chunk, index, count, empty);
    }

    bool BytecodeRewriter::insertInstructions(Chunk& chunk, size_t index, const std::vector<uint8_t>& instructions)
    {
        // Use rewriteAtRaw with size 0 (insertion, not replacement)
        return rewriteAtRaw(chunk, index, 0, instructions);
    }

    void BytecodeRewriter::clearRules()
    {
        rules.clear();
        advancedRules.clear();
    }

    size_t BytecodeRewriter::getActualInstructionSize(const Chunk& chunk, size_t offset) const
    {
        if (offset >= chunk.code.size())
        {
            return 0;
        }

        OpCode opcode = static_cast<OpCode>(chunk.code[offset]);

        // Handle variable-length OP_Closure instruction
        if (opcode == OpCode::OP_Closure)
        {
            // OP_Closure format: opcode + 1 byte constant index + 2 bytes per upvalue
            size_t baseSize = 2; // opcode + constant index

            // Get the function from the constant pool
            if (offset + 1 < chunk.code.size())
            {
                uint8_t constantIndex = chunk.code[offset + 1];

                if (constantIndex < chunk.constants.size())
                {
                    const Value& constant = chunk.constants[constantIndex];

                    if (IS_FUNC(constant) and vm != nullptr)
                    {
                        ObjFunction* function = vm->asFunction(constant);
                        if (function != nullptr)
                        {
                            // Each upvalue takes 2 bytes (isLocal + index)
                            return baseSize + (function->upvalueCount * 2);
                        }
                    }
                }
            }

            // If we can't determine upvalue count, return base size
            LOG_WARNING("BytecodeRewriter", "Cannot determine upvalue count for OP_Closure at offset " << offset);
            return baseSize;
        }

        // For all other instructions, use the standard size lookup
        return pg::getInstructionSize(opcode);
    }

    bool BytecodeRewriter::findAndApplyAdvancedRewrites(Chunk& chunk)
    {
        bool anyChanges = false;

        collectJumpTargets(chunk);

        for (size_t i = 0; i < chunk.code.size();)
        {
            bool foundMatch = false;

            const OpCode here = static_cast<OpCode>(chunk.code[i]);

            for (const auto& rule : advancedRules)
            {
                // Most rules cannot start here: their first opcode says so without a match
                if (not rule.pattern.empty() and not rule.pattern.front().matches(here))
                    continue;

                auto match = matchesAdvancedPattern(chunk, i, rule.pattern);

                if (match)
                {
                    auto& [captured, patternByteSize] = *match;

                    // See if the match is valid for this rule
                    if (not rule.condition(captured))
                    {
                        // Match was not valid we continue looking
                        continue;
                    }

                    LOG_MILE("BytecodeRewriter", "Found advanced pattern match at offset " << i
                             << " with " << captured.size() << " captured instructions");

                    // Generate replacement using the transform lambda
                    std::vector<uint8_t> replacement = rule.transform(captured);

                    int sizeDelta = static_cast<int>(replacement.size()) - static_cast<int>(patternByteSize);

                    // Adjust jump offsets immediately if size changed
                    if (sizeDelta != 0)
                    {
                        adjustJumpOffsetsBeforeRewrite(chunk, i, patternByteSize, sizeDelta);
                    }

                    forgetJumpsIn(chunk, i, patternByteSize);

                    applyAdvancedRewrite(chunk, i, patternByteSize, replacement);

                    // The bytecode has shifted: the jumps follow it
                    followRewrite(chunk, i, patternByteSize, replacement.size());

                    foundMatch = true;
                    anyChanges = true;
                    i += replacement.size();
                    break;
                }
            }

            if (not foundMatch)
            {
                i += getActualInstructionSize(chunk, i);
            }
        }

        if (anyChanges)
        {
            LOG_MILE("BytecodeRewriter", "Advanced pattern-based rewrite completed with changes");
        }

        return anyChanges;
    }

    void BytecodeRewriter::collectJumpTargets(const Chunk& chunk)
    {
        jumpSites.clear();
        jumpTargets.clear();

        for (size_t i = 0; i < chunk.code.size();)
        {
            OpCode opcode = static_cast<OpCode>(chunk.code[i]);

            if (isJumpInstruction(opcode))
            {
                jumpSites.push_back(i);
                jumpTargets.push_back(calculateJumpTarget(chunk, i, opcode));
            }

            // Never 0 inside the chunk: a walk always moves on
            i += std::max<size_t>(1, getActualInstructionSize(chunk, i));
        }

        std::sort(jumpTargets.begin(), jumpTargets.end());

        trackedChunk = &chunk;
        trackedSize = chunk.code.size();
    }

    void BytecodeRewriter::trackJumps(const Chunk& chunk)
    {
        if (trackedChunk != &chunk or trackedSize != chunk.code.size())
            collectJumpTargets(chunk);
    }

    bool BytecodeRewriter::isJumpTarget(size_t offset) const
    {
        return std::binary_search(jumpTargets.begin(), jumpTargets.end(), offset);
    }

    void BytecodeRewriter::forgetJumpsIn(const Chunk& chunk, size_t index, size_t size)
    {
        auto first = std::lower_bound(jumpSites.begin(), jumpSites.end(), index);
        auto last = std::lower_bound(first, jumpSites.end(), index + size);

        // Their bytes are as they were: adjustJumpOffsetsBeforeRewrite leaves the region's jumps alone
        for (auto site = first; site != last; ++site)
        {
            const size_t target = calculateJumpTarget(chunk, *site, static_cast<OpCode>(chunk.code[*site]));

            auto it = std::lower_bound(jumpTargets.begin(), jumpTargets.end(), target);

            if (it != jumpTargets.end() and *it == target)
                jumpTargets.erase(it);
        }

        jumpSites.erase(first, last);
    }

    void BytecodeRewriter::followRewrite(const Chunk& chunk, size_t index, size_t oldSize, size_t newSize)
    {
        const long long delta = static_cast<long long>(newSize) - static_cast<long long>(oldSize);

        if (delta != 0)
        {
            // What lay past the region moved with it (the region's own jumps are already gone)
            for (auto it = std::lower_bound(jumpSites.begin(), jumpSites.end(), index); it != jumpSites.end(); ++it)
                *it = static_cast<size_t>(static_cast<long long>(*it) + delta);

            // A landing past the start of the region moved too: adjustJumpOffsetsBeforeRewrite
            // rewrote every jump so that it still lands on the same instruction, and one at the
            // start stays where the new bytes begin
            for (auto it = std::upper_bound(jumpTargets.begin(), jumpTargets.end(), index); it != jumpTargets.end(); ++it)
                *it = static_cast<size_t>(std::max(0LL, static_cast<long long>(*it) + delta));

            // Only a landing inside the region shrunk away can fall behind its start
            if (delta < 0 and not std::is_sorted(jumpTargets.begin(), jumpTargets.end()))
                std::sort(jumpTargets.begin(), jumpTargets.end());
        }

        // The jumps the new bytes hold
        for (size_t i = index; i < index + newSize and i < chunk.code.size();)
        {
            OpCode opcode = static_cast<OpCode>(chunk.code[i]);

            if (isJumpInstruction(opcode))
            {
                const size_t target = calculateJumpTarget(chunk, i, opcode);

                jumpSites.insert(std::lower_bound(jumpSites.begin(), jumpSites.end(), i), i);
                jumpTargets.insert(std::lower_bound(jumpTargets.begin(), jumpTargets.end(), target), target);
            }

            i += std::max<size_t>(1, getActualInstructionSize(chunk, i));
        }

        trackedChunk = &chunk;
        trackedSize = chunk.code.size();
    }

    void BytecodeRewriter::spliceCode(Chunk& chunk, size_t offset, size_t size, const std::vector<uint8_t>& replacement)
    {
        // The replacement takes the line of the first instruction it replaces
        const bool hasLines = offset < chunk.lines.size();
        const int line = hasLines ? chunk.lines[offset] : 0;

        // The bytes both have in common are written over; only the difference moves the rest
        const size_t common = std::min(size, replacement.size());

        std::copy(replacement.begin(), replacement.begin() + common, chunk.code.begin() + offset);

        if (replacement.size() < size)
            chunk.code.erase(chunk.code.begin() + offset + common, chunk.code.begin() + offset + size);
        else
            chunk.code.insert(chunk.code.begin() + offset + common, replacement.begin() + common, replacement.end());

        if (hasLines)
        {
            const size_t lineCommon = std::min(common, chunk.lines.size() - offset);
            const size_t linesReplaced = std::min(size, chunk.lines.size() - offset);

            std::fill(chunk.lines.begin() + offset, chunk.lines.begin() + offset + lineCommon, line);

            if (replacement.size() < linesReplaced)
                chunk.lines.erase(chunk.lines.begin() + offset + replacement.size(), chunk.lines.begin() + offset + linesReplaced);
            else
                chunk.lines.insert(chunk.lines.begin() + offset + lineCommon, replacement.size() - lineCommon, line);
        }
        else
        {
            chunk.lines.insert(chunk.lines.begin() + std::min(offset, chunk.lines.size()), replacement.size(), line);
        }
    }

    bool BytecodeRewriter::findAndApplyRewrites(Chunk& chunk)
    {
        bool anyChanges = false;

        collectJumpTargets(chunk);

        for (size_t i = 0; i < chunk.code.size();)
        {
            bool foundMatch = false;

            for (const auto& rule : rules)
            {
                if (matchesPattern(chunk, i, rule.pattern))
                {
                    size_t patternSize = getPatternByteSize(rule.pattern);
                    size_t replacementSize = getReplacementByteSize(rule.replacement);
                    int sizeDelta = static_cast<int>(replacementSize) - static_cast<int>(patternSize);

                    // Adjust jump offsets immediately if size changed
                    if (sizeDelta != 0)
                    {
                        adjustJumpOffsetsBeforeRewrite(chunk, i, patternSize, sizeDelta);
                    }

                    forgetJumpsIn(chunk, i, patternSize);

                    applyRewrite(chunk, i, rule);

                    // The bytecode has shifted: the jumps follow it
                    followRewrite(chunk, i, patternSize, replacementSize);

                    LOG_MILE("BytecodeRewriter", "Applied rewrite at offset " << i <<
                             " (size change: " << sizeDelta << ")");

                    foundMatch = true;
                    anyChanges = true;
                    i += replacementSize;
                    break;
                }
            }

            if (not foundMatch)
            {
                i += getActualInstructionSize(chunk, i);
            }
        }

        if (anyChanges)
        {
            LOG_MILE("BytecodeRewriter", "Pattern-based rewrite completed with changes");
        }

        return anyChanges;
    }

    std::optional<std::pair<std::vector<CapturedInstruction>, size_t>>
    BytecodeRewriter::matchesAdvancedPattern(const Chunk& chunk, size_t offset, const std::vector<PatternElement>& pattern)
    {
        if (offset >= chunk.code.size())
        {
            return std::nullopt;
        }

        size_t currentOffset = offset;
        size_t totalPatternSize = 0;
        bool   firstElement     = true;
        size_t captures         = 0;

        // The whole pattern first, nothing captured: most tries fail, and they now fail
        // without building the captures
        for (const auto& element : pattern)
        {
            if (currentOffset >= chunk.code.size())
            {
                return std::nullopt;
            }

            if (not firstElement and isJumpTarget(currentOffset))
            {
                return std::nullopt;
            }
            firstElement = false;

            if (not element.matches(static_cast<OpCode>(chunk.code[currentOffset])))
            {
                return std::nullopt;
            }

            if (element.capture)
                ++captures;

            const size_t instructionSize = getActualInstructionSize(chunk, currentOffset);

            currentOffset += instructionSize;
            totalPatternSize += instructionSize;
        }

        // It matched: the same walk again, capturing
        std::vector<CapturedInstruction> captured;
        captured.reserve(captures);

        currentOffset = offset;
        totalPatternSize = 0;
        firstElement = true;

        for (const auto& element : pattern)
        {
            if (currentOffset >= chunk.code.size())
            {
                return std::nullopt;
            }

            // A jump landing AT the start of the pattern is fine — execution
            // will run the fused replacement, which is semantically equivalent.
            // But a jump landing INSIDE the pattern (between elements 2..N)
            // would skip into the middle of a fused op — unsafe, must reject.
            if (not firstElement and isJumpTarget(currentOffset))
            {
                return std::nullopt;
            }
            firstElement = false;

            OpCode currentOpcode = static_cast<OpCode>(chunk.code[currentOffset]);

            // Check if the pattern element matches
            if (not element.matches(currentOpcode))
            {
                return std::nullopt;
            }

            // Payload-aware size: a wildcard/anyOf element that matches an
            // OP_Closure with upvalues must account for the variable-length
            // payload, or the rest of the pattern would be matched against
            // payload bytes (and the rewrite would delete the wrong region).
            size_t instructionSize = getActualInstructionSize(chunk, currentOffset);

            // Capture instruction if requested
            if (element.capture)
            {
                CapturedInstruction captured_inst;
                captured_inst.opcode = currentOpcode;
                captured_inst.offset = currentOffset;

                // Extract operand bytes
                if (instructionSize > 1 and currentOffset + instructionSize <= chunk.code.size())
                {
                    captured_inst.operands.assign(
                        chunk.code.begin() + currentOffset + 1,
                        chunk.code.begin() + currentOffset + instructionSize
                    );
                }

                captured.push_back(std::move(captured_inst));
            }

            currentOffset += instructionSize;
            totalPatternSize += instructionSize;
        }

        return std::make_pair(std::move(captured), totalPatternSize);
    }

    bool BytecodeRewriter::matchesPattern(const Chunk& chunk, size_t offset, const std::vector<OpCode>& pattern)
    {
        if (offset >= chunk.code.size())
        {
            return false;
        }

        size_t currentOffset = offset;
        bool   firstElement  = true;

        for (OpCode expectedOpcode : pattern)
        {
            if (currentOffset >= chunk.code.size())
            {
                return false;
            }

            // See matchesAdvancedPattern: only reject if the jump target is
            // INSIDE the pattern (would land mid-fused-op). A target AT the
            // pattern start is safe — equivalent execution.
            if (not firstElement and isJumpTarget(currentOffset))
            {
                return false;
            }
            firstElement = false;

            if (static_cast<OpCode>(chunk.code[currentOffset]) != expectedOpcode)
            {
                return false;
            }

            // Simple rules rewrite with table sizes (getPatternByteSize), so
            // a variable-length instruction (OP_Closure with upvalues) can't
            // be matched safely — reject instead of desyncing into payload.
            size_t actualSize = getActualInstructionSize(chunk, currentOffset);
            if (actualSize != static_cast<size_t>(pg::getInstructionSize(expectedOpcode)))
            {
                return false;
            }

            currentOffset += actualSize;
        }

        return true;
    }

    void BytecodeRewriter::applyRewrite(Chunk& chunk, size_t offset, const RewriteRule& rule)
    {
        size_t patternByteSize = getPatternByteSize(rule.pattern);
        // size_t replacementByteSize = getReplacementByteSize(rule.replacement);

        // Remove original pattern bytes
        chunk.code.erase(chunk.code.begin() + offset, chunk.code.begin() + offset + patternByteSize);
        chunk.lines.erase(chunk.lines.begin() + offset, chunk.lines.begin() + offset + patternByteSize);

        // Insert replacement bytes
        size_t insertPos = offset;
        for (OpCode opcode : rule.replacement)
        {
            int line = (insertPos < chunk.lines.size()) ? chunk.lines[insertPos] : 0;

            chunk.code.insert(chunk.code.begin() + insertPos, static_cast<uint8_t>(opcode));
            chunk.lines.insert(chunk.lines.begin() + insertPos, line);
            insertPos++;

            // Add operand bytes for multi-byte instructions
            size_t instSize = pg::getInstructionSize(opcode);
            for (size_t i = 1; i < instSize; ++i)
            {
                chunk.code.insert(chunk.code.begin() + insertPos, 0); // Placeholder for operand
                chunk.lines.insert(chunk.lines.begin() + insertPos, line);
                insertPos++;
            }
        }
    }

    void BytecodeRewriter::applyAdvancedRewrite(Chunk& chunk, size_t offset, size_t patternSize, const std::vector<uint8_t>& replacement) {
        spliceCode(chunk, offset, patternSize, replacement);

        LOG_MILE("BytecodeRewriter", "Applied advanced rewrite: " << patternSize << " bytes -> " << replacement.size() << " bytes");
    }

    size_t BytecodeRewriter::getPatternByteSize(const std::vector<OpCode>& pattern) const
    {
        size_t totalSize = 0;

        for (OpCode opcode : pattern)
        {
            totalSize += pg::getInstructionSize(opcode);
        }

        return totalSize;
    }

    size_t BytecodeRewriter::getReplacementByteSize(const std::vector<OpCode>& replacement) const
    {
        size_t totalSize = 0;

        for (OpCode opcode : replacement)
        {
            totalSize += pg::getInstructionSize(opcode);
        }

        return totalSize;
    }

    bool BytecodeRewriter::isJumpInstruction(OpCode opcode) const
    {
        return isLongJumpInstruction(opcode) or isShortJumpInstruction(opcode);
    }

    bool BytecodeRewriter::isLongJumpInstruction(OpCode opcode) const
    {
        return opcode == OpCode::OP_Long_Jump or
            opcode == OpCode::OP_Long_Jump_If_False or
            opcode == OpCode::OP_Long_Jump_If_False_Popping or
            opcode == OpCode::OP_Long_Loop;
    }

    bool BytecodeRewriter::isShortJumpInstruction(OpCode opcode) const
    {
        return opcode == OpCode::OP_Jump or
            opcode == OpCode::OP_Jump_If_False or
            opcode == OpCode::OP_Jump_If_False_Popping or
            opcode == OpCode::OP_Loop;
    }

    uint32_t BytecodeRewriter::extractLongJumpOffset(const Chunk& chunk, size_t offset) const
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

    uint16_t BytecodeRewriter::extractShortJumpOffset(const Chunk& chunk, size_t offset) const
    {
        if (offset + 2 >= chunk.code.size())
        {
            return 0;
        }

        return (static_cast<uint16_t>(chunk.code[offset + 1]) << 8) |
                static_cast<uint16_t>(chunk.code[offset + 2]);
    }

    void BytecodeRewriter::writeLongJumpOffset(Chunk& chunk, size_t offset, uint32_t jumpOffset) const
    {
        if (offset + 4 < chunk.code.size())
        {
            chunk.code[offset + 1] = (jumpOffset >> 24) & 0xFF;
            chunk.code[offset + 2] = (jumpOffset >> 16) & 0xFF;
            chunk.code[offset + 3] = (jumpOffset >> 8)  & 0xFF;
            chunk.code[offset + 4] = jumpOffset & 0xFF;
        }
    }

    void BytecodeRewriter::writeShortJumpOffset(Chunk& chunk, size_t offset, uint16_t jumpOffset) const
    {
        if (offset + 2 < chunk.code.size())
        {
            chunk.code[offset + 1] = (jumpOffset >> 8) & 0xFF;
            chunk.code[offset + 2] = jumpOffset & 0xFF;
        }
    }

    size_t BytecodeRewriter::calculateJumpTarget(const Chunk& chunk, size_t jumpOffset, OpCode jumpOpcode) const
    {
        size_t instructionEnd;
        uint32_t distance;

        if (isLongJumpInstruction(jumpOpcode))
        {
            instructionEnd = jumpOffset + 5;
            distance = extractLongJumpOffset(chunk, jumpOffset);
        }
        else
        {
            instructionEnd = jumpOffset + 3;
            distance = extractShortJumpOffset(chunk, jumpOffset);
        }

        if (jumpOpcode == OpCode::OP_Loop or jumpOpcode == OpCode::OP_Long_Loop)
        {
            // Backward jump
            return (distance <= instructionEnd) ? (instructionEnd - distance) : 0;
        }
        else
        {
            // Forward jump
            return instructionEnd + distance;
        }
    }

    void BytecodeRewriter::adjustJumpOffsetsBeforeRewrite(Chunk& chunk, size_t rewriteIndex, size_t rewriteSize, int sizeDelta)
    {
        LOG_MILE("BytecodeRewriter", "Adjusting jump offsets BEFORE rewrite: at " << rewriteIndex
                 << ", size=" << rewriteSize << ", delta=" << sizeDelta);

        // True when the rewrite is fusing a pattern whose first instruction
        // is the landing point of a backward jump. In that narrow case the
        // target offset stays at rewriteIndex (the fused op starts there) but
        // the jump position moved by sizeDelta, so we still need to adjust
        // the encoded distance. The jump lists are exact for every edit, raw
        // ones included, so this holds whenever some jump lands there.
        const bool rewriteIsAtJumpTarget = isJumpTarget(rewriteIndex);

        // Only the jumps are visited, not every instruction of the chunk
        for (size_t i : jumpSites)
        {
            OpCode opcode = static_cast<OpCode>(chunk.code[i]);

            if (not isJumpInstruction(opcode))
            {
                LOG_ERROR("BytecodeRewriter", "No jump at " << i << " where one was tracked: the chunk changed outside the rewriter");
            }
            else
            {
                // Exclude jumps that are within the rewrite region (they will be replaced)
                if (i >= rewriteIndex and i < rewriteIndex + rewriteSize)
                {
                    LOG_MILE("BytecodeRewriter", "Skipping jump at " << i << " (in rewrite region)");
                    continue;
                }

                size_t currentTarget = calculateJumpTarget(chunk, i, opcode);
                bool needsAdjustment = false;

                if (opcode == OpCode::OP_Loop or opcode == OpCode::OP_Long_Loop)
                {
                    // Backward jump: adjust ONLY if target < rewriteIndex < jump.
                    //
                    // Special case: target == rewriteIndex is also adjusted, but
                    // ONLY when the rewrite happens AT a jump target (i.e., the
                    // first instruction of the fused pattern is the jump's
                    // landing point). In that case the target offset doesn't
                    // move but the jump position does, so the encoded distance
                    // must shrink by sizeDelta. For unrelated backward jumps
                    // whose target coincidentally lands at rewriteIndex without
                    // the rewriter intentionally matching at a jump target,
                    // staying strict avoids corrupting their offsets.
                    needsAdjustment = (currentTarget < rewriteIndex and i > rewriteIndex);
                    if (not needsAdjustment
                        and currentTarget == rewriteIndex
                        and i > rewriteIndex
                        and rewriteIsAtJumpTarget)
                    {
                        needsAdjustment = true;
                    }
                }
                else
                {
                    // Forward jump: adjust ONLY if jump < rewriteIndex < target.
                    needsAdjustment = (i < rewriteIndex and currentTarget > rewriteIndex);
                }

                if (needsAdjustment)
                {
                    // Get current index and adjust it
                    uint32_t currentIndex;
                    if (isLongJumpInstruction(opcode))
                        currentIndex = extractLongJumpOffset(chunk, i);
                    else
                        currentIndex = extractShortJumpOffset(chunk, i);

                    LOG_MILE("BytecodeRewriter", "Adjusting jump at " << i << " (opcode=" << static_cast<int>(opcode)
                             << ", currentTarget=" << currentTarget << ", currentIndex=" << currentIndex
                             << ", rewriteIndex=" << rewriteIndex << ", sizeDelta=" << sizeDelta << ")");

                    // Calculate new index based on direction
                    int newIndex;
                    if (opcode == OpCode::OP_Loop or opcode == OpCode::OP_Long_Loop)
                    {
                        // Backward jump: jump instruction moved by sizeDelta, target stayed same
                        // If sizeDelta = -2 (chunk shrank), jump moved 2 bytes closer to target, so index decreases by 2
                        newIndex = static_cast<int>(currentIndex) + sizeDelta;
                    }
                    else
                    {
                        // Forward jump: target moved by sizeDelta, jump instruction stayed same
                        // If sizeDelta = -2 (chunk shrank), target moved 2 bytes closer, so index decreases by 2
                        newIndex = static_cast<int>(currentIndex) + sizeDelta;
                    }

                    LOG_MILE("BytecodeRewriter", "Index adjusted from " << currentIndex << " to " << newIndex);

                    if (newIndex < 0)
                    {
                        LOG_WARNING("BytecodeRewriter", "Jump index became negative, setting to 0");
                        newIndex = 0;
                    }

                    // Write adjusted index - PRESERVE INSTRUCTION TYPE
                    if (isLongJumpInstruction(opcode))
                    {
                        writeLongJumpOffset(chunk, i, static_cast<uint32_t>(newIndex));
                    }
                    else
                    {
                        if (newIndex <= 65535)
                        {
                            writeShortJumpOffset(chunk, i, static_cast<uint16_t>(newIndex));
                        }
                        else
                        {
                            LOG_WARNING("BytecodeRewriter", "Short jump index overflow: " << newIndex << " at position " << i << " - keeping original index");
                            // Keep original index rather than converting to long jump
                        }
                    }
                }
                else
                {
                    LOG_MILE("BytecodeRewriter", "Not Adjusting jump at " << i << " (opcode=" << opcodeToString(static_cast<OpCode>(opcode))
                             << ", currentTarget=" << currentTarget << ", rewriteIndex=" << rewriteIndex << ", sizeDelta=" << sizeDelta << ")");
                }
            }
        }

        // The jump lists are brought up to date by the caller, once the bytes have changed
        // (forgetJumpsIn, followRewrite): collected here they would mix old and new offsets

        LOG_MILE("BytecodeRewriter", "Jump offset adjustment completed");
    }

}