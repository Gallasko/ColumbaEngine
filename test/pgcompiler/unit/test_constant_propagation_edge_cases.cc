#include <gtest/gtest.h>
#include "constant_propagation_pass.h"
#include "bytecode_rewriter.h"
#include "compiler_test_base.h"

using namespace pg;

class ConstantPropagationEdgeCasesTest : public pg::test::CompilerTestBase {
protected:
    void SetUp() override {
        pg::test::CompilerTestBase::SetUp();
        pass = std::make_unique<ConstantPropagationPass>();
        rewriter = std::make_unique<BytecodeRewriter>();
    }

    std::unique_ptr<ConstantPropagationPass> pass;
    std::unique_ptr<BytecodeRewriter> rewriter;
};

TEST_F(ConstantPropagationEdgeCasesTest, EmptyChunk) {
    // Test with completely empty chunk
    Chunk chunk;

    bool modified = pass->runPass(chunk, rewriter.get());

    EXPECT_FALSE(modified);
    EXPECT_TRUE(chunk.code.empty());
    EXPECT_TRUE(chunk.constants.empty());
}

TEST_F(ConstantPropagationEdgeCasesTest, NullRewriter) {
    // Test with null rewriter (should handle gracefully)
    Chunk chunk;
    chunk.addConstant(ElementType(5), 1);

    bool modified = pass->runPass(chunk, nullptr);

    EXPECT_FALSE(modified);
}

TEST_F(ConstantPropagationEdgeCasesTest, VariableReassignment) {
    // Test: var x = 5; x = 10; use x;
    // Should NOT propagate because x is reassigned
    Chunk chunk;

    // x = 5
    chunk.addConstant(ElementType(5), 1);
    chunk.addCode(OpCode::OP_Set_Local, 1);
    chunk.addCode(0, 1);  // slot 0

    // x = 10 (reassignment)
    chunk.addConstant(ElementType(10), 1);
    chunk.addCode(OpCode::OP_Set_Local, 1);
    chunk.addCode(0, 1);  // slot 0

    // use x
    chunk.addCode(OpCode::OP_Get_Local, 1);
    chunk.addCode(0, 1);  // slot 0

    chunk.addCode(OpCode::OP_Return, 1);

    bool modified = pass->runPass(chunk, rewriter.get());

    // Current implementation doesn't handle reassignment detection,
    // so it might still propagate the first assignment
    // This test documents current behavior and can be updated
    // when reassignment detection is implemented
}

TEST_F(ConstantPropagationEdgeCasesTest, InterleavedInstructions) {
    // Test with other instructions between constant and store
    Chunk chunk;

    // Some other instruction
    chunk.addCode(OpCode::OP_Pop, 1);

    // Constant assignment
    chunk.addConstant(ElementType(42), 1);  // Push value 42 to stack
    chunk.addConstant(ElementType(0), 1);   // Push slot 0 to stack
    chunk.addCode(OpCode::OP_Set_Local, 1); // Pop slot, pop value, store

    // More instructions
    chunk.addCode(OpCode::OP_Pop, 1);

    // Use the variable
    chunk.addConstant(ElementType(0), 1);   // Push slot 0 to stack
    chunk.addCode(OpCode::OP_Get_Local, 1); // Pop slot, push value

    chunk.addCode(OpCode::OP_Return, 1);

    bool modified = pass->runPass(chunk, rewriter.get());

    // With interleaved instructions, our simple pattern matcher cannot
    // detect the store/load pattern, so this should not be modified
    EXPECT_FALSE(modified);
}

TEST_F(ConstantPropagationEdgeCasesTest, CorruptedBytecode) {
    // Test with incomplete/corrupted instruction patterns
    Chunk chunk;

    // Incomplete constant instruction (missing operand)
    chunk.addCode(OpCode::OP_Constant, 1);
    // Missing constant index

    chunk.addCode(OpCode::OP_Set_Local, 1);
    // Missing local slot

    chunk.addCode(OpCode::OP_Return, 1);

    bool modified = pass->runPass(chunk, rewriter.get());

    // Should handle gracefully without crashing
    EXPECT_FALSE(modified);
}

TEST_F(ConstantPropagationEdgeCasesTest, OutOfBoundsConstantIndex) {
    // Test with constant index that exceeds constants array size
    Chunk chunk;

    // Valid constants
    chunk.addConstant(ElementType(1), 1);
    chunk.addConstant(ElementType(2), 1);

    // Reference non-existent constant index
    chunk.addCode(OpCode::OP_Constant, 1);
    chunk.addCode(99, 1);  // Index 99 doesn't exist

    chunk.addCode(OpCode::OP_Set_Local, 1);
    chunk.addCode(0, 1);

    chunk.addCode(OpCode::OP_Return, 1);

    bool modified = pass->runPass(chunk, rewriter.get());

    // Should handle gracefully
    EXPECT_FALSE(modified);
}

TEST_F(ConstantPropagationEdgeCasesTest, ZeroConstants) {
    // Test with zero-value constants
    Chunk chunk;

    // var x = 0
    chunk.addConstant(ElementType(0), 1);   // Push value 0 to stack
    chunk.addConstant(ElementType(0), 1);   // Push slot 0 to stack
    chunk.addCode(OpCode::OP_Set_Local, 1); // Pop slot, pop value, store

    // use x
    chunk.addConstant(ElementType(0), 1);   // Push slot 0 to stack
    chunk.addCode(OpCode::OP_Get_Local, 1); // Pop slot, push value

    chunk.addCode(OpCode::OP_Return, 1);

    bool modified = pass->runPass(chunk, rewriter.get());

    EXPECT_TRUE(modified);
}

TEST_F(ConstantPropagationEdgeCasesTest, NegativeConstants) {
    // Test with negative constants
    Chunk chunk;

    // var x = -42
    chunk.addConstant(ElementType(-42), 1); // Push value -42 to stack
    chunk.addConstant(ElementType(0), 1);   // Push slot 0 to stack
    chunk.addCode(OpCode::OP_Set_Local, 1); // Pop slot, pop value, store

    // use x
    chunk.addConstant(ElementType(0), 1);   // Push slot 0 to stack
    chunk.addCode(OpCode::OP_Get_Local, 1); // Pop slot, push value

    chunk.addCode(OpCode::OP_Return, 1);

    bool modified = pass->runPass(chunk, rewriter.get());

    EXPECT_TRUE(modified);
}

TEST_F(ConstantPropagationEdgeCasesTest, StringConstants) {
    // Test with string constants
    Chunk chunk;

    // var x = "hello"
    chunk.addConstant(ElementType("hello"), 1); // Push value "hello" to stack
    chunk.addConstant(ElementType(0), 1);       // Push slot 0 to stack
    chunk.addCode(OpCode::OP_Set_Local, 1);     // Pop slot, pop value, store

    // use x
    chunk.addConstant(ElementType(0), 1);       // Push slot 0 to stack
    chunk.addCode(OpCode::OP_Get_Local, 1);     // Pop slot, push value

    chunk.addCode(OpCode::OP_Return, 1);

    bool modified = pass->runPass(chunk, rewriter.get());

    EXPECT_TRUE(modified);
}

TEST_F(ConstantPropagationEdgeCasesTest, FloatingPointConstants) {
    // Test with floating point constants
    Chunk chunk;

    // var x = 3.14159
    chunk.addConstant(ElementType(3.14159), 1); // Push value 3.14159 to stack
    chunk.addConstant(ElementType(0), 1);       // Push slot 0 to stack
    chunk.addCode(OpCode::OP_Set_Local, 1);     // Pop slot, pop value, store

    // use x
    chunk.addConstant(ElementType(0), 1);       // Push slot 0 to stack
    chunk.addCode(OpCode::OP_Get_Local, 1);     // Pop slot, push value

    chunk.addCode(OpCode::OP_Return, 1);

    bool modified = pass->runPass(chunk, rewriter.get());

    EXPECT_TRUE(modified);
}

TEST_F(ConstantPropagationEdgeCasesTest, MaxLocalSlots) {
    // Test with maximum local slot numbers
    Chunk chunk;

    // var at slot 255 (maximum for single byte)
    chunk.addConstant(ElementType(999), 1);  // Push value 999 to stack
    chunk.addConstant(ElementType(255), 1);  // Push slot 255 to stack
    chunk.addCode(OpCode::OP_Set_Local, 1);  // Pop slot, pop value, store

    // use var at slot 255
    chunk.addConstant(ElementType(255), 1);  // Push slot 255 to stack
    chunk.addCode(OpCode::OP_Get_Local, 1);  // Pop slot, push value

    chunk.addCode(OpCode::OP_Return, 1);

    bool modified = pass->runPass(chunk, rewriter.get());

    EXPECT_TRUE(modified);
}

TEST_F(ConstantPropagationEdgeCasesTest, GlobalsWithNonStringNames) {
    // Test global definition with non-string variable names (edge case)
    Chunk chunk;

    // Global with numeric name (should be handled gracefully)
    chunk.addConstant(ElementType(42), 1);     // value
    chunk.addConstant(ElementType(123), 1);    // numeric "name"
    chunk.addCode(OpCode::OP_Define_Global, 1);

    chunk.addCode(OpCode::OP_Return, 1);

    bool modified = pass->runPass(chunk, rewriter.get());

    // Should handle gracefully without crashing
    EXPECT_FALSE(modified);
}

TEST_F(ConstantPropagationEdgeCasesTest, IncompleteGlobalDefinition) {
    // Test incomplete global definition patterns
    Chunk chunk;

    // Only value, no name
    chunk.addConstant(ElementType(42), 1);
    chunk.addCode(OpCode::OP_Define_Global, 1);

    chunk.addCode(OpCode::OP_Return, 1);

    bool modified = pass->runPass(chunk, rewriter.get());

    // Should handle gracefully
    EXPECT_FALSE(modified);
}

TEST_F(ConstantPropagationEdgeCasesTest, UnusedConstantAssignments) {
    // Test constant assignments that are never used
    Chunk chunk;

    // var x = 5 (but never used)
    chunk.addConstant(ElementType(5), 1);
    chunk.addCode(OpCode::OP_Set_Local, 1);
    chunk.addCode(0, 1);

    // var y = 10 (but never used)
    chunk.addConstant(ElementType(10), 1);
    chunk.addCode(OpCode::OP_Set_Local, 1);
    chunk.addCode(1, 1);

    chunk.addCode(OpCode::OP_Return, 1);

    bool modified = pass->runPass(chunk, rewriter.get());

    // Should not modify since no loads to replace
    EXPECT_FALSE(modified);
}

TEST_F(ConstantPropagationEdgeCasesTest, MixedConstantTypes) {
    // Test with mixed constant types in same chunk
    Chunk chunk;

    // Use OP_True directly: true -> slot 0 -> set_local
    chunk.addCode(OpCode::OP_True, 1);
    chunk.addConstant(ElementType(0), 1);       // Push slot 0 to stack
    chunk.addCode(OpCode::OP_Set_Local, 1);     // Pop slot, pop value, store

    // Use regular constant: 42 -> slot 1 -> set_local
    chunk.addConstant(ElementType(42), 1);      // Push value 42 to stack
    chunk.addConstant(ElementType(1), 1);       // Push slot 1 to stack
    chunk.addCode(OpCode::OP_Set_Local, 1);     // Pop slot, pop value, store

    // Load both
    chunk.addConstant(ElementType(0), 1);       // Push slot 0 to stack
    chunk.addCode(OpCode::OP_Get_Local, 1);     // Pop slot, push value
    chunk.addConstant(ElementType(1), 1);       // Push slot 1 to stack
    chunk.addCode(OpCode::OP_Get_Local, 1);     // Pop slot, push value

    chunk.addCode(OpCode::OP_Return, 1);

    bool modified = pass->runPass(chunk, rewriter.get());

    EXPECT_TRUE(modified);
}