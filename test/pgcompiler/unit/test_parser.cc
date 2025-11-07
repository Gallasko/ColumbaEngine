#include "gtest/gtest.h"
#include "compiler_test_base.h"
#include "cparser.h"
#include "compiler.h"
#include "test_helpers.h"

namespace pg {
namespace test {

class ParserTest : public CompilerTestBase {
protected:
    Parser parser;

    void SetUp() override {
        CompilerTestBase::SetUp();
        parser.reset();
    }

    std::queue<Token> createTokens(const std::vector<std::pair<TokenType, std::string>>& tokenData) {
        return TestHelpers::makeTokenQueue(tokenData);
    }
};

// Token Navigation Tests
TEST_F(ParserTest, BasicTokenNavigation) {
    auto tokens = createTokens({
        {TokenType::NUMBER, "42"},
        {TokenType::PLUS, "+"},
        {TokenType::NUMBER, "3"}
    });

    parser.parse(tokens);

    EXPECT_EQ(parser.peek(), TokenType::NUMBER);
    EXPECT_FALSE(parser.isAtEnd());

    parser.advance();
    EXPECT_EQ(parser.peek(), TokenType::PLUS);

    parser.advance();
    EXPECT_EQ(parser.peek(), TokenType::NUMBER);

    parser.advance();
    EXPECT_EQ(parser.peek(), TokenType::ENDOFFILE);
    EXPECT_TRUE(parser.isAtEnd());
}

TEST_F(ParserTest, CheckTokenType) {
    auto tokens = createTokens({
        {TokenType::NUMBER, "42"},
        {TokenType::PLUS, "+"}
    });

    parser.parse(tokens);

    EXPECT_TRUE(parser.check(TokenType::NUMBER));
    EXPECT_FALSE(parser.check(TokenType::MINUS));

    parser.advance();
    EXPECT_TRUE(parser.check(TokenType::PLUS));
    EXPECT_FALSE(parser.check(TokenType::NUMBER));
}

TEST_F(ParserTest, CheckMultipleTokenTypes) {
    auto tokens = createTokens({
        {TokenType::PLUS, "+"}
    });

    parser.parse(tokens);

    EXPECT_TRUE(parser.check(TokenType::PLUS, TokenType::MINUS, TokenType::STAR));
    EXPECT_FALSE(parser.check(TokenType::NUMBER, TokenType::STRING));
}

TEST_F(ParserTest, MatchTokens) {
    auto tokens = createTokens({
        {TokenType::NUMBER, "42"},
        {TokenType::PLUS, "+"},
        {TokenType::NUMBER, "3"}
    });

    parser.parse(tokens);

    EXPECT_TRUE(parser.match(TokenType::NUMBER));
    EXPECT_EQ(parser.peek(), TokenType::PLUS);

    EXPECT_FALSE(parser.match(TokenType::MINUS));
    EXPECT_EQ(parser.peek(), TokenType::PLUS);

    EXPECT_TRUE(parser.match(TokenType::PLUS));
    EXPECT_EQ(parser.peek(), TokenType::NUMBER);
}

// Error Handling Tests
TEST_F(ParserTest, ErrorState) {
    EXPECT_FALSE(parser.hasError());

    auto tokens = createTokens({
        {TokenType::NUMBER, "42"}
    });
    parser.parse(tokens);

    // Simulate an error
    parser.errorAt(parser.currentToken(), "Test error");
    EXPECT_TRUE(parser.hasError());
}

TEST_F(ParserTest, ConsumeExpectedToken) {
    auto tokens = createTokens({
        {TokenType::NUMBER, "42"},
        {TokenType::PLUS, "+"}
    });

    parser.parse(tokens);

    // Should consume without error
    EXPECT_NO_THROW(parser.consume("Expected number", TokenType::NUMBER));
    EXPECT_EQ(parser.peek(), TokenType::PLUS);
    EXPECT_FALSE(parser.hasError());
}

TEST_F(ParserTest, ConsumeUnexpectedToken) {
    auto tokens = createTokens({
        {TokenType::NUMBER, "42"}
    });

    parser.parse(tokens);

    // Should generate error for wrong token type
    parser.consume("Expected plus", TokenType::PLUS);
    EXPECT_TRUE(parser.hasError());
}

// Precedence Tests
TEST_F(ParserTest, PrecedenceEnum) {
    // Test that precedence levels are ordered correctly
    EXPECT_LT(static_cast<int>(Precedence::NONE), static_cast<int>(Precedence::ASSIGNMENT));
    EXPECT_LT(static_cast<int>(Precedence::ASSIGNMENT), static_cast<int>(Precedence::OR));
    EXPECT_LT(static_cast<int>(Precedence::OR), static_cast<int>(Precedence::AND));
    EXPECT_LT(static_cast<int>(Precedence::AND), static_cast<int>(Precedence::EQUALITY));
    EXPECT_LT(static_cast<int>(Precedence::EQUALITY), static_cast<int>(Precedence::COMPARISON));
    EXPECT_LT(static_cast<int>(Precedence::COMPARISON), static_cast<int>(Precedence::TERM));
    EXPECT_LT(static_cast<int>(Precedence::TERM), static_cast<int>(Precedence::FACTOR));
    EXPECT_LT(static_cast<int>(Precedence::FACTOR), static_cast<int>(Precedence::UNARY));
    EXPECT_LT(static_cast<int>(Precedence::UNARY), static_cast<int>(Precedence::CALL));
    EXPECT_LT(static_cast<int>(Precedence::CALL), static_cast<int>(Precedence::PRIMARY));
}

// Parser Rule Tests
TEST_F(ParserTest, GetRuleForTokens) {
    // Test that parser rules exist for basic tokens
    auto numberRule = parser.getRule(TokenType::NUMBER);
    EXPECT_NE(numberRule.prefix, nullptr);

    auto plusRule = parser.getRule(TokenType::PLUS);
    EXPECT_NE(plusRule.infix, nullptr);
    EXPECT_EQ(plusRule.precedence, Precedence::TERM);

    auto minusRule = parser.getRule(TokenType::MINUS);
    EXPECT_NE(minusRule.prefix, nullptr);  // Unary minus
    EXPECT_NE(minusRule.infix, nullptr);   // Binary minus
    EXPECT_EQ(minusRule.precedence, Precedence::TERM);
}

TEST_F(ParserTest, OperatorPrecedence) {
    auto addRule = parser.getRule(TokenType::PLUS);
    auto mulRule = parser.getRule(TokenType::STAR);
    auto eqRule = parser.getRule(TokenType::EQUALEQUAL);
    auto ltRule = parser.getRule(TokenType::INF);

    EXPECT_EQ(addRule.precedence, Precedence::TERM);
    EXPECT_EQ(mulRule.precedence, Precedence::FACTOR);
    EXPECT_EQ(eqRule.precedence, Precedence::EQUALITY);
    EXPECT_EQ(ltRule.precedence, Precedence::COMPARISON);

    // Verify precedence ordering (higher number = higher precedence)
    EXPECT_GT(static_cast<int>(mulRule.precedence), static_cast<int>(addRule.precedence));
    EXPECT_GT(static_cast<int>(addRule.precedence), static_cast<int>(ltRule.precedence));
    EXPECT_GT(static_cast<int>(ltRule.precedence), static_cast<int>(eqRule.precedence));
}

// Bytecode Generation Tests
TEST_F(ParserTest, WriteByteToChunk) {
    // Set up compiler for parser to use
    Compiler testCompiler;
    parser.setCompiler(&testCompiler);

    parser.writeByte(OpCode::OP_Return);

    auto& chunk = testCompiler.getCurrentChunk();
    EXPECT_EQ(chunk.code.size(), 1);
    EXPECT_EQ(static_cast<OpCode>(chunk.code[0]), OpCode::OP_Return);
}

TEST_F(ParserTest, WriteConstantToChunk) {
    // Set up compiler for parser to use
    Compiler testCompiler;
    parser.setCompiler(&testCompiler);

    ElementType value(3.14);
    parser.writeConstant(value);

    auto& chunk = testCompiler.getCurrentChunk();
    EXPECT_EQ(chunk.constants.size(), 1);
    EXPECT_FLOAT_EQ(chunk.constants[0].get<float>(), 3.14);

    // Should also generate bytecode for loading the constant
    EXPECT_GE(chunk.code.size(), 2);  // At least OP_Constant + index
}

TEST_F(ParserTest, EmitReturnInstruction) {
    // Set up compiler for parser to use
    Compiler testCompiler;
    parser.setCompiler(&testCompiler);

    parser.emitReturn();

    auto& chunk = testCompiler.getCurrentChunk();
    EXPECT_EQ(chunk.code.size(), 1);
    EXPECT_EQ(static_cast<OpCode>(chunk.code[0]), OpCode::OP_Return);
}

TEST_F(ParserTest, EmitTwoBytes) {
    // Set up compiler for parser to use
    Compiler testCompiler;
    parser.setCompiler(&testCompiler);

    parser.emitBytes(OpCode::OP_True, OpCode::OP_Not);

    auto& chunk = testCompiler.getCurrentChunk();
    EXPECT_EQ(chunk.code.size(), 2);
    EXPECT_EQ(static_cast<OpCode>(chunk.code[0]), OpCode::OP_True);
    EXPECT_EQ(static_cast<OpCode>(chunk.code[1]), OpCode::OP_Not);
}

// EOL Handling Tests
TEST_F(ParserTest, SkipEndOfLine) {
    auto tokens = createTokens({
        {TokenType::NUMBER, "42"},
        {TokenType::EOL, "\n"},
        {TokenType::EOL, "\n"},
        {TokenType::PLUS, "+"}
    });

    parser.parse(tokens);

    parser.advance();  // Move to first EOL
    EXPECT_EQ(parser.peek(), TokenType::EOL);

    parser.skipEOL();
    EXPECT_EQ(parser.peek(), TokenType::PLUS);
}

// Empty Token Queue Tests
TEST_F(ParserTest, EmptyTokenQueue) {
    std::queue<Token> emptyTokens;
    emptyTokens.push(Token(TokenType::ENDOFFILE, "", 1, 1));  // Add EOF token
    parser.parse(emptyTokens);

    auto current = parser.currentToken();
    EXPECT_EQ(current.type, TokenType::ENDOFFILE);
    EXPECT_TRUE(parser.isAtEnd());
}

// Reset Functionality Tests
TEST_F(ParserTest, ResetParser) {
    auto tokens = createTokens({
        {TokenType::NUMBER, "42"}
    });

    parser.parse(tokens);
    parser.errorAt(parser.currentToken(), "Test error");

    EXPECT_TRUE(parser.hasError());

    parser.reset();

    EXPECT_FALSE(parser.hasError());
    EXPECT_FALSE(parser.panicMode);
    EXPECT_TRUE(parser.tokens.empty());
}

// Expression Parsing Tests
TEST_F(ParserTest, ExpressionParsing) {
    auto tokens = createTokens({
        {TokenType::NUMBER, "42"},
        {TokenType::PLUS, "+"},
        {TokenType::NUMBER, "3"}
    });

    parser.parse(tokens);

    // Set up compiler for parser to use
    Compiler testCompiler;
    parser.setCompiler(&testCompiler);

    // This would normally call the expression parsing method
    // For now, just verify we can call parsePrecedence
    EXPECT_NO_THROW(parser.parsePrecedence(Precedence::ASSIGNMENT));
}

} // namespace test
} // namespace pg