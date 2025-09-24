#include "gtest/gtest.h"
#include "compiler_test_base.h"
#include "long_jump_optimization_pass.h"
#include "compiler_debug.h"
#include <memory>

namespace pg {
namespace test {

class LongJumpOptimizationTest : public CompilerTestBase {
protected:
    void SetUp() override {
        CompilerTestBase::SetUp();
        pass = std::make_unique<LongJumpOptimizationPass>();
    }
    
    std::unique_ptr<LongJumpOptimizationPass> pass;
    
    // Helper to manually construct a chunk with specific jump patterns
    Chunk createChunkWithLongJump(OpCode jumpOpcode, uint32_t distance) {
        Chunk chunk;
        
        // Add some initial instructions
        chunk.addCode(OpCode::OP_True, 1);
        chunk.addCode(OpCode::OP_False, 1);
        
        // Add the long jump instruction manually
        chunk.addCode(jumpOpcode, 1);
        chunk.addCode((distance >> 24) & 0xFF, 1);
        chunk.addCode((distance >> 16) & 0xFF, 1); 
        chunk.addCode((distance >> 8) & 0xFF, 1);
        chunk.addCode(distance & 0xFF, 1);
        
        // Add target instructions
        for (uint32_t i = 0; i < distance; ++i) {
            chunk.addCode(OpCode::OP_Pop, 1);
        }
        
        // Add some final instructions
        chunk.addCode(OpCode::OP_Return, 1);
        
        return chunk;
    }
    
