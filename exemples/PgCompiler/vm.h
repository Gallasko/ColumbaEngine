#pragma once

#include "chunk.h"

#include <stack>

// Todo add this as a flag in when compiling in debug
#define DEBUG_TRACE_EXECUTION

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

        inline void push(Value value)
        {
            stack.push(value);
        }

        Value pop()
        {
            if (stack.empty())
                throw std::runtime_error("Trying to pop on an empty stack");

            Value value = stack.top();
            stack.pop();

            return value;
        }

        inline void resetStack()
        {
            stack = std::stack<Value, std::vector<Value>>();
        }

        /* The chunk being interpreted */
        Chunk chunk;

        /* Instruction pointer */
        size_t ip = 0;

        /* The stack of the VM */
        std::stack<Value, std::vector<Value>> stack;
    };

}