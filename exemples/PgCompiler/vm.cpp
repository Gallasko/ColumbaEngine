#include "vm.h"

#include <iostream>

#include "compiler_debug.h"

namespace pg
{
    InterpretResult VM::interpret(const std::queue<Token>& tokens)
    {
        Chunk chunk;

        if (not compiler.compile(tokens, chunk))
            return InterpretResult::COMPILE_ERROR;

        this->chunk = chunk;
        ip = 0;

        return run();
    }

    InterpretResult VM::run()
    {
        if (chunk.code.empty())
            return InterpretResult::OK;

        for (;;)
        {
#ifdef DEBUG_TRACE_EXECUTION
            std::cout << "          ";
            auto stackCopy = stack;
            while (not stackCopy.empty())
            {
                auto value = stackCopy.top();
                stackCopy.pop();
                std::cout << "[" << value << "] ";
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
                        std::cout << "Stack underflow on OP_Negate" << std::endl;
                        return InterpretResult::RUNTIME_ERROR;
                    }

                    auto value = pop();

                    std::cout << value << std::endl;

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
                    if (stack.empty())
                    {
                        std::cout << "Stack underflow on OP_Negate" << std::endl;
                        return InterpretResult::RUNTIME_ERROR;
                    }

                    auto value = pop();

                    push(-value);
                    break;
                }

                case OpCode::OP_Add:
                {
                    binaryOp(std::plus<Value>());
                    break;
                }

                case OpCode::OP_Subtract:
                {
                    binaryOp(std::minus<Value>());
                    break;
                }

                case OpCode::OP_Multiply:
                {
                    binaryOp(std::multiplies<Value>());
                    break;
                }

                case OpCode::OP_Divide:
                {
                    binaryOp(std::divides<Value>());
                    break;
                }

                default:
                    std::cout << "Unknown opcode " << static_cast<int>(instruction) << std::endl;
                    return InterpretResult::RUNTIME_ERROR;
            }
        }

        return InterpretResult::OK;
    }

    Value VM::readConstant()
    {
        uint8_t constantIndex = chunk.code[ip++];
        return chunk.constants[constantIndex];
    }

    Value VM::readLongConstant()
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

        return chunk.constants[constantIndex];
    }

    void VM::binaryOp(std::function<Value(Value, Value)> op)
    {
        if (stack.size() < 2)
        {
            std::cout << "Stack underflow on binary operation" << std::endl;
            throw std::runtime_error("Stack underflow on binary operation");
        }

        auto b = pop();
        auto a = pop();

        push(op(a, b));
    }
}