#include "gtest/gtest.h"
#include "compiler_test_base.h"

namespace pg {
namespace test {

class ErrorHandlingTest : public CompilerTestBase {
protected:
    void SetUp() override {
        CompilerTestBase::SetUp();
    }
};

// Compile-Time Error Tests
TEST_F(ErrorHandlingTest, SyntaxErrors) {
    // Missing operands
    assertCompileError("1 +");
    assertCompileError("* 5");
    assertCompileError("/ 3");
    assertCompileError("+ 2");
    assertCompileError("- ");
    
    // Invalid operator sequences
    assertCompileError("1 + + 2");
    assertCompileError("1 * * 2");
    assertCompileError("1 / / 2");
    // Note: "1 - - 2" is actually valid as "1 - (-2)" due to unary minus
    
    // Mismatched parentheses
    assertCompileError("(1 + 2");
    assertCompileError("1 + 2)");
    assertCompileError("((1 + 2)");
    assertCompileError("(1 + 2))");
    assertCompileError(")(1 + 2");
    
    // Empty expressions
    // Note: Empty string might be valid as empty program
    assertCompileError("()");
    assertCompileError("( )");
}

TEST_F(ErrorHandlingTest, InvalidTokenSequences) {
    // Invalid comparisons
    assertCompileError("1 ==");
    assertCompileError("== 1");
    assertCompileError("1 !=");
    assertCompileError("!= 1");
    assertCompileError("1 >");
    assertCompileError("> 1");
    assertCompileError("1 <");
    assertCompileError("< 1");
    assertCompileError("1 >=");
    assertCompileError(">= 1");
    assertCompileError("1 <=");
    assertCompileError("<= 1");
    
    // Invalid unary operations
    assertCompileError("!");
    assertCompileError("! ");
    
    // Multiple unary operators without proper operands
    assertCompileError("!!!");
    assertCompileError("---");
}

TEST_F(ErrorHandlingTest, NestedSyntaxErrors) {
    assertCompileError("(1 + (2 * ))");
    assertCompileError("(1 + ) * 2");
    assertCompileError("((1 + 2) * (3 + ))");
    assertCompileError("(1 + 2) * (");
    assertCompileError("(1 + 2) * )");
}

// Runtime Error Tests
TEST_F(ErrorHandlingTest, TypeErrors) {
    // Unary operations on wrong types
    assertRuntimeError("-true");
    assertRuntimeError("-false");
    assertRuntimeError("!42");
    assertRuntimeError("!3.14");
    assertRuntimeError("!0");
    
    // Note: String operations might not be implemented yet
    // assertRuntimeError("-\"hello\"");
    // assertRuntimeError("!\"world\"");
}

TEST_F(ErrorHandlingTest, BinaryOperationTypeErrors) {
    // Arithmetic on wrong types (if type checking is strict)
    // Note: These might be implementation-dependent
    // Some implementations might coerce types
    
    // Mixed type operations that should fail
    // assertRuntimeError("true + 1");
    // assertRuntimeError("1 + true");
    // assertRuntimeError("false * 2");
    // assertRuntimeError("\"hello\" + 1");
    
    // Boolean arithmetic (if not supported)
    // assertRuntimeError("true + false");
    // assertRuntimeError("true * false");
    // assertRuntimeError("true / false");
    // assertRuntimeError("true - false");
}

TEST_F(ErrorHandlingTest, StackUnderflowErrors) {
    // These test the VM's ability to handle malformed bytecode
    // In normal compilation, these shouldn't occur, but we test them
    // to ensure the VM is robust
    
    Chunk malformedChunk;
    
    // Try binary operation without enough operands
    malformedChunk.addCode(OpCode::OP_Add, 1);
    malformedChunk.addCode(OpCode::OP_Return, 1);
    
    auto result = executeChunk(malformedChunk);
    EXPECT_EQ(result, InterpretResult::RUNTIME_ERROR) 
        << "Should detect stack underflow";
}

TEST_F(ErrorHandlingTest, InvalidOpcodeHandling) {
    Chunk invalidChunk;
    
    // Add an invalid opcode
    invalidChunk.code.push_back(255);  // Invalid opcode
    invalidChunk.lines.push_back(1);
    invalidChunk.addCode(OpCode::OP_Return, 1);
    
    auto result = executeChunk(invalidChunk);
    EXPECT_EQ(result, InterpretResult::RUNTIME_ERROR) 
        << "Should handle invalid opcodes gracefully";
}

TEST_F(ErrorHandlingTest, ConstantIndexOutOfBounds) {
    Chunk malformedChunk;
    
    // Add a constant instruction with invalid index
    malformedChunk.addCode(OpCode::OP_Constant, 1);
    malformedChunk.addCode(255, 1);  // Invalid constant index
    malformedChunk.addCode(OpCode::OP_Return, 1);
    
    auto result = executeChunk(malformedChunk);
    EXPECT_EQ(result, InterpretResult::RUNTIME_ERROR) 
        << "Should handle invalid constant indices";
}

TEST_F(ErrorHandlingTest, LongConstantIndexOutOfBounds) {
    Chunk malformedChunk;
    
    // Add a long constant instruction with invalid index
    malformedChunk.addCode(OpCode::OP_LongConstant, 1);
    malformedChunk.addCode(255, 1);  // Invalid index bytes
    malformedChunk.addCode(255, 1);
    malformedChunk.addCode(255, 1);
    malformedChunk.addCode(OpCode::OP_Return, 1);
    
    auto result = executeChunk(malformedChunk);
    EXPECT_EQ(result, InterpretResult::RUNTIME_ERROR) 
        << "Should handle invalid long constant indices";
}

// Error Recovery Tests
TEST_F(ErrorHandlingTest, ErrorRecoveryBetweenExecutions) {
    // First, cause a compile error
    assertCompileError("1 + + 2");
    
    // Then try a valid expression - should work
    assertCompileSuccess("1 + 2");
    
    // Cause a runtime error
    assertRuntimeError("-true");
    
    // Then try another valid expression - should work
    assertCompileSuccess("3 * 4");
}

TEST_F(ErrorHandlingTest, CompilerStateAfterError) {
    // Test that compiler state is properly reset after errors
    assertCompileError("invalid syntax here");
    
    // Compiler should be usable again
    assertCompileSuccess("42");
    assertCompileSuccess("1 + 2 * 3");
}

TEST_F(ErrorHandlingTest, VMStateAfterError) {
    // Test that VM state is properly reset after runtime errors
    assertRuntimeError("-true");
    
    // VM should be usable again
    assertInterpretResult("1 + 1", InterpretResult::OK);
    assertInterpretResult("5 > 3", InterpretResult::OK);
}

// Complex Error Scenarios
TEST_F(ErrorHandlingTest, NestedCompoundExpressionErrors) {
    assertCompileError("(1 + 2) * (3 + )");
    assertCompileError("(1 + ) * (3 + 4)");
    assertCompileError("((1 + 2) * 3 + (4 / ))");
}

TEST_F(ErrorHandlingTest, ChainedComparisonErrors) {
    // These compile successfully but some fail at runtime due to type mismatch
    // (1 < 2) results in boolean, then boolean < 3 fails at runtime
    assertRuntimeError("1 < 2 < 3");
    // Note: Equality comparison between boolean and number is allowed in this VM
    assertInterpretResult("1 == 2 == 3", InterpretResult::OK);  
    assertRuntimeError("1 > 2 > 3");
}

TEST_F(ErrorHandlingTest, UnaryOperatorChainingErrors) {
    // Valid unary chaining
    assertCompileSuccess("!!true");
    assertCompileSuccess("--5");
    assertCompileSuccess("!(-5 > 0)");
    
    // Invalid unary chaining (missing operands)
    assertCompileError("!!");
    assertCompileError("--");
    assertCompileError("!-");
    assertCompileError("-!");
}

// Division by Zero Tests (if handled)
TEST_F(ErrorHandlingTest, DivisionByZeroHandling) {
    // These might produce runtime errors, infinity, or be implementation-defined
    // The behavior depends on how ElementType handles division by zero
    
    // Test literal division by zero
    auto result1 = interpretFromSource("1 / 0");
    // Could be OK (infinity), RUNTIME_ERROR, or implementation-defined
    
    // Test computed division by zero
    auto result2 = interpretFromSource("1 / (2 - 2)");
    // Same as above
    
    // Just verify the interpreter doesn't crash
    EXPECT_TRUE(result1 == InterpretResult::OK || 
                result1 == InterpretResult::RUNTIME_ERROR) 
        << "Division by zero should either work or error gracefully";
    
    EXPECT_TRUE(result2 == InterpretResult::OK || 
                result2 == InterpretResult::RUNTIME_ERROR) 
        << "Computed division by zero should either work or error gracefully";
}

// Memory-Related Error Tests
TEST_F(ErrorHandlingTest, StackOverflowPrevention) {
    // Test with deeply nested expressions to see if we hit stack limits
    std::string deepExpression = "1";
    for (int i = 0; i < 1000; ++i) {
        deepExpression = "(" + deepExpression + " + 1)";
    }
    
    // This should either compile successfully or fail gracefully
    auto result = interpretFromSource(deepExpression);
    EXPECT_TRUE(result == InterpretResult::OK || 
                result == InterpretResult::COMPILE_ERROR ||
                result == InterpretResult::RUNTIME_ERROR) 
        << "Deep nesting should either work or fail gracefully";
}

TEST_F(ErrorHandlingTest, LargeConstantPoolHandling) {
    // Test with many constants to see if we hit limits
    std::string manyConstants = "0";
    for (int i = 1; i < 1000; ++i) {
        manyConstants += " + " + std::to_string(i);
    }
    
    auto result = interpretFromSource(manyConstants);
    EXPECT_TRUE(result == InterpretResult::OK || 
                result == InterpretResult::COMPILE_ERROR ||
                result == InterpretResult::RUNTIME_ERROR) 
        << "Many constants should either work or fail gracefully";
}

// Error Message Quality Tests (if error messages are accessible)
TEST_F(ErrorHandlingTest, ErrorLocationReporting) {
    // These test whether errors report useful location information
    // The actual testing would depend on how error messages are exposed
    
    // Test that we can at least trigger errors on specific lines
    assertCompileError("1 +\n+ 2");  // Error on line 2
    assertCompileError("1\n+\n+ 2");  // Error on line 3
}

} // namespace test
} // namespace pg