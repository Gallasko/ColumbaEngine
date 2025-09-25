#include "compiler_test_base.h"
#include <iostream>
#include <sstream>

namespace pg {
namespace test {

void CompilerTestBase::SetUp() {
    resetVM();
    resetCompiler();
}

void CompilerTestBase::TearDown() {
    // Nothing special needed for teardown
}

Chunk CompilerTestBase::compileExpression(const std::string& source) {
    Chunk chunk;
    
    // Reset compiler state before each compilation
    compiler.reset();
    
    // Tokenize the source
    Lexer lexer;
    lexer.readFromText(source);
    auto tokens = lexer.getTokens();
    
    // Compile to chunk
    bool success = compiler.compile(tokens, chunk);
    if (!success) {
        throw std::runtime_error("Compilation failed for: " + source);
    }
    
    return chunk;
}

InterpretResult CompilerTestBase::executeChunk(const Chunk& chunk) {
    vm.chunk = chunk;
    vm.ip = 0;
    
    try {
        return vm.run();
    } catch (const std::exception& e) {
        return InterpretResult::RUNTIME_ERROR;
    }
}

std::vector<Token> CompilerTestBase::tokenize(const std::string& source) {
    Lexer lexer;
    lexer.readFromText(source);
    auto tokenQueue = lexer.getTokens();
    
    std::vector<Token> tokens;
    while (!tokenQueue.empty()) {
        tokens.push_back(tokenQueue.front());
        tokenQueue.pop();
    }
    return tokens;
}

InterpretResult CompilerTestBase::interpretFromSource(const std::string& source) {
    return vm.interpretFromText(source);
}

void CompilerTestBase::assertCompileSuccess(const std::string& source) {
    EXPECT_NO_THROW({
        auto chunk = compileExpression(source);
        EXPECT_FALSE(chunk.code.empty()) << "Compiled chunk should not be empty for: " << source;
    }) << "Compilation should succeed for: " << source;
}

void CompilerTestBase::assertCompileError(const std::string& source) {
    EXPECT_THROW({
        compileExpression(source);
    }, std::runtime_error) << "Compilation should fail for: " << source;
}

void CompilerTestBase::assertRuntimeError(const std::string& source) {
    auto result = interpretFromSource(source);
    EXPECT_EQ(result, InterpretResult::RUNTIME_ERROR) 
        << "Should produce runtime error for: " << source;
}

void CompilerTestBase::assertInterpretResult(const std::string& source, InterpretResult expected) {
    auto result = interpretFromSource(source);
    EXPECT_EQ(result, expected) << "Unexpected result for: " << source;
}

void CompilerTestBase::assertBytecode(const Chunk& chunk, const std::vector<OpCode>& expected) {
    ASSERT_BYTECODE_EQ(chunk, expected);
}

void CompilerTestBase::assertConstantCount(const Chunk& chunk, size_t expectedCount) {
    EXPECT_EQ(chunk.constants.size(), expectedCount) 
        << "Expected " << expectedCount << " constants, got " << chunk.constants.size();
}

void CompilerTestBase::assertConstantValue(const Chunk& chunk, size_t index, const ElementType& expected) {
    ASSERT_LT(index, chunk.constants.size()) << "Constant index out of bounds";
    
    const auto& actual = chunk.constants[index];
    
    // Compare based on type
    if (expected.isNumber() && actual.isNumber()) {
        EXPECT_FLOAT_EQ(expected.get<float>(), actual.get<float>()) 
            << "Constant values don't match at index " << index;
    } else if (expected.isBool() && actual.isBool()) {
        EXPECT_EQ(expected.isTrue(), actual.isTrue()) 
            << "Boolean constant values don't match at index " << index;
    } else if (expected.isLitteral() && actual.isLitteral()) {
        EXPECT_EQ(expected.toString(), actual.toString()) 
            << "String constant values don't match at index " << index;
    } else {
        FAIL() << "Constant types don't match at index " << index 
               << ": expected " << expected.getTypeString() 
               << ", got " << actual.getTypeString();
    }
}

void CompilerTestBase::resetVM() {
    // Clean up stack first
    vm.stack.clear();  // This already frees Values properly
    
    // Properly free globals 
    for (auto& pair : vm.globals) {
        freeValue(pair.second);
    }
    vm.globals.clear();
    
    vm.testOutput.clear();
    vm.ip = 0;
}

void CompilerTestBase::resetCompiler() {
    compiler.reset();
}

void CompilerTestBase::expectPrintedOutput(const std::string& source, const std::string& expected) {
    // Clear any previous test output
    vm.testOutput.clear();
    
    auto result = interpretFromSource(source);
    
    EXPECT_EQ(result, InterpretResult::OK) << "Code should execute successfully: " << source;
    EXPECT_EQ(vm.testOutput, expected) << "Output mismatch for: " << source;
}

} // namespace test
} // namespace pg