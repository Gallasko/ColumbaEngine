#include "bytecode_rewriter.h"
#include "logger.h"
#include <iostream>

namespace pg {

    void demonstrateRewriter() {
        LOG_INFO("RewriterDemo", "Creating demonstration bytecode rewriter");
        
        // Create a test chunk with some bytecode
        Chunk chunk;
        
        // Add some test bytecode: 
        // OP_True, OP_Not, OP_Pop (can be optimized to OP_False, OP_Pop)
        chunk.addCode(OpCode::OP_True, 1);
        chunk.addCode(OpCode::OP_Not, 1);  
        chunk.addCode(OpCode::OP_Pop, 1);
        
        // Add a jump instruction to test offset adjustment
        chunk.addCode(OpCode::OP_Jump_If_False, 2);
        chunk.addCode(0, 2); // Jump offset (high byte)
        chunk.addCode(10, 2); // Jump offset (low byte) - jumps 10 bytes forward
        
        // Add more instructions after the jump
        for (int i = 0; i < 5; i++) {
            chunk.addCode(OpCode::OP_Add, 3);
        }
        
        LOG_INFO("RewriterDemo", "Original chunk size: " << chunk.code.size() << " bytes");
        
        // Create and configure the rewriter
        BytecodeRewriter rewriter;
        
        // Add rewrite rules
        rewriter.addRule(
            {OpCode::OP_True, OpCode::OP_Not}, 
            {OpCode::OP_False}
        );
        
        rewriter.addRule(OpCode::OP_Negate, OpCode::OP_Subtract);
        
        LOG_INFO("RewriterDemo", "Added " << rewriter.getRuleCount() << " rewrite rules");
        
        // Apply the rewrites
        bool changed = rewriter.rewrite(chunk);
        
        if (changed) {
            LOG_INFO("RewriterDemo", "Rewrite successful! New chunk size: " << chunk.code.size() << " bytes");
            
            // Print the resulting bytecode
            std::cout << "Rewritten bytecode:" << std::endl;
            for (size_t i = 0; i < chunk.code.size(); ++i) {
                std::cout << "  [" << i << "] " << static_cast<int>(chunk.code[i]) << std::endl;
            }
        } else {
            LOG_INFO("RewriterDemo", "No changes applied");
        }
    }
    
    void demonstrateComplexRewrite() {
        LOG_INFO("RewriterDemo", "Creating complex rewrite demonstration");
        
        Chunk chunk;
        
        // Create: OP_Constant 1, OP_Constant 2, OP_Add -> OP_Constant 3 (constant folding simulation)
        chunk.addCode(OpCode::OP_Constant, 1);
        chunk.addCode(1, 1); // constant index 1
        chunk.addCode(OpCode::OP_Constant, 1); 
        chunk.addCode(2, 1); // constant index 2
        chunk.addCode(OpCode::OP_Add, 1);
        
        // Add a backward jump (loop)
        chunk.addCode(OpCode::OP_Loop, 2);
        chunk.addCode(0, 2); // Jump back 8 bytes (high byte)  
        chunk.addCode(8, 2); // (low byte)
        
        LOG_INFO("RewriterDemo", "Complex chunk size: " << chunk.code.size() << " bytes");
        
        BytecodeRewriter rewriter;
        
        // Simulate peephole optimization: remove redundant add with constants
        rewriter.addRule(
            {OpCode::OP_Add, OpCode::OP_Pop}, 
            {OpCode::OP_Pop}
        );
        
        bool changed = rewriter.rewrite(chunk);
        
        LOG_INFO("RewriterDemo", "Complex rewrite " << (changed ? "succeeded" : "found no matches"));
        LOG_INFO("RewriterDemo", "Final chunk size: " << chunk.code.size() << " bytes");
    }

}

// Example usage
int main() {
    using namespace pg;
    
    std::cout << "=== Bytecode Rewriter Demonstration ===" << std::endl;
    
    demonstrateRewriter();
    
    std::cout << "\n=== Complex Rewrite Example ===" << std::endl;
    
    demonstrateComplexRewrite();
    
    return 0;
}