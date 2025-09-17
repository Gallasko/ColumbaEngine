#include "compiler_debug.h"

#include <iostream>
#include <iomanip>

namespace pg
{
    namespace
    {
        int simpleInstruction(const std::string& name, int offset)
        {
            std::cout << name << std::endl;
            return offset + 1;
        }

        int constantInstruction(const std::string& name, const Chunk& chunk, int offset)
        {
            uint8_t cIndex = chunk.code[offset + 1];
            std::cout << std::left << std::setw(16) << name << " " << static_cast<int>(cIndex) << " '"
                      << chunk.constants[cIndex] << "'" << std::endl;

            return offset + 2;
        }
    }

    void disassembleChunk(const Chunk& chunk, const std::string& name)
    {
        std::cout << "== " << name << " ==" << std::endl;

        for (size_t offset = 0; offset < chunk.code.size();)
        {
            offset = disassembleInstruction(chunk, offset);
        }
    }

    int disassembleInstruction(const Chunk& chunk, int offset)
    {
        std::cout << std::setw(4) << offset << " ";

        OpCode instruction = static_cast<OpCode>(chunk.code[offset]);

        switch (instruction)
        {
            case OpCode::OP_Return:
                return simpleInstruction("OP_Return", offset);

            case OpCode::OP_Constant:
                return constantInstruction("OP_Constant", chunk, offset);

            default:
                std::cout << "Unknown opcode " << static_cast<uint8_t>(instruction) << std::endl;
                return offset + 1;
        }
    }
}