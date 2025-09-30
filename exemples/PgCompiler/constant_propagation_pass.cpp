#include "constant_propagation_pass.h"
#include "logger.h"

namespace pg {

    bool ConstantPropagationPass::runPass(Chunk& chunk, BytecodeRewriter* rewriter) {
        if (!rewriter) {
            LOG_ERROR("ConstantPropagationPass", "No rewriter provided");
            return false;
        }

        LOG_INFO("ConstantPropagationPass", "Starting constant propagation");

        // Step 1: Analyze the bytecode to find constant assignments
        localConstants.clear();
        globalConstants.clear();
        analyzeConstantAssignments(chunk);

        // Step 2: Clear existing rules and add our constant propagation patterns
        setupConstantPropagationPatterns(rewriter);

        // Step 3: Apply transformations
        bool modified = rewriter->rewrite(chunk);

        if (modified) {
            LOG_INFO("ConstantPropagationPass", "Successfully propagated constants");
        } else {
            LOG_INFO("ConstantPropagationPass", "No constant propagation opportunities found");
        }

        return modified;
    }

    void ConstantPropagationPass::analyzeConstantAssignments(const Chunk& chunk) {
        size_t offset = 0;

        while (offset < chunk.code.size()) {
            OpCode currentOp = static_cast<OpCode>(chunk.code[offset]);

            // Look for new local pattern: constant -> index_access -> set_local
            if (isConstantInstruction(currentOp)) {
                size_t constantOffset = offset;
                size_t constantSize = getInstructionSize(currentOp);

                // Extract constant operands (directly from bytecode after opcode)
                std::vector<uint8_t> constantOperands;
                for (size_t i = 1; i < constantSize; ++i) {
                    if (constantOffset + i < chunk.code.size()) {
                        constantOperands.push_back(chunk.code[constantOffset + i]);
                    }
                }

                // Look for the next instruction (should be OP_Index_Access)
                size_t indexOffset = offset + constantSize;
                if (indexOffset < chunk.code.size()) {
                    OpCode indexOp = static_cast<OpCode>(chunk.code[indexOffset]);

                    if (indexOp == OpCode::OP_Index_Access || indexOp == OpCode::OP_Long_Index_Access) {
                        size_t indexSize = getInstructionSize(indexOp);
                        
                        // Extract slot index from OP_Index_Access
                        uint8_t variableSlot = 0;
                        if (indexOp == OpCode::OP_Index_Access && indexOffset + 1 < chunk.code.size()) {
                            size_t slotConstantIndex = chunk.code[indexOffset + 1];
                            if (slotConstantIndex < chunk.constants.size()) {
                                variableSlot = chunk.constants[slotConstantIndex].get<int>();
                            }
                        } else if (indexOp == OpCode::OP_Long_Index_Access && indexOffset + 3 < chunk.code.size()) {
                            size_t slotConstantIndex = (chunk.code[indexOffset + 1] << 16) |
                                                     (chunk.code[indexOffset + 2] << 8) |
                                                      chunk.code[indexOffset + 3];
                            if (slotConstantIndex < chunk.constants.size()) {
                                variableSlot = chunk.constants[slotConstantIndex].get<int>();
                            }
                        }

                        // Look for OP_Set_Local after OP_Index_Access
                        size_t setOffset = indexOffset + indexSize;
                        if (setOffset < chunk.code.size()) {
                            OpCode setOp = static_cast<OpCode>(chunk.code[setOffset]);
                            
                            if (setOp == OpCode::OP_Set_Local) {
                                ElementType constantValue = extractConstantValue(chunk, currentOp, constantOperands);

                                VariableConstantInfo info(constantValue, currentOp, constantOperands, constantOffset);
                                localConstants[variableSlot] = info;
                                LOG_INFO("ConstantPropagationPass", "Found local constant assignment: slot "
                                         << (int)variableSlot << " = " << constantValue.toString() << " at offset " << constantOffset);
                            }
                        }
                    }
                }
            }

            // Look for global pattern: constant_value -> constant_name -> define_global
            if (isConstantInstruction(currentOp)) {
                size_t valueOffset = offset;
                size_t valueSize = getInstructionSize(currentOp);

                // Extract value constant operands
                std::vector<uint8_t> valueOperands;
                for (size_t i = 1; i < valueSize; ++i) {
                    if (valueOffset + i < chunk.code.size()) {
                        valueOperands.push_back(chunk.code[valueOffset + i]);
                    }
                }

                // Look for name constant
                size_t nameOffset = offset + valueSize;
                if (nameOffset < chunk.code.size()) {
                    OpCode nameOp = static_cast<OpCode>(chunk.code[nameOffset]);

                    if (isConstantInstruction(nameOp)) {
                        size_t nameSize = getInstructionSize(nameOp);

                        // Extract name constant operands
                        std::vector<uint8_t> nameOperands;
                        for (size_t i = 1; i < nameSize; ++i) {
                            if (nameOffset + i < chunk.code.size()) {
                                nameOperands.push_back(chunk.code[nameOffset + i]);
                            }
                        }

                        // Look for define_global
                        size_t defineOffset = nameOffset + nameSize;
                        if (defineOffset < chunk.code.size()) {
                            OpCode defineOp = static_cast<OpCode>(chunk.code[defineOffset]);

                            if (defineOp == OpCode::OP_Define_Global) {
                                // Found the global definition pattern!
                                ElementType constantValue = extractConstantValue(chunk, currentOp, valueOperands);
                                ElementType variableName = extractConstantValue(chunk, nameOp, nameOperands);

                                // Convert variable name to string
                                std::string varNameStr;
                                if (variableName.isLitteral()) {
                                    varNameStr = variableName.toString();
                                } else {
                                    // Convert other types to string
                                    varNameStr = "var_" + std::to_string(nameOperands.empty() ? 0 : nameOperands[0]);
                                }

                                VariableConstantInfo info(constantValue, currentOp, valueOperands, valueOffset);
                                globalConstants[varNameStr] = info;
                                LOG_INFO("ConstantPropagationPass", "Found global constant assignment: '"
                                         << varNameStr << "' at offset " << valueOffset);
                            }
                        }
                    }
                }
            }

            offset += getInstructionSize(currentOp);
        }
    }

