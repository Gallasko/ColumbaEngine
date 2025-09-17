#include "vm.h"

#include <iostream>

#include "compiler_debug.h"

namespace pg
{
    InterpretResult VM::interpret(const Chunk& chunk)
    {
        this->chunk = chunk;
        ip = 0;

        return run();
    }

    InterpretResult VM::run()
    {
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
                    return InterpretResult::OK;

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
}