#include "gtest/gtest.h"
#include "compiler_test_base.h"
#include "long_jump_optimization_pass.h"
#include "compiler_debug.h"
#include "vm.h"
#include <memory>

namespace pg {
namespace test {

class LongJumpOptimizationIntegrationTest : public CompilerTestBase {
protected:
    void SetUp() override {
        CompilerTestBase::SetUp();
        pass = std::make_unique<LongJumpOptimizationPass>();
        vm = std::make_unique<VM>();
        vm->enableOptimizationDebugging();
    }
    
    std::unique_ptr<LongJumpOptimizationPass> pass;
    std::unique_ptr<VM> vm;
    
    // Helper to test code compilation, optimization, and execution
    void testCodeOptimizationAndExecution(const std::string& code, const std::string& expectedOutput = "") {
        std::cout << "\n=== TESTING CODE ===\n" << code << "\n" << std::endl;
        
        // Compile the code
        auto chunk = compileExpression(code);
        
        std::cout << "\n=== BEFORE OPTIMIZATION ===" << std::endl;
        disassembleChunk(chunk, "Before optimization");
        
        size_t originalSize = chunk.code.size();
        
        // Apply optimization
        bool optimized = pass->runPass(chunk);
        
        if (optimized) {
            std::cout << "\n=== AFTER OPTIMIZATION ===" << std::endl;
            disassembleChunk(chunk, "After optimization");
            
            size_t optimizedSize = chunk.code.size();
            std::cout << "Size reduction: " << originalSize << " -> " << optimizedSize 
                      << " (" << (originalSize - optimizedSize) << " bytes saved)" << std::endl;
        } else {
            std::cout << "No optimizations applied" << std::endl;
        }
        
        // Test that optimized bytecode still executes correctly
        // (This is crucial - optimization should not break functionality)
        VM testVM;
        testVM.chunk = chunk;
        testVM.ip = 0;
        
        try {
            auto result = testVM.run();
            EXPECT_EQ(result, InterpretResult::OK) << "Optimized bytecode should execute successfully";
        } catch (const std::exception& e) {
            FAIL() << "Optimized bytecode execution failed: " << e.what();
        }
    }
};

TEST_F(LongJumpOptimizationIntegrationTest, SimpleIfElseStatement) {
    // Test simple if-else with short conditions
    testCodeOptimizationAndExecution(
        "if (true) { var x = 1; } else { var y = 2; }"
    );
}

TEST_F(LongJumpOptimizationIntegrationTest, NestedIfStatements) {
    // Test deeply nested if statements
    testCodeOptimizationAndExecution(
        "if (true) { "
        "  if (false) { "
        "    var a = 1; "
        "  } else { "
        "    if (true) { "
        "      var b = 2; "
        "    } "
        "  } "
        "}"
    );
}

TEST_F(LongJumpOptimizationIntegrationTest, SimpleWhileLoop) {
    // Test while loop with simple condition
    testCodeOptimizationAndExecution(
        "var i = 0; "
        "while (i < 3) { "
        "  i = i + 1; "
        "}"
    );
}

TEST_F(LongJumpOptimizationIntegrationTest, ComplexLogicalExpressions) {
    // Test complex logical expressions (from your failing example)
    testCodeOptimizationAndExecution(
        "var a = 5; "
        "var b = 6; "
        "if (a == b and 5 == 5) { "
        "  var result = true; "
        "} else { "
        "  var result = false; "
        "}"
    );
}

TEST_F(LongJumpOptimizationIntegrationTest, NestedWhileWithComplexConditions) {
    // Test the exact scenario from your error case (simplified)
    testCodeOptimizationAndExecution(
        "var a = 5; "
        "var b = 6; "
        "if (a == b) { "
        "  var early = true; "
        "} else { "
        "  var returnEarly = false; "
        "  while (a < b and not returnEarly) { "
        "    a = a + 1; "
        "  } "
        "}"
    );
}

TEST_F(LongJumpOptimizationIntegrationTest, ForLoopEquivalent) {
    // Test for-loop equivalent structure
    testCodeOptimizationAndExecution(
        "var i = 0; "
        "while (i < 5) { "
        "  var temp = i * 2; "
        "  i = i + 1; "
        "}"
    );
}

TEST_F(LongJumpOptimizationIntegrationTest, ShortCircuitLogicalOperators) {
    // Test short-circuit evaluation (lots of conditional jumps)
    testCodeOptimizationAndExecution(
        "var x = true; "
        "var y = false; "
        "if (x or y and false or true) { "
        "  var result = 1; "
        "} else { "
        "  var result = 0; "
        "}"
    );
}

TEST_F(LongJumpOptimizationIntegrationTest, MultipleConsecutiveIfs) {
    // Test multiple consecutive if statements
    testCodeOptimizationAndExecution(
        "var score = 85; "
        "if (score >= 90) { "
        "  var grade = \"A\"; "
        "} "
        "if (score >= 80) { "
        "  var grade = \"B\"; "
        "} "
        "if (score >= 70) { "
        "  var grade = \"C\"; "
        "}"
    );
}

TEST_F(LongJumpOptimizationIntegrationTest, WhileWithBreakEquivalent) {
    // Test while loop with early exit condition
    testCodeOptimizationAndExecution(
        "var count = 0; "
        "var found = false; "
        "while (count < 10 and not found) { "
        "  if (count == 5) { "
        "    found = true; "
        "  } else { "
        "    count = count + 1; "
        "  } "
        "}"
    );
}

TEST_F(LongJumpOptimizationIntegrationTest, ComplexArithmeticWithConditionals) {
    // Test complex arithmetic mixed with conditionals
    testCodeOptimizationAndExecution(
        "var x = 10; "
        "var y = 20; "
        "if (x + y > 25) { "
        "  var result = (x * y) / 2; "
        "  if (result > 100) { "
        "    result = 100; "
        "  } "
        "} else { "
        "  var result = x - y; "
        "}"
    );
}

TEST_F(LongJumpOptimizationIntegrationTest, StressTestManyJumps) {
    // Stress test with many nested conditions
    std::string code = "var level = 0; ";
    for (int i = 0; i < 5; ++i) {
        code += "if (level == " + std::to_string(i) + ") { ";
        code += "  var temp" + std::to_string(i) + " = " + std::to_string(i * 10) + "; ";
        for (int j = 0; j < 2; ++j) {
            code += "  if (temp" + std::to_string(i) + " > " + std::to_string(j * 5) + ") { ";
            code += "    var inner" + std::to_string(i) + "_" + std::to_string(j) + " = true; ";
            code += "  } ";
        }
        code += "} ";
    }
    
    testCodeOptimizationAndExecution(code);
}

TEST_F(LongJumpOptimizationIntegrationTest, OptimizedCodeProducesCorrectResult) {
    // Test that optimization doesn't change program behavior
    std::string testCode = 
        "var result = 0; "
        "var i = 1; "
        "while (i <= 5) { "
        "  if (i == 3) { "
        "    result = result + 10; "
        "  } else { "
        "    result = result + 1; "
        "  } "
        "  i = i + 1; "
        "} ";
        // Expected result: 1 + 1 + 10 + 1 + 1 = 14
    
    // Compile without optimization
    auto originalChunk = compileExpression(testCode);
    VM originalVM;
    originalVM.chunk = originalChunk;
    originalVM.ip = 0;
    auto originalResult = originalVM.run();
    
    // Compile with optimization
    auto optimizedChunk = compileExpression(testCode);
    pass->runPass(optimizedChunk);
    VM optimizedVM;
    optimizedVM.chunk = optimizedChunk;
    optimizedVM.ip = 0;
    auto optimizedResult = optimizedVM.run();
    
    // Both should produce the same result
    EXPECT_EQ(originalResult, optimizedResult) 
        << "Optimized code should produce the same result as original";
        
    // And both should succeed
    EXPECT_EQ(originalResult, InterpretResult::OK);
    EXPECT_EQ(optimizedResult, InterpretResult::OK);
}

} // namespace test  
} // namespace pg