    void ConstantPropagationPass::setupConstantPropagationPatterns(BytecodeRewriter* rewriter) {
        // Pattern: constant -> store -> load (same variable)
        // Transform: replace the load with the constant

        // Local variable constant propagation
        // Pattern: constant_value -> constant_slot -> set_local -> constant_slot -> get_local
        std::vector<PatternElement> localPattern = {
            PatternElement::constant(true),                           // Capture: constant value
            PatternElement::match(OpCode::OP_Index_Access, true),     // Capture: slot index access
            PatternElement::match(OpCode::OP_Set_Local, true),        // Capture: store instruction
            PatternElement::match(OpCode::OP_Index_Access, true),     // Capture: slot index access (again)
            PatternElement::match(OpCode::OP_Get_Local, true)         // Capture: load instruction
        };

        auto localTransform = [](const std::vector<CapturedInstruction>& captured) -> std::vector<uint8_t> {
            if (captured.size() != 5) {
                return {};
            }

            const auto& constantInstr = captured[0];  // value constant
            const auto& slotInstr1 = captured[1];     // slot constant (for store)
            const auto& storeInstr = captured[2];     // set_local
            const auto& slotInstr2 = captured[3];     // slot constant (for load)
            const auto& loadInstr = captured[4];      // get_local

            // Check if both slot constants refer to the same slot
            if (slotInstr1.operands != slotInstr2.operands) {
                return {}; // Different slot numbers
            }

            // CRITICAL: Check if there are any control flow instructions between store and load
            // If there are jumps, loops, or branches, the variable might be modified
            size_t storeOffset = storeInstr.offset;
            size_t loadOffset = loadInstr.offset;
            
            // For safety, only propagate if the load immediately follows the store sequence
            // This prevents propagation across control flow boundaries
            size_t expectedLoadOffset = storeOffset + 2; // set_local + pop typically = 2 bytes
            if (loadOffset > expectedLoadOffset + 5) { // Allow small gap for other safe instructions
                return {}; // Too much code between store and load - unsafe to propagate
            }
            
            LOG_INFO("ConstantPropagationPass", "Replacing local variable load with constant");

            // Replace the load with the constant
            std::vector<uint8_t> result;
            result.push_back(static_cast<uint8_t>(constantInstr.opcode));
            result.insert(result.end(), constantInstr.operands.begin(), constantInstr.operands.end());

            return result;
        };

        rewriter->addAdvancedRule(localPattern, localTransform);

        // Boolean constant propagation (OP_True/OP_False)
        // Pattern: OP_True -> constant_slot -> set_local -> constant_slot -> get_local
        std::vector<PatternElement> boolTruePattern = {
            PatternElement::match(OpCode::OP_True, true),               // Capture: OP_True
            PatternElement::match(OpCode::OP_Index_Access, true),       // Capture: slot index access
            PatternElement::match(OpCode::OP_Set_Local, true),          // Capture: store instruction
            PatternElement::match(OpCode::OP_Index_Access, true),       // Capture: slot index access (again)
            PatternElement::match(OpCode::OP_Get_Local, true)           // Capture: load instruction
        };

        auto boolTrueTransform = [](const std::vector<CapturedInstruction>& captured) -> std::vector<uint8_t> {
            if (captured.size() != 5) {
                return {};
            }

            const auto& slotInstr1 = captured[1];     // slot constant (for store)
            const auto& slotInstr2 = captured[3];     // slot constant (for load)

            // Check if both slot constants refer to the same slot
            if (slotInstr1.operands != slotInstr2.operands) {
                return {}; // Different slot numbers
            }

            LOG_INFO("ConstantPropagationPass", "Replacing local boolean load with OP_True");

            // Replace the load with OP_True
            std::vector<uint8_t> result;
            result.push_back(static_cast<uint8_t>(OpCode::OP_True));

            return result;
        };

        rewriter->addAdvancedRule(boolTruePattern, boolTrueTransform);

        // Similar pattern for OP_False
        std::vector<PatternElement> boolFalsePattern = {
            PatternElement::match(OpCode::OP_False, true),              // Capture: OP_False
            PatternElement::match(OpCode::OP_Index_Access, true),       // Capture: slot index access
            PatternElement::match(OpCode::OP_Set_Local, true),          // Capture: store instruction
            PatternElement::match(OpCode::OP_Index_Access, true),       // Capture: slot index access (again)
            PatternElement::match(OpCode::OP_Get_Local, true)           // Capture: load instruction
        };

        auto boolFalseTransform = [](const std::vector<CapturedInstruction>& captured) -> std::vector<uint8_t> {
            if (captured.size() != 5) {
                return {};
            }

            const auto& slotInstr1 = captured[1];     // slot constant (for store)
            const auto& slotInstr2 = captured[3];     // slot constant (for load)

            // Check if both slot constants refer to the same slot
            if (slotInstr1.operands != slotInstr2.operands) {
                return {}; // Different slot numbers
            }

            LOG_INFO("ConstantPropagationPass", "Replacing local boolean load with OP_False");

            // Replace the load with OP_False
            std::vector<uint8_t> result;
            result.push_back(static_cast<uint8_t>(OpCode::OP_False));

            return result;
        };

        rewriter->addAdvancedRule(boolFalsePattern, boolFalseTransform);

        LOG_INFO("ConstantPropagationPass", "Added constant propagation patterns");
    }

