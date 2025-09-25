#include "post_increment_optimization_pass.h"
#include "logger.h"

namespace pg {

    bool PostIncrementOptimizationPass::runPass(Chunk& chunk, BytecodeRewriter* rewriter) {
        if (!rewriter) {
            LOG_ERROR("PostIncrementOptimizationPass", "No rewriter provided");
            return false;
        }

        LOG_INFO("PostIncrementOptimizationPass", "Setting up post-increment optimization rule");

        rewriter->clearRules();

        // Pattern: OP_Constant(capture) + OP_Get_Global + OP_Constant(capture) + OP_Post_Incr_Global
        // We want to match and capture the two constants to verify they're the same variable
        std::vector<PatternElement> pattern = {
            PatternElement::match(OpCode::OP_Constant, true),    // capture first constant
            PatternElement::match(OpCode::OP_Get_Global),        // match OP_Get_Global
            PatternElement::match(OpCode::OP_Constant, true),    // capture second constant
            PatternElement::match(OpCode::OP_Post_Incr_Global)   // match OP_Post_Incr_Global
        };

        // Lambda to transform the captured pattern
        auto transform = [](const std::vector<CapturedInstruction>& captured) -> std::vector<uint8_t> {
            if (captured.size() != 2) {
                LOG_WARNING("PostIncrementOptimizationPass", "Expected 2 captured instructions, got " << captured.size());
                return {}; // Return empty to indicate no transformation
            }

            const auto& firstConst = captured[0];
            const auto& secondConst = captured[1];

            // Verify both constants refer to the same variable
            uint32_t firstIndex = firstConst.getConstantIndex();
            uint32_t secondIndex = secondConst.getConstantIndex();

            if (firstIndex != secondIndex) {
                LOG_INFO("PostIncrementOptimizationPass",
                        "Constants refer to different variables (" << firstIndex
                        << " != " << secondIndex << "), skipping optimization");
                return {}; // No transformation
            }

            LOG_INFO("PostIncrementOptimizationPass",
                    "Optimizing post-increment pattern for variable index " << secondIndex);

            // Generate replacement: OP_Constant + operands + OP_Post_Incr_Global
            std::vector<uint8_t> replacement;

            // Add the second constant (the one we want to keep)
            replacement.push_back(static_cast<uint8_t>(OpCode::OP_Constant));
            replacement.insert(replacement.end(), secondConst.operands.begin(), secondConst.operands.end());

            // Add the post-increment instruction
            replacement.push_back(static_cast<uint8_t>(OpCode::OP_Post_Incr_Global));

            LOG_INFO("PostIncrementOptimizationPass",
                    "Generated " << replacement.size() << " byte replacement");

            return replacement;
        };

        // Add the advanced rule
        rewriter->addAdvancedRule(pattern, transform);

        LOG_INFO("PostIncrementOptimizationPass", "Running bytecode rewriter");

        // Apply the rewrite
        bool modified = rewriter->rewrite(chunk);

        if (modified) {
            LOG_INFO("PostIncrementOptimizationPass", "Successfully applied post-increment optimization");
        } else {
            LOG_INFO("PostIncrementOptimizationPass", "No post-increment patterns found to optimize");
        }

        return modified;
    }

}