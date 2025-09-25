#pragma once

#include "chunk.h"
#include <vector>
#include <functional>
#include <optional>

namespace pg {

    // Represents a captured instruction with its operands
    struct CapturedInstruction {
        OpCode opcode;
        std::vector<uint8_t> operands; // Raw operand bytes
        size_t offset; // Position in original bytecode

        // Helper to extract 32-bit constant index
        uint32_t getConstantIndex() const {
            if (operands.size() >= 4) {
                return static_cast<uint32_t>(operands[0]) |
                       (static_cast<uint32_t>(operands[1]) << 8) |
                       (static_cast<uint32_t>(operands[2]) << 16) |
                       (static_cast<uint32_t>(operands[3]) << 24);
            }
            return 0;
        }
    };

    // Pattern element - can match specific opcode or be a wildcard
    struct PatternElement {
        std::optional<OpCode> opcode; // nullopt means wildcard
        bool capture; // Whether to capture this instruction

        PatternElement(OpCode op, bool cap = false) : opcode(op), capture(cap) {}
        PatternElement() : capture(false) {} // Wildcard constructor

        static PatternElement wildcard(bool cap = false) {
            PatternElement elem;
            elem.capture = cap;
            return elem;
        }

        static PatternElement match(OpCode op, bool cap = false) {
            return PatternElement(op, cap);
        }
    };

    // Advanced rewrite rule with lambda support
    struct AdvancedRewriteRule {
        std::vector<PatternElement> pattern;
        std::function<std::vector<uint8_t>(const std::vector<CapturedInstruction>&)> transform;

        AdvancedRewriteRule(const std::vector<PatternElement>& pat,
                           std::function<std::vector<uint8_t>(const std::vector<CapturedInstruction>&)> trans)
            : pattern(pat), transform(trans) {}
    };

    struct RewriteRule {
        std::vector<OpCode> pattern;
        std::vector<OpCode> replacement;

        RewriteRule(const std::vector<OpCode>& pat, const std::vector<OpCode>& repl)
            : pattern(pat), replacement(repl) {}
    };


    class BytecodeRewriter {
    private:
        std::vector<RewriteRule> rules;
        std::vector<AdvancedRewriteRule> advancedRules;

    public:
        void addRule(const std::vector<OpCode>& pattern, const std::vector<OpCode>& replacement);

        void addRule(OpCode pattern, OpCode replacement);

        // New advanced pattern matching API
        void addAdvancedRule(const std::vector<PatternElement>& pattern,
                            std::function<std::vector<uint8_t>(const std::vector<CapturedInstruction>&)> transform);

        bool rewrite(Chunk& chunk);
        
        bool rewriteAt(Chunk& chunk, size_t index, size_t size, const std::vector<OpCode>& replacement);
        
        bool rewriteAtRaw(Chunk& chunk, size_t index, size_t size, const std::vector<uint8_t>& replacement);

        bool removeInstructions(Chunk& chunk, size_t index, size_t count);

        void clearRules();
        
        size_t getRuleCount() const { return rules.size(); }
        
    private:
        bool findAndApplyRewrites(Chunk& chunk);

        bool findAndApplyAdvancedRewrites(Chunk& chunk);

        bool matchesPattern(const Chunk& chunk, size_t offset, const std::vector<OpCode>& pattern);

        std::optional<std::pair<std::vector<CapturedInstruction>, size_t>>
        matchesAdvancedPattern(const Chunk& chunk, size_t offset, const std::vector<PatternElement>& pattern);

        void applyRewrite(Chunk& chunk, size_t offset, const RewriteRule& rule);

        void applyAdvancedRewrite(Chunk& chunk, size_t offset, size_t patternSize, const std::vector<uint8_t>& replacement);
        
        void adjustJumpOffsetsAfterRewrite(Chunk& chunk, size_t rewriteIndex, int sizeDelta);

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