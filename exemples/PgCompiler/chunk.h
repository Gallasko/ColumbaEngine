#pragma once

#include <vector>
#include <cstdint>
#include <cstddef>

#include <stdexcept>

#include "object.h"

#include "Memory/elementtype.h"

namespace pg
{
    enum class OpCode : uint8_t
    {
        OP_Return = 0,
        OP_Constant,
        OP_LongConstant,
        OP_Negate,
        OP_Add,
        OP_Subtract,
        OP_Multiply,
        OP_Divide,
        OP_True,
        OP_False,
        OP_Not,
        OP_And,
        OP_Or,
        OP_Equal,
        OP_NotEqual,
        OP_Greater,
        OP_GreaterEqual,
        OP_Less,
        OP_LessEqual,
        OP_Pop,
        OP_Define_Global,
        OP_Get_Global,
        OP_Set_Global,
        OP_Get_Local,
        OP_Set_Local,
        OP_Jump_If_False,
        OP_Long_Jump_If_False,
        OP_Jump,
        OP_Long_Jump,
        OP_Loop,
        OP_Long_Loop,
        OP_Debug_Print,

        OP_Post_Incr_Global,
        OP_Incr_Global,
        OP_Post_Incr_Local,
        OP_Incr_Local,
        OP_Post_Decr_Global,
        OP_Decr_Global,
        OP_Post_Decr_Local,
        OP_Decr_Local,

        OP_Call,
    };

    struct Chunk
    {
        std::vector<uint8_t> code;

        std::vector<Value> constants;

        std::vector<int> lines;

        size_t addConstant(const Value& value, int line)
        {
            constants.push_back(value);
            auto cIndex = constants.size() - 1;

            // If more than 256 constants in one chunk, we can't store the index in one byte
            // So we store it as a 4 bytes instruction (OpLongConstant, byte1, byte2, byte3, with the 3 bytes forming the constant index)
            if (cIndex > 255)
            {
                addCode(OpCode::OP_LongConstant, line);
                addCode((cIndex >> 16) & 0xFF, line);
                addCode((cIndex >> 8) & 0xFF, line);
                addCode(cIndex & 0xFF, line);
            }
            else if (cIndex > 0xFFFFFF)
            {
                // Should never happen
                throw std::runtime_error("Too many constants in one chunk (> 16 millions)");
            }
            else
            {
                addCode(OpCode::OP_Constant, line);
                addCode(cIndex, line);
            }

            return code.size() - 1;
        }

        size_t addConstant(const ElementType& value, int line)
        {
            return addConstant(elementToValue(value), line);
        }

        size_t addConstant(ObjFunction* value, int line)
        {
            return addConstant(FUNC_VAL(value), line);
        }

        size_t addCode(const OpCode& op, int line)
        {
            code.push_back(static_cast<uint8_t>(op));
            lines.push_back(line);

            return code.size() - 1;
        }

        size_t addCode(uint8_t byte, int line)
        {
            code.push_back(byte);
            lines.push_back(line);

            return code.size() - 1;
        }

        void clear()
        {
            code.clear();
            constants.clear();
            lines.clear();
        }
    };

    // Utility function to get the size of an instruction in bytes
    inline size_t getInstructionSize(OpCode opcode) {
        switch (opcode) {
            case OpCode::OP_Constant:
            case OpCode::OP_Call:
                return 2; // opcode + 1 byte operand

            case OpCode::OP_Define_Global:
            case OpCode::OP_Get_Global:
            case OpCode::OP_Set_Global:
            case OpCode::OP_Get_Local:
            case OpCode::OP_Set_Local:
            case OpCode::OP_Return:
            case OpCode::OP_Negate:
            case OpCode::OP_Add:
            case OpCode::OP_Subtract:
            case OpCode::OP_Multiply:
            case OpCode::OP_Divide:
            case OpCode::OP_True:
            case OpCode::OP_False:
            case OpCode::OP_Not:
            case OpCode::OP_And:
            case OpCode::OP_Or:
            case OpCode::OP_Equal:
            case OpCode::OP_NotEqual:
            case OpCode::OP_Greater:
            case OpCode::OP_GreaterEqual:
            case OpCode::OP_Less:
            case OpCode::OP_LessEqual:
            case OpCode::OP_Pop:
            case OpCode::OP_Debug_Print:
            case OpCode::OP_Post_Incr_Global:
            case OpCode::OP_Incr_Global:
            case OpCode::OP_Post_Incr_Local:
            case OpCode::OP_Incr_Local:
            case OpCode::OP_Post_Decr_Global:
            case OpCode::OP_Decr_Global:
            case OpCode::OP_Post_Decr_Local:
            case OpCode::OP_Decr_Local:
                return 1; // opcode only, no operand

            case OpCode::OP_LongConstant:
                return 4; // opcode + 3 byte operand

            case OpCode::OP_Jump_If_False:
            case OpCode::OP_Jump:
            case OpCode::OP_Loop:
                return 3; // opcode + 2 byte operand

            case OpCode::OP_Long_Jump_If_False:
            case OpCode::OP_Long_Jump:
            case OpCode::OP_Long_Loop:
                return 5; // opcode + 4 byte operand

            default:
                return 1; // default to single byte for unknown opcodes
        }
    }

    struct ObjFunction
    {
        Chunk chunk;
        int arity; // Number of parameters
        std::string name;
    };
}