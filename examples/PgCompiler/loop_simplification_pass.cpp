#include "loop_simplification_pass.h"
#include "logger.h"

namespace pg {

    bool LoopSimplificationPass::runPass(Chunk& chunk, BytecodeRewriter* rewriter) {
        if (!rewriter) {
            LOG_ERROR("LoopSimplificationPass", "No rewriter provided");
            return false;
        }

        LOG_INFO("LoopSimplificationPass", "Setting up counting loop patterns");
        
        // Clear existing rules and add our loop simplification pattern
        rewriter->clearRules();
        setupSimpleCountingLoopPattern(rewriter);
        
        // Apply the rewrite patterns
        bool modified = rewriter->rewrite(chunk);
        
        if (modified) {
            LOG_INFO("LoopSimplificationPass", "Successfully simplified counting loops");
        } else {
            LOG_INFO("LoopSimplificationPass", "No counting loops found to simplify");
        }
        
        return modified;
    }

    void LoopSimplificationPass::setupSimpleCountingLoopPattern(BytecodeRewriter* rewriter) {
        // Pattern: var = 0; while (var < constant) { var++; }
        // 
        // Bytecode pattern:
        // 1. OP_Constant 0 (or OP_LongConstant)
        // 2. OP_Set_Local/Global var
        // 3. OP_Get_Local/Global var  (loop condition start)
        // 4. OP_Constant/LongConstant limit
        // 5. OP_Less
        // 6. OP_Jump_If_False (exit loop)
        // 7. OP_Incr_Local/Global var (loop body)
        // 8. OP_Loop (back to condition)
        
        std::vector<PatternElement> pattern = {
            // Initialization: var = 0
            PatternElement::constant(true),           // Capture: initial value (should be 0)
            PatternElement::store(true),              // Capture: variable store
            
            // Loop condition: var < limit
            PatternElement::load(true),               // Capture: variable load
            PatternElement::constant(true),           // Capture: limit constant  
            PatternElement::match(OpCode::OP_Less),   // Comparison
            PatternElement::conditionalJump(),       // Jump if false (exit loop)
            
            // Loop body: var++
            PatternElement::increment(true),          // Capture: increment instruction
            
            // Loop back
            PatternElement::loop()                    // Loop back to condition
        };
        
        // Transform function: replace loop with direct assignment
        auto transform = [](const std::vector<CapturedInstruction>& captured) -> std::vector<uint8_t> {
            if (captured.size() != 5) {
                LOG_WARNING("LoopSimplificationPass", "Unexpected number of captured instructions: " << captured.size());
                return {}; // Return empty to indicate no transformation
            }
            
            const auto& initialValue = captured[0];   // Should be 0
            const auto& storeVar = captured[1];       // Variable store instruction
            const auto& loadVar = captured[2];        // Variable load instruction  
            const auto& limitConst = captured[3];     // Limit constant
            const auto& incrVar = captured[4];        // Increment instruction
            
            // Validate this is a simple counting loop (starts at 0)
            if (initialValue.operands.size() < 1 || initialValue.operands[0] != 0) {
                LOG_INFO("LoopSimplificationPass", "Loop doesn't start at 0, skipping");
                return {}; // Not a simple 0->N loop
            }
            
            // Validate that the same variable is used throughout
            if (storeVar.operands != loadVar.operands || 
                storeVar.operands != incrVar.operands ||
                storeVar.opcode != (incrVar.opcode == OpCode::OP_Incr_Local ? OpCode::OP_Set_Local : OpCode::OP_Set_Global)) {
                LOG_INFO("LoopSimplificationPass", "Different variables used, skipping");
                return {}; // Different variables, can't optimize
            }
            
            LOG_INFO("LoopSimplificationPass", "Simplifying counting loop to direct assignment");
            
            // Generate: var = limit_constant
            std::vector<uint8_t> result;
            
            // Add the limit constant
            result.push_back(static_cast<uint8_t>(limitConst.opcode));
            result.insert(result.end(), limitConst.operands.begin(), limitConst.operands.end());
            
            // Add the variable assignment
            result.push_back(static_cast<uint8_t>(storeVar.opcode));
            result.insert(result.end(), storeVar.operands.begin(), storeVar.operands.end());
            
            return result;
        };
        
        rewriter->addAdvancedRule(pattern, transform);
        
        LOG_INFO("LoopSimplificationPass", "Added counting loop simplification pattern");
    }

}