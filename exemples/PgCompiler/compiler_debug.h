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
}