#pragma once

#include "chunk.h"

namespace pg
{
    enum class InterpretResult
    {
        OK,
        COMPILE_ERROR,
        RUNTIME_ERROR
    };

    struct VM
    {
        InterpretResult interpret(const Chunk& chunk);

        InterpretResult run();

        Value readConstant();
        Value readLongConstant();

        /* The chunk being interpreted */
        Chunk chunk;

        /* Instruction pointer */
        size_t ip = 0;
    };

}