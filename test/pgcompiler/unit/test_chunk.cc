#include "gtest/gtest.h"
#include "compiler_test_base.h"
#include "chunk.h"

namespace pg {
namespace test {

class ChunkTest : public CompilerTestBase {
protected:
    Chunk chunk;
};

TEST_F(ChunkTest, EmptyChunkInitialization) {
    EXPECT_TRUE(chunk.code.empty());
    EXPECT_TRUE(chunk.constants.empty());
    EXPECT_TRUE(chunk.lines.empty());
}

TEST_F(ChunkTest, AddSimpleCode) {
    auto index = chunk.addCode(OpCode::OP_Return, 1);

    EXPECT_EQ(chunk.code.size(), 1);
    EXPECT_EQ(chunk.lines.size(), 1);
    EXPECT_EQ(chunk.code[0], static_cast<uint8_t>(OpCode::OP_Return));
    EXPECT_EQ(chunk.lines[0], 1);
    EXPECT_EQ(index, 0);
}

TEST_F(ChunkTest, AddMultipleCodes) {
    chunk.addCode(OpCode::OP_True, 1);
    chunk.addCode(OpCode::OP_Not, 2);
    chunk.addCode(OpCode::OP_Return, 3);

    EXPECT_EQ(chunk.code.size(), 3);
    EXPECT_EQ(chunk.lines.size(), 3);
    EXPECT_EQ(static_cast<OpCode>(chunk.code[0]), OpCode::OP_True);
    EXPECT_EQ(static_cast<OpCode>(chunk.code[1]), OpCode::OP_Not);
    EXPECT_EQ(static_cast<OpCode>(chunk.code[2]), OpCode::OP_Return);
}

TEST_F(ChunkTest, AddByteCode) {
    chunk.addCode(static_cast<uint8_t>(42), 1);

    EXPECT_EQ(chunk.code.size(), 1);
    EXPECT_EQ(chunk.code[0], 42);
    EXPECT_EQ(chunk.lines[0], 1);
}

TEST_F(ChunkTest, AddConstantShort) {
    ElementType value(3.14);
    chunk.addConstant(value, 1);

    // Should generate OP_Constant + index
    EXPECT_EQ(chunk.code.size(), 2);
    EXPECT_EQ(static_cast<OpCode>(chunk.code[0]), OpCode::OP_Constant);
    EXPECT_EQ(chunk.code[1], 0);  // First constant has index 0

    EXPECT_EQ(chunk.constants.size(), 1);
    EXPECT_FLOAT_EQ(chunk.constants[0].get<float>(), 3.14);
}

TEST_F(ChunkTest, AddMultipleConstants) {
    chunk.addConstant(ElementType(1.0), 1);
    chunk.addConstant(ElementType(2.0), 1);
    chunk.addConstant(ElementType(3.0), 1);

    EXPECT_EQ(chunk.constants.size(), 3);
    EXPECT_FLOAT_EQ(chunk.constants[0].get<float>(), 1.0);
    EXPECT_FLOAT_EQ(chunk.constants[1].get<float>(), 2.0);
    EXPECT_FLOAT_EQ(chunk.constants[2].get<float>(), 3.0);

    // Should have 6 bytes of code (3 * (OP_Constant + index))
    EXPECT_EQ(chunk.code.size(), 6);
}

TEST_F(ChunkTest, AddLongConstant) {
    // Add enough constants to force long constant encoding
    for (int i = 0; i < 300; ++i) {
        chunk.constants.push_back(ElementType(static_cast<double>(i)));
    }

    // This should trigger OP_LongConstant
    ElementType value(999.0);
    chunk.addConstant(value, 1);

    // Should generate OP_LongConstant + 3 bytes for index
    EXPECT_GE(chunk.code.size(), 4);

    // Find the OP_LongConstant instruction
    bool foundLongConstant = false;
    for (size_t i = 0; i < chunk.code.size(); ++i) {
        if (static_cast<OpCode>(chunk.code[i]) == OpCode::OP_LongConstant) {
            foundLongConstant = true;
            break;
        }
    }
    EXPECT_TRUE(foundLongConstant) << "Should contain OP_LongConstant instruction";

    EXPECT_EQ(chunk.constants.size(), 301);
    EXPECT_FLOAT_EQ(chunk.constants[300].get<float>(), 999.0);
}

TEST_F(ChunkTest, ConstantPoolLimit) {
    // Test the theoretical limit (should throw if exceeded)
    Chunk limitChunk;

    // The implementation checks for > 0xFFFFFF (16M constants)
    // We can't actually test this due to memory constraints,
    // but we can verify the check exists by looking at the code structure

    // Add a reasonable number and verify it works
    for (int i = 0; i < 1000; ++i) {
        EXPECT_NO_THROW(limitChunk.addConstant(ElementType(static_cast<double>(i)), 1));
    }

    EXPECT_EQ(limitChunk.constants.size(), 1000);
}

TEST_F(ChunkTest, LineNumberTracking) {
    chunk.addCode(OpCode::OP_True, 5);
    chunk.addConstant(ElementType(42.0), 10);
    chunk.addCode(OpCode::OP_Return, 15);

    EXPECT_EQ(chunk.lines[0], 5);   // OP_True
    EXPECT_EQ(chunk.lines[1], 10);  // OP_Constant
    EXPECT_EQ(chunk.lines[2], 10);  // constant index
    EXPECT_EQ(chunk.lines[3], 15);  // OP_Return
}

TEST_F(ChunkTest, BooleanConstants) {
    chunk.addConstant(ElementType(true), 1);
    chunk.addConstant(ElementType(false), 1);

    EXPECT_EQ(chunk.constants.size(), 2);
    EXPECT_TRUE(chunk.constants[0].isTrue());
    EXPECT_FALSE(chunk.constants[1].isTrue());
}

TEST_F(ChunkTest, StringConstants) {
    chunk.addConstant(ElementType("hello"), 1);
    chunk.addConstant(ElementType("world"), 1);

    EXPECT_EQ(chunk.constants.size(), 2);
    EXPECT_EQ(chunk.constants[0].toString(), "hello");
    EXPECT_EQ(chunk.constants[1].toString(), "world");
}

} // namespace test
} // namespace pg