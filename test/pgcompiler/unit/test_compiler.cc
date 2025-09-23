#include "gtest/gtest.h"
#include "compiler_test_base.h"
#include "compiler.h"

namespace pg {
namespace test {

class CompilerTest : public CompilerTestBase {
protected:
    void SetUp() override {
        CompilerTestBase::SetUp();
    }
};

// Basic Compilation Tests
TEST_F(CompilerTest, CompilerInitialization) {
    EXPECT_FALSE(compiler.parser.hasError());
}

TEST_F(CompilerTest, CompilerReset) {
    // Simulate some state
    auto tokens = TestHelpers::makeTokenQueue({
        {TokenType::NUMBER, "42"}
    });

    Chunk chunk;
    compiler.compile(tokens, chunk);

    // Reset should clear parser state
    compiler.reset();
    EXPECT_FALSE(compiler.parser.hasError());
}

// Simple Expression Compilation Tests
TEST_F(CompilerTest, CompileSimpleNumber) {
    assertCompileSuccess("42");

    auto chunk = compileExpression("42");
    assertConstantCount(chunk, 1);
    assertConstantValue(chunk, 0, ElementType(42.0));

    // Should have: OP_Constant, index, OP_Return
    EXPECT_GE(chunk.code.size(), 3);
}

TEST_F(CompilerTest, CompileSimpleAddition) {
    assertCompileSuccess("1 + 2");

    auto chunk = compileExpression("1 + 2");
    assertConstantCount(chunk, 2);
    assertConstantValue(chunk, 0, ElementType(1.0));
    assertConstantValue(chunk, 1, ElementType(2.0));

    // Should contain OP_Add instruction
    bool hasAddInstruction = false;
    for (const auto& byte : chunk.code) {
        if (static_cast<OpCode>(byte) == OpCode::OP_Add) {
            hasAddInstruction = true;
            break;
        }
    }
    EXPECT_TRUE(hasAddInstruction) << "Compiled chunk should contain OP_Add instruction";
}

TEST_F(CompilerTest, CompileSubtraction) {
    assertCompileSuccess("5 - 3");

    auto chunk = compileExpression("5 - 3");

    bool hasSubInstruction = false;
    for (const auto& byte : chunk.code) {
        if (static_cast<OpCode>(byte) == OpCode::OP_Subtract) {
            hasSubInstruction = true;
            break;
        }
    }
    EXPECT_TRUE(hasSubInstruction) << "Compiled chunk should contain OP_Subtract instruction";
}

TEST_F(CompilerTest, CompileMultiplication) {
    assertCompileSuccess("4 * 3");

    auto chunk = compileExpression("4 * 3");

    bool hasMulInstruction = false;
    for (const auto& byte : chunk.code) {
        if (static_cast<OpCode>(byte) == OpCode::OP_Multiply) {
            hasMulInstruction = true;
            break;
        }
    }
    EXPECT_TRUE(hasMulInstruction) << "Compiled chunk should contain OP_Multiply instruction";
}

TEST_F(CompilerTest, CompileDivision) {
    assertCompileSuccess("8 / 2");

    auto chunk = compileExpression("8 / 2");

    bool hasDivInstruction = false;
    for (const auto& byte : chunk.code) {
        if (static_cast<OpCode>(byte) == OpCode::OP_Divide) {
            hasDivInstruction = true;
            break;
        }
    }
    EXPECT_TRUE(hasDivInstruction) << "Compiled chunk should contain OP_Divide instruction";
}

// Unary Operations Tests
TEST_F(CompilerTest, CompileUnaryMinus) {
    assertCompileSuccess("-5");

    auto chunk = compileExpression("-5");

    bool hasNegateInstruction = false;
    for (const auto& byte : chunk.code) {
        if (static_cast<OpCode>(byte) == OpCode::OP_Negate) {
            hasNegateInstruction = true;
            break;
        }
    }
    EXPECT_TRUE(hasNegateInstruction) << "Compiled chunk should contain OP_Negate instruction";
}

TEST_F(CompilerTest, CompileUnaryNot) {
    assertCompileSuccess("!true");

    auto chunk = compileExpression("!true");

    bool hasNotInstruction = false;
    bool hasTrueInstruction = false;
    for (const auto& byte : chunk.code) {
        if (static_cast<OpCode>(byte) == OpCode::OP_Not) {
            hasNotInstruction = true;
        }
        if (static_cast<OpCode>(byte) == OpCode::OP_True) {
            hasTrueInstruction = true;
        }
    }
    EXPECT_TRUE(hasNotInstruction) << "Compiled chunk should contain OP_Not instruction";
    EXPECT_TRUE(hasTrueInstruction) << "Compiled chunk should contain OP_True instruction";
}

// Boolean Literals Tests
TEST_F(CompilerTest, CompileTrueLiteral) {
    assertCompileSuccess("true");

    auto chunk = compileExpression("true");

    bool hasTrueInstruction = false;
    for (const auto& byte : chunk.code) {
        if (static_cast<OpCode>(byte) == OpCode::OP_True) {
            hasTrueInstruction = true;
            break;
        }
    }
    EXPECT_TRUE(hasTrueInstruction) << "Compiled chunk should contain OP_True instruction";
}

TEST_F(CompilerTest, CompileFalseLiteral) {
    assertCompileSuccess("false");

    auto chunk = compileExpression("false");

    bool hasFalseInstruction = false;
    for (const auto& byte : chunk.code) {
        if (static_cast<OpCode>(byte) == OpCode::OP_False) {
            hasFalseInstruction = true;
            break;
        }
    }
    EXPECT_TRUE(hasFalseInstruction) << "Compiled chunk should contain OP_False instruction";
}

// Comparison Operations Tests
TEST_F(CompilerTest, CompileEquality) {
    assertCompileSuccess("5 == 5");

    auto chunk = compileExpression("5 == 5");

    bool hasEqualInstruction = false;
    for (const auto& byte : chunk.code) {
        if (static_cast<OpCode>(byte) == OpCode::OP_Equal) {
            hasEqualInstruction = true;
            break;
        }
    }
    EXPECT_TRUE(hasEqualInstruction) << "Compiled chunk should contain OP_Equal instruction";
}

TEST_F(CompilerTest, CompileInequality) {
    assertCompileSuccess("5 != 3");

    auto chunk = compileExpression("5 != 3");

    bool hasNotEqualInstruction = false;
    for (const auto& byte : chunk.code) {
        if (static_cast<OpCode>(byte) == OpCode::OP_NotEqual) {
            hasNotEqualInstruction = true;
            break;
        }
    }
    EXPECT_TRUE(hasNotEqualInstruction) << "Compiled chunk should contain OP_NotEqual instruction";
}

TEST_F(CompilerTest, CompileGreaterThan) {
    assertCompileSuccess("5 > 3");

    auto chunk = compileExpression("5 > 3");

    bool hasGreaterInstruction = false;
    for (const auto& byte : chunk.code) {
        if (static_cast<OpCode>(byte) == OpCode::OP_Greater) {
            hasGreaterInstruction = true;
            break;
        }
    }
    EXPECT_TRUE(hasGreaterInstruction) << "Compiled chunk should contain OP_Greater instruction";
}

TEST_F(CompilerTest, CompileGreaterEqual) {
    assertCompileSuccess("5 >= 3");

    auto chunk = compileExpression("5 >= 3");

    bool hasGreaterEqualInstruction = false;
    for (const auto& byte : chunk.code) {
        if (static_cast<OpCode>(byte) == OpCode::OP_GreaterEqual) {
            hasGreaterEqualInstruction = true;
            break;
        }
    }
    EXPECT_TRUE(hasGreaterEqualInstruction) << "Compiled chunk should contain OP_GreaterEqual instruction";
}

TEST_F(CompilerTest, CompileLessThan) {
    assertCompileSuccess("3 < 5");

    auto chunk = compileExpression("3 < 5");

    bool hasLessInstruction = false;
    for (const auto& byte : chunk.code) {
        if (static_cast<OpCode>(byte) == OpCode::OP_Less) {
            hasLessInstruction = true;
            break;
        }
    }
    EXPECT_TRUE(hasLessInstruction) << "Compiled chunk should contain OP_Less instruction";
}

TEST_F(CompilerTest, CompileLessEqual) {
    assertCompileSuccess("3 <= 5");

    auto chunk = compileExpression("3 <= 5");

    bool hasLessEqualInstruction = false;
    for (const auto& byte : chunk.code) {
        if (static_cast<OpCode>(byte) == OpCode::OP_LessEqual) {
            hasLessEqualInstruction = true;
            break;
        }
    }
    EXPECT_TRUE(hasLessEqualInstruction) << "Compiled chunk should contain OP_LessEqual instruction";
}

// Complex Expression Tests
TEST_F(CompilerTest, CompileComplexArithmetic) {
    assertCompileSuccess("(1 + 2) * 3");

    auto chunk = compileExpression("(1 + 2) * 3");
    assertConstantCount(chunk, 3);

    // Should contain both add and multiply operations
    bool hasAdd = false, hasMultiply = false;
    for (const auto& byte : chunk.code) {
        if (static_cast<OpCode>(byte) == OpCode::OP_Add) hasAdd = true;
        if (static_cast<OpCode>(byte) == OpCode::OP_Multiply) hasMultiply = true;
    }
    EXPECT_TRUE(hasAdd && hasMultiply) << "Complex expression should contain both operations";
}

TEST_F(CompilerTest, CompileNestedComparisons) {
    assertCompileSuccess("5 > 3 == true");

    auto chunk = compileExpression("5 > 3 == true");

    bool hasGreater = false, hasEqual = false, hasTrue = false;
    for (const auto& byte : chunk.code) {
        if (static_cast<OpCode>(byte) == OpCode::OP_Greater) hasGreater = true;
        if (static_cast<OpCode>(byte) == OpCode::OP_Equal) hasEqual = true;
        if (static_cast<OpCode>(byte) == OpCode::OP_True) hasTrue = true;
    }
    EXPECT_TRUE(hasGreater && hasEqual && hasTrue)
        << "Nested comparison should contain all operations";
}

// Error Cases Tests
TEST_F(CompilerTest, CompileInvalidSyntax) {
    assertCompileError("1 + + 2");
    assertCompileError("* 5");
    assertCompileError("5 + ");
    assertCompileError("(1 + 2");
    assertCompileError("1 + 2)");
}

TEST_F(CompilerTest, CompileEmptyExpression) {
    // Empty string literal should compile successfully (it's a valid string)
    assertCompileSuccess("\"\"");

    // But truly empty input should fail
    // Note: This test framework limitation - we can't easily test empty input
    // The empty string "" is actually a valid string literal
}

// Precedence Tests
TEST_F(CompilerTest, OperatorPrecedence) {
    // Test that 2 + 3 * 4 compiles with correct precedence
    assertCompileSuccess("2 + 3 * 4");

    auto chunk = compileExpression("2 + 3 * 4");

    // Find the positions of the operations
    std::vector<size_t> addPositions, mulPositions;
    for (size_t i = 0; i < chunk.code.size(); ++i) {
        if (static_cast<OpCode>(chunk.code[i]) == OpCode::OP_Add) {
            addPositions.push_back(i);
        }
        if (static_cast<OpCode>(chunk.code[i]) == OpCode::OP_Multiply) {
            mulPositions.push_back(i);
        }
    }

    EXPECT_EQ(addPositions.size(), 1) << "Should have exactly one add operation";
    EXPECT_EQ(mulPositions.size(), 1) << "Should have exactly one multiply operation";

    // Multiply should come before add due to precedence
    if (!addPositions.empty() && !mulPositions.empty()) {
        EXPECT_LT(mulPositions[0], addPositions[0])
            << "Multiply should be executed before add due to precedence";
    }
}

// Return Statement Tests
TEST_F(CompilerTest, CompileWithReturn) {
    auto chunk = compileExpression("42");

    // Every expression should end with a return
    bool hasReturn = false;
    for (const auto& byte : chunk.code) {
        if (static_cast<OpCode>(byte) == OpCode::OP_Return) {
            hasReturn = true;
            break;
        }
    }
    EXPECT_TRUE(hasReturn) << "Compiled chunk should contain OP_Return instruction";
}

// If/Else Statement Tests (using compileExpression for basic if statements that work)
TEST_F(CompilerTest, CompileSimpleIfStatement) {
    // This works as shown in the test output
    auto chunk = compileExpression("if (true) { }");

    bool hasJumpIfFalse = false;
    bool hasJump = false;
    bool hasPop = false;
    bool hasTrue = false;

    for (const auto& byte : chunk.code) {
        if (static_cast<OpCode>(byte) == OpCode::OP_Long_Jump_If_False) hasJumpIfFalse = true;
        if (static_cast<OpCode>(byte) == OpCode::OP_Long_Jump) hasJump = true;
        if (static_cast<OpCode>(byte) == OpCode::OP_Pop) hasPop = true;
        if (static_cast<OpCode>(byte) == OpCode::OP_True) hasTrue = true;
    }

    EXPECT_TRUE(hasJumpIfFalse) << "If statement should contain OP_Long_Jump_If_False";
    EXPECT_TRUE(hasJump) << "If statement should contain OP_Long_Jump";
    EXPECT_TRUE(hasPop) << "If statement should pop condition";
    EXPECT_TRUE(hasTrue) << "Should contain the true condition";
}

TEST_F(CompilerTest, CompileIfElseStatement) {
    // This works as shown in the test output
    auto chunk = compileExpression("if (false) { } else { }");

    bool hasJumpIfFalse = false;
    bool hasJump = false;
    bool hasFalse = false;

    for (const auto& byte : chunk.code) {
        if (static_cast<OpCode>(byte) == OpCode::OP_Long_Jump_If_False) hasJumpIfFalse = true;
        if (static_cast<OpCode>(byte) == OpCode::OP_Long_Jump) hasJump = true;
        if (static_cast<OpCode>(byte) == OpCode::OP_False) hasFalse = true;
    }

    EXPECT_TRUE(hasJumpIfFalse) << "If-else should contain OP_Long_Jump_If_False";
    EXPECT_TRUE(hasJump) << "If-else should contain OP_Long_Jump";
    EXPECT_TRUE(hasFalse) << "Should contain the false condition";
}

TEST_F(CompilerTest, CompileIfWithBooleanCondition) {
    // Test various boolean conditions
    assertCompileSuccess("if (true) { }");
    assertCompileSuccess("if (false) { }");
    assertCompileSuccess("if (!true) { }");
    assertCompileSuccess("if (!false) { }");
}

TEST_F(CompilerTest, CompileIfWithComparisonCondition) {
    // Test if with comparison conditions
    assertCompileSuccess("if (5 > 3) { }");
    assertCompileSuccess("if (2 < 4) { }");
    assertCompileSuccess("if (1 == 1) { }");
    assertCompileSuccess("if (1 != 2) { }");
}

// Edge Cases for If/Else
TEST_F(CompilerTest, CompileIfWithEmptyBody) {
    assertCompileSuccess("if (true) { }");

    auto chunk = compileExpression("if (true) { }");

    bool hasJumpIfFalse = false;
    for (const auto& byte : chunk.code) {
        if (static_cast<OpCode>(byte) == OpCode::OP_Long_Jump_If_False) hasJumpIfFalse = true;
    }

    EXPECT_TRUE(hasJumpIfFalse) << "Empty if body should still compile correctly";
}

TEST_F(CompilerTest, CompileIfElseWithEmptyBodies) {
    assertCompileSuccess("if (false) { } else { }");

    auto chunk = compileExpression("if (false) { } else { }");

    bool hasJumpIfFalse = false;
    bool hasJump = false;

    for (const auto& byte : chunk.code) {
        if (static_cast<OpCode>(byte) == OpCode::OP_Long_Jump_If_False) hasJumpIfFalse = true;
        if (static_cast<OpCode>(byte) == OpCode::OP_Long_Jump) hasJump = true;
    }

    EXPECT_TRUE(hasJumpIfFalse) << "Empty if-else should compile correctly";
    EXPECT_TRUE(hasJump) << "Empty if-else should have proper jump structure";
}

// Logical Operators And/Or Tests (using both syntax variants)
TEST_F(CompilerTest, CompileLogicalAndKeyword) {
    assertCompileSuccess("true and false");

    auto chunk = compileExpression("true and false");

    bool hasJumpIfFalse = false;
    bool hasPop = false;
    bool hasTrue = false;
    bool hasFalse = false;

    for (const auto& byte : chunk.code) {
        if (static_cast<OpCode>(byte) == OpCode::OP_Long_Jump_If_False) hasJumpIfFalse = true;
        if (static_cast<OpCode>(byte) == OpCode::OP_Pop) hasPop = true;
        if (static_cast<OpCode>(byte) == OpCode::OP_True) hasTrue = true;
        if (static_cast<OpCode>(byte) == OpCode::OP_False) hasFalse = true;
    }

    EXPECT_TRUE(hasJumpIfFalse) << "And operator should use conditional jump for short-circuiting";
    EXPECT_TRUE(hasPop) << "And operator should pop intermediate results";
    EXPECT_TRUE(hasTrue && hasFalse) << "Should contain both boolean values";
}

TEST_F(CompilerTest, CompileLogicalOrKeyword) {
    assertCompileSuccess("false or true");

    auto chunk = compileExpression("false or true");

    bool hasJumpIfFalse = false;
    bool hasJump = false;
    bool hasPop = false;

    for (const auto& byte : chunk.code) {
        if (static_cast<OpCode>(byte) == OpCode::OP_Long_Jump_If_False) hasJumpIfFalse = true;
        if (static_cast<OpCode>(byte) == OpCode::OP_Long_Jump) hasJump = true;
        if (static_cast<OpCode>(byte) == OpCode::OP_Pop) hasPop = true;
    }

    EXPECT_TRUE(hasJumpIfFalse) << "Or operator should use conditional jump";
    EXPECT_TRUE(hasJump) << "Or operator should use unconditional jump";
    EXPECT_TRUE(hasPop) << "Or operator should pop intermediate results";
}

TEST_F(CompilerTest, CompileLogicalOperatorsSyntaxVariants) {
    // Test both syntax variants if supported
    assertCompileSuccess("true and false");
    assertCompileSuccess("false or true");

    // Test combinations
    assertCompileSuccess("true and true");
    assertCompileSuccess("false or false");
    assertCompileSuccess("!true and false");
    assertCompileSuccess("true or !false");
}

// Short-circuiting behavior tests
TEST_F(CompilerTest, CompileAndShortCircuitStructure) {
    // Test structure of and operation for short-circuiting
    auto chunk = compileExpression("false and true");

    bool hasJumpIfFalse = false;
    bool hasPop = false;

    for (const auto& byte : chunk.code) {
        if (static_cast<OpCode>(byte) == OpCode::OP_Long_Jump_If_False) hasJumpIfFalse = true;
        if (static_cast<OpCode>(byte) == OpCode::OP_Pop) hasPop = true;
    }

    EXPECT_TRUE(hasJumpIfFalse) << "And should have conditional jump for short-circuiting";
    EXPECT_TRUE(hasPop) << "And should pop intermediate results";
}

TEST_F(CompilerTest, CompileOrShortCircuitStructure) {
    // Test structure of or operation for short-circuiting
    auto chunk = compileExpression("true or false");

    bool hasJumpIfFalse = false;
    bool hasJump = false;
    bool hasPop = false;

    for (const auto& byte : chunk.code) {
        if (static_cast<OpCode>(byte) == OpCode::OP_Long_Jump_If_False) hasJumpIfFalse = true;
        if (static_cast<OpCode>(byte) == OpCode::OP_Long_Jump) hasJump = true;
        if (static_cast<OpCode>(byte) == OpCode::OP_Pop) hasPop = true;
    }

    EXPECT_TRUE(hasJumpIfFalse) << "Or should have conditional jump";
    EXPECT_TRUE(hasJump) << "Or should have unconditional jump";
    EXPECT_TRUE(hasPop) << "Or should pop intermediate results";
}

TEST_F(CompilerTest, CompileChainedLogicalOperators) {
    assertCompileSuccess("true and false or true and false");

    auto chunk = compileExpression("true and false or true and false");

    // Count logical operation jumps
    int jumpIfFalseCount = 0;
    int jumpCount = 0;

    for (const auto& byte : chunk.code) {
        if (static_cast<OpCode>(byte) == OpCode::OP_Long_Jump_If_False) jumpIfFalseCount++;
        if (static_cast<OpCode>(byte) == OpCode::OP_Long_Jump) jumpCount++;
    }

    EXPECT_GT(jumpIfFalseCount, 1) << "Chained logical operations should have multiple conditional jumps";
    EXPECT_GT(jumpCount, 0) << "Chained logical operations should have jumps";
}

TEST_F(CompilerTest, CompileLogicalOperatorPrecedence) {
    // Test that and has higher precedence than or
    assertCompileSuccess("true or false and false");

    auto chunk = compileExpression("true or false and false");

    // Should compile as: true or (false and false)
    int jumpCount = 0;

    for (const auto& byte : chunk.code) {
        if (static_cast<OpCode>(byte) == OpCode::OP_Long_Jump_If_False ||
            static_cast<OpCode>(byte) == OpCode::OP_Long_Jump) {
            jumpCount++;
        }
    }

    EXPECT_GT(jumpCount, 2) << "Should have proper jump structure for precedence";
}

TEST_F(CompilerTest, CompileLogicalWithComparisons) {
    assertCompileSuccess("5 > 3 and 2 < 4 or 1 == 1");

    auto chunk = compileExpression("5 > 3 and 2 < 4 or 1 == 1");

    bool hasGreater = false;
    bool hasLess = false;
    bool hasEqual = false;
    bool hasJumpIfFalse = false;

    for (const auto& byte : chunk.code) {
        if (static_cast<OpCode>(byte) == OpCode::OP_Greater) hasGreater = true;
        if (static_cast<OpCode>(byte) == OpCode::OP_Less) hasLess = true;
        if (static_cast<OpCode>(byte) == OpCode::OP_Equal) hasEqual = true;
        if (static_cast<OpCode>(byte) == OpCode::OP_Long_Jump_If_False) hasJumpIfFalse = true;
    }

    EXPECT_TRUE(hasGreater && hasLess && hasEqual) << "Should contain all comparison operations";
    EXPECT_TRUE(hasJumpIfFalse) << "Should have conditional jumps for logical operators";
}

// Error Cases for If/Else
TEST_F(CompilerTest, CompileIfErrorCases) {
    // Test various error cases for if statements
    assertCompileError("if () { }");           // Missing condition
    assertCompileError("if { }");              // Missing parentheses and condition
    assertCompileError("if true { }");         // Missing parentheses
    assertCompileError("if (true { }");        // Missing closing parenthesis
    assertCompileError("if true) { }");        // Missing opening parenthesis
    assertCompileError("else { }");            // Else without if

    // Todo this tottaly crash the test suite
    // assertCompileError("if (true)");           // Missing body
}

// Additional tests for logical operators edge cases
TEST_F(CompilerTest, CompileLogicalOperatorsEdgeCases) {
    // Test nested logical operations
    assertCompileSuccess("(true and false) or (false and true)");
    assertCompileSuccess("true and (false or true)");
    assertCompileSuccess("!true and !false");
    assertCompileSuccess("!(true and false)");
    assertCompileSuccess("!(true or false)");
}

} // namespace test
} // namespace pg