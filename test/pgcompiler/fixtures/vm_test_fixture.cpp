#include "vm_test_fixture.h"

namespace pg {
namespace test {

void VMTestFixture::SetUp() {
    CompilerTestBase::SetUp();
    clearStack();
}

void VMTestFixture::TearDown() {
    CompilerTestBase::TearDown();
}

void VMTestFixture::pushToStack(const ElementType& value) {
    vm.push(value);
}

ElementType VMTestFixture::popFromStack() {
    return vm.pop();
}

ElementType VMTestFixture::peekStack(size_t distance) {
    return vm.peek(distance);
}

void VMTestFixture::clearStack() {
    vm.resetStack();
}

size_t VMTestFixture::stackSize() const {
    return vm.stack.size();
}

bool VMTestFixture::isStackEmpty() const {
    return vm.stack.empty();
}

void VMTestFixture::assertStackSize(size_t expectedSize) {
    EXPECT_EQ(stackSize(), expectedSize)
        << "Expected stack size " << expectedSize << ", got " << stackSize();
}

void VMTestFixture::assertStackTop(const ElementType& expected) {
    ASSERT_FALSE(isStackEmpty()) << "Stack is empty, cannot check top";

    auto actual = peekStack(0);
    if (expected.isNumber() && actual.isNumber()) {
        EXPECT_FLOAT_EQ(expected.get<float>(), actual.get<float>())
            << "Stack top value mismatch";
    } else if (expected.isBool() && actual.isBool()) {
        EXPECT_EQ(expected.isTrue(), actual.isTrue())
            << "Stack top boolean value mismatch";
    } else {
        FAIL() << "Stack top type mismatch: expected " << expected.getTypeString()
               << ", got " << actual.getTypeString();
    }
}

void VMTestFixture::assertStackEmpty() {
    EXPECT_TRUE(isStackEmpty()) << "Expected empty stack, but stack size is " << stackSize();
}

void VMTestFixture::assertStackContains(const std::vector<ElementType>& expected) {
    EXPECT_EQ(stackSize(), expected.size())
        << "Stack size mismatch: expected " << expected.size() << ", got " << stackSize();

    auto stackContents = getStackContents();
    for (size_t i = 0; i < expected.size() && i < stackContents.size(); ++i) {
        const auto& expectedVal = expected[i];
        const auto& actualVal = stackContents[i];

        if (expectedVal.isNumber() && actualVal.isNumber()) {
            EXPECT_FLOAT_EQ(expectedVal.get<float>(), actualVal.get<float>())
                << "Stack value mismatch at position " << i;
        } else if (expectedVal.isBool() && actualVal.isBool()) {
            EXPECT_EQ(expectedVal.isTrue(), actualVal.isTrue())
                << "Stack boolean value mismatch at position " << i;
        }
    }
}

void VMTestFixture::setInstructionPointer(size_t ip) {
    vm.ip = ip;
}

size_t VMTestFixture::getInstructionPointer() const {
    return vm.ip;
}

void VMTestFixture::loadChunk(const Chunk& chunk) {
    vm.chunk = chunk;
    vm.ip = 0;
}

InterpretResult VMTestFixture::executeNextInstruction() {
    try {
        // This is a bit tricky - we need to execute just one instruction
        // For now, we'll use the full run method and catch early returns
        return vm.run();
    } catch (const std::exception& e) {
        return InterpretResult::RUNTIME_ERROR;
    }
}

InterpretResult VMTestFixture::executeInstructions(size_t count) {
    for (size_t i = 0; i < count; ++i) {
        auto result = executeNextInstruction();
        if (result != InterpretResult::OK) {
            return result;
        }
    }
    return InterpretResult::OK;
}

InterpretResult VMTestFixture::executeUntilReturn() {
    return executeChunk(vm.chunk);
}

InterpretResult VMTestFixture::executeChunkWithoutReturn() {
    // Execute a chunk that doesn't contain OP_Return
    if (vm.chunk.code.empty()) {
        return InterpretResult::OK;
    }
    
    // Manually execute each instruction since VM's run() expects OP_Return
    while (vm.ip < vm.chunk.code.size()) {
        auto instruction = static_cast<OpCode>(vm.chunk.code[vm.ip++]);
        
        switch (instruction) {
            case OpCode::OP_Constant: {
                uint8_t constantIndex = vm.chunk.code[vm.ip++];
                vm.push(vm.chunk.constants[constantIndex]);
                break;
            }
            case OpCode::OP_LongConstant: {
                if (vm.ip + 2 >= vm.chunk.code.size()) {
                    return InterpretResult::RUNTIME_ERROR;
                }
                uint32_t constantIndex = (static_cast<uint32_t>(vm.chunk.code[vm.ip]) << 16);
                vm.ip++;
                constantIndex |= (static_cast<uint32_t>(vm.chunk.code[vm.ip]) << 8);
                vm.ip++;
                constantIndex |= static_cast<uint32_t>(vm.chunk.code[vm.ip]);
                vm.ip++;
                vm.push(vm.chunk.constants[constantIndex]);
                break;
            }
            case OpCode::OP_Add: {
                vm.binaryOp(std::plus<ElementType>());
                break;
            }
            case OpCode::OP_Subtract: {
                vm.binaryOp(std::minus<ElementType>());
                break;
            }
            case OpCode::OP_Multiply: {
                vm.binaryOp(std::multiplies<ElementType>());
                break;
            }
            case OpCode::OP_Divide: {
                vm.binaryOp(std::divides<ElementType>());
                break;
            }
            case OpCode::OP_Negate: {
                if (!vm.peek(0).isNumber()) {
                    return InterpretResult::RUNTIME_ERROR;
                }
                auto value = vm.pop();
                vm.push(-value);
                break;
            }
            case OpCode::OP_True: {
                vm.push(ElementType(true));
                break;
            }
            case OpCode::OP_False: {
                vm.push(ElementType(false));
                break;
            }
            case OpCode::OP_Not: {
                if (vm.peek(0).getTypeString() != "bool") {
                    return InterpretResult::RUNTIME_ERROR;
                }
                auto value = vm.pop();
                vm.push(ElementType(!value.isTrue()));
                break;
            }
            case OpCode::OP_Equal: {
                vm.checkBooleanBinaryOp();
                auto b = vm.pop();
                auto a = vm.pop();
                vm.push(a == b);
                break;
            }
            case OpCode::OP_NotEqual: {
                vm.checkBooleanBinaryOp();
                auto b = vm.pop();
                auto a = vm.pop();
                vm.push(a != b);
                break;
            }
            case OpCode::OP_Greater: {
                vm.checkBooleanBinaryOp();
                auto b = vm.pop();
                auto a = vm.pop();
                vm.push(a > b);
                break;
            }
            case OpCode::OP_GreaterEqual: {
                vm.checkBooleanBinaryOp();
                auto b = vm.pop();
                auto a = vm.pop();
                vm.push(a >= b);
                break;
            }
            case OpCode::OP_Less: {
                vm.checkBooleanBinaryOp();
                auto b = vm.pop();
                auto a = vm.pop();
                vm.push(a < b);
                break;
            }
            case OpCode::OP_LessEqual: {
                vm.checkBooleanBinaryOp();
                auto b = vm.pop();
                auto a = vm.pop();
                vm.push(a <= b);
                break;
            }
            default:
                return InterpretResult::RUNTIME_ERROR;
        }
    }
    
    return InterpretResult::OK;
}

Chunk VMTestFixture::buildStackTestChunk() {
    Chunk chunk;
    // Push some values for testing
    chunk.addConstant(ElementType(42.0), 1);
    chunk.addConstant(ElementType(3.14), 1);
    chunk.addCode(OpCode::OP_Return, 1);
    return chunk;
}

Chunk VMTestFixture::buildArithmeticChunk(OpCode operation) {
    Chunk chunk;
    // Two operands for binary operations
    chunk.addConstant(ElementType(10.0), 1);
    chunk.addConstant(ElementType(3.0), 1);
    chunk.addCode(operation, 1);
    chunk.addCode(OpCode::OP_Return, 1);
    return chunk;
}

Chunk VMTestFixture::buildComparisonChunk(OpCode operation) {
    Chunk chunk;
    // Two operands for comparison
    chunk.addConstant(ElementType(5.0), 1);
    chunk.addConstant(ElementType(3.0), 1);
    chunk.addCode(operation, 1);
    chunk.addCode(OpCode::OP_Return, 1);
    return chunk;
}

Chunk VMTestFixture::buildUnaryChunk(OpCode operation) {
    Chunk chunk;
    if (operation == OpCode::OP_Negate) {
        chunk.addConstant(ElementType(5.0), 1);
    } else if (operation == OpCode::OP_Not) {
        chunk.addCode(OpCode::OP_True, 1);
    }
    chunk.addCode(operation, 1);
    chunk.addCode(OpCode::OP_Return, 1);
    return chunk;
}

Chunk VMTestFixture::buildConstantChunk(const ElementType& value) {
    Chunk chunk;
    chunk.addConstant(value, 1);
    chunk.addCode(OpCode::OP_Return, 1);
    return chunk;
}

Chunk VMTestFixture::buildLongConstantChunk(const ElementType& value) {
    Chunk chunk;
    // Force long constant by adding many constants first
    for (int i = 0; i < 300; ++i) {
        chunk.constants.push_back(ElementType(static_cast<double>(i)));
    }
    chunk.addConstant(value, 1);  // This should generate OP_LongConstant
    chunk.addCode(OpCode::OP_Return, 1);
    return chunk;
}

Chunk VMTestFixture::buildStackUnderflowChunk() {
    Chunk chunk;
    // Try to add without enough operands
    chunk.addCode(OpCode::OP_Add, 1);
    chunk.addCode(OpCode::OP_Return, 1);
    return chunk;
}

Chunk VMTestFixture::buildInvalidOperationChunk() {
    Chunk chunk;
    // Invalid opcode
    chunk.code.push_back(255);  // Invalid opcode
    chunk.lines.push_back(1);
    chunk.addCode(OpCode::OP_Return, 1);
    return chunk;
}

std::vector<ElementType> VMTestFixture::getStackContents() {
    std::vector<ElementType> contents;
    auto tempStack = vm.stack;

    // Pop all elements to get them in order (bottom to top)
    std::vector<ElementType> reversed;
    while (!tempStack.empty()) {
        reversed.push_back(tempStack.top());
        tempStack.pop();
    }

    // Reverse to get bottom-to-top order
    for (auto it = reversed.rbegin(); it != reversed.rend(); ++it) {
        contents.push_back(*it);
    }

    return contents;
}

void VMTestFixture::verifyVMState() {
    // Basic sanity checks
    EXPECT_GE(vm.ip, 0) << "Instruction pointer should not be negative";
    if (!vm.chunk.code.empty()) {
        EXPECT_LE(vm.ip, vm.chunk.code.size()) << "Instruction pointer out of bounds";
    }
}


} // namespace test
} // namespace pg