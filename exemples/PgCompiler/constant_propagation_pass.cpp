#include "constant_propagation_pass.h"
#include "logger.h"

namespace pg {

    bool ConstantPropagationPass::runPass(Chunk& chunk, BytecodeRewriter* rewriter) {
        if (!rewriter) {
            LOG_ERROR("ConstantPropagationPass", "No rewriter provided");
            return false;
        }

        LOG_INFO("ConstantPropagationPass", "Starting constant propagation analysis");

        // Reset state from previous runs
        resetState();

        // Phase 1: Analyze the bytecode to find constant assignments
        analyzeConstantAssignments(chunk);

        // Log what we found
        LOG_INFO("ConstantPropagationPass", "Found " << localConstants.size()
                 << " local constants and " << globalConstants.size() << " global constants");

        if (localConstants.empty() && globalConstants.empty()) {
            LOG_INFO("ConstantPropagationPass", "No constant assignments found");
            return false;
        }

        // Phase 2: Set up patterns to replace loads with constants
        rewriter->clearRules();
        setupConstantPropagationPatterns(rewriter);

        // Phase 3: Apply transformations
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

            // Look for local pattern: constant -> set_local
            if (isConstantInstruction(currentOp)) {
                size_t constantOffset = offset;
                size_t constantSize = getInstructionSize(currentOp);

                // Extract constant operands
                std::vector<uint8_t> constantOperands;
                for (size_t i = 1; i < constantSize; ++i) {
                    if (constantOffset + i < chunk.code.size()) {
                        constantOperands.push_back(chunk.code[constantOffset + i]);
                    }
                }

                // Look for the next instruction (should be a store)
                size_t nextOffset = offset + constantSize;
                if (nextOffset < chunk.code.size()) {
                    OpCode nextOp = static_cast<OpCode>(chunk.code[nextOffset]);

                    if (nextOp == OpCode::OP_Set_Local) {
                        // Found constant -> set_local pattern
                        size_t storeSize = getInstructionSize(nextOp);
                        std::vector<uint8_t> storeOperands;

                        for (size_t i = 1; i < storeSize; ++i) {
                            if (nextOffset + i < chunk.code.size()) {
                                storeOperands.push_back(chunk.code[nextOffset + i]);
                            }
                        }

                        if (!storeOperands.empty()) {
                            uint8_t variableSlot = storeOperands[0];
                            ElementType constantValue = extractConstantValue(chunk, currentOp, constantOperands);

                            VariableConstantInfo info(constantValue, currentOp, constantOperands, constantOffset);
                            localConstants[variableSlot] = info;
                            LOG_INFO("ConstantPropagationPass", "Found local constant assignment: slot "
                                     << (int)variableSlot << " at offset " << constantOffset);
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
        // Pattern for local variable constant propagation
        for (const auto& pair : localConstants) {
            uint8_t slot = pair.first;
            const VariableConstantInfo& info = pair.second;

            std::vector<PatternElement> pattern = {
                PatternElement::match(OpCode::OP_Get_Local, true)  // Capture the load
            };

            auto transform = [slot, info](const std::vector<CapturedInstruction>& captured) -> std::vector<uint8_t> {
                if (captured.size() != 1) {
                    return {};
                }

                const auto& loadInstr = captured[0];

                // Check if this load is for our constant variable
                if (loadInstr.operands.empty() || loadInstr.operands[0] != slot) {
                    return {}; // Different variable
                }

                LOG_INFO("ConstantPropagationPass", "Replacing local load with constant");

                // Generate the constant instruction directly
                std::vector<uint8_t> result;
                result.push_back(static_cast<uint8_t>(info.constantOpcode));
                result.insert(result.end(), info.constantOperands.begin(), info.constantOperands.end());

                return result;
            };

            rewriter->addAdvancedRule(pattern, transform);
        }

        // For now, skip global constant propagation as it requires more complex analysis
        // The pattern is: constant_name -> get_global, and we need access to the chunk
        // in the transform function to validate the variable name
        LOG_INFO("ConstantPropagationPass", "Global constant propagation not yet implemented");

        // TODO: Implement global constant propagation with chunk access in transform

        LOG_INFO("ConstantPropagationPass", "Added " << (localConstants.size() + globalConstants.size())
                 << " constant propagation patterns");
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