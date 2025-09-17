#pragma once

#include <vector>
#include <cstdint>
#include <cstddef>

namespace pg
{
    enum class OpCode : uint8_t
    {
        OP_Return = 0,
        OP_Constant,
    };

    typedef double Value;

    struct Chunk
    {
        std::vector<uint8_t> code;

        std::vector<Value> constants;

        size_t addConstant(Value value)
        {
            constants.push_back(value);

            return constants.size() - 1;
        }

        size_t addCode(const OpCode& op)
        {
            code.push_back(static_cast<uint8_t>(op));

            return code.size() - 1;
        }

        size_t addCode(uint8_t byte)
        {
            code.push_back(byte);

            return code.size() - 1;
        }
    };
}