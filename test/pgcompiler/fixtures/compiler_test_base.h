#pragma once

#include "gtest/gtest.h"
#include "vm.h"
#include "compiler.h"
#include "chunk.h"
#include "Memory/elementtype.h"
#include "Interpreter/token.h"
#include "Interpreter/lexer.h"
#include "test_helpers.h"

namespace pg {
namespace test {

/**
 * Base test fixture for all PgCompiler tests
 * Provides common setup, teardown, and helper methods
 */
class CompilerTestBase : public ::testing::Test {
protected:
    void SetUp() override;
    void TearDown() override;
    
    // Compilation helpers
    Chunk compileExpression(const std::string& source);
    InterpretResult executeChunk(const Chunk& chunk);
    std::vector<Token> tokenize(const std::string& source);
    InterpretResult interpretFromSource(const std::string& source);
    
    // Assertions for compilation
    void assertCompileSuccess(const std::string& source);
    void assertCompileError(const std::string& source);
    void assertRuntimeError(const std::string& source);
    void assertInterpretResult(const std::string& source, InterpretResult expected);
    
    // Assertions for bytecode
    void assertBytecode(const Chunk& chunk, const std::vector<OpCode>& expected);
    void assertConstantCount(const Chunk& chunk, size_t expectedCount);
    void assertConstantValue(const Chunk& chunk, size_t index, const ElementType& expected);
    
    // Test state
    VM vm;
    Compiler compiler;
    bool captureOutput;
    std::string capturedOutput;
    
private:
    void resetVM();
    void resetCompiler();
    void captureStdout();
    void restoreStdout();
    
    std::streambuf* originalCoutBuffer;
    std::ostringstream capturedStream;
};

} // namespace test
} // namespace pg