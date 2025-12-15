#pragma once

#include "chunk.h"

#include <string>

namespace pg
{
    // Forward declaration
    struct VM;

    void printValue(VM* vm, const Value& value);

    void disassembleChunk(VM* vm, const Chunk& chunk, const std::string& name);
    int disassembleInstruction(VM* vm, const Chunk& chunk, int offset);

    /**
     * @brief Convert opcode enum to string name
     * @param opcode The opcode to convert
     * @return String representation of the opcode
     */
    std::string opcodeToString(OpCode opcode);
}