    bool ConstantPropagationPass::isConstantInstruction(OpCode opcode) const {
        return opcode == OpCode::OP_Constant ||
               opcode == OpCode::OP_LongConstant ||
               opcode == OpCode::OP_True ||
               opcode == OpCode::OP_False;
    }

    ElementType ConstantPropagationPass::extractConstantValue(const Chunk& chunk, OpCode opcode,
                                                            const std::vector<uint8_t>& operands) const {
        switch (opcode) {
            case OpCode::OP_Constant:
                if (!operands.empty() && operands[0] < chunk.constants.size()) {
                    return chunk.constants[operands[0]];
                }
                break;

            case OpCode::OP_LongConstant:
                if (operands.size() >= 3) {
                    uint32_t index = static_cast<uint32_t>(operands[0]) |
                                   (static_cast<uint32_t>(operands[1]) << 8) |
                                   (static_cast<uint32_t>(operands[2]) << 16);
                    if (index < chunk.constants.size()) {
                        return chunk.constants[index];
                    }
                }
                break;

            case OpCode::OP_True:
                return ElementType(true);

            case OpCode::OP_False:
                return ElementType(false);

            default:
                break;
        }

        // Default fallback
        return ElementType(0);
    }

    uint8_t ConstantPropagationPass::extractVariableSlot(const std::vector<uint8_t>& operands) const {
        return operands.empty() ? 0 : operands[0];
    }

    void ConstantPropagationPass::resetState() {
        localConstants.clear();
        globalConstants.clear();
    }

}