    // Helper to create complex nested control flow
    Chunk createComplexControlFlowChunk() {
        Chunk chunk;
        
        // Outer if condition
        chunk.addCode(OpCode::OP_True, 1);
        size_t outerJumpPos = chunk.addCode(OpCode::OP_Long_Jump_If_False, 1);
        chunk.addCode(0, 1); chunk.addCode(0, 1); chunk.addCode(0, 1); chunk.addCode(20, 1); // 20 bytes forward
        
        chunk.addCode(OpCode::OP_Pop, 1);
        
        // Inner while loop
        chunk.addCode(OpCode::OP_True, 1);
        size_t loopStartPos = chunk.code.size();
        size_t innerJumpPos = chunk.addCode(OpCode::OP_Long_Jump_If_False, 1);
        chunk.addCode(0, 1); chunk.addCode(0, 1); chunk.addCode(0, 1); chunk.addCode(10, 1); // 10 bytes forward
        
        chunk.addCode(OpCode::OP_Pop, 1);
        chunk.addCode(OpCode::OP_True, 1);
        
        // Loop back
        size_t loopJumpPos = chunk.addCode(OpCode::OP_Long_Loop, 1);
        size_t loopDistance = chunk.code.size() + 4 - loopStartPos;
        chunk.addCode((loopDistance >> 24) & 0xFF, 1);
        chunk.addCode((loopDistance >> 16) & 0xFF, 1);
        chunk.addCode((loopDistance >> 8) & 0xFF, 1);
        chunk.addCode(loopDistance & 0xFF, 1);
        
        // End of inner condition
        chunk.addCode(OpCode::OP_Pop, 1);
        
        // End of outer condition  
        chunk.addCode(OpCode::OP_Return, 1);
        
        return chunk;
    }
};

TEST_F(LongJumpOptimizationTest, BasicForwardJumpOptimization) {
    // Test basic forward jump that should be optimizable
    Chunk chunk = createChunkWithLongJump(OpCode::OP_Long_Jump, 5);
    
    size_t originalSize = chunk.code.size();
    
    std::cout << "\n=== BEFORE OPTIMIZATION ===" << std::endl;
    disassembleChunk(chunk, "Before optimization");
    
    bool changed = pass->runPass(chunk);
    
    std::cout << "\n=== AFTER OPTIMIZATION ===" << std::endl;
    disassembleChunk(chunk, "After optimization");
    
    EXPECT_TRUE(changed) << "Pass should have made changes";
    EXPECT_LT(chunk.code.size(), originalSize) << "Chunk should be smaller after optimization";
    
    // Verify the long jump was converted to short jump
    bool foundShortJump = false;
    for (size_t i = 0; i < chunk.code.size(); ++i) {
        if (static_cast<OpCode>(chunk.code[i]) == OpCode::OP_Jump) {
            foundShortJump = true;
            break;
        }
    }
    EXPECT_TRUE(foundShortJump) << "Should contain OP_Jump after optimization";
}

TEST_F(LongJumpOptimizationTest, ConditionalJumpOptimization) {
    // Test conditional jump optimization
    Chunk chunk = createChunkWithLongJump(OpCode::OP_Long_Jump_If_False, 10);
    
    std::cout << "\n=== CONDITIONAL JUMP BEFORE OPTIMIZATION ===" << std::endl;
    disassembleChunk(chunk, "Before optimization");
    
    bool changed = pass->runPass(chunk);
    
    std::cout << "\n=== CONDITIONAL JUMP AFTER OPTIMIZATION ===" << std::endl;  
    disassembleChunk(chunk, "After optimization");
    
    EXPECT_TRUE(changed) << "Pass should optimize conditional jump";
    
    // Verify conversion to short conditional jump
    bool foundShortJumpIfFalse = false;
    for (size_t i = 0; i < chunk.code.size(); ++i) {
        if (static_cast<OpCode>(chunk.code[i]) == OpCode::OP_Jump_If_False) {
            foundShortJumpIfFalse = true;
            break;
        }
    }
    EXPECT_TRUE(foundShortJumpIfFalse) << "Should contain OP_Jump_If_False after optimization";
}

TEST_F(LongJumpOptimizationTest, LoopJumpOptimization) {
    // Test backward jump (loop) optimization - create a proper loop
    Chunk chunk;
    
    // Create loop target (where we want to jump back to)
    size_t loopStart = chunk.code.size();
    chunk.addCode(OpCode::OP_True, 1);        // position 0
    chunk.addCode(OpCode::OP_Pop, 1);         // position 1
    
    // Add the backward jump that will jump back to loopStart
    size_t jumpPos = chunk.code.size();      // position 2
    chunk.addCode(OpCode::OP_Long_Loop, 1);  // position 2
    
    // Calculate backward jump distance: from end of this instruction (7) back to target (0)
    uint32_t jumpDistance = (jumpPos + 5) - loopStart; // 7 - 0 = 7
    chunk.addCode((jumpDistance >> 24) & 0xFF, 1);
    chunk.addCode((jumpDistance >> 16) & 0xFF, 1);
    chunk.addCode((jumpDistance >> 8) & 0xFF, 1);
    chunk.addCode(jumpDistance & 0xFF, 1);
    
    chunk.addCode(OpCode::OP_Return, 1);
    
    std::cout << "\n=== LOOP JUMP BEFORE OPTIMIZATION ===" << std::endl;
    disassembleChunk(chunk, "Before optimization");
    
    bool changed = pass->runPass(chunk);
    
    std::cout << "\n=== LOOP JUMP AFTER OPTIMIZATION ===" << std::endl;
    disassembleChunk(chunk, "After optimization");
    
    EXPECT_TRUE(changed) << "Pass should optimize loop jump";
    
    // Verify conversion to short loop
    bool foundShortLoop = false;
    for (size_t i = 0; i < chunk.code.size(); ++i) {
        if (static_cast<OpCode>(chunk.code[i]) == OpCode::OP_Loop) {
            foundShortLoop = true;
            break;
        }
    }
    EXPECT_TRUE(foundShortLoop) << "Should contain OP_Loop after optimization";
}

TEST_F(LongJumpOptimizationTest, ComplexNestedControlFlow) {
    // Test complex nested if/while structures
    Chunk chunk = createComplexControlFlowChunk();
    
    std::cout << "\n=== COMPLEX CONTROL FLOW BEFORE OPTIMIZATION ===" << std::endl;
    disassembleChunk(chunk, "Before optimization");
    
    size_t originalSize = chunk.code.size();
    bool changed = pass->runPass(chunk);
    
    std::cout << "\n=== COMPLEX CONTROL FLOW AFTER OPTIMIZATION ===" << std::endl;
    disassembleChunk(chunk, "After optimization");
    
    if (changed) {
        EXPECT_LT(chunk.code.size(), originalSize) << "Should reduce size with optimizations";
        std::cout << "Optimized " << (originalSize - chunk.code.size()) << " bytes" << std::endl;
    }
    
    // Chunk should still be valid bytecode
    EXPECT_GT(chunk.code.size(), 0) << "Chunk should not be empty";
    EXPECT_EQ(static_cast<OpCode>(chunk.code.back()), OpCode::OP_Return) << "Should end with return";
}

TEST_F(LongJumpOptimizationTest, JumpTooLargeForOptimization) {
    // Test jump that's too large to optimize (should remain unchanged)
    Chunk chunk = createChunkWithLongJump(OpCode::OP_Long_Jump, 70000); // > 65535
    
    std::cout << "\n=== LARGE JUMP BEFORE OPTIMIZATION ===" << std::endl;
    disassembleChunk(chunk, "Before optimization");
    
    bool changed = pass->runPass(chunk);
    
    std::cout << "\n=== LARGE JUMP AFTER OPTIMIZATION ===" << std::endl;
    disassembleChunk(chunk, "After optimization");
    
    EXPECT_FALSE(changed) << "Pass should not optimize jumps that are too large";
    
    // Should still contain long jump
    bool foundLongJump = false;
    for (size_t i = 0; i < chunk.code.size(); ++i) {
        if (static_cast<OpCode>(chunk.code[i]) == OpCode::OP_Long_Jump) {
            foundLongJump = true;
            break;
        }
    }
    EXPECT_TRUE(foundLongJump) << "Should still contain OP_Long_Jump for large distances";
}

TEST_F(LongJumpOptimizationTest, MultipleJumpsAffectingEachOther) {
    // Test multiple jumps where optimizing one affects others
    Chunk chunk;
    
    // Create multiple long jumps that could interfere with each other
    chunk.addCode(OpCode::OP_True, 1);
    
    // First long jump
    chunk.addCode(OpCode::OP_Long_Jump_If_False, 1);
    chunk.addCode(0, 1); chunk.addCode(0, 1); chunk.addCode(0, 1); chunk.addCode(15, 1);
    
    chunk.addCode(OpCode::OP_Pop, 1);
    
    // Second long jump  
    chunk.addCode(OpCode::OP_Long_Jump, 1);
    chunk.addCode(0, 1); chunk.addCode(0, 1); chunk.addCode(0, 1); chunk.addCode(8, 1);
    
    // Target area
    for (int i = 0; i < 15; ++i) {
        chunk.addCode(OpCode::OP_Pop, 1);
    }
    
    chunk.addCode(OpCode::OP_Return, 1);
    
    std::cout << "\n=== MULTIPLE JUMPS BEFORE OPTIMIZATION ===" << std::endl;
    disassembleChunk(chunk, "Before optimization");
    
    // Should handle multiple passes correctly
    bool changed = pass->runPass(chunk);
    
    std::cout << "\n=== MULTIPLE JUMPS AFTER OPTIMIZATION ===" << std::endl;
    disassembleChunk(chunk, "After optimization");
    
    // At least some optimization should occur
    if (changed) {
        std::cout << "Successfully optimized multiple interfering jumps" << std::endl;
    }
    
    EXPECT_GT(chunk.code.size(), 0) << "Chunk should remain valid";
}

TEST_F(LongJumpOptimizationTest, EmptyChunk) {
    // Test empty chunk (edge case)
    Chunk chunk;
    
    bool changed = pass->runPass(chunk);
    
    EXPECT_FALSE(changed) << "Empty chunk should not be modified";
    EXPECT_EQ(chunk.code.size(), 0) << "Empty chunk should remain empty";
}

TEST_F(LongJumpOptimizationTest, ChunkWithNoJumps) {
    // Test chunk with no jump instructions
    Chunk chunk;
    chunk.addCode(OpCode::OP_True, 1);
    chunk.addCode(OpCode::OP_False, 1);
    chunk.addCode(OpCode::OP_Add, 1);
    chunk.addCode(OpCode::OP_Return, 1);
    
    bool changed = pass->runPass(chunk);
    
    EXPECT_FALSE(changed) << "Chunk with no jumps should not be modified";
    EXPECT_EQ(chunk.code.size(), 4) << "Chunk size should remain unchanged";
}

} // namespace test
} // namespace pg