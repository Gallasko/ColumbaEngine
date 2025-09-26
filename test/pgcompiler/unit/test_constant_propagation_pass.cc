#include <gtest/gtest.h>
#include "constant_propagation_pass.h"
#include "bytecode_rewriter.h"
#include "compiler_test_base.h"

using namespace pg;

class ConstantPropagationPassTest : public pg::test::CompilerTestBase {
protected:
    void SetUp() override {
        pg::test::CompilerTestBase::SetUp();
        pass = std::make_unique<ConstantPropagationPass>();
        rewriter = std::make_unique<BytecodeRewriter>();
    }

    std::unique_ptr<ConstantPropagationPass> pass;
    std::unique_ptr<BytecodeRewriter> rewriter;
};

TEST_F(ConstantPropagationPassTest, BasicLocalConstantPropagation) {
    // Test: var x = 5; use x; -> should replace use with constant 5
    Chunk chunk;

    // Create proper stack-based bytecode: value -> slot -> set_local
    chunk.addConstant(ElementType(5), 1);   // Push value 5 to stack
    chunk.addConstant(ElementType(0), 1);   // Push slot 0 to stack
    chunk.addCode(OpCode::OP_Set_Local, 1); // Pop slot, then value, store value in local[slot]

    chunk.addConstant(ElementType(0), 1);   // Push slot 0 to stack
    chunk.addCode(OpCode::OP_Get_Local, 1); // Pop slot, push local[slot] value

    chunk.addCode(OpCode::OP_Return, 1);

    // Run the constant propagation pass
    bool modified = pass->runPass(chunk, rewriter.get());

    EXPECT_TRUE(modified);

    // The OP_Get_Local should be replaced with OP_Constant
    // Find the get_local instruction and verify it's been replaced
    bool foundConstantReplacement = false;
    for (size_t i = 0; i < chunk.code.size(); i++) {
        OpCode op = static_cast<OpCode>(chunk.code[i]);
        if (op == OpCode::OP_Constant && i > 3) { // Skip the original constant
            foundConstantReplacement = true;
            break;
        }
    }

    EXPECT_TRUE(foundConstantReplacement);
}

TEST_F(ConstantPropagationPassTest, LocalConstantWithDifferentSlots) {
    // Test multiple local variables with constants
    Chunk chunk;

    // var x = 10 (slot 0)
    chunk.addConstant(ElementType(10), 1); // Push value 10
    chunk.addConstant(ElementType(0), 1);  // Push slot 0
    chunk.addCode(OpCode::OP_Set_Local, 1);

    // var y = 20 (slot 1)
    chunk.addConstant(ElementType(20), 1); // Push value 20
    chunk.addConstant(ElementType(1), 1);  // Push slot 1
    chunk.addCode(OpCode::OP_Set_Local, 1);

    // use x (should be replaced with 10)
    chunk.addConstant(ElementType(0), 1);  // Push slot 0
    chunk.addCode(OpCode::OP_Get_Local, 1);

    // use y (should be replaced with 20)
    chunk.addConstant(ElementType(1), 1);  // Push slot 1
    chunk.addCode(OpCode::OP_Get_Local, 1);

    chunk.addCode(OpCode::OP_Return, 1);

    bool modified = pass->runPass(chunk, rewriter.get());

    EXPECT_TRUE(modified);

    // Should have replaced both local variable accesses
    int constantCount = 0;
    for (size_t i = 0; i < chunk.code.size(); i++) {
        OpCode op = static_cast<OpCode>(chunk.code[i]);
        if (op == OpCode::OP_Constant) {
            constantCount++;
        }
    }

    // Should have 4 constants: original 10, original 20, replacement 10, replacement 20
    EXPECT_GE(constantCount, 4);
}

TEST_F(ConstantPropagationPassTest, GlobalConstantDetection) {
    // Test: var a = 0; (global definition pattern)
    Chunk chunk;

    // Global definition pattern: value_constant -> name_constant -> define_global
    chunk.addConstant(ElementType(0), 1);      // value: 0
    chunk.addConstant(ElementType("a"), 1);    // name: "a"
    chunk.addCode(OpCode::OP_Define_Global, 1);

    chunk.addCode(OpCode::OP_Return, 1);

    bool modified = pass->runPass(chunk, rewriter.get());

    // Should detect the global constant but not modify anything yet
    // (global propagation is not fully implemented)
    EXPECT_FALSE(modified);
}

TEST_F(ConstantPropagationPassTest, BooleanConstants) {
    // Test propagation of boolean constants
    Chunk chunk;

    // var flag = true (using OP_True)
    chunk.addCode(OpCode::OP_True, 1);      // Push true to stack
    chunk.addConstant(ElementType(0), 1);   // Push slot 0 to stack
    chunk.addCode(OpCode::OP_Set_Local, 1); // Pop slot, then value, store

    // use flag
    chunk.addConstant(ElementType(0), 1);   // Push slot 0 to stack
    chunk.addCode(OpCode::OP_Get_Local, 1); // Pop slot, push value

    chunk.addCode(OpCode::OP_Return, 1);

    bool modified = pass->runPass(chunk, rewriter.get());

    EXPECT_TRUE(modified);

    // Should find OP_True replacement
    bool foundTrueReplacement = false;
    int trueCount = 0;
    for (size_t i = 0; i < chunk.code.size(); i++) {
        OpCode op = static_cast<OpCode>(chunk.code[i]);
        if (op == OpCode::OP_True) {
            trueCount++;
            if (trueCount > 1) { // More than the original
                foundTrueReplacement = true;
            }
        }
    }

    EXPECT_TRUE(foundTrueReplacement);
}

TEST_F(ConstantPropagationPassTest, NoConstantAssignments) {
    // Test chunk with no constant assignments
    Chunk chunk;

    // Just some arithmetic without constants assigned to variables
    chunk.addConstant(ElementType(5), 1);
    chunk.addConstant(ElementType(10), 1);
    chunk.addCode(OpCode::OP_Add, 1);
    chunk.addCode(OpCode::OP_Return, 1);

    bool modified = pass->runPass(chunk, rewriter.get());

    EXPECT_FALSE(modified);
}

TEST_F(ConstantPropagationPassTest, NonConstantAssignment) {
    // Test variable assigned from non-constant (should not be propagated)
    Chunk chunk;

    // var x = some_expression (not a direct constant)
    chunk.addConstant(ElementType(5), 1);
    chunk.addConstant(ElementType(10), 1);
    chunk.addCode(OpCode::OP_Add, 1);        // 5 + 10
    chunk.addCode(OpCode::OP_Set_Local, 1);  // store result
    chunk.addCode(0, 1);                     // slot 0

    // use x
    chunk.addCode(OpCode::OP_Get_Local, 1);
    chunk.addCode(0, 1);  // slot 0

    chunk.addCode(OpCode::OP_Return, 1);

    bool modified = pass->runPass(chunk, rewriter.get());

    // Should not propagate because assignment is not from a direct constant
    EXPECT_FALSE(modified);
}

TEST_F(ConstantPropagationPassTest, LongConstantSupport) {
    // Test long constants (more than 255 constant indices)
    Chunk chunk;

    // Simulate a scenario where we need OP_LongConstant
    // We'll manually create the bytecode pattern
    chunk.constants.resize(300); // Force long constant usage
    chunk.constants[256] = ElementType(42);

    // Proper stack-based pattern: value -> slot -> set_local
    chunk.addCode(OpCode::OP_LongConstant, 1);
    chunk.addCode(0, 1);   // index low byte
    chunk.addCode(1, 1);   // index middle byte
    chunk.addCode(0, 1);   // index high byte (256 = 0x100)
    chunk.addConstant(ElementType(0), 1);   // Push slot 0 to stack
    chunk.addCode(OpCode::OP_Set_Local, 1); // Pop slot, pop value, store

    // Load the variable: slot -> get_local
    chunk.addConstant(ElementType(0), 1);   // Push slot 0 to stack
    chunk.addCode(OpCode::OP_Get_Local, 1); // Pop slot, push value

    chunk.addCode(OpCode::OP_Return, 1);

    bool modified = pass->runPass(chunk, rewriter.get());

    EXPECT_TRUE(modified);
}

TEST_F(ConstantPropagationPassTest, MultiplePassesRequired) {
    // Test that requiresMultiplePasses returns true
    EXPECT_TRUE(pass->requiresMultiplePasses());
}

TEST_F(ConstantPropagationPassTest, DoesNotChangeSize) {
    // The pass should not change bytecode size for simple replacements
    EXPECT_FALSE(pass->changesSize());
}

TEST_F(ConstantPropagationPassTest, PassName) {
    // Test the pass name
    EXPECT_EQ(pass->getName(), "ConstantPropagation");
}