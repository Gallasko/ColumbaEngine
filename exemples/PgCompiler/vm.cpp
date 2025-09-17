#include "vm.h"

#include <iostream>

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
            auto instruction = static_cast<OpCode>(chunk.code[ip++]);

            switch (instruction)
            {
                case OpCode::OP_Return:
                    return InterpretResult::OK;

                case OpCode::OP_Constant:
                {
                    auto constant = readConstant();
                    std::cout << "Constant: " << constant << std::endl;
                    break;
                }

                case OpCode::OP_LongConstant:
                {
                    auto constant = readLongConstant();
                    std::cout << "Long Constant: " << constant << std::endl;
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
        uint32_t constantIndex = (static_cast<uint32_t>(chunk.code[ip++]) << 16) |
                                 (static_cast<uint32_t>(chunk.code[ip++]) << 8)  |
                                 (static_cast<uint32_t>(chunk.code[ip++]));
        return chunk.constants[constantIndex];
    }
}