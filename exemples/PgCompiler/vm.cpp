#include "vm.h"

#include <iostream>

#include "compiler_debug.h"

#include <chrono>

namespace pg
{
    InterpretResult VM::interpret(const std::queue<Token>& tokens)
    {
        Chunk chunk;

        // Todo change this
        // Reset the compiler state before compiling a new chunk
        compiler.reset();

        std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();

        if (not compiler.compile(tokens, chunk))
            return InterpretResult::COMPILE_ERROR;

        std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();

#ifdef DEBUG_PROFILE_COMPILE
        std::cout << "Compilation took: "
                  << std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count()
                  << " ns"
                  << std::endl;
#endif
        // LOG_INFO("VM", "Compilation took " << elapsed_seconds.count() << "s");


        this->chunk = chunk;
        ip = 0;

        try
        {
            begin = std::chrono::steady_clock::now();
            auto result = run();
            end = std::chrono::steady_clock::now();

#ifdef DEBUG_PROFILE_COMPILE
            std::cout << "Execution took: "
                      << std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count()
                      << " ns"
                      << std::endl;
#endif
            return result;
        }
        catch(const std::exception& e)
        {
            LOG_ERROR("VM", e.what());

            return InterpretResult::RUNTIME_ERROR;
        }

    }

    InterpretResult VM::run()
    {
        if (chunk.code.empty())
            return InterpretResult::OK;

        for (;;)
        {
            if (ip >= chunk.code.size())
                return InterpretResult::OK;

#ifdef DEBUG_TRACE_EXECUTION
            std::cout << "          ";
            for (size_t i = 0; i < stack.size(); ++i)
            {
                std::cout << "[" << stack[i].toString() << "] ";
            }
            std::cout << std::endl;

            disassembleInstruction(chunk, ip);
#endif
            auto instruction = static_cast<OpCode>(chunk.code[ip++]);

            switch (instruction)
            {
                case OpCode::OP_Return:
                {
                    if (stack.empty())
                    {
                        EMIT_RUNTIME_ERROR("Nothing in the stack for return.");
                    }

                    auto value = pop();

                    std::cout << value.toString() << std::endl;

                    return InterpretResult::OK;
                }

                case OpCode::OP_Constant:
                {
                    auto constant = readConstant();
                    push(constant);
                    break;
                }

                case OpCode::OP_LongConstant:
                {
                    auto constant = readLongConstant();
                    push(constant);
                    break;
                }

                case OpCode::OP_Negate:
                {
                    if (not peek(0).isNumber())
                    {
                        EMIT_RUNTIME_ERROR("Operand after an unary (-) must be a number.");
                    }

                    auto value = pop();

                    push(-value);
                    break;
                }

                case OpCode::OP_Add:
                {
                    binaryOp(std::plus<ElementType>());
                    break;
                }

                case OpCode::OP_Subtract:
                {
                    binaryOp(std::minus<ElementType>());
                    break;
                }

                case OpCode::OP_Multiply:
                {
                    binaryOp(std::multiplies<ElementType>());
                    break;
                }

                case OpCode::OP_Divide:
                {
                    binaryOp(std::divides<ElementType>());
                    break;
                }

                case OpCode::OP_True:
                {
                    push(ElementType(true));
                    break;
                }

                case OpCode::OP_False:
                {
                    push(ElementType(false));
                    break;
                }

                case OpCode::OP_Not:
                {
                    if (peek(0).getTypeString() != "bool")
                    {
                        EMIT_RUNTIME_ERROR("Operand after an unary (!) must be a boolean.");
                    }

                    auto value = pop();

                    push(ElementType(not value.isTrue()));
                    break;
                }

                case OpCode::OP_And:
                {
                    checkBooleanBinaryOp();
                    auto b = pop();
                    auto a = pop();

                    push(a and b);
                    break;
                }

                case OpCode::OP_Or:
                {
                    checkBooleanBinaryOp();
                    auto b = pop();
                    auto a = pop();

                    push(a or b);
                    break;
                }

// Macro to generate comparison operation cases with exception handling
// Usage: COMPARISON_OP(==) generates a complete case block for equality comparison
// Handles stack operations, type checking, and runtime error management
#define COMPARISON_OP(op) \
                { \
                    checkBooleanBinaryOp(); \
                    auto b = pop(); \
                    auto a = pop(); \
                    try { \
                        push(a op b); \
                    } catch (const std::exception& e) { \
                        EMIT_RUNTIME_ERROR("Comparison operation failed: " << e.what()); \
                    } \
                    break; \
                }

                case OpCode::OP_Equal:
                    COMPARISON_OP(==)

                case OpCode::OP_NotEqual:
                    COMPARISON_OP(!=)

                case OpCode::OP_Greater:
                    COMPARISON_OP(>)

                case OpCode::OP_GreaterEqual:
                    COMPARISON_OP(>=)

                case OpCode::OP_Less:
                    COMPARISON_OP(<)

                case OpCode::OP_LessEqual:
                    COMPARISON_OP(<=)

                case OpCode::OP_Pop:
                {
                    if (stack.empty())
                    {
                        EMIT_RUNTIME_ERROR("Nothing to pop from the stack.");
                    }
                    // pop();

                    // Todo to remove
                    auto value = pop();
                    std::cout << value.toString() << std::endl;

                    break;
                }

                case OpCode::OP_Define_Global:
                {
                    if (stack.size() < 2)
                    {
                        EMIT_RUNTIME_ERROR("Not enough values on stack for variable definition.");
                    }

                    auto name = pop();  // variable name
                    auto value = pop(); // variable value

                    if (not name.isLitteral())
                    {
                        EMIT_RUNTIME_ERROR("Global variable name must be a litteral.");
                    }

                    globals[name.toString()] = value;
                    break;
                }

                case OpCode::OP_Get_Global:
                {
                    if (stack.empty())
                    {
                        EMIT_RUNTIME_ERROR("Not enough values on stack for variable retrieval.");
                    }

                    auto name = pop();  // variable name

                    if (not name.isLitteral())
                    {
                        EMIT_RUNTIME_ERROR("Global variable name must be a litteral.");
                    }

                    auto it = globals.find(name.toString());
                    if (it == globals.end())
                    {
                        EMIT_RUNTIME_ERROR("Undefined global variable '" << name.toString() << "'.");
                    }

                    push(it->second);
                    break;
                }

                case OpCode::OP_Set_Global:
                {
                    if (stack.size() < 2)
                    {
                        EMIT_RUNTIME_ERROR("Not enough values on stack for variable assignment.");
                    }

                    auto value = pop(); // new variable value
                    auto name = pop();  // variable name

                    if (not name.isLitteral())
                    {
                        EMIT_RUNTIME_ERROR("Global variable name must be a litteral.");
                    }

                    auto it = globals.find(name.toString());
                    if (it == globals.end())
                    {
                        EMIT_RUNTIME_ERROR("Undefined global variable '" << name.toString() << "'.");
                    }

                    it->second = value;
                    push(value);
                    break;
                }

                case OpCode::OP_Get_Local:
                {
                    if (stack.empty())
                    {
                        EMIT_RUNTIME_ERROR("Stack underflow for local variable access.");
                    }

                    auto slot = pop(); // Get the slot index from stack
                    if (!slot.isNumber())
                    {
                        EMIT_RUNTIME_ERROR("Local variable slot must be a number.");
                    }

                    int index = slot.get<int>();
                    if (index < 0 || index >= static_cast<int>(stack.size()))
                    {
                        EMIT_RUNTIME_ERROR("Local variable index out of bounds.");
                    }

                    push(stack[index]);
                    break;
                }

                case OpCode::OP_Set_Local:
                {
                    if (stack.size() < 2)
                    {
                        EMIT_RUNTIME_ERROR("Not enough values on stack for local assignment.");
                    }

                    auto value = pop(); // New value
                    auto slot = pop();  // Slot index

                    if (!slot.isNumber())
                    {
                        EMIT_RUNTIME_ERROR("Local variable slot must be a number.");
                    }

                    int index = slot.get<int>();
                    if (index < 0 || index >= static_cast<int>(stack.size()))
                    {
                        EMIT_RUNTIME_ERROR("Local variable index out of bounds.");
                    }

                    stack[index] = value;
                    push(value); // Assignment expression returns the value
                    break;
                }

                default:
                    std::cout << "Unknown opcode " << static_cast<int>(instruction) << std::endl;
                    return InterpretResult::RUNTIME_ERROR;
            }
        }

        return InterpretResult::OK;
    }

