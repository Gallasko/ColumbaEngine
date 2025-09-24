#pragma once

#include "chunk.h"
#include <vector>

namespace pg {

    struct RewriteRule {
        std::vector<OpCode> pattern;
        std::vector<OpCode> replacement;
        
        RewriteRule(const std::vector<OpCode>& pat, const std::vector<OpCode>& repl)
            : pattern(pat), replacement(repl) {}
    };


    class BytecodeRewriter {
    private:
        std::vector<RewriteRule> rules;
        
    public:
        void addRule(const std::vector<OpCode>& pattern, const std::vector<OpCode>& replacement);
        
        void addRule(OpCode pattern, OpCode replacement);
        
        bool rewrite(Chunk& chunk);
        
        bool rewriteAt(Chunk& chunk, size_t index, size_t size, const std::vector<OpCode>& replacement);
        
        bool rewriteAtRaw(Chunk& chunk, size_t index, size_t size, const std::vector<uint8_t>& replacement);
        
        void clearRules();
        
        size_t getRuleCount() const { return rules.size(); }
        
    private:
        bool findAndApplyRewrites(Chunk& chunk);
        
        bool matchesPattern(const Chunk& chunk, size_t offset, const std::vector<OpCode>& pattern);
        
        void applyRewrite(Chunk& chunk, size_t offset, const RewriteRule& rule);
        
        void adjustJumpOffsetsAfterRewrite(Chunk& chunk, size_t rewriteIndex, int sizeDelta);
        
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