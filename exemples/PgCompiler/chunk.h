#pragma once

#include <vector>
#include <cstdint>
#include <cstddef>

#include <stdexcept>

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
    };

    struct Chunk
    {
        std::vector<uint8_t> code;

        std::vector<ElementType> constants;

        std::vector<int> lines;

        size_t addConstant(const ElementType& value, int line)
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
    };
}