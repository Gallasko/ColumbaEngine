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
                      << chunk.constants[cIndex].toString() << "'" << std::endl;

            return offset + 2;
        }

        int longConstantInstruction(const std::string& name, const Chunk& chunk, int offset)
        {
            uint32_t cIndex = (static_cast<uint32_t>(chunk.code[offset + 1]) << 16) |
                              (static_cast<uint32_t>(chunk.code[offset + 2]) << 8) |
                              (static_cast<uint32_t>(chunk.code[offset + 3]));

            std::cout << std::left << std::setw(16) << name << " " << cIndex << " '"
                      << chunk.constants[cIndex].toString() << "'" << std::endl;

            return offset + 4;
        }

        int jumpInstruction(const std::string& name, const Chunk& chunk, int offset, int sign = 1)
        {
            uint16_t jump = (static_cast<uint16_t>(chunk.code[offset + 1]) << 8) |
                            (static_cast<uint16_t>(chunk.code[offset + 2]));

            std::cout << std::left << std::setw(16) << name << " " << offset << " -> " << (offset + 3 + sign * jump) << std::endl;

            return offset + 3;
        }

        int longJumpInstruction(const std::string& name, const Chunk& chunk, int offset, int sign = 1)
        {
            uint32_t jump = (static_cast<uint32_t>(chunk.code[offset + 1]) << 24) |
                            (static_cast<uint32_t>(chunk.code[offset + 2]) << 16) |
                            (static_cast<uint32_t>(chunk.code[offset + 3]) << 8) |
                            (static_cast<uint32_t>(chunk.code[offset + 4]));

            std::cout << std::left << std::setw(16) << name << " " << offset << " -> " << (offset + 5 + sign * jump) << std::endl;

            return offset + 5;
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

        if (offset > 0 and chunk.lines[offset] == chunk.lines[offset - 1])
            std::cout << "   | ";
        else
            std::cout << std::setw(4) << chunk.lines[offset] << " ";

        OpCode instruction = static_cast<OpCode>(chunk.code[offset]);

        switch (instruction)
        {
            case OpCode::OP_Return:
                return simpleInstruction("OP_Return", offset);

            case OpCode::OP_Constant:
                return constantInstruction("OP_Constant", chunk, offset);

            case OpCode::OP_LongConstant:
                return longConstantInstruction("OP_LongConstant", chunk, offset);

            case OpCode::OP_Negate:
                return simpleInstruction("OP_Negate", offset);

            case OpCode::OP_Add:
                return simpleInstruction("OP_Add", offset);

            case OpCode::OP_Subtract:
                return simpleInstruction("OP_Subtract", offset);

            case OpCode::OP_Multiply:
                return simpleInstruction("OP_Multiply", offset);

            case OpCode::OP_Divide:
                return simpleInstruction("OP_Divide", offset);

            case OpCode::OP_True:
                return simpleInstruction("OP_True", offset);

            case OpCode::OP_False:
                return simpleInstruction("OP_False", offset);

            case OpCode::OP_Not:
                return simpleInstruction("OP_Not", offset);

            case OpCode::OP_And:
                return simpleInstruction("OP_And", offset);

            case OpCode::OP_Or:
                return simpleInstruction("OP_Or", offset);

            case OpCode::OP_Equal:
                return simpleInstruction("OP_Equal", offset);

            case OpCode::OP_NotEqual:
                return simpleInstruction("OP_NotEqual", offset);

            case OpCode::OP_Greater:
                return simpleInstruction("OP_Greater", offset);

            case OpCode::OP_GreaterEqual:
                return simpleInstruction("OP_GreaterEqual", offset);

            case OpCode::OP_Less:
                return simpleInstruction("OP_Less", offset);

            case OpCode::OP_LessEqual:
                return simpleInstruction("OP_LessEqual", offset);

            case OpCode::OP_Pop:
                return simpleInstruction("OP_Pop", offset);

            case OpCode::OP_Define_Global:
                return simpleInstruction("OP_Define_Global", offset);

            case OpCode::OP_Get_Global:
                return simpleInstruction("OP_Get_Global", offset);

            case OpCode::OP_Set_Global:
                return simpleInstruction("OP_Set_Global", offset);

            case OpCode::OP_Get_Local:
                return simpleInstruction("OP_Get_Local", offset);

            case OpCode::OP_Set_Local:
                return simpleInstruction("OP_Set_Local", offset);

            case OpCode::OP_Jump_If_False:
                return jumpInstruction("OP_Jump_If_False", chunk, offset);

            case OpCode::OP_Long_Jump_If_False:
                return longJumpInstruction("OP_Long_Jump_If_False", chunk, offset);

            case OpCode::OP_Jump:
                return jumpInstruction("OP_Jump", chunk, offset);

            case OpCode::OP_Long_Jump:
                return longJumpInstruction("OP_Long_Jump", chunk, offset);

            case OpCode::OP_Loop:
                return jumpInstruction("OP_Loop", chunk, offset, -1);

            case OpCode::OP_Long_Loop:
                return longJumpInstruction("OP_Long_Loop", chunk, offset, -1);

            case OpCode::OP_Debug_Print:
                return simpleInstruction("OP_Debug_Print", offset);

            case OpCode::OP_Post_Incr_Global:
                return simpleInstruction("OP_Post_Incr_Global", offset);

            case OpCode::OP_Incr_Global:
                return simpleInstruction("OP_Incr_Global", offset);

            case OpCode::OP_Post_Incr_Local:
                return simpleInstruction("OP_Post_Incr_Local", offset);

            case OpCode::OP_Incr_Local:
                return simpleInstruction("OP_Incr_Local", offset);

            case OpCode::OP_Post_Decr_Global:
                return simpleInstruction("OP_Post_Decr_Global", offset);

            case OpCode::OP_Decr_Global:
                return simpleInstruction("OP_Decr_Global", offset);

            case OpCode::OP_Post_Decr_Local:
                return simpleInstruction("OP_Post_Decr_Local", offset);

            case OpCode::OP_Decr_Local:
                return simpleInstruction("OP_Decr_Local", offset);

            default:
                std::cout << "Unknown opcode " << static_cast<uint8_t>(instruction) << std::endl;
                return offset + 1;
        }
    }
}