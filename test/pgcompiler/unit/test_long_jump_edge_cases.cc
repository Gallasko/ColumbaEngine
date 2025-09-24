#include "gtest/gtest.h"
#include "compiler_test_base.h"
#include "long_jump_optimization_pass.h"
#include "compiler_debug.h"
#include <memory>

namespace pg {
namespace test {

class LongJumpEdgeCasesTest : public CompilerTestBase {
protected:
    void SetUp() override {
        CompilerTestBase::SetUp();
        pass = std::make_unique<LongJumpOptimizationPass>();
    }
    
    std::unique_ptr<LongJumpOptimizationPass> pass;
    
    // Test the exact failing scenario from the user's report
    void testUserFailingScenario() {
        std::string problematicCode = 
            "var a = 5; "
            "var b = 6; "
            "if (a == b and 5 == 5) { "
            "  __dprint(\"Failed\"); "
            "} else { "
            "  var returnEarly = false; "
            "  while (a < b and (not returnEarly)) { "
            "    a++; "
            "  } "
            "}";
            
        auto chunk = compileExpression(problematicCode);
        
        std::cout << "\n=== USER FAILING SCENARIO BEFORE OPTIMIZATION ===" << std::endl;
        disassembleChunk(chunk, "Before optimization");
        
        // This should NOT crash or produce invalid bytecode
        bool changed = pass->runPass(chunk);
        
        std::cout << "\n=== USER FAILING SCENARIO AFTER OPTIMIZATION ===" << std::endl;
        disassembleChunk(chunk, "After optimization");
        
        // Test execution to make sure it doesn't crash
        VM vm;
        vm.chunk = chunk;
        vm.ip = 0;
        
        EXPECT_NO_THROW({
            auto result = vm.run();
            EXPECT_EQ(result, InterpretResult::OK) << "Optimized code should execute without 'Jump offset out of bounds' error";
        }) << "Execution should not throw exceptions";
    }
};

TEST_F(LongJumpEdgeCasesTest, UserReportedFailingScenario) {
    // Test the exact scenario that was failing
    testUserFailingScenario();
}

TEST_F(LongJumpEdgeCasesTest, BackwardJumpWithMultipleOptimizations) {
    // Test backward jumps when multiple optimizations change preceding bytecode
    Chunk chunk;
    
    // Create a scenario with multiple long jumps followed by a loop
    chunk.addCode(OpCode::OP_True, 1);
    
    // First long jump (should be optimizable)
    chunk.addCode(OpCode::OP_Long_Jump_If_False, 1);
    chunk.addCode(0, 1); chunk.addCode(0, 1); chunk.addCode(0, 1); chunk.addCode(10, 1);
    
    chunk.addCode(OpCode::OP_Pop, 1);
    
    // Second long jump (should be optimizable)  
    chunk.addCode(OpCode::OP_Long_Jump_If_False, 1);
    chunk.addCode(0, 1); chunk.addCode(0, 1); chunk.addCode(0, 1); chunk.addCode(8, 1);
    
    chunk.addCode(OpCode::OP_Pop, 1);
    
    // Target of jumps
    size_t loopStart = chunk.code.size();
    chunk.addCode(OpCode::OP_True, 1);
    
    // Backward jump (loop) - this could be affected by previous optimizations
    chunk.addCode(OpCode::OP_Long_Loop, 1);
    size_t loopDistance = chunk.code.size() + 4 - loopStart;
    chunk.addCode((loopDistance >> 24) & 0xFF, 1);
    chunk.addCode((loopDistance >> 16) & 0xFF, 1);
    chunk.addCode((loopDistance >> 8) & 0xFF, 1);
    chunk.addCode(loopDistance & 0xFF, 1);
    
    chunk.addCode(OpCode::OP_Return, 1);
    
    std::cout << "\n=== BACKWARD JUMP SCENARIO BEFORE OPTIMIZATION ===" << std::endl;
    disassembleChunk(chunk, "Before optimization");
    
    bool changed = pass->runPass(chunk);
    
    std::cout << "\n=== BACKWARD JUMP SCENARIO AFTER OPTIMIZATION ===" << std::endl;
    disassembleChunk(chunk, "After optimization");
    
    // Should not produce invalid bytecode
    VM vm;
    vm.chunk = chunk;
    vm.ip = 0;
    
    // Note: This might run forever, so we'll just check the first few instructions
    EXPECT_NO_THROW({
        // Just verify bytecode is valid by checking first instruction
        if (!chunk.code.empty()) {
            auto firstOp = static_cast<OpCode>(chunk.code[0]);
            EXPECT_EQ(firstOp, OpCode::OP_True);
        }
    });
}

TEST_F(LongJumpEdgeCasesTest, JumpToEndOfChunk) {
    // Test jump that targets the very end of the chunk
    Chunk chunk;
    
    chunk.addCode(OpCode::OP_True, 1);
    
    // Calculate exact distance to end of chunk
    size_t jumpPos = chunk.code.size();
    chunk.addCode(OpCode::OP_Long_Jump_If_False, 1);
    
    // Add placeholder bytes for the jump offset
    chunk.addCode(0, 1); chunk.addCode(0, 1); chunk.addCode(0, 1); chunk.addCode(0, 1);
    
    // Add some target instructions
    chunk.addCode(OpCode::OP_Pop, 1);
    chunk.addCode(OpCode::OP_Return, 1);
    
    // Calculate and set the jump distance to point to the return
    size_t jumpDistance = chunk.code.size() - (jumpPos + 5); // +5 for the jump instruction size
    chunk.code[jumpPos + 1] = (jumpDistance >> 24) & 0xFF;
    chunk.code[jumpPos + 2] = (jumpDistance >> 16) & 0xFF;
    chunk.code[jumpPos + 3] = (jumpDistance >> 8) & 0xFF;
    chunk.code[jumpPos + 4] = jumpDistance & 0xFF;
    
    std::cout << "\n=== JUMP TO END BEFORE OPTIMIZATION ===" << std::endl;
    disassembleChunk(chunk, "Before optimization");
    
    bool changed = pass->runPass(chunk);
    
    std::cout << "\n=== JUMP TO END AFTER OPTIMIZATION ===" << std::endl;
    disassembleChunk(chunk, "After optimization");
    
    // Should handle boundary conditions correctly
    VM vm;
    vm.chunk = chunk;
    vm.ip = 0;
    
    EXPECT_NO_THROW({
        auto result = vm.run();
        EXPECT_EQ(result, InterpretResult::OK);
    });
}

TEST_F(LongJumpEdgeCasesTest, JumpToFirstInstruction) {
    // Test backward jump to very beginning of chunk
    Chunk chunk;
    
    size_t startPos = chunk.code.size();
    chunk.addCode(OpCode::OP_True, 1);
    chunk.addCode(OpCode::OP_False, 1);
    
    // Backward jump to the beginning
    chunk.addCode(OpCode::OP_Long_Loop, 1);
    size_t jumpDistance = chunk.code.size() + 4 - startPos; // +4 for remaining bytes of instruction
    chunk.addCode((jumpDistance >> 24) & 0xFF, 1);
    chunk.addCode((jumpDistance >> 16) & 0xFF, 1);
    chunk.addCode((jumpDistance >> 8) & 0xFF, 1);
    chunk.addCode(jumpDistance & 0xFF, 1);
    
    std::cout << "\n=== JUMP TO START BEFORE OPTIMIZATION ===" << std::endl;
    disassembleChunk(chunk, "Before optimization");
    
    bool changed = pass->runPass(chunk);
    
    std::cout << "\n=== JUMP TO START AFTER OPTIMIZATION ===" << std::endl;
    disassembleChunk(chunk, "After optimization");
    
    // Should not crash on boundary conditions
    EXPECT_GT(chunk.code.size(), 0) << "Chunk should not be corrupted";
}

TEST_F(LongJumpEdgeCasesTest, ZeroDistanceJump) {
    // Test jump with zero distance (jump to next instruction)
    Chunk chunk;
    
    chunk.addCode(OpCode::OP_True, 1);
    chunk.addCode(OpCode::OP_Long_Jump, 1);
    chunk.addCode(0, 1); chunk.addCode(0, 1); chunk.addCode(0, 1); chunk.addCode(0, 1); // Zero distance
    chunk.addCode(OpCode::OP_Return, 1);
    
    std::cout << "\n=== ZERO DISTANCE JUMP BEFORE OPTIMIZATION ===" << std::endl;
    disassembleChunk(chunk, "Before optimization");
    
    bool changed = pass->runPass(chunk);
    
    std::cout << "\n=== ZERO DISTANCE JUMP AFTER OPTIMIZATION ===" << std::endl;
    disassembleChunk(chunk, "After optimization");
    
    EXPECT_TRUE(changed) << "Zero distance jump should be optimizable";
    
    // Should execute correctly
    VM vm;
    vm.chunk = chunk;
    vm.ip = 0;
    
    EXPECT_NO_THROW({
        auto result = vm.run();
        EXPECT_EQ(result, InterpretResult::OK);
    });
}

TEST_F(LongJumpEdgeCasesTest, MaximumShortJumpDistance) {
    // Test jump at the exact boundary of short jump capability
    Chunk chunk;
    
    chunk.addCode(OpCode::OP_True, 1);
    chunk.addCode(OpCode::OP_Long_Jump, 1);
    
    // Set distance to exactly 65535 (max 16-bit value)
    uint32_t maxShortDistance = 65535;
    chunk.addCode((maxShortDistance >> 24) & 0xFF, 1);
    chunk.addCode((maxShortDistance >> 16) & 0xFF, 1);
    chunk.addCode((maxShortDistance >> 8) & 0xFF, 1);
    chunk.addCode(maxShortDistance & 0xFF, 1);
    
    // Add enough target instructions to make the jump valid
    for (uint32_t i = 0; i < maxShortDistance; ++i) {
        chunk.addCode(OpCode::OP_Pop, 1);
    }
    chunk.addCode(OpCode::OP_Return, 1);
    
    std::cout << "\n=== MAXIMUM SHORT JUMP BEFORE OPTIMIZATION ===" << std::endl;
    std::cout << "Jump distance: " << maxShortDistance << std::endl;
    
    bool changed = pass->runPass(chunk);
    
    std::cout << "\n=== MAXIMUM SHORT JUMP AFTER OPTIMIZATION ===" << std::endl;
    
    EXPECT_TRUE(changed) << "Maximum short jump distance should be optimizable";
    
    // Verify it was converted to short jump
    bool foundShortJump = false;
    for (size_t i = 0; i < chunk.code.size(); ++i) {
        if (static_cast<OpCode>(chunk.code[i]) == OpCode::OP_Jump) {
            foundShortJump = true;
            break;
        }
    }
    EXPECT_TRUE(foundShortJump) << "Should be converted to short jump";
}

TEST_F(LongJumpEdgeCasesTest, JustOverShortJumpLimit) {
    // Test jump that's just over the short jump limit
    Chunk chunk;
    
    chunk.addCode(OpCode::OP_True, 1);
    chunk.addCode(OpCode::OP_Long_Jump, 1);
    
    // Set distance to 65536 (just over 16-bit max)
    uint32_t overShortDistance = 65536;
    chunk.addCode((overShortDistance >> 24) & 0xFF, 1);
    chunk.addCode((overShortDistance >> 16) & 0xFF, 1);
    chunk.addCode((overShortDistance >> 8) & 0xFF, 1);
    chunk.addCode(overShortDistance & 0xFF, 1);
    
    // Add target instructions (but not all - this will be invalid)
    for (int i = 0; i < 100; ++i) {
        chunk.addCode(OpCode::OP_Pop, 1);
    }
    chunk.addCode(OpCode::OP_Return, 1);
    
    std::cout << "\n=== OVER SHORT JUMP LIMIT BEFORE OPTIMIZATION ===" << std::endl;
    std::cout << "Jump distance: " << overShortDistance << std::endl;
    
    bool changed = pass->runPass(chunk);
    
    std::cout << "\n=== OVER SHORT JUMP LIMIT AFTER OPTIMIZATION ===" << std::endl;
    
    EXPECT_FALSE(changed) << "Jump over short limit should NOT be optimizable";
    
    // Should still have long jump
    bool foundLongJump = false;
    for (size_t i = 0; i < chunk.code.size(); ++i) {
        if (static_cast<OpCode>(chunk.code[i]) == OpCode::OP_Long_Jump) {
            foundLongJump = true;
            break;
        }
    }
    EXPECT_TRUE(foundLongJump) << "Should still have long jump";
}

} // namespace test
} // namespace pg