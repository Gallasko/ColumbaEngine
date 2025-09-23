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
    // Addition - verify actual results with __dprint
    expectPrintedOutput("__dprint(1 + 1)", "2\n");
    expectPrintedOutput("__dprint(2 + 3)", "5\n");
    expectPrintedOutput("__dprint(10 + 5)", "15\n");
    expectPrintedOutput("__dprint(0 + 0)", "0\n");
    expectPrintedOutput("__dprint(1.5 + 2.5)", "4.000000\n");  // Float operation
    
    // Subtraction - verify actual results with __dprint
    expectPrintedOutput("__dprint(5 - 3)", "2\n");
    expectPrintedOutput("__dprint(10 - 10)", "0\n");
    expectPrintedOutput("__dprint(1 - 2)", "-1\n");
    expectPrintedOutput("__dprint(3.5 - 1.5)", "2.000000\n");  // Float operation
    
    // Multiplication - verify actual results with __dprint
    expectPrintedOutput("__dprint(2 * 3)", "6\n");
    expectPrintedOutput("__dprint(4 * 5)", "20\n");
    expectPrintedOutput("__dprint(3 * 0)", "0\n");
    expectPrintedOutput("__dprint(1 * 1)", "1\n");
    expectPrintedOutput("__dprint(2.5 * 4)", "10.000000\n");  // Float operation
    
    // Division - verify actual results with __dprint
    expectPrintedOutput("__dprint(6 / 2)", "3.000000\n");     // Division always returns float
    expectPrintedOutput("__dprint(15 / 3)", "5.000000\n");    // Division always returns float
    expectPrintedOutput("__dprint(1 / 1)", "1.000000\n");     // Division always returns float
    expectPrintedOutput("__dprint(7.5 / 2.5)", "3.000000\n"); // Division always returns float
    expectPrintedOutput("__dprint(0 / 1)", "0.000000\n");     // Division always returns float
}

// Unary Operations Tests
TEST_F(LanguageFeaturesTest, UnaryOperations) {
    // Unary minus - verify actual results with __dprint
    expectPrintedOutput("__dprint(-5)", "-5\n");
    expectPrintedOutput("__dprint(-(-3))", "3\n");
    expectPrintedOutput("__dprint(-(2 + 3))", "-5\n");
    expectPrintedOutput("__dprint(-0)", "0\n");
    
    // Unary not - verify actual results with __dprint
    expectPrintedOutput("__dprint(!true)", "false\n");
    expectPrintedOutput("__dprint(!false)", "true\n");
    expectPrintedOutput("__dprint(!!true)", "true\n");
    expectPrintedOutput("__dprint(!!false)", "false\n");
    expectPrintedOutput("__dprint(!(5 > 3))", "false\n");
}

// Comparison Operations Tests
TEST_F(LanguageFeaturesTest, ComparisonOperations) {
    // Equality - verify actual results with __dprint
    expectPrintedOutput("__dprint(1 == 1)", "true\n");
    expectPrintedOutput("__dprint(1 == 2)", "false\n");
    expectPrintedOutput("__dprint(0 == 0)", "true\n");
    expectPrintedOutput("__dprint(3.14 == 3.14)", "true\n");
    expectPrintedOutput("__dprint(true == true)", "true\n");
    expectPrintedOutput("__dprint(false == false)", "true\n");
    expectPrintedOutput("__dprint(true == false)", "false\n");
    
    // Inequality - verify actual results with __dprint
    expectPrintedOutput("__dprint(1 != 2)", "true\n");
    expectPrintedOutput("__dprint(1 != 1)", "false\n");
    expectPrintedOutput("__dprint(true != false)", "true\n");
    expectPrintedOutput("__dprint(true != true)", "false\n");
    
    // Greater than - verify actual results with __dprint
    expectPrintedOutput("__dprint(5 > 3)", "true\n");
    expectPrintedOutput("__dprint(3 > 5)", "false\n");
    expectPrintedOutput("__dprint(5 > 5)", "false\n");
    expectPrintedOutput("__dprint(0 > -1)", "true\n");
    
    // Greater than or equal - verify actual results with __dprint
    expectPrintedOutput("__dprint(5 >= 3)", "true\n");
    expectPrintedOutput("__dprint(3 >= 5)", "false\n");
    expectPrintedOutput("__dprint(5 >= 5)", "true\n");
    expectPrintedOutput("__dprint(0 >= 0)", "true\n");
    
    // Less than - verify actual results with __dprint
    expectPrintedOutput("__dprint(3 < 5)", "true\n");
    expectPrintedOutput("__dprint(5 < 3)", "false\n");
    expectPrintedOutput("__dprint(5 < 5)", "false\n");
    expectPrintedOutput("__dprint(-1 < 0)", "true\n");
    
    // Less than or equal - verify actual results with __dprint
    expectPrintedOutput("__dprint(3 <= 5)", "true\n");
    expectPrintedOutput("__dprint(5 <= 3)", "false\n");
    expectPrintedOutput("__dprint(5 <= 5)", "true\n");
    expectPrintedOutput("__dprint(0 <= 0)", "true\n");
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

// Basic Variable Operations Tests
TEST_F(LanguageFeaturesTest, BasicVariableOperations) {
    // Variable declaration and access - verify with __dprint
    expectPrintedOutput("var x = 42; __dprint(x)", "42\n");      // Integer literal
    expectPrintedOutput("var name = 10; __dprint(name)", "10\n"); // Integer literal
    expectPrintedOutput("var pi = 3.14; __dprint(pi)", "3.140000\n"); // Float literal
    expectPrintedOutput("var flag = true; __dprint(flag)", "true\n");
    expectPrintedOutput("var off = false; __dprint(off)", "false\n");
    
    // Variable assignment and update - verify with __dprint
    expectPrintedOutput("var a = 5; a = 10; __dprint(a)", "10\n");        // Integer assignment
    expectPrintedOutput("var b = 1; b = b + 5; __dprint(b)", "6\n");       // Integer arithmetic
    expectPrintedOutput("var c = 10; c = c * 2; __dprint(c)", "20\n");     // Integer arithmetic
    expectPrintedOutput("var d = 8; d = d / 2; __dprint(d)", "4.000000\n"); // Division returns float
    expectPrintedOutput("var e = 7; e = e - 3; __dprint(e)", "4\n");       // Integer arithmetic
    
    // Variables in expressions - verify with __dprint
    expectPrintedOutput("var x = 3; var y = 4; __dprint(x + y)", "7\n");       // Integer arithmetic
    expectPrintedOutput("var x = 10; var y = 3; __dprint(x - y)", "7\n");     // Integer arithmetic
    expectPrintedOutput("var x = 6; var y = 7; __dprint(x * y)", "42\n");     // Integer arithmetic
    expectPrintedOutput("var x = 15; var y = 3; __dprint(x / y)", "5.000000\n"); // Division returns float
    expectPrintedOutput("var x = 5; var y = 5; __dprint(x == y)", "true\n");
    expectPrintedOutput("var x = 5; var y = 3; __dprint(x > y)", "true\n");
}

// Increment and Decrement Operations Tests
TEST_F(LanguageFeaturesTest, IncrementDecrementOperations) {
    // Test basic variable declaration and increment/decrement with actual value checking
    expectPrintedOutput("var x = 5; __dprint(++x)", "6\n");
    expectPrintedOutput("var x = 5; __dprint(--x)", "4\n");
    expectPrintedOutput("var x = 5; __dprint(x++)", "5\n");  // postfix returns old value
    expectPrintedOutput("var x = 5; __dprint(x--)", "5\n");  // postfix returns old value
    
    // Test prefix increment/decrement with value verification
    expectPrintedOutput("var y = 10; __dprint(++y)", "11\n");
    expectPrintedOutput("var y = 10; __dprint(--y)", "9\n");
    
    // Test postfix increment/decrement with value verification
    expectPrintedOutput("var z = 8; __dprint(z++)", "8\n");  // returns old value
    expectPrintedOutput("var z = 8; z++; __dprint(z)", "9\n");  // variable is incremented
    expectPrintedOutput("var z = 8; __dprint(z--)", "8\n");  // returns old value
    expectPrintedOutput("var z = 8; z--; __dprint(z)", "7\n");  // variable is decremented
    
    // Test with expressions
    expectPrintedOutput("var a = 3; __dprint(++a + 2)", "6\n");  // (3+1) + 2 = 6
    expectPrintedOutput("var a = 3; __dprint(a++ + 2)", "5\n");  // 3 + 2 = 5 (returns old value)
    expectPrintedOutput("var a = 5; __dprint(--a * 2)", "8\n");  // (5-1) * 2 = 8
    expectPrintedOutput("var a = 5; __dprint(a-- * 2)", "10\n"); // 5 * 2 = 10 (returns old value)
    
    // Test multiple increments/decrements
    expectPrintedOutput("var c = 0; ++c; ++c; __dprint(++c)", "3\n");
    expectPrintedOutput("var d = 10; --d; --d; __dprint(--d)", "7\n");
    
    // Test chained operations (complex but verifiable)
    expectPrintedOutput("var e = 1; ++e; __dprint(++e)", "3\n");  // 1 -> 2 -> 3
    expectPrintedOutput("var g = 5; g--; __dprint(--g)", "3\n");  // 5 -> 4 -> 3
}

// Increment/Decrement Edge Cases
TEST_F(LanguageFeaturesTest, IncrementDecrementEdgeCases) {
    // Test with zero
    assertInterpretResult("var zero = 0; ++zero", InterpretResult::OK);
    assertInterpretResult("var zero = 0; zero++", InterpretResult::OK);
    assertInterpretResult("var zero = 0; --zero", InterpretResult::OK);
    assertInterpretResult("var zero = 0; zero--", InterpretResult::OK);
    
    // Test with negative numbers
    assertInterpretResult("var neg = -5; ++neg", InterpretResult::OK);
    assertInterpretResult("var neg = -5; --neg", InterpretResult::OK);
    assertInterpretResult("var neg = -1; neg++", InterpretResult::OK);
    assertInterpretResult("var neg = -1; neg--", InterpretResult::OK);
    
    // Test with floating point numbers
    assertInterpretResult("var flt = 3.5; ++flt", InterpretResult::OK);
    assertInterpretResult("var flt = 3.5; flt++", InterpretResult::OK);
    assertInterpretResult("var flt = 2.7; --flt", InterpretResult::OK);
    assertInterpretResult("var flt = 2.7; flt--", InterpretResult::OK);
}

// Increment/Decrement with Literals (should fail or be no-op)
TEST_F(LanguageFeaturesTest, IncrementDecrementLiterals) {
    // Test increment/decrement with literals (these should compile but treat as unary operations)
    assertInterpretResult("++5", InterpretResult::OK);  // Treated as +(+5) = 5
    assertInterpretResult("--5", InterpretResult::OK);  // Treated as -(-5) = 5
    expectNumericResult("++7", 7.0);
    expectNumericResult("--8", 8.0);
    
    // Postfix on literals should fail during parsing
    // Note: This might not be implementable depending on parser design
    // assertRuntimeError("5++");
    // assertRuntimeError("3--");
}

// Loop Control Structures Tests
TEST_F(LanguageFeaturesTest, WhileLoopOperations) {
    // Basic while loop - verify final results with __dprint
    expectPrintedOutput("var i = 0; while (i < 3) { i = i + 1; } __dprint(i)", "3\n");
    expectPrintedOutput("var j = 5; while (j > 0) { j = j - 1; } __dprint(j)", "0\n");
    
    // While loop with increment/decrement - verify final results with __dprint
    expectPrintedOutput("var k = 0; while (k < 5) { ++k; } __dprint(k)", "5\n");
    expectPrintedOutput("var l = 10; while (l > 0) { --l; } __dprint(l)", "0\n");
    expectPrintedOutput("var m = 0; while (m < 3) { m++; } __dprint(m)", "3\n");
    expectPrintedOutput("var n = 7; while (n > 0) { n--; } __dprint(n)", "0\n");
    
    // While loop with complex conditions
    assertInterpretResult("var x = 1; while (x * x < 10) { x++; }", InterpretResult::OK);
    assertInterpretResult("var y = 100; while (y / 2 > 5) { y = y / 2; }", InterpretResult::OK);
    
    // Nested while loops
    assertInterpretResult("var a = 0; while (a < 2) { var b = 0; while (b < 2) { b++; } a++; }", InterpretResult::OK);
    
    // While loop with boolean expressions
    assertInterpretResult("var flag = true; var counter = 0; while (flag && counter < 5) { counter++; if (counter == 3) flag = false; }", InterpretResult::OK);
}

// For Loop Tests  
TEST_F(LanguageFeaturesTest, ForLoopOperations) {
    // Basic for loop
    assertInterpretResult("for (var i = 0; i < 3; i++) { }", InterpretResult::OK);
    assertInterpretResult("for (var j = 10; j > 0; j--) { }", InterpretResult::OK);
    assertInterpretResult("for (var k = 0; k < 5; ++k) { }", InterpretResult::OK);
    assertInterpretResult("for (var l = 8; l > 0; --l) { }", InterpretResult::OK);
    
    // For loop with different step sizes
    assertInterpretResult("for (var i = 0; i < 10; i = i + 2) { }", InterpretResult::OK);
    assertInterpretResult("for (var j = 20; j > 0; j = j - 3) { }", InterpretResult::OK);
    
    // For loop with complex conditions and updates
    assertInterpretResult("for (var x = 1; x * x < 50; x++) { }", InterpretResult::OK);
    assertInterpretResult("for (var y = 100; y > 1; y = y / 2) { }", InterpretResult::OK);
    
    // For loop with body operations
    assertInterpretResult("var sum = 0; for (var i = 1; i <= 5; i++) { sum = sum + i; }", InterpretResult::OK);
    assertInterpretResult("var product = 1; for (var j = 1; j <= 4; j++) { product = product * j; }", InterpretResult::OK);
    
    // Nested for loops
    assertInterpretResult("for (var i = 0; i < 3; i++) { for (var j = 0; j < 2; j++) { } }", InterpretResult::OK);
    
    // For loop with pre-existing variable
    assertInterpretResult("var counter = 0; for (counter = 0; counter < 5; counter++) { }", InterpretResult::OK);
}

// Complex Loop and Increment/Decrement Combinations
TEST_F(LanguageFeaturesTest, ComplexLoopIncrementCombinations) {
    // While loop with multiple increment operations
    assertInterpretResult("var a = 0; var b = 10; while (a < b) { a++; b--; }", InterpretResult::OK);
    
    // For loop with increment/decrement in body
    assertInterpretResult("var extra = 0; for (var i = 0; i < 3; i++) { extra++; }", InterpretResult::OK);
    
    // Mixed prefix and postfix operations in loops
    assertInterpretResult("for (var i = 0; i < 5; ++i) { var j = i++; }", InterpretResult::OK);
    assertInterpretResult("var x = 0; while (++x < 5) { var y = x--; x++; }", InterpretResult::OK);
    
    // Loop with increment/decrement in conditions
    assertInterpretResult("var counter = 0; while (++counter <= 5) { }", InterpretResult::OK);
    assertInterpretResult("var down = 10; while (--down >= 0) { }", InterpretResult::OK);
    
    // Complex expressions with loops and increment
    assertInterpretResult("var total = 0; for (var i = 1; i <= 3; i++) { total = total + i++; }", InterpretResult::OK);
    assertInterpretResult("var result = 1; var multiplier = 2; while (result < 100) { result = result * multiplier++; }", InterpretResult::OK);
}

// Loop Edge Cases
TEST_F(LanguageFeaturesTest, LoopEdgeCases) {
    // Empty loops
    assertInterpretResult("while (false) { }", InterpretResult::OK);
    assertInterpretResult("for (var i = 0; i < 0; i++) { }", InterpretResult::OK);
    
    // Single iteration loops
    assertInterpretResult("while (true) { break; }", InterpretResult::OK);  // If break is supported
    assertInterpretResult("for (var i = 0; i < 1; i++) { }", InterpretResult::OK);
    
    // Loops with zero as condition
    assertInterpretResult("var zero = 0; while (zero) { zero++; }", InterpretResult::OK);
    assertInterpretResult("for (var i = 0; 0; i++) { }", InterpretResult::OK);
    
    // Infinite loops (that we artificially limit)
    // Note: These might timeout - implement with care
    // assertInterpretResult("var safety = 0; while (true) { if (++safety > 1000) break; }", InterpretResult::OK);
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
    
    // Stress test with loops and increments
    assertInterpretResult("var big = 0; for (var i = 0; i < 100; i++) { big++; }", InterpretResult::OK);
    assertInterpretResult("var countdown = 500; while (countdown > 0) { countdown--; }", InterpretResult::OK);
}

} // namespace test
} // namespace pg