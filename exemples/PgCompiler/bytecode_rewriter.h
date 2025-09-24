#pragma once

#include "chunk.h"
#include <vector>
#include <functional>
#include <unordered_map>

namespace pg {

    struct RewriteRule {
        std::vector<OpCode> pattern;
        std::vector<OpCode> replacement;
        
        RewriteRule(const std::vector<OpCode>& pat, const std::vector<OpCode>& repl)
            : pattern(pat), replacement(repl) {}
    };

    struct RewriteContext {
        size_t matchOffset;
        size_t patternSize;
        size_t replacementSize;
        std::vector<uint8_t> originalBytes;
        std::vector<int> originalLines;
        
        int sizeDelta() const { 
            return static_cast<int>(replacementSize) - static_cast<int>(patternSize); 
        }
    };

    class BytecodeRewriter {
    private:
        std::vector<RewriteRule> rules;
        std::unordered_map<size_t, int> offsetAdjustments;
        
    public:
        void addRule(const std::vector<OpCode>& pattern, const std::vector<OpCode>& replacement);
        
        void addRule(OpCode pattern, OpCode replacement);
        
        bool rewrite(Chunk& chunk);
        
        bool rewriteAt(Chunk& chunk, size_t index, size_t size, const std::vector<OpCode>& replacement);
        
        void clearRules();
        
        size_t getRuleCount() const { return rules.size(); }
        
    private:
        bool findAndApplyRewrites(Chunk& chunk);
        
        bool matchesPattern(const Chunk& chunk, size_t offset, const std::vector<OpCode>& pattern);
        
        void applyRewrite(Chunk& chunk, size_t offset, const RewriteRule& rule);
        
        void adjustJumpOffsets(Chunk& chunk, const std::vector<RewriteContext>& contexts);
        
        void recalculateAllJumpOffsets(Chunk& chunk);
        
        void adjustSingleJump(Chunk& chunk, size_t jumpOffset, OpCode jumpOpcode, int totalAdjustment);
        
        size_t getInstructionSize(OpCode opcode) const;
        
        size_t getPatternByteSize(const std::vector<OpCode>& pattern) const;
        
        size_t getReplacementByteSize(const std::vector<OpCode>& replacement) const;
        
        bool isJumpInstruction(OpCode opcode) const;
        
        bool isLongJumpInstruction(OpCode opcode) const;
        
        bool isShortJumpInstruction(OpCode opcode) const;
        
        uint32_t extractLongJumpOffset(const Chunk& chunk, size_t offset) const;
        
        uint16_t extractShortJumpOffset(const Chunk& chunk, size_t offset) const;
        
        void writeLongJumpOffset(Chunk& chunk, size_t offset, uint32_t jumpOffset) const;
        
        void writeShortJumpOffset(Chunk& chunk, size_t offset, uint16_t jumpOffset) const;
        
        size_t calculateJumpTarget(const Chunk& chunk, size_t jumpOffset, OpCode jumpOpcode) const;
    };

}