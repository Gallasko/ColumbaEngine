#pragma once

#include "chunk.h"

#include "compiler.h"

#include "Interpreter/lexer.h"

#include "logger.h"

#include <stack>
#include <functional>

// Todo add this as a flag in when compiling in debug
// #define DEBUG_TRACE_EXECUTION

// #define DEBUG_PROFILE_COMPILE

#define EMIT_RUNTIME_ERROR(msg) do {runtimeError((Strfy() << msg).getData()); return InterpretResult::RUNTIME_ERROR;} while(0);

namespace pg
{
    enum class InterpretResult
    {
        OK,
        COMPILE_ERROR,
        RUNTIME_ERROR
    };

    class IndexableStack
    {
        std::vector<ElementType> data;
    public:
        void push(const ElementType& value) { data.push_back(value); }

        ElementType pop() {
            if (data.empty())
                throw std::runtime_error("Trying to pop on an empty stack");
            ElementType value = data.back();
            data.pop_back();
            return value;
        }

        ElementType& operator[](size_t index) { return data[index]; }
        const ElementType& operator[](size_t index) const { return data[index]; }

        ElementType top() const {
            if (data.empty())
                throw std::runtime_error("Stack is empty");
            return data.back();
        }

        bool empty() const { return data.empty(); }
        size_t size() const { return data.size(); }

        void clear() { data.clear(); }
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

            return stack[stack.size() - 1 - distance];
        }

        inline void resetStack()
        {
            stack.clear();
        }

        void runtimeError(const std::string& message)
        {
            LOG_ERROR("VM", "[line " << chunk.lines[ip - 1] << "] in script");
            LOG_ERROR("VM", message);
            resetStack();
        }

        bool checkBooleanBinaryOp()
        {
            if (stack.size() < 2)
            {
                runtimeError("Stack underflow on binary operation.");
                return false;
            }

            return true;
        }

        Compiler compiler;

        /* The chunk being interpreted */
        Chunk chunk;

        /* Instruction pointer */
        size_t ip = 0;

        /* The stack of the VM */
        IndexableStack stack;

        std::unordered_map<std::string, ElementType> globals;
    };

}