#include "gtest/gtest.h"
#include "compiler_test_base.h"
#include "compiler_debug.h"
#include <sstream>
#include <iostream>

namespace pg {
namespace test {

class CompilerDebugTest : public CompilerTestBase {
protected:
    void SetUp() override {
        CompilerTestBase::SetUp();
        originalCoutBuffer = nullptr;
        captureOutput = false;
    }
    
    void TearDown() override {
        if (captureOutput && originalCoutBuffer) {
            restoreStdout();
        }
        CompilerTestBase::TearDown();
    }
    
    // Simple output capture for debug tests only
    void captureStdout() {
        if (!captureOutput) {
            originalCoutBuffer = std::cout.rdbuf();
            std::cout.rdbuf(capturedStream.rdbuf());
            captureOutput = true;
            capturedStream.str("");
        }
    }
    
    std::string restoreStdout() {
        if (captureOutput && originalCoutBuffer) {
            std::string output = capturedStream.str();
            std::cout.rdbuf(originalCoutBuffer);
            captureOutput = false;
            originalCoutBuffer = nullptr;
            return output;
        }
        return "";
    }
    
private:
    std::streambuf* originalCoutBuffer;
    std::ostringstream capturedStream;
    bool captureOutput;
};

// Chunk Disassembly Tests
TEST_F(CompilerDebugTest, DisassembleEmptyChunk) {
    Chunk emptyChunk;
    
    captureStdout();
    disassembleChunk(emptyChunk, "empty");
    auto output = restoreStdout();
    
    EXPECT_FALSE(output.empty()) << "Should produce some output for empty chunk";
    EXPECT_NE(output.find("empty"), std::string::npos) << "Should contain chunk name";
}

TEST_F(CompilerDebugTest, DisassembleSimpleChunk) {
    auto chunk = TestHelpers::makeSimpleArithmeticChunk();
    
    captureStdout();
    disassembleChunk(chunk, "arithmetic");
    auto output = restoreStdout();
    
    EXPECT_NE(output.find("arithmetic"), std::string::npos) << "Should contain chunk name";
    EXPECT_NE(output.find("OP_"), std::string::npos) << "Should contain opcode names";
}

TEST_F(CompilerDebugTest, DisassembleInstructionSimple) {
    Chunk chunk;
    chunk.addCode(OpCode::OP_Return, 1);
    
    captureStdout();
    auto offset = disassembleInstruction(chunk, 0);
    auto output = restoreStdout();
    
    EXPECT_EQ(offset, 1) << "OP_Return should advance offset by 1";
    EXPECT_NE(output.find("OP_Return"), std::string::npos) << "Should show instruction name";
}

TEST_F(CompilerDebugTest, DisassembleConstantInstruction) {
    Chunk chunk;
    chunk.addConstant(ElementType(3.14), 1);
    
    // Test that disassembleInstruction returns correct offset for constant instruction
    auto offset = disassembleInstruction(chunk, 0);
    EXPECT_EQ(offset, 2) << "OP_Constant should advance offset by 2";
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
    
    // Test that disassembleInstruction returns correct offset for long constant
    auto offset = disassembleInstruction(chunk, longConstOffset);
    EXPECT_EQ(offset, longConstOffset + 4) << "OP_LongConstant should advance offset by 4";
}

TEST_F(CompilerDebugTest, DisassembleArithmeticInstructions) {
    Chunk chunk;
    chunk.addCode(OpCode::OP_Add, 1);
    chunk.addCode(OpCode::OP_Subtract, 1);
    chunk.addCode(OpCode::OP_Multiply, 1);
    chunk.addCode(OpCode::OP_Divide, 1);
    
    // Test that arithmetic instructions advance offset correctly
    for (size_t i = 0; i < 4; ++i) {
        auto offset = disassembleInstruction(chunk, i);
        EXPECT_EQ(offset, i + 1) << "Arithmetic instructions should advance by 1";
    }
}

TEST_F(CompilerDebugTest, DisassembleUnaryInstructions) {
    Chunk chunk;
    chunk.addCode(OpCode::OP_Negate, 1);
    chunk.addCode(OpCode::OP_Not, 1);
    
    // Test that unary instructions disassemble without crashing
    EXPECT_NO_THROW(disassembleInstruction(chunk, 0));
    EXPECT_NO_THROW(disassembleInstruction(chunk, 1));
}

TEST_F(CompilerDebugTest, DisassembleBooleanInstructions) {
    Chunk chunk;
    chunk.addCode(OpCode::OP_True, 1);
    chunk.addCode(OpCode::OP_False, 1);
    
    captureStdout();
    disassembleInstruction(chunk, 0);
    auto output1 = restoreStdout();
    
    captureStdout();
    disassembleInstruction(chunk, 1);
    auto output2 = restoreStdout();
    
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
        captureStdout();
        auto offset = disassembleInstruction(chunk, i);
        auto output = restoreStdout();
        
        EXPECT_EQ(offset, i + 1) << "Comparison instructions should advance by 1";
        EXPECT_NE(output.find(expectedNames[i]), std::string::npos) 
            << "Should show correct instruction name: " << expectedNames[i];
    }
}

TEST_F(CompilerDebugTest, DisassembleUnknownInstruction) {
    Chunk chunk;
    chunk.code.push_back(255);  // Invalid opcode
    chunk.lines.push_back(1);
    
    captureStdout();
    auto offset = disassembleInstruction(chunk, 0);
    auto output = restoreStdout();
    
    EXPECT_EQ(offset, 1) << "Unknown instruction should advance by 1";
    EXPECT_NE(output.find("Unknown"), std::string::npos) << "Should indicate unknown opcode";
    // The unknown opcode is printed as "Unknown opcode" followed by the raw byte character
    EXPECT_NE(output.find("Unknown opcode"), std::string::npos) << "Should show unknown opcode message";
}

TEST_F(CompilerDebugTest, DisassembleWithLineNumbers) {
    Chunk chunk;
    chunk.addCode(OpCode::OP_True, 10);
    chunk.addCode(OpCode::OP_Not, 20);
    chunk.addCode(OpCode::OP_Return, 30);
    
    captureStdout();
    disassembleChunk(chunk, "line_test");
    auto output = restoreStdout();
    
    EXPECT_NE(output.find("10"), std::string::npos) << "Should show line 10";
    EXPECT_NE(output.find("20"), std::string::npos) << "Should show line 20";
    EXPECT_NE(output.find("30"), std::string::npos) << "Should show line 30";
}

TEST_F(CompilerDebugTest, DisassembleComplexChunk) {
    // Create a more complex chunk with mixed instructions
    auto chunk = compileExpression("(1 + 2) * !true");
    
    captureStdout();
    disassembleChunk(chunk, "complex");
    auto output = restoreStdout();
    
    EXPECT_NE(output.find("complex"), std::string::npos) << "Should contain chunk name";
    
    // Should contain various instruction types (expressions end with OP_Pop, not OP_Return)
    std::vector<std::string> expectedInstructions = {
        "OP_Constant", "OP_Add", "OP_Multiply", "OP_True", "OP_Not", "OP_Pop"
    };
    
    for (const auto& instruction : expectedInstructions) {
        EXPECT_NE(output.find(instruction), std::string::npos) 
            << "Should contain instruction: " << instruction;
    }
}

TEST_F(CompilerDebugTest, OffsetFormatting) {
    Chunk chunk;
    chunk.addCode(OpCode::OP_Return, 1);
    
    captureStdout();
    disassembleInstruction(chunk, 0);
    auto output = restoreStdout();
    
    // Should have formatted offset like "   0" (right-aligned 4 chars)
    EXPECT_NE(output.find("   0"), std::string::npos) << "Should format offset with leading spaces";
}

TEST_F(CompilerDebugTest, ConstantValueDisplay) {
    Chunk chunk;
    
    // Test different constant types
    chunk.addConstant(ElementType(42.0), 1);      // Number
    chunk.addConstant(ElementType(true), 2);      // Boolean
    chunk.addConstant(ElementType("test"), 3);    // String
    
    for (int i = 0; i < 3; ++i) {
        captureStdout();
        disassembleInstruction(chunk, i * 2);  // Each constant takes 2 bytes
        auto output = restoreStdout();
        
        EXPECT_NE(output.find("OP_Constant"), std::string::npos) 
            << "Should show constant instruction";
    }
}

TEST_F(CompilerDebugTest, EmptyChunkHandling) {
    Chunk emptyChunk;
    
    // Should handle empty chunk gracefully
    captureStdout();
    disassembleChunk(emptyChunk, "empty");
    auto output = restoreStdout();
    
    EXPECT_FALSE(output.empty()) << "Should produce output even for empty chunk";
    EXPECT_NE(output.find("empty"), std::string::npos) << "Should show chunk name";
}

} // namespace test
} // namespace pg