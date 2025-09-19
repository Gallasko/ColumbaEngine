#include "test_helpers.h"
#include <sstream>
#include <queue>

namespace pg {
namespace test {

Token TestHelpers::makeToken(TokenType type, const std::string& lexeme, int line, int column) {
    return Token(type, lexeme, line, column);
}

std::queue<Token> TestHelpers::makeTokenQueue(const std::vector<std::pair<TokenType, std::string>>& tokens) {
    std::queue<Token> tokenQueue;
    int line = 1;
    for (const auto& pair : tokens) {
        tokenQueue.push(makeToken(pair.first, pair.second, line++));
    }
    // Add EOF token
    tokenQueue.push(makeToken(TokenType::ENDOFFILE, "", line));
    return tokenQueue;
}

ElementType TestHelpers::makeNumber(double value) {
    return ElementType(value);
}

ElementType TestHelpers::makeBool(bool value) {
    return ElementType(value);
}

ElementType TestHelpers::makeString(const std::string& value) {
    return ElementType(value);
}

Chunk TestHelpers::makeSimpleArithmeticChunk() {
    Chunk chunk;
    // Simple: 1 + 2
    chunk.addConstant(ElementType(1.0), 1);
    chunk.addConstant(ElementType(2.0), 1);
    chunk.addCode(OpCode::OP_Add, 1);
    chunk.addCode(OpCode::OP_Return, 1);
    return chunk;
}

Chunk TestHelpers::makeSimpleComparisonChunk() {
    Chunk chunk;
    // Simple: 5 > 3
    chunk.addConstant(ElementType(5.0), 1);
    chunk.addConstant(ElementType(3.0), 1);
    chunk.addCode(OpCode::OP_Greater, 1);
    chunk.addCode(OpCode::OP_Return, 1);
    return chunk;
}

Chunk TestHelpers::makeBooleanChunk() {
    Chunk chunk;
    // Simple: !true
    chunk.addCode(OpCode::OP_True, 1);
    chunk.addCode(OpCode::OP_Not, 1);
    chunk.addCode(OpCode::OP_Return, 1);
    return chunk;
}

bool TestHelpers::compareBytecode(const Chunk& actual, const std::vector<OpCode>& expected) {
    if (actual.code.size() != expected.size()) {
        return false;
    }
    
    for (size_t i = 0; i < expected.size(); ++i) {
        if (static_cast<OpCode>(actual.code[i]) != expected[i]) {
            return false;
        }
    }
    return true;
}

std::string TestHelpers::bytecodeToString(const Chunk& chunk) {
    std::stringstream ss;
    ss << "[";
    for (size_t i = 0; i < chunk.code.size(); ++i) {
        if (i > 0) ss << ", ";
        ss << static_cast<int>(chunk.code[i]);
    }
    ss << "]";
    return ss.str();
}

std::string TestHelpers::simpleExpression() {
    return "1 + 2";
}

std::string TestHelpers::complexExpression() {
    return "(3 + 4) * (5 - 2) / 2";
}

std::string TestHelpers::invalidExpression() {
    return "1 + + 2";
}

} // namespace test
} // namespace pg