    ElementType VM::readConstant()
    {
        uint8_t constantIndex = chunk.code[ip++];
        if (constantIndex >= chunk.constants.size())
        {
            throw std::runtime_error("Constant index out of bounds.");
        }
        return chunk.constants[constantIndex];
    }

    ElementType VM::readLongConstant()
    {
        if (ip + 2 >= chunk.code.size())
        {
            throw std::runtime_error("Not enough bytes to read long constant index.");
        }

        uint32_t constantIndex = (static_cast<uint32_t>(chunk.code[ip]) << 16);
        ip++;

        constantIndex |= (static_cast<uint32_t>(chunk.code[ip]) << 8);
        ip++;

        constantIndex |= static_cast<uint32_t>(chunk.code[ip]);
        ip++;

        if (constantIndex >= chunk.constants.size())
        {
            throw std::runtime_error("Long constant index out of bounds.");
        }

        return chunk.constants[constantIndex];
    }

    void VM::binaryOp(std::function<ElementType(ElementType, ElementType)> op)
    {
        if (stack.size() < 2)
        {
            runtimeError((Strfy() << "Stack underflow on binary operation.").getData());
            return;
        }

        auto e1 = peek(0);
        auto e2 = peek(1);

        if (not (e1.isNumber() and e2.isNumber()) and not (e1.isLitteral() and e2.isLitteral()))
        {
            runtimeError((Strfy() << "Operands after a binary operator should be the same type: " << e1.getTypeString() << " and " << e2.getTypeString()).getData());
            return;
        }

        auto b = pop();
        auto a = pop();

        push(op(a, b));
    }
}