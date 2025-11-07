#pragma once

#include <vector>
#include <string>
#include <queue>
#include "chunk.h"
#include "Memory/elementtype.h"
#include "Interpreter/token.h"

namespace pg {
namespace test {

/**
 * Helper functions for testing the PgCompiler
 */
class TestHelpers {
public:
    // Token creation helpers
    static Token makeToken(TokenType type, const std::string& lexeme, int line = 1, int column = 1);
    static std::queue<Token> makeTokenQueue(const std::vector<std::pair<TokenType, std::string>>& tokens);
    
    // ElementType helpers
    static ElementType makeNumber(double value);
    static ElementType makeBool(bool value);
    static ElementType makeString(const std::string& value);
    
    // Chunk creation helpers
    static Chunk makeSimpleArithmeticChunk();
    static Chunk makeSimpleComparisonChunk();
    static Chunk makeBooleanChunk();
    
    // Bytecode verification
    static bool compareBytecode(const Chunk& actual, const std::vector<OpCode>& expected);
    static std::string bytecodeToString(const Chunk& chunk);
    
    // Test data creation
    static std::string simpleExpression();
    static std::string complexExpression();
    static std::string invalidExpression();
};

/**
 * Custom matchers for GoogleTest
 */
#define EXPECT_BYTECODE_EQ(chunk, expected) \
    EXPECT_TRUE(pg::test::TestHelpers::compareBytecode(chunk, expected)) \
    << "Expected bytecode: " << #expected << "\n" \
    << "Actual bytecode: " << pg::test::TestHelpers::bytecodeToString(chunk)

#define ASSERT_BYTECODE_EQ(chunk, expected) \
    ASSERT_TRUE(pg::test::TestHelpers::compareBytecode(chunk, expected)) \
    << "Expected bytecode: " << #expected << "\n" \
    << "Actual bytecode: " << pg::test::TestHelpers::bytecodeToString(chunk)

} // namespace test
} // namespace pg