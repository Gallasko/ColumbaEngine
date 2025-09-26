#pragma once

#include "bytecode_pass.h"
#include "chunk.h"
#include <vector>
#include <unordered_map>
#include <string>
#include <cstdint>

namespace pg {

    struct VariableConstantInfo {
        bool isConstant;
        ElementType constantValue;
        OpCode constantOpcode;  // OP_Constant, OP_LongConstant, OP_True, OP_False
        std::vector<uint8_t> constantOperands;
        size_t definitionOffset;  // Where the constant was assigned

        VariableConstantInfo() : isConstant(false), definitionOffset(0) {}

        VariableConstantInfo(const ElementType& value, OpCode opcode,
                           const std::vector<uint8_t>& operands, size_t offset)
            : isConstant(true), constantValue(value), constantOpcode(opcode),
              constantOperands(operands), definitionOffset(offset) {}
    };

    class ConstantPropagationPass : public BytecodePass {
    public:
        std::string getName() const override { return "ConstantPropagation"; }

        bool runPass(Chunk& chunk, BytecodeRewriter* rewriter = nullptr) override;

        bool changesSize() const override { return false; }

        bool requiresMultiplePasses() const override { return true; }

    private:
        // Track constant values for local and global variables
        std::unordered_map<uint8_t, VariableConstantInfo> localConstants;     // local slot -> constant info
        std::unordered_map<std::string, VariableConstantInfo> globalConstants; // global name -> constant info

        // Analysis phase: identify constant assignments
        void analyzeConstantAssignments(const Chunk& chunk);

        // Transform phase: replace loads with constants
        void setupConstantPropagationPatterns(BytecodeRewriter* rewriter);

        // Helper methods
        bool isConstantInstruction(OpCode opcode) const;
        ElementType extractConstantValue(const Chunk& chunk, OpCode opcode,
                                       const std::vector<uint8_t>& operands) const;
        uint8_t extractVariableSlot(const std::vector<uint8_t>& operands) const;

        // Clear state between passes
        void resetState();
    };

}