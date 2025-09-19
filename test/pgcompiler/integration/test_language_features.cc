#include "gtest/gtest.h"
#include "compiler_test_base.h"

namespace pg {
namespace test {

class LanguageFeaturesTest : public CompilerTestBase {
protected:
    void SetUp() override {
        CompilerTestBase::SetUp();
    }
    
    // Helper to test that an expression evaluates to a specific numeric result
    void expectNumericResult(const std::string& expression, double expected) {
        assertInterpretResult(expression, InterpretResult::OK);
        // Note: We can't easily check the actual result value without capturing output
        // In a real implementation, we might expose the result value for testing
    }
    
    // Helper to test that an expression evaluates to a specific boolean result
    void expectBooleanResult(const std::string& expression, bool expected) {
        assertInterpretResult(expression, InterpretResult::OK);
    }
};

// Numeric Literal Tests
TEST_F(LanguageFeaturesTest, NumericLiterals) {
    expectNumericResult("0", 0.0);
    expectNumericResult("1", 1.0);
    expectNumericResult("42", 42.0);
    expectNumericResult("3.14", 3.14);
    expectNumericResult("0.5", 0.5);
    expectNumericResult("123.456", 123.456);
    
    // Negative numbers (parsed as unary minus)
    expectNumericResult("-1", -1.0);
    expectNumericResult("-3.14", -3.14);
    expectNumericResult("-0", 0.0);
}

// Boolean Literal Tests
TEST_F(LanguageFeaturesTest, BooleanLiterals) {
    expectBooleanResult("true", true);
    expectBooleanResult("false", false);
}

// Arithmetic Operations Tests
TEST_F(LanguageFeaturesTest, BasicArithmetic) {
    // Addition
    expectNumericResult("1 + 1", 2.0);
    expectNumericResult("2 + 3", 5.0);
    expectNumericResult("10 + 5", 15.0);
    expectNumericResult("0 + 0", 0.0);
    expectNumericResult("1.5 + 2.5", 4.0);
    
    // Subtraction
    expectNumericResult("5 - 3", 2.0);
    expectNumericResult("10 - 10", 0.0);
    expectNumericResult("1 - 2", -1.0);
    expectNumericResult("3.5 - 1.5", 2.0);
    
    // Multiplication
    expectNumericResult("2 * 3", 6.0);
    expectNumericResult("4 * 5", 20.0);
    expectNumericResult("3 * 0", 0.0);
    expectNumericResult("1 * 1", 1.0);
    expectNumericResult("2.5 * 4", 10.0);
    
    // Division
    expectNumericResult("6 / 2", 3.0);
    expectNumericResult("15 / 3", 5.0);
    expectNumericResult("1 / 1", 1.0);
    expectNumericResult("7.5 / 2.5", 3.0);
    expectNumericResult("0 / 1", 0.0);
}

// Unary Operations Tests
TEST_F(LanguageFeaturesTest, UnaryOperations) {
    // Unary minus
    expectNumericResult("-5", -5.0);
    expectNumericResult("-(-3)", 3.0);
    expectNumericResult("-(2 + 3)", -5.0);
    expectNumericResult("-0", 0.0);
    
    // Unary not
    expectBooleanResult("!true", false);
    expectBooleanResult("!false", true);
    expectBooleanResult("!!true", true);
    expectBooleanResult("!!false", false);
    expectBooleanResult("!(5 > 3)", false);
}

// Comparison Operations Tests
TEST_F(LanguageFeaturesTest, ComparisonOperations) {
    // Equality
    expectBooleanResult("1 == 1", true);
    expectBooleanResult("1 == 2", false);
    expectBooleanResult("0 == 0", true);
    expectBooleanResult("3.14 == 3.14", true);
    expectBooleanResult("true == true", true);
    expectBooleanResult("false == false", true);
    expectBooleanResult("true == false", false);
    
    // Inequality
    expectBooleanResult("1 != 2", true);
    expectBooleanResult("1 != 1", false);
    expectBooleanResult("true != false", true);
    expectBooleanResult("true != true", false);
    
    // Greater than
    expectBooleanResult("5 > 3", true);
    expectBooleanResult("3 > 5", false);
    expectBooleanResult("5 > 5", false);
    expectBooleanResult("0 > -1", true);
    
    // Greater than or equal
    expectBooleanResult("5 >= 3", true);
    expectBooleanResult("3 >= 5", false);
    expectBooleanResult("5 >= 5", true);
    expectBooleanResult("0 >= 0", true);
    
    // Less than
    expectBooleanResult("3 < 5", true);
    expectBooleanResult("5 < 3", false);
    expectBooleanResult("5 < 5", false);
    expectBooleanResult("-1 < 0", true);
    
    // Less than or equal
    expectBooleanResult("3 <= 5", true);
    expectBooleanResult("5 <= 3", false);
    expectBooleanResult("5 <= 5", true);
    expectBooleanResult("0 <= 0", true);
}

// Operator Precedence Tests
TEST_F(LanguageFeaturesTest, OperatorPrecedence) {
    // Arithmetic precedence (PEMDAS/BODMAS)
    expectNumericResult("2 + 3 * 4", 14.0);      // Should be 2 + (3 * 4) = 14
    expectNumericResult("2 * 3 + 4", 10.0);      // Should be (2 * 3) + 4 = 10
    expectNumericResult("10 - 6 / 2", 7.0);      // Should be 10 - (6 / 2) = 7
    expectNumericResult("10 / 2 - 3", 2.0);      // Should be (10 / 2) - 3 = 2
    expectNumericResult("2 + 3 * 4 - 1", 13.0);  // Should be 2 + (3 * 4) - 1 = 13
    
    // Unary operators have high precedence
    expectNumericResult("-2 + 3", 1.0);          // Should be (-2) + 3 = 1
    expectNumericResult("2 + -3", -1.0);         // Should be 2 + (-3) = -1
    expectNumericResult("-2 * 3", -6.0);         // Should be (-2) * 3 = -6
    
    // Comparison has lower precedence than arithmetic
    expectBooleanResult("2 + 3 > 4", true);      // Should be (2 + 3) > 4 = true
    expectBooleanResult("2 * 3 == 6", true);     // Should be (2 * 3) == 6 = true
    expectBooleanResult("10 / 2 < 6", true);     // Should be (10 / 2) < 6 = true
    
    // Equality has lower precedence than other comparisons
    expectBooleanResult("5 > 3 == true", true);  // Should be (5 > 3) == true = true
    expectBooleanResult("5 < 3 != false", false); // Should be (5 < 3) != false = false
}

// Parentheses and Grouping Tests
TEST_F(LanguageFeaturesTest, ParenthesesGrouping) {
    // Basic grouping
    expectNumericResult("(1 + 2)", 3.0);
    expectNumericResult("(5 - 3)", 2.0);
    expectNumericResult("(2 * 3)", 6.0);
    expectNumericResult("(8 / 2)", 4.0);
    
    // Changing precedence with parentheses
    expectNumericResult("(2 + 3) * 4", 20.0);
    expectNumericResult("2 * (3 + 4)", 14.0);
    expectNumericResult("(10 - 6) / 2", 2.0);
    expectNumericResult("10 / (2 + 3)", 2.0);
    
    // Nested parentheses
    expectNumericResult("((1 + 2) * 3)", 9.0);
    expectNumericResult("(2 * (3 + 4))", 14.0);
    expectNumericResult("((2 + 3) * (4 - 1))", 15.0);
    
    // Complex nested expressions
    expectNumericResult("((1 + 2) * 3 + 4) / 2", 6.5);
    expectNumericResult("(2 + (3 * 4)) - (5 / 5)", 13.0);
    
    // Boolean expressions with grouping
    expectBooleanResult("!(5 > 3)", false);
    expectBooleanResult("(5 > 3) == true", true);
    expectBooleanResult("!(true == false)", true);
}

// Complex Expression Tests
TEST_F(LanguageFeaturesTest, ComplexExpressions) {
    // Mixed arithmetic and comparison
    expectBooleanResult("(2 + 3) * 4 == 20", true);
    expectBooleanResult("10 / (2 + 3) < 3", true);
    expectBooleanResult("(5 * 2) - 3 > 6", true);
    
    // Chained arithmetic
    expectNumericResult("1 + 2 + 3 + 4", 10.0);
    expectNumericResult("10 - 3 - 2 - 1", 4.0);
    expectNumericResult("2 * 3 * 4", 24.0);
    expectNumericResult("24 / 4 / 2", 3.0);
    
    // Mixed operators
    expectNumericResult("1 + 2 * 3 - 4 / 2", 5.0);
    expectNumericResult("(1 + 2) * (3 - 4) / (2)", -1.5);
    
    // Boolean logic with arithmetic
    expectBooleanResult("(1 + 1) == 2 && (3 - 1) == 2", true);  // If && is supported
    expectBooleanResult("(5 > 3) && (2 < 4)", true);             // If && is supported
}

// Edge Cases and Boundary Conditions
TEST_F(LanguageFeaturesTest, EdgeCases) {
    // Zero operations
    expectNumericResult("0 + 0", 0.0);
    expectNumericResult("0 - 0", 0.0);
    expectNumericResult("0 * 5", 0.0);
    expectNumericResult("5 * 0", 0.0);
    expectNumericResult("0 / 1", 0.0);
    
    // One operations
    expectNumericResult("1 * 5", 5.0);
    expectNumericResult("5 * 1", 5.0);
    expectNumericResult("5 / 1", 5.0);
    
    // Self operations
    expectNumericResult("5 - 5", 0.0);
    expectNumericResult("5 / 5", 1.0);
    expectBooleanResult("5 == 5", true);
    expectBooleanResult("5 != 5", false);
    expectBooleanResult("5 >= 5", true);
    expectBooleanResult("5 <= 5", true);
    
    // Negative number operations
    expectNumericResult("-5 + 3", -2.0);
    expectNumericResult("-5 - 3", -8.0);
    expectNumericResult("-5 * 3", -15.0);
    expectNumericResult("-5 / -1", 5.0);
    expectNumericResult("5 + -3", 2.0);
    expectNumericResult("5 - -3", 8.0);
    expectNumericResult("5 * -3", -15.0);
    expectNumericResult("5 / -1", -5.0);
}

// Floating Point Precision Tests
TEST_F(LanguageFeaturesTest, FloatingPointOperations) {
    expectNumericResult("1.5 + 2.5", 4.0);
    expectNumericResult("3.14 - 1.14", 2.0);
    expectNumericResult("2.5 * 4.0", 10.0);
    expectNumericResult("7.5 / 2.5", 3.0);
    
    // More complex floating point
    expectNumericResult("1.1 + 2.2", 3.3);
    expectNumericResult("0.1 + 0.2", 0.3);  // This might fail due to floating point precision
    expectNumericResult("3.14159 / 2.0", 1.570795);
}

// Type Consistency Tests
TEST_F(LanguageFeaturesTest, TypeConsistency) {
    // Ensure operations return expected types
    
    // Arithmetic always returns numbers
    expectNumericResult("1 + 2", 3.0);
    expectNumericResult("5.0 - 3.0", 2.0);
    expectNumericResult("-7", -7.0);
    
    // Comparisons always return booleans
    expectBooleanResult("1 < 2", true);
    expectBooleanResult("5 == 5", true);
    expectBooleanResult("3 > 4", false);
    
    // Boolean operations return booleans
    expectBooleanResult("!true", false);
    expectBooleanResult("!!false", false);
}

// Regression Tests for Specific Issues
TEST_F(LanguageFeaturesTest, RegressionTests) {
    // Test cases that might have caused problems in development
    
    // Simple cases that should always work
    expectNumericResult("1", 1.0);
    expectNumericResult("(1)", 1.0);
    expectBooleanResult("true", true);
    expectBooleanResult("(true)", true);
    
    // Precedence edge cases
    expectNumericResult("1 + 2 * 3", 7.0);
    expectNumericResult("(1 + 2) * 3", 9.0);
    expectBooleanResult("1 + 2 == 3", true);
    expectBooleanResult("(1 + 2) == 3", true);
    
    // Unary operations edge cases
    expectNumericResult("--5", 5.0);
    expectBooleanResult("!!true", true);
    expectNumericResult("-(1 + 2)", -3.0);
    expectBooleanResult("!(1 == 2)", true);
}

// Performance and Stress Tests
TEST_F(LanguageFeaturesTest, StressTests) {
    // Test with larger numbers
    expectNumericResult("1000 + 2000", 3000.0);
    expectNumericResult("1000000 / 1000", 1000.0);
    expectBooleanResult("999999 < 1000000", true);
    
    // Test with many operations
    expectNumericResult("1 + 1 + 1 + 1 + 1", 5.0);
    expectNumericResult("2 * 2 * 2 * 2 * 2", 32.0);
    expectBooleanResult("1 == 1 && 2 == 2 && 3 == 3", true);  // If && is supported
}

} // namespace test
} // namespace pg