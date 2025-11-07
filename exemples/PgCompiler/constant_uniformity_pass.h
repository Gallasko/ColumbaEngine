#pragma once

#include "bytecode_pass.h"
#include "chunk.h"
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace pg {

    struct ConstantMapping {
        size_t originalIndex;
        size_t newIndex;
        bool isDuplicate;

        ConstantMapping(size_t orig, size_t newIdx, bool dup = false)
            : originalIndex(orig), newIndex(newIdx), isDuplicate(dup) {}
    };

    class ConstantUniformityPass : public BytecodePass {
    public:
        std::string getName() const override { return "ConstantUniformity"; }

        bool runPass(Chunk& chunk, BytecodeRewriter* rewriter = nullptr) override;

        bool changesSize() const override { return true; }

        bool requiresMultiplePasses() const override { return false; }

    private:
        // Find duplicated constants and create mapping
        std::vector<ConstantMapping> analyzeDuplicates(const Chunk& chunk);

        // Update all constant references in bytecode with final mapping
        void updateAllConstantReferences(Chunk& chunk, const std::vector<size_t>& finalMapping,
                                        BytecodeRewriter* rewriter);

        // Helper methods
        bool areConstantsEqual(const ElementType& a, const ElementType& b);
    };

}