#include "gtest/gtest.h"
#include "vm_test_fixture.h"

namespace pg {
namespace test {

class VMTest : public VMTestFixture {
protected:
    void SetUp() override {
        VMTestFixture::SetUp();
    }
};

// Stack Operations Tests
TEST_F(VMTest, StackPushPop) {
    ElementType value(42.0);
    pushToStack(value);

    assertStackSize(1);
    assertStackTop(ElementType(42.0));

    auto popped = popFromStack();
    EXPECT_FLOAT_EQ(popped.get<float>(), 42.0);
    assertStackEmpty();
}

TEST_F(VMTest, StackPeek) {
    pushToStack(ElementType(1.0));
    pushToStack(ElementType(2.0));
    pushToStack(ElementType(3.0));

    assertStackSize(3);
    assertStackTop(ElementType(3.0));

    // Peek should not modify stack
    auto top = peekStack(0);
    EXPECT_FLOAT_EQ(top.get<float>(), 3.0);
    assertStackSize(3);

    auto second = peekStack(1);
    EXPECT_FLOAT_EQ(second.get<float>(), 2.0);
    assertStackSize(3);
}

TEST_F(VMTest, StackUnderflow) {
    assertStackEmpty();
    EXPECT_THROW(popFromStack(), std::runtime_error);
}

TEST_F(VMTest, StackPeekOutOfBounds) {
    pushToStack(ElementType(1.0));
    EXPECT_THROW(peekStack(1), std::runtime_error);
}

// Constant Loading Tests
TEST_F(VMTest, LoadConstant) {
    auto chunk = buildConstantChunk(ElementType(3.14));

    // Remove the OP_Return instruction to test stack state
    chunk.code.pop_back();  // Remove OP_Return
    chunk.lines.pop_back(); // Remove corresponding line

    loadChunk(chunk);
    auto result = executeChunkWithoutReturn();
    EXPECT_EQ(result, InterpretResult::OK);
    assertStackSize(1);
    assertStackTop(ElementType(3.14));
}

TEST_F(VMTest, LoadLongConstant) {
    auto chunk = buildLongConstantChunk(ElementType(999.0));

    // Remove the OP_Return instruction to test stack state
    chunk.code.pop_back();  // Remove OP_Return
    chunk.lines.pop_back(); // Remove corresponding line

    loadChunk(chunk);
    auto result = executeChunkWithoutReturn();
    EXPECT_EQ(result, InterpretResult::OK);
    assertStackSize(1);
    assertStackTop(ElementType(999.0));
}

// Arithmetic Operations Tests
TEST_F(VMTest, Addition) {
    auto chunk = buildArithmeticChunk(OpCode::OP_Add);

    // Remove the OP_Return instruction to test stack state
    chunk.code.pop_back();  // Remove OP_Return
    chunk.lines.pop_back(); // Remove corresponding line

    loadChunk(chunk);
    auto result = executeChunkWithoutReturn();
    EXPECT_EQ(result, InterpretResult::OK);
    assertStackSize(1);
    assertStackTop(ElementType(13.0));  // 10 + 3
}

TEST_F(VMTest, Subtraction) {
    auto chunk = buildArithmeticChunk(OpCode::OP_Subtract);

    // Remove the OP_Return instruction to test stack state
    chunk.code.pop_back();  // Remove OP_Return
    chunk.lines.pop_back(); // Remove corresponding line

    loadChunk(chunk);
    auto result = executeChunkWithoutReturn();
    EXPECT_EQ(result, InterpretResult::OK);
    assertStackSize(1);
    assertStackTop(ElementType(7.0));  // 10 - 3
}

TEST_F(VMTest, Multiplication) {
    auto chunk = buildArithmeticChunk(OpCode::OP_Multiply);

    // Remove the OP_Return instruction to test stack state
    chunk.code.pop_back();  // Remove OP_Return
    chunk.lines.pop_back(); // Remove corresponding line

    loadChunk(chunk);
    auto result = executeChunkWithoutReturn();
    EXPECT_EQ(result, InterpretResult::OK);
    assertStackSize(1);
    assertStackTop(ElementType(30.0));  // 10 * 3
}

TEST_F(VMTest, Division) {
    auto chunk = buildArithmeticChunk(OpCode::OP_Divide);

    // Remove the OP_Return instruction to test stack state
    chunk.code.pop_back();  // Remove OP_Return
    chunk.lines.pop_back(); // Remove corresponding line

    loadChunk(chunk);
    auto result = executeChunkWithoutReturn();
    EXPECT_EQ(result, InterpretResult::OK);
    assertStackSize(1);
    assertStackTop(ElementType(10.0/3.0));  // 10 / 3
}

// Unary Operations Tests
TEST_F(VMTest, Negation) {
    auto chunk = buildUnaryChunk(OpCode::OP_Negate);

    // Remove the OP_Return instruction to test stack state
    chunk.code.pop_back();  // Remove OP_Return
    chunk.lines.pop_back(); // Remove corresponding line

    loadChunk(chunk);
    auto result = executeChunkWithoutReturn();
    EXPECT_EQ(result, InterpretResult::OK);
    assertStackSize(1);
    assertStackTop(ElementType(-5.0));  // -5.0
}

TEST_F(VMTest, BooleanNot) {
    auto chunk = buildUnaryChunk(OpCode::OP_Not);

    // Remove the OP_Return instruction to test stack state
    chunk.code.pop_back();  // Remove OP_Return
    chunk.lines.pop_back(); // Remove corresponding line

    loadChunk(chunk);
    auto result = executeChunkWithoutReturn();
    EXPECT_EQ(result, InterpretResult::OK);
    assertStackSize(1);
    assertStackTop(ElementType(false));  // !true = false
}

// Boolean Constants Tests
TEST_F(VMTest, TrueConstant) {
    Chunk chunk;
    chunk.addCode(OpCode::OP_True, 1);
    // Remove OP_Return to test stack state
    loadChunk(chunk);

    auto result = executeChunkWithoutReturn();
    EXPECT_EQ(result, InterpretResult::OK);
    assertStackSize(1);
    assertStackTop(ElementType(true));
}

TEST_F(VMTest, FalseConstant) {
    Chunk chunk;
    chunk.addCode(OpCode::OP_False, 1);
    // Remove OP_Return to test stack state
    loadChunk(chunk);

    auto result = executeChunkWithoutReturn();
    EXPECT_EQ(result, InterpretResult::OK);
    assertStackSize(1);
    assertStackTop(ElementType(false));
}

// Comparison Operations Tests
TEST_F(VMTest, Equal) {
    Chunk chunk;
    chunk.addConstant(ElementType(5.0), 1);
    chunk.addConstant(ElementType(5.0), 1);
    chunk.addCode(OpCode::OP_Equal, 1);
    // Remove OP_Return to test stack state
    loadChunk(chunk);

    auto result = executeChunkWithoutReturn();
    EXPECT_EQ(result, InterpretResult::OK);
    assertStackSize(1);

    auto top = peekStack(0);
    EXPECT_TRUE(top.isBool());
    EXPECT_TRUE(top.isTrue());
}

TEST_F(VMTest, NotEqual) {
    Chunk chunk;
    chunk.addConstant(ElementType(5.0), 1);
    chunk.addConstant(ElementType(3.0), 1);
    chunk.addCode(OpCode::OP_NotEqual, 1);
    // Remove OP_Return to test stack state
    loadChunk(chunk);

    auto result = executeChunkWithoutReturn();
    EXPECT_EQ(result, InterpretResult::OK);
    assertStackSize(1);

    auto top = peekStack(0);
    EXPECT_TRUE(top.isBool());
    EXPECT_TRUE(top.isTrue());
}

TEST_F(VMTest, Greater) {
    auto chunk = buildComparisonChunk(OpCode::OP_Greater);

    // Remove the OP_Return instruction to test stack state
    chunk.code.pop_back();  // Remove OP_Return
    chunk.lines.pop_back(); // Remove corresponding line

    loadChunk(chunk);

    auto result = executeChunkWithoutReturn();
    EXPECT_EQ(result, InterpretResult::OK);
    assertStackSize(1);

    auto top = peekStack(0);
    EXPECT_TRUE(top.isBool());
    EXPECT_TRUE(top.isTrue());  // 5 > 3
}

TEST_F(VMTest, GreaterEqual) {
    auto chunk = buildComparisonChunk(OpCode::OP_GreaterEqual);

    // Remove the OP_Return instruction to test stack state
    chunk.code.pop_back();  // Remove OP_Return
    chunk.lines.pop_back(); // Remove corresponding line

    loadChunk(chunk);

    auto result = executeChunkWithoutReturn();
    EXPECT_EQ(result, InterpretResult::OK);
    assertStackSize(1);

    auto top = peekStack(0);
    EXPECT_TRUE(top.isBool());
    EXPECT_TRUE(top.isTrue());  // 5 >= 3
}

TEST_F(VMTest, Less) {
    auto chunk = buildComparisonChunk(OpCode::OP_Less);

    // Remove the OP_Return instruction to test stack state
    chunk.code.pop_back();  // Remove OP_Return
    chunk.lines.pop_back(); // Remove corresponding line

    loadChunk(chunk);

    auto result = executeChunkWithoutReturn();
    EXPECT_EQ(result, InterpretResult::OK);
    assertStackSize(1);

    auto top = peekStack(0);
    EXPECT_TRUE(top.isBool());
    EXPECT_FALSE(top.isTrue());  // 5 < 3 is false
}

TEST_F(VMTest, LessEqual) {
    auto chunk = buildComparisonChunk(OpCode::OP_LessEqual);

    // Remove the OP_Return instruction to test stack state
    chunk.code.pop_back();  // Remove OP_Return
    chunk.lines.pop_back(); // Remove corresponding line

    loadChunk(chunk);

    auto result = executeChunkWithoutReturn();
    EXPECT_EQ(result, InterpretResult::OK);
    assertStackSize(1);

    auto top = peekStack(0);
    EXPECT_TRUE(top.isBool());
    EXPECT_FALSE(top.isTrue());  // 5 <= 3 is false
}

// Error Handling Tests
TEST_F(VMTest, EmptyChunk) {
    Chunk emptyChunk;
    loadChunk(emptyChunk);

    auto result = executeUntilReturn();
    EXPECT_EQ(result, InterpretResult::OK);
}

TEST_F(VMTest, StackUnderflowError) {
    auto chunk = buildStackUnderflowChunk();
    loadChunk(chunk);

    auto result = executeUntilReturn();
    EXPECT_EQ(result, InterpretResult::RUNTIME_ERROR);
}

TEST_F(VMTest, InvalidOpcode) {
    auto chunk = buildInvalidOperationChunk();
    loadChunk(chunk);

    auto result = executeUntilReturn();
    EXPECT_EQ(result, InterpretResult::RUNTIME_ERROR);
}

TEST_F(VMTest, NegateNonNumber) {
    Chunk chunk;
    chunk.addCode(OpCode::OP_True, 1);  // Push boolean
    chunk.addCode(OpCode::OP_Negate, 1);  // Try to negate boolean
    chunk.addCode(OpCode::OP_Return, 1);
    loadChunk(chunk);

    auto result = executeUntilReturn();
    EXPECT_EQ(result, InterpretResult::RUNTIME_ERROR);
}

TEST_F(VMTest, NotNonBoolean) {
    Chunk chunk;
    chunk.addConstant(ElementType(42.0), 1);  // Push number
    chunk.addCode(OpCode::OP_Not, 1);  // Try to apply NOT to number
    chunk.addCode(OpCode::OP_Return, 1);
    loadChunk(chunk);

    auto result = executeUntilReturn();
    EXPECT_EQ(result, InterpretResult::RUNTIME_ERROR);
}

// Instruction Pointer Tests
TEST_F(VMTest, InstructionPointerProgression) {
    auto chunk = buildConstantChunk(ElementType(42.0));
    loadChunk(chunk);

    EXPECT_EQ(getInstructionPointer(), 0);

    // After execution, IP should have moved
    executeUntilReturn();
    EXPECT_GT(getInstructionPointer(), 0);
}

// Global Variables Tests
TEST_F(VMTest, DefineGlobalVariable) {
    auto chunk = buildDefineGlobalChunk("testVar", ElementType(42.0));
    
    // Remove the OP_Return instruction to test stack state
    chunk.code.pop_back();  // Remove OP_Return
    chunk.lines.pop_back(); // Remove corresponding line
    
    loadChunk(chunk);
    auto result = executeChunkWithoutReturn();
    EXPECT_EQ(result, InterpretResult::OK);
    
    // After defining a global, stack should be empty
    assertStackEmpty();
    
    // Verify global was actually stored
    EXPECT_TRUE(hasGlobal("testVar"));
    EXPECT_FLOAT_EQ(getGlobal("testVar").get<float>(), 42.0);
}

TEST_F(VMTest, DefineMultipleGlobalVariables) {
    Chunk chunk;
    
    // Define first global variable: "var1" = 10.0
    chunk.addConstant(ElementType(10.0), 1);  // value
    chunk.addConstant(ElementType("var1"), 1);  // name
    chunk.addCode(OpCode::OP_Define_Global, 1);
    
    // Define second global variable: "var2" = 20.0
    chunk.addConstant(ElementType(20.0), 1);  // value
    chunk.addConstant(ElementType("var2"), 1);  // name
    chunk.addCode(OpCode::OP_Define_Global, 1);
    
    loadChunk(chunk);
    auto result = executeChunkWithoutReturn();
    EXPECT_EQ(result, InterpretResult::OK);
    
    assertStackEmpty();
    
    // Verify both globals were stored
    EXPECT_TRUE(hasGlobal("var1"));
    EXPECT_TRUE(hasGlobal("var2"));
    EXPECT_FLOAT_EQ(getGlobal("var1").get<float>(), 10.0);
    EXPECT_FLOAT_EQ(getGlobal("var2").get<float>(), 20.0);
}

TEST_F(VMTest, GetGlobalVariable) {
    // First define a global variable
    defineGlobal("myVar", ElementType(3.14));
    
    auto chunk = buildGetGlobalChunk("myVar");
    
    // Remove the OP_Return instruction to test stack state
    chunk.code.pop_back();  // Remove OP_Return
    chunk.lines.pop_back(); // Remove corresponding line
    
    loadChunk(chunk);
    auto result = executeChunkWithoutReturn();
    EXPECT_EQ(result, InterpretResult::OK);
    
    // Should have pushed the global value onto the stack
    assertStackSize(1);
    assertStackTop(ElementType(3.14));
}

TEST_F(VMTest, GetUndefinedGlobalVariable) {
    auto chunk = buildGetGlobalChunk("undefinedVar");
    
    loadChunk(chunk);
    auto result = executeUntilReturn();
    EXPECT_EQ(result, InterpretResult::RUNTIME_ERROR);
}

TEST_F(VMTest, SetGlobalVariable) {
    // First define a global variable
    defineGlobal("myVar", ElementType(10.0));
    
    auto chunk = buildSetGlobalChunk("myVar", ElementType(99.0));
    
    // Remove the OP_Return instruction to test stack state
    chunk.code.pop_back();  // Remove OP_Return
    chunk.lines.pop_back(); // Remove corresponding line
    
    loadChunk(chunk);
    auto result = executeChunkWithoutReturn();
    EXPECT_EQ(result, InterpretResult::OK);
    
    // Should have pushed the new value onto the stack
    assertStackSize(1);
    assertStackTop(ElementType(99.0));
    
    // Verify global was actually updated
    EXPECT_FLOAT_EQ(getGlobal("myVar").get<float>(), 99.0);
}

TEST_F(VMTest, SetUndefinedGlobalVariable) {
    auto chunk = buildSetGlobalChunk("undefinedVar", ElementType(42.0));
    
    loadChunk(chunk);
    auto result = executeUntilReturn();
    EXPECT_EQ(result, InterpretResult::RUNTIME_ERROR);
}

TEST_F(VMTest, GlobalVariableWithDifferentTypes) {
    // Test with boolean
    defineGlobal("boolVar", ElementType(true));
    auto chunk1 = buildGetGlobalChunk("boolVar");
    chunk1.code.pop_back();  // Remove OP_Return
    chunk1.lines.pop_back();
    
    loadChunk(chunk1);
    auto result1 = executeChunkWithoutReturn();
    EXPECT_EQ(result1, InterpretResult::OK);
    assertStackSize(1);
    auto top = peekStack(0);
    EXPECT_TRUE(top.isBool());
    EXPECT_TRUE(top.isTrue());
    
    clearStack();
    
    // Test with string
    defineGlobal("stringVar", ElementType("hello"));
    auto chunk2 = buildGetGlobalChunk("stringVar");
    chunk2.code.pop_back();  // Remove OP_Return
    chunk2.lines.pop_back();
    
    loadChunk(chunk2);
    auto result2 = executeChunkWithoutReturn();
    EXPECT_EQ(result2, InterpretResult::OK);
    assertStackSize(1);
    top = peekStack(0);
    EXPECT_TRUE(top.isLitteral());
    EXPECT_EQ(top.toString(), "hello");
}

TEST_F(VMTest, ComplexGlobalVariableOperations) {
    // Define, modify, and use global variables in sequence
    Chunk chunk;
    
    // Define global: "counter" = 0
    chunk.addConstant(ElementType(0.0), 1);
    chunk.addConstant(ElementType("counter"), 1);
    chunk.addCode(OpCode::OP_Define_Global, 1);
    
    // Get "counter" value
    chunk.addConstant(ElementType("counter"), 1);
    chunk.addCode(OpCode::OP_Get_Global, 1);
    
    // Add 1 to it
    chunk.addConstant(ElementType(1.0), 1);
    chunk.addCode(OpCode::OP_Add, 1);
    
    // For OP_Set_Global, we need: stack bottom [name, value] stack top
    // where value is popped first, then name
    // After ADD, we have [result=1.0] on stack
    
    // Pop the result to save it
    chunk.addCode(OpCode::OP_Pop, 1);  // Remove the result temporarily
    
    // Push name and the new value in correct order
    chunk.addConstant(ElementType("counter"), 1);  // Push name first (bottom)
    chunk.addConstant(ElementType(1.0), 1);        // Push value second (top)
    chunk.addCode(OpCode::OP_Set_Global, 1);
    
    loadChunk(chunk);
    auto result = executeChunkWithoutReturn();
    EXPECT_EQ(result, InterpretResult::OK);
    
    // Should have the final value (1.0) on the stack
    assertStackSize(1);
    assertStackTop(ElementType(1.0));
    
    // Verify global was updated
    EXPECT_FLOAT_EQ(getGlobal("counter").get<float>(), 1.0);
}

TEST_F(VMTest, GlobalVariableStackErrorHandling) {
    // Test OP_Define_Global with insufficient stack values
    Chunk chunk1;
    chunk1.addConstant(ElementType("onlyName"), 1);  // Only name, no value
    chunk1.addCode(OpCode::OP_Define_Global, 1);
    chunk1.addCode(OpCode::OP_Return, 1);
    
    loadChunk(chunk1);
    auto result1 = executeUntilReturn();
    EXPECT_EQ(result1, InterpretResult::RUNTIME_ERROR);
    
    // Test OP_Get_Global with empty stack
    Chunk chunk2;
    chunk2.addCode(OpCode::OP_Get_Global, 1);  // No name on stack
    chunk2.addCode(OpCode::OP_Return, 1);
    
    loadChunk(chunk2);
    auto result2 = executeUntilReturn();
    EXPECT_EQ(result2, InterpretResult::RUNTIME_ERROR);
    
    // Test OP_Set_Global with insufficient stack values
    defineGlobal("existingVar", ElementType(42.0));
    Chunk chunk3;
    chunk3.addConstant(ElementType("existingVar"), 1);  // Only name, no new value
    chunk3.addCode(OpCode::OP_Set_Global, 1);
    chunk3.addCode(OpCode::OP_Return, 1);
    
    loadChunk(chunk3);
    auto result3 = executeUntilReturn();
    EXPECT_EQ(result3, InterpretResult::RUNTIME_ERROR);
}

TEST_F(VMTest, GlobalVariableNameValidation) {
    // Test non-literal variable name for Define_Global
    Chunk chunk1;
    chunk1.addConstant(ElementType(42.0), 1);  // value
    chunk1.addConstant(ElementType(123.0), 1);  // number instead of string name
    chunk1.addCode(OpCode::OP_Define_Global, 1);
    chunk1.addCode(OpCode::OP_Return, 1);
    
    loadChunk(chunk1);
    auto result1 = executeUntilReturn();
    EXPECT_EQ(result1, InterpretResult::RUNTIME_ERROR);
    
    // Test non-literal variable name for Get_Global
    Chunk chunk2;
    chunk2.addConstant(ElementType(123.0), 1);  // number instead of string name
    chunk2.addCode(OpCode::OP_Get_Global, 1);
    chunk2.addCode(OpCode::OP_Return, 1);
    
    loadChunk(chunk2);
    auto result2 = executeUntilReturn();
    EXPECT_EQ(result2, InterpretResult::RUNTIME_ERROR);
    
    // Test non-literal variable name for Set_Global
    defineGlobal("validVar", ElementType(42.0));
    Chunk chunk3;
    chunk3.addConstant(ElementType(99.0), 1);  // new value
    chunk3.addConstant(ElementType(123.0), 1);  // number instead of string name
    chunk3.addCode(OpCode::OP_Set_Global, 1);
    chunk3.addCode(OpCode::OP_Return, 1);
    
    loadChunk(chunk3);
    auto result3 = executeUntilReturn();
    EXPECT_EQ(result3, InterpretResult::RUNTIME_ERROR);
}

} // namespace test
} // namespace pg