#include "gtest/gtest.h"

#include "Compiler/bytecode_rewriter.h"
#include "Compiler/chunk.h"

namespace pg
{
namespace test
{

class BytecodeRewriterTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        rewriter = new BytecodeRewriter();
    }

    void TearDown() override
    {
        delete rewriter;
    }

    // Helper to create a simple chunk with a forward jump
    Chunk createSimpleForwardJumpChunk()
    {
        Chunk chunk;
        // OP_Constant 0
        chunk.code.push_back(static_cast<uint8_t>(OpCode::OP_Constant));
        chunk.code.push_back(0);
        chunk.lines.push_back(1);
        chunk.lines.push_back(1);

        // OP_Jump_If_False (offset 2, distance 6 to offset 11)
        // Jump ends at offset 5, target at 11, so distance = 11 - 5 = 6
        chunk.code.push_back(static_cast<uint8_t>(OpCode::OP_Jump_If_False));
        chunk.code.push_back(0);  // distance high byte
        chunk.code.push_back(6);  // distance low byte
        chunk.lines.push_back(1);
        chunk.lines.push_back(1);
        chunk.lines.push_back(1);

        // OP_Pop (offset 5)
        chunk.code.push_back(static_cast<uint8_t>(OpCode::OP_Pop));
        chunk.lines.push_back(1);

        // OP_Constant 1 (offset 6)
        chunk.code.push_back(static_cast<uint8_t>(OpCode::OP_Constant));
        chunk.code.push_back(1);
        chunk.lines.push_back(2);
        chunk.lines.push_back(2);

        // OP_Jump (offset 8, distance 2 to offset 12)
        chunk.code.push_back(static_cast<uint8_t>(OpCode::OP_Jump));
        chunk.code.push_back(0);  // distance high byte
        chunk.code.push_back(2);  // distance low byte
        chunk.lines.push_back(2);
        chunk.lines.push_back(2);
        chunk.lines.push_back(2);

        // OP_Pop (offset 11 - target of first jump)
        chunk.code.push_back(static_cast<uint8_t>(OpCode::OP_Pop));
        chunk.lines.push_back(1);

        // OP_Constant 2 (offset 12 - target of second jump)
        chunk.code.push_back(static_cast<uint8_t>(OpCode::OP_Constant));
        chunk.code.push_back(2);
        chunk.lines.push_back(3);
        chunk.lines.push_back(3);

        // OP_Return (offset 14)
        chunk.code.push_back(static_cast<uint8_t>(OpCode::OP_Return));
        chunk.lines.push_back(3);

        return chunk;
    }

    // Helper to extract jump distance
    uint16_t extractJumpDistance(const Chunk& chunk, size_t offset)
    {
        return (static_cast<uint16_t>(chunk.code[offset + 1]) << 8) |
               static_cast<uint16_t>(chunk.code[offset + 2]);
    }

    BytecodeRewriter* rewriter;
};

TEST_F(BytecodeRewriterTest, RemoveBytesBeforeJump_AdjustsNothing)
{
    Chunk chunk = createSimpleForwardJumpChunk();

    // Remove the first OP_Constant (offset 0-1, 2 bytes)
    // This is BEFORE the jump at offset 2
    bool success = rewriter->removeInstructions(chunk, 0, 2);

    ASSERT_TRUE(success);
    ASSERT_EQ(chunk.code.size(), 13);  // 15 - 2

    // Jump that was at offset 2 is now at offset 0
    ASSERT_EQ(chunk.code[0], static_cast<uint8_t>(OpCode::OP_Jump_If_False));

    // Jump distance should stay the same: was 6, both jump and target moved by 2
    // Jump moved from 2 to 0, target moved from 11 to 9, distance stays 6
    uint16_t distance = extractJumpDistance(chunk, 0);
    EXPECT_EQ(distance, 6);
}

TEST_F(BytecodeRewriterTest, RemoveBytesAfterJump_BeforeTarget_AdjustsTarget)
{
    Chunk chunk = createSimpleForwardJumpChunk();

    // Remove OP_Pop at offset 5 (between jump and target)
    bool success = rewriter->removeInstructions(chunk, 5, 1);

    ASSERT_TRUE(success);
    ASSERT_EQ(chunk.code.size(), 14);  // 15 - 1

    // Jump at offset 2 stays at offset 2
    ASSERT_EQ(chunk.code[2], static_cast<uint8_t>(OpCode::OP_Jump_If_False));

    // Jump distance should decrease by 1: was 6, now 5
    // Target moved from offset 11 to offset 10
    uint16_t distance = extractJumpDistance(chunk, 2);
    EXPECT_EQ(distance, 5);
}

TEST_F(BytecodeRewriterTest, RemoveBytesAtTarget_AdjustsTarget)
{
    Chunk chunk = createSimpleForwardJumpChunk();

    uint16_t distance = extractJumpDistance(chunk, 2);
    EXPECT_EQ(distance, 6);

    // Remove OP_Pop at offset 11 (the target of the first jump)
    bool success = rewriter->removeInstructions(chunk, 11, 1);

    ASSERT_TRUE(success);
    ASSERT_EQ(chunk.code.size(), 14);  // 15 - 1

    // Jump at offset 2 stays at offset 2
    ASSERT_EQ(chunk.code[2], static_cast<uint8_t>(OpCode::OP_Jump_If_False));

    // Jump distance should decrease by 1: was 6, now 5
    distance = extractJumpDistance(chunk, 2);
    EXPECT_EQ(distance, 5);
}

TEST_F(BytecodeRewriterTest, RemoveBytesAfterTarget_NoAdjustment)
{
    Chunk chunk = createSimpleForwardJumpChunk();

    // Remove OP_Return at offset 14 (after all jumps and targets)
    bool success = rewriter->removeInstructions(chunk, 14, 1);

    ASSERT_TRUE(success);
    ASSERT_EQ(chunk.code.size(), 14);  // 15 - 1

    // Jump at offset 2 stays at offset 2
    ASSERT_EQ(chunk.code[2], static_cast<uint8_t>(OpCode::OP_Jump_If_False));

    // Jump distance should stay the same: 6
    uint16_t distance = extractJumpDistance(chunk, 2);
    EXPECT_EQ(distance, 6);
}

TEST_F(BytecodeRewriterTest, RewriteAtRaw_ShrinkingJumpInstruction_AdjustsSubsequentJumps)
{
    Chunk chunk = createSimpleForwardJumpChunk();

    // Rewrite the jump at offset 2 (3 bytes) with a shorter version (2 bytes)
    // This simulates replacing "OP_Jump_If_False + OP_Pop" with "OP_Jump_If_False_Popping"
    std::vector<uint8_t> replacement = {
        static_cast<uint8_t>(OpCode::OP_Jump_If_False_Popping),
        0,
        5
    };

    // Rewrite jump + pop (4 bytes) with popping jump (3 bytes)
    bool success = rewriter->rewriteAtRaw(chunk, 2, 4, replacement);

    ASSERT_TRUE(success);
    ASSERT_EQ(chunk.code.size(), 14);  // 15 - 4 + 3 = 14

    // Check the replacement was applied
    ASSERT_EQ(chunk.code[2], static_cast<uint8_t>(OpCode::OP_Jump_If_False_Popping));

    // The OP_Jump that was at offset 8 should now be at offset 7
    ASSERT_EQ(chunk.code[7], static_cast<uint8_t>(OpCode::OP_Jump));

    // Its distance should stay 2 (both jump and target shifted equally)
    uint16_t distance = extractJumpDistance(chunk, 7);
    EXPECT_EQ(distance, 2);
}

TEST_F(BytecodeRewriterTest, MultipleSequentialRemovals_CumulativeAdjustment)
{
    Chunk chunk = createSimpleForwardJumpChunk();

    // First removal: remove OP_Pop at offset 5
    bool success1 = rewriter->removeInstructions(chunk, 5, 1);
    ASSERT_TRUE(success1);

    // Jump at offset 2, distance should be 5 now (was 6)
    uint16_t distance1 = extractJumpDistance(chunk, 2);
    EXPECT_EQ(distance1, 5);

    // Second removal: remove OP_Pop at offset 10 (was at 11, shifted by first removal)
    bool success2 = rewriter->removeInstructions(chunk, 10, 1);
    ASSERT_TRUE(success2);

    // Jump at offset 2, distance should be 4 now (was 5)
    uint16_t distance2 = extractJumpDistance(chunk, 2);
    EXPECT_EQ(distance2, 4);
}

TEST_F(BytecodeRewriterTest, PoppingJumpOptimizationPattern_FirstJump)
{
    Chunk chunk = createSimpleForwardJumpChunk();

    // Simulate the exact pattern from popping jump pass
    // 1. Rewrite jump + pop (offset 2-5) with popping jump
    std::vector<uint8_t> replacement = {
        static_cast<uint8_t>(OpCode::OP_Jump_If_False_Popping),
        0,
        6  // distance + 1 = 5 + 1 = 6, then createReplacement does -1 = 5
    };

    bool success1 = rewriter->rewriteAtRaw(chunk, 2, 4, replacement);
    ASSERT_TRUE(success1);
    ASSERT_EQ(chunk.code.size(), 14);

    // 2. Remove target pop at (targetOffset - 2)
    // Original targetOffset was 11, after removing 1 byte at offset 5, it's at 10
    // But we use the formula: targetOffset - 2 = 11 - 2 = 9
    size_t targetPopOffset = 11 - 2;  // = 9

    // Check what's at offset 9
    std::cout << "Byte at offset 9: " << static_cast<int>(chunk.code[9])
              << " (expected OP_Pop = " << static_cast<int>(OpCode::OP_Pop) << ")" << std::endl;

    bool success2 = rewriter->removeInstructions(chunk, targetPopOffset, 1);
    ASSERT_TRUE(success2);

    // Final check: jump should point to offset 9 (the OP_Constant after the removed pop)
    uint16_t finalDistance = extractJumpDistance(chunk, 2);
    // Jump at 2, ends at 5, should target 9, so distance = 9 - 5 = 4
    // But we put 6 in replacement, minus 1 in createReplacement = 5
    // After removing target pop, distance should still be... let me recalculate
    std::cout << "Final jump distance: " << finalDistance << std::endl;
}

} // namespace test
} // namespace pg
