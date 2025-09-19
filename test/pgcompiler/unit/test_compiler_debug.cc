#include "gtest/gtest.h"
#include "compiler_test_base.h"
#include "compiler_debug.h"

namespace pg {
namespace test {

class CompilerDebugTest : public CompilerTestBase {
protected:
    void SetUp() override {
        CompilerTestBase::SetUp();
    }
    
    // Helper to capture console output for testing debug functions
    void captureOutput() {
        captureStdout();
    }
    
    std::string getCapturedOutput() {
        restoreStdout();
        return capturedOutput;
    }
};

// Chunk Disassembly Tests
TEST_F(CompilerDebugTest, DisassembleEmptyChunk) {
    Chunk emptyChunk;
    
    captureOutput();
    disassembleChunk(emptyChunk, "empty");
    auto output = getCapturedOutput();
    
    EXPECT_FALSE(output.empty()) << "Should produce some output for empty chunk";
    EXPECT_NE(output.find("empty"), std::string::npos) << "Should contain chunk name";
}

TEST_F(CompilerDebugTest, DisassembleSimpleChunk) {
    auto chunk = TestHelpers::makeSimpleArithmeticChunk();
    
    captureOutput();
    disassembleChunk(chunk, "arithmetic");
    auto output = getCapturedOutput();
    
    EXPECT_NE(output.find("arithmetic"), std::string::npos) << "Should contain chunk name";
    EXPECT_NE(output.find("OP_"), std::string::npos) << "Should contain opcode names";
}

TEST_F(CompilerDebugTest, DisassembleInstructionSimple) {
    Chunk chunk;
    chunk.addCode(OpCode::OP_Return, 1);
    
    captureOutput();
    auto offset = disassembleInstruction(chunk, 0);
    auto output = getCapturedOutput();
    
    EXPECT_EQ(offset, 1) << "OP_Return should advance offset by 1";
    EXPECT_NE(output.find("OP_Return"), std::string::npos) << "Should show instruction name";
    EXPECT_NE(output.find("0000"), std::string::npos) << "Should show offset";
}

TEST_F(CompilerDebugTest, DisassembleConstantInstruction) {
    Chunk chunk;
    chunk.addConstant(ElementType(3.14), 1);
    
    captureOutput();
    auto offset = disassembleInstruction(chunk, 0);
    auto output = getCapturedOutput();
    
    EXPECT_EQ(offset, 2) << "OP_Constant should advance offset by 2";
    EXPECT_NE(output.find("OP_Constant"), std::string::npos) << "Should show instruction name";
    EXPECT_NE(output.find("3.14"), std::string::npos) << "Should show constant value";
}

TEST_F(CompilerDebugTest, DisassembleLongConstantInstruction) {
    Chunk chunk;
    
    // Force long constant by adding many constants
    for (int i = 0; i < 300; ++i) {
        chunk.constants.push_back(ElementType(static_cast<double>(i)));
    }
    chunk.addConstant(ElementType(999.0), 1);
    
    // Find the OP_LongConstant instruction
    size_t longConstOffset = 0;
    for (size_t i = 0; i < chunk.code.size(); ++i) {
        if (static_cast<OpCode>(chunk.code[i]) == OpCode::OP_LongConstant) {
            longConstOffset = i;
            break;
        }
    }
    
    captureOutput();
    auto offset = disassembleInstruction(chunk, longConstOffset);
    auto output = getCapturedOutput();
    
    EXPECT_EQ(offset, longConstOffset + 4) << "OP_LongConstant should advance offset by 4";
    EXPECT_NE(output.find("OP_LongConstant"), std::string::npos) << "Should show instruction name";
    EXPECT_NE(output.find("999"), std::string::npos) << "Should show constant value";
}

TEST_F(CompilerDebugTest, DisassembleArithmeticInstructions) {
    Chunk chunk;
    chunk.addCode(OpCode::OP_Add, 1);
    chunk.addCode(OpCode::OP_Subtract, 1);
    chunk.addCode(OpCode::OP_Multiply, 1);
    chunk.addCode(OpCode::OP_Divide, 1);
    
    for (size_t i = 0; i < 4; ++i) {
        captureOutput();
        auto offset = disassembleInstruction(chunk, i);
        auto output = getCapturedOutput();
        
        EXPECT_EQ(offset, i + 1) << "Arithmetic instructions should advance by 1";
        EXPECT_NE(output.find("OP_"), std::string::npos) << "Should show instruction name";
    }
}

TEST_F(CompilerDebugTest, DisassembleUnaryInstructions) {
    Chunk chunk;
    chunk.addCode(OpCode::OP_Negate, 1);
    chunk.addCode(OpCode::OP_Not, 1);
    
    captureOutput();
    disassembleInstruction(chunk, 0);
    auto output1 = getCapturedOutput();
    
    captureOutput();
    disassembleInstruction(chunk, 1);
    auto output2 = getCapturedOutput();
    
    EXPECT_NE(output1.find("OP_Negate"), std::string::npos);
    EXPECT_NE(output2.find("OP_Not"), std::string::npos);
}

TEST_F(CompilerDebugTest, DisassembleBooleanInstructions) {
    Chunk chunk;
    chunk.addCode(OpCode::OP_True, 1);
    chunk.addCode(OpCode::OP_False, 1);
    
    captureOutput();
    disassembleInstruction(chunk, 0);
    auto output1 = getCapturedOutput();
    
    captureOutput();
    disassembleInstruction(chunk, 1);
    auto output2 = getCapturedOutput();
    
    EXPECT_NE(output1.find("OP_True"), std::string::npos);
    EXPECT_NE(output2.find("OP_False"), std::string::npos);
}

TEST_F(CompilerDebugTest, DisassembleComparisonInstructions) {
    Chunk chunk;
    chunk.addCode(OpCode::OP_Equal, 1);
    chunk.addCode(OpCode::OP_NotEqual, 1);
    chunk.addCode(OpCode::OP_Greater, 1);
    chunk.addCode(OpCode::OP_GreaterEqual, 1);
    chunk.addCode(OpCode::OP_Less, 1);
    chunk.addCode(OpCode::OP_LessEqual, 1);
    
    std::vector<std::string> expectedNames = {
        "OP_Equal", "OP_NotEqual", "OP_Greater", 
        "OP_GreaterEqual", "OP_Less", "OP_LessEqual"
    };
    
    for (size_t i = 0; i < 6; ++i) {
        captureOutput();
        auto offset = disassembleInstruction(chunk, i);
        auto output = getCapturedOutput();
        
        EXPECT_EQ(offset, i + 1) << "Comparison instructions should advance by 1";
        EXPECT_NE(output.find(expectedNames[i]), std::string::npos) 
            << "Should show correct instruction name: " << expectedNames[i];
    }
}

TEST_F(CompilerDebugTest, DisassembleUnknownInstruction) {
    Chunk chunk;
    chunk.code.push_back(255);  // Invalid opcode
    chunk.lines.push_back(1);
    
    captureOutput();
    auto offset = disassembleInstruction(chunk, 0);
    auto output = getCapturedOutput();
    
    EXPECT_EQ(offset, 1) << "Unknown instruction should advance by 1";
    EXPECT_NE(output.find("Unknown"), std::string::npos) << "Should indicate unknown opcode";
    EXPECT_NE(output.find("255"), std::string::npos) << "Should show the unknown opcode value";
}

TEST_F(CompilerDebugTest, DisassembleWithLineNumbers) {
    Chunk chunk;
    chunk.addCode(OpCode::OP_True, 10);
    chunk.addCode(OpCode::OP_Not, 20);
    chunk.addCode(OpCode::OP_Return, 30);
    
    captureOutput();
    disassembleChunk(chunk, "line_test");
    auto output = getCapturedOutput();
    
    EXPECT_NE(output.find("10"), std::string::npos) << "Should show line 10";
    EXPECT_NE(output.find("20"), std::string::npos) << "Should show line 20";
    EXPECT_NE(output.find("30"), std::string::npos) << "Should show line 30";
}

TEST_F(CompilerDebugTest, DisassembleComplexChunk) {
    // Create a more complex chunk with mixed instructions
    auto chunk = compileExpression("(1 + 2) * !true");
    
    captureOutput();
    disassembleChunk(chunk, "complex");
    auto output = getCapturedOutput();
    
    EXPECT_NE(output.find("complex"), std::string::npos) << "Should contain chunk name";
    
    // Should contain various instruction types
    std::vector<std::string> expectedInstructions = {
        "OP_Constant", "OP_Add", "OP_Multiply", "OP_True", "OP_Not", "OP_Return"
    };
    
    for (const auto& instruction : expectedInstructions) {
        EXPECT_NE(output.find(instruction), std::string::npos) 
            << "Should contain instruction: " << instruction;
    }
}

TEST_F(CompilerDebugTest, OffsetFormatting) {
    Chunk chunk;
    chunk.addCode(OpCode::OP_Return, 1);
    
    captureOutput();
    disassembleInstruction(chunk, 0);
    auto output = getCapturedOutput();
    
    // Should have formatted offset like "0000"
    EXPECT_NE(output.find("0000"), std::string::npos) << "Should format offset with leading zeros";
}

TEST_F(CompilerDebugTest, ConstantValueDisplay) {
    Chunk chunk;
    
    // Test different constant types
    chunk.addConstant(ElementType(42.0), 1);      // Number
    chunk.addConstant(ElementType(true), 2);      // Boolean
    chunk.addConstant(ElementType("test"), 3);    // String
    
    for (int i = 0; i < 3; ++i) {
        captureOutput();
        disassembleInstruction(chunk, i * 2);  // Each constant takes 2 bytes
        auto output = getCapturedOutput();
        
        EXPECT_NE(output.find("OP_Constant"), std::string::npos) 
            << "Should show constant instruction";
    }
}

TEST_F(CompilerDebugTest, EmptyChunkHandling) {
    Chunk emptyChunk;
    
    // Should handle empty chunk gracefully
    captureOutput();
    disassembleChunk(emptyChunk, "empty");
    auto output = getCapturedOutput();
    
    EXPECT_FALSE(output.empty()) << "Should produce output even for empty chunk";
    EXPECT_NE(output.find("empty"), std::string::npos) << "Should show chunk name";
}

} // namespace test
} // namespace pg