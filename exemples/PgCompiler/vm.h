#pragma once

#include "chunk.h"

#include "compiler.h"

#include "Interpreter/lexer.h"

#include "logger.h"

#include <stack>
#include <functional>

// Todo add this as a flag in when compiling in debug
// #define DEBUG_TRACE_EXECUTION

#define EMIT_RUNTIME_ERROR(msg) do {runtimeError((Strfy() << msg).getData()); return InterpretResult::RUNTIME_ERROR;} while(0);

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
        InterpretResult interpretFromText(const std::string& source)
        {
            Lexer lexer;

            try
            {
                lexer.readFromText(source);
            }
            catch(const std::exception& e)
            {
                LOG_ERROR("VM", e.what());
                return InterpretResult::COMPILE_ERROR;
            }

            auto tokens = lexer.getTokens();

            return interpret(tokens);
        }

        InterpretResult interpret(const std::queue<Token>& tokens);

        InterpretResult run();

        ElementType readConstant();
        ElementType readLongConstant();

        void binaryOp(std::function<ElementType(ElementType, ElementType)> op);

        inline void push(const ElementType& value)
        {
            stack.push(value);
        }

        ElementType pop()
        {
            if (stack.empty())
                throw std::runtime_error("Trying to pop on an empty stack");

            ElementType value = stack.top();
            stack.pop();

            return value;
        }

        ElementType peek(size_t distance = 0) const
        {
            if (distance >= stack.size())
                throw std::runtime_error("Trying to peek too far in the stack");

            auto tempStack = stack;

            for (size_t i = 0; i < distance; ++i)
                tempStack.pop();

            return tempStack.top();
        }

        inline void resetStack()
        {
            stack = std::stack<ElementType, std::vector<ElementType>>();
        }

        void runtimeError(const std::string& message)
        {
            LOG_ERROR("VM", "[line " << chunk.lines[ip - 1] << "] in script");
            LOG_ERROR("VM", message);
            resetStack();
        }

        Compiler compiler;

        /* The chunk being interpreted */
        Chunk chunk;

        /* Instruction pointer */
        size_t ip = 0;

        /* The stack of the VM */
        std::stack<ElementType, std::vector<ElementType>> stack;
    };

}