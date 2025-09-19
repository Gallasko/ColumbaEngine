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

} // namespace test
} // namespace pg