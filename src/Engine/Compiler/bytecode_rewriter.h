#pragma once

#include "chunk.h"
#include <vector>
#include <functional>
#include <optional>
#include <algorithm>
#include <set>

namespace pg
{
    struct VM;

    // Represents a captured instruction with its operands
    struct CapturedInstruction
    {
        OpCode opcode;
        std::vector<uint8_t> operands; // Raw operand bytes
        size_t offset; // Position in original bytecode

        // Helper to extract 32-bit constant index
        uint32_t getConstantIndex() const
        {
            if (operands.size() >= 4)
            {
                return static_cast<uint32_t>(operands[0])         |
                       (static_cast<uint32_t>(operands[1]) << 8)  |
                       (static_cast<uint32_t>(operands[2]) << 16) |
                       (static_cast<uint32_t>(operands[3]) << 24);
            }

            if (operands.size() == 1)
            {
                return static_cast<uint32_t>(operands[0]);
            }

            return 0;
        }
    };

    // Pattern element - can match specific opcode(s) or be a wildcard
    struct PatternElement
    {
        std::vector<OpCode> opcodes; // Empty means wildcard, multiple means OR matching
        bool capture; // Whether to capture this instruction

        PatternElement(OpCode op, bool cap = false) : opcodes({op}), capture(cap) {}
        PatternElement(const std::vector<OpCode>& ops, bool cap = false) : opcodes(ops), capture(cap) {}
        PatternElement() : capture(false) {} // Wildcard constructor

        static PatternElement wildcard(bool cap = false)
        {
            PatternElement elem;
            elem.capture = cap;
            return elem;
        }

        static PatternElement match(OpCode op, bool cap = false)
        {
            return PatternElement(op, cap);
        }

        static PatternElement anyOf(const std::vector<OpCode>& ops, bool cap = false)
        {
            return PatternElement(ops, cap);
        }

        // Predefined common pattern groups
        static PatternElement constant(bool cap = false)
        {
            return anyOf({OpCode::OP_Constant, OpCode::OP_LongConstant}, cap);
        }

        static PatternElement load(bool cap = false)
        {
            return anyOf({OpCode::OP_Get_Local, OpCode::OP_Get_Global}, cap);
        }

        static PatternElement store(bool cap = false)
        {
            return anyOf({OpCode::OP_Set_Local, OpCode::OP_Set_Global}, cap);
        }

        static PatternElement increment(bool cap = false)
        {
            return anyOf({OpCode::OP_Incr_Local, OpCode::OP_Incr_Global,
                          OpCode::OP_Post_Incr_Local, OpCode::OP_Post_Incr_Global}, cap);
        }

        static PatternElement decrement(bool cap = false)
        {
            return anyOf({OpCode::OP_Decr_Local, OpCode::OP_Decr_Global,
                          OpCode::OP_Post_Decr_Local, OpCode::OP_Post_Decr_Global}, cap);
        }

        static PatternElement jump(bool cap = false)
        {
            return anyOf({OpCode::OP_Jump, OpCode::OP_Long_Jump}, cap);
        }

        static PatternElement conditionalJump(bool cap = false)
        {
            return anyOf({OpCode::OP_Jump_If_False, OpCode::OP_Long_Jump_If_False}, cap);
        }

        static PatternElement loop(bool cap = false)
        {
            return anyOf({OpCode::OP_Loop, OpCode::OP_Long_Loop}, cap);
        }

        static PatternElement comparison(bool cap = false)
        {
            return anyOf({OpCode::OP_Equal, OpCode::OP_NotEqual, OpCode::OP_Greater,
                         OpCode::OP_GreaterEqual, OpCode::OP_Less, OpCode::OP_LessEqual}, cap);
        }

        // Check if this pattern matches a given opcode
        bool matches(OpCode opcode) const
        {
            if (opcodes.empty())
                return true; // Wildcard matches everything

            return std::find(opcodes.begin(), opcodes.end(), opcode) != opcodes.end();
        }
    };

    // Advanced rewrite rule with lambda support
    struct AdvancedRewriteRule
    {
        std::vector<PatternElement> pattern;
        std::function<std::vector<uint8_t>(const std::vector<CapturedInstruction>&)> transform;
        std::function<bool(const std::vector<CapturedInstruction>&)> condition;

        AdvancedRewriteRule(const std::vector<PatternElement>& pat,
                           std::function<std::vector<uint8_t>(const std::vector<CapturedInstruction>&)> trans,
                           std::function<bool(const std::vector<CapturedInstruction>&)> cond) :
                           pattern(pat), transform(trans), condition(cond) {}
    };

    struct RewriteRule
    {
        std::vector<OpCode> pattern;
        std::vector<OpCode> replacement;

        RewriteRule(const std::vector<OpCode>& pat, const std::vector<OpCode>& repl) : pattern(pat), replacement(repl) {}
    };

    class BytecodeRewriter
    {
    private:
        std::vector<RewriteRule> rules;
        std::vector<AdvancedRewriteRule> advancedRules;

        std::set<size_t> jumpTargets;

        VM *vm = nullptr;

    public:
        void setVm(VM* vmInstance) { vm = vmInstance; }
        VM* getVm() const { return vm; }

        void addRule(const std::vector<OpCode>& pattern, const std::vector<OpCode>& replacement);

        void addRule(OpCode pattern, OpCode replacement);

        // New advanced pattern matching API
        void addAdvancedRule(const std::vector<PatternElement>& pattern,
                            std::function<std::vector<uint8_t>(const std::vector<CapturedInstruction>&)> transform,
                            std::function<bool(const std::vector<CapturedInstruction>&)> condition = [](const std::vector<CapturedInstruction>&)-> bool { return true; });

        bool rewrite(Chunk& chunk);

        bool rewriteAt(Chunk& chunk, size_t index, size_t size, const std::vector<OpCode>& replacement);

        bool rewriteAtRaw(Chunk& chunk, size_t index, size_t size, const std::vector<uint8_t>& replacement);

        bool removeInstructions(Chunk& chunk, size_t index, size_t count);

        void clearRules();

        size_t getRuleCount() const { return rules.size(); }

    private:
        void collectJumpTargets(const Chunk& chunk);

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