#pragma once

#include "compiler_test_base.h"
#include <stack>

namespace pg {
namespace test {

/**
 * Specialized test fixture for VM-specific testing
 * Provides stack manipulation and VM state verification
 */
class VMTestFixture : public CompilerTestBase {
protected:
    void SetUp() override;
    void TearDown() override;

    // Stack manipulation helpers
    void pushToStack(const ElementType& value);
    ElementType popFromStack();
    ElementType peekStack(size_t distance = 0);
    void clearStack();
    size_t stackSize() const;
    bool isStackEmpty() const;

    // Stack state assertions
    void assertStackSize(size_t expectedSize);
    void assertStackTop(const ElementType& expected);
    void assertStackEmpty();
    void assertStackContains(const std::vector<ElementType>& expected);

    // VM state helpers
    void setInstructionPointer(size_t ip);
    size_t getInstructionPointer() const;
    void loadChunk(const Chunk& chunk);

    // Bytecode execution helpers
    InterpretResult executeNextInstruction();
    InterpretResult executeInstructions(size_t count);
    InterpretResult executeUntilReturn();
    InterpretResult executeChunkWithoutReturn();  // Execute chunk that has no OP_Return

    // Specialized chunk builders for VM testing
    Chunk buildStackTestChunk();
    Chunk buildArithmeticChunk(OpCode operation);
    Chunk buildComparisonChunk(OpCode operation);
    Chunk buildUnaryChunk(OpCode operation);
    Chunk buildConstantChunk(const ElementType& value);
    Chunk buildLongConstantChunk(const ElementType& value);

    // Error testing helpers
    Chunk buildStackUnderflowChunk();
    Chunk buildInvalidOperationChunk();

    // Global variable testing helpers
    Chunk buildDefineGlobalChunk(const std::string& name, const ElementType& value);
    Chunk buildGetGlobalChunk(const std::string& name);
    Chunk buildSetGlobalChunk(const std::string& name, const ElementType& value);
    void defineGlobal(const std::string& name, const ElementType& value);
    bool hasGlobal(const std::string& name) const;
    ElementType getGlobal(const std::string& name) const;
    void clearGlobals();

private:
    std::vector<ElementType> getStackContents();
    void verifyVMState();
};

} // namespace test
} // namespace pg