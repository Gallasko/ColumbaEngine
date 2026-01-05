#include "constant_uniformity_pass.h"
#include "logger.h"
#include <algorithm>

namespace pg {

    bool ConstantUniformityPass::runPass(Chunk& chunk, BytecodeRewriter* rewriter) {
        if (chunk.constants.size() <= 1) {
            // No constants or only one constant, nothing to optimize
            return false;
        }

        if (!rewriter) {
            LOG_ERROR("ConstantUniformityPass", "No rewriter provided");
            return false;
        }

        LOG_INFO("ConstantUniformityPass", "Analyzing " << chunk.constants.size() << " constants for duplicates");

        // Step 1: Analyze for duplicates
        auto mapping = analyzeDuplicates(chunk);

        // Count duplicates
        size_t duplicateCount = 0;
        for (const auto& map : mapping) {
            if (map.isDuplicate) {
                duplicateCount++;
            }
        }

        if (duplicateCount == 0) {
            LOG_INFO("ConstantUniformityPass", "No duplicate constants found");
            return false;
        }

        LOG_INFO("ConstantUniformityPass", "Found " << duplicateCount << " duplicate constants");

        // Step 2: Create compact constants array and final mapping
        std::vector<ElementType> newConstants;
        std::vector<size_t> finalMapping; // old index -> new compact index
        finalMapping.resize(chunk.constants.size());

        // Build new constants array and final mapping
        for (size_t i = 0; i < mapping.size(); ++i) {
            if (!mapping[i].isDuplicate) {
                finalMapping[i] = newConstants.size();
                // newConstants.push_back(chunk.constants[i]);
            }
        }

        // Set final mapping for duplicates
        for (size_t i = 0; i < mapping.size(); ++i) {
            if (mapping[i].isDuplicate) {
                size_t originalFirstIndex = mapping[i].newIndex;
                // Use the final mapping of the first occurrence
                finalMapping[i] = finalMapping[originalFirstIndex];
            }
        }


        // Step 3: Update all constant references with final indices
        updateAllConstantReferences(chunk, finalMapping, rewriter);

        // Step 4: Replace constants array
        // chunk.constants = std::move(newConstants);

        LOG_INFO("ConstantUniformityPass", "Optimization complete. Constants reduced from "
                 << (mapping.size()) << " to " << chunk.constants.size());

        return true;
    }

    std::vector<ConstantMapping> ConstantUniformityPass::analyzeDuplicates(const Chunk& chunk) {
        std::vector<ConstantMapping> mapping;
        std::unordered_map<std::string, size_t> seenConstants; // value string -> first index

        for (size_t i = 0; i < chunk.constants.size(); ++i) {
            // const auto& constant = chunk.constants[i];
            // std::string valueStr = constant.toString();
            auto valueStr = "";

            auto it = seenConstants.find(valueStr);
            if (it != seenConstants.end()) {
                // Found duplicate - map to the first occurrence
                mapping.emplace_back(i, it->second, true);
                LOG_INFO("ConstantUniformityPass", "Duplicate constant at index " << i
                         << " (" << valueStr << ") -> maps to index " << it->second);
            } else {
                // First occurrence - maps to itself
                seenConstants[valueStr] = i;
                mapping.emplace_back(i, i, false);
            }
        }

        return mapping;
    }

    void ConstantUniformityPass::updateAllConstantReferences(Chunk& chunk,
                                                             const std::vector<size_t>& finalMapping,
                                                             BytecodeRewriter* rewriter) {
        // Collect all constant reference locations first to avoid issues with shifting offsets
        struct ConstantRef {
            size_t offset;
            OpCode opcode;
            size_t originalIndex;
            size_t newIndex;
        };

        std::vector<ConstantRef> constantRefs;

        // First pass: collect all constant references
        size_t codeOffset = 0;
        while (codeOffset < chunk.code.size()) {
            OpCode opcode = static_cast<OpCode>(chunk.code[codeOffset]);

            if (opcode == OpCode::OP_Constant) {
                if (codeOffset + 1 >= chunk.code.size()) break;

                size_t originalIndex = chunk.code[codeOffset + 1];
                if (originalIndex < finalMapping.size()) {
                    size_t newIndex = finalMapping[originalIndex];
                    // Always add to update list - we need to update ALL references to use final indices
                    constantRefs.push_back({codeOffset, opcode, originalIndex, newIndex});
                }
                codeOffset += 2;

            } else if (opcode == OpCode::OP_LongConstant) {
                if (codeOffset + 3 >= chunk.code.size()) break;

                size_t originalIndex = (chunk.code[codeOffset + 1] << 16) |
                                     (chunk.code[codeOffset + 2] << 8) |
                                      chunk.code[codeOffset + 3];

                if (originalIndex < finalMapping.size()) {
                    size_t newIndex = finalMapping[originalIndex];
                    // Always add to update list - we need to update ALL references to use final indices
                    constantRefs.push_back({codeOffset, opcode, originalIndex, newIndex});
                }
                codeOffset += 4;

            } else {
                codeOffset += pg::getInstructionSize(opcode);
            }
        }

        // Second pass: update all references (reverse order to avoid offset issues)
        for (int i = constantRefs.size() - 1; i >= 0; --i) {
            const auto& ref = constantRefs[i];

            if (ref.originalIndex != ref.newIndex) {
                LOG_INFO("ConstantUniformityPass", "Updating " <<
                        (ref.opcode == OpCode::OP_Constant ? "OP_Constant" : "OP_LongConstant") <<
                        " reference: " << ref.originalIndex << " -> " << ref.newIndex <<
                        " at offset " << ref.offset);
            }

            if (ref.opcode == OpCode::OP_Constant) {
                std::vector<uint8_t> newInstruction = {
                    static_cast<uint8_t>(OpCode::OP_Constant),
                    static_cast<uint8_t>(ref.newIndex)
                };
                rewriter->rewriteAtRaw(chunk, ref.offset, 2, newInstruction);
            } else if (ref.opcode == OpCode::OP_LongConstant) {
                std::vector<uint8_t> newInstruction = {
                    static_cast<uint8_t>(OpCode::OP_LongConstant),
                    static_cast<uint8_t>((ref.newIndex >> 16) & 0xFF),
                    static_cast<uint8_t>((ref.newIndex >> 8) & 0xFF),
                    static_cast<uint8_t>(ref.newIndex & 0xFF)
                };
                rewriter->rewriteAtRaw(chunk, ref.offset, 4, newInstruction);
            }
        }

        LOG_INFO("ConstantUniformityPass", "Updated " << constantRefs.size() <<
                 " constant references to final indices");
    }

    bool ConstantUniformityPass::areConstantsEqual(const ElementType& a, const ElementType& b) {
        // Use string representation for comparison as it handles all types uniformly
        return a.toString() == b.toString();
    }

}