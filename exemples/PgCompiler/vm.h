#pragma once

#include "chunk.h"

#include "compiler.h"

#include "Interpreter/lexer.h"

#include "logger.h"

#include "bytecode_pass.h"

#include <stack>
#include <functional>

// Todo add this as a flag in when compiling in debug
// #define DEBUG_TRACE_EXECUTION

// #define DEBUG_CHECK_STACK

#define DEBUG_PROFILE_COMPILE

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
    private:
        static constexpr size_t MAX_STACK_SIZE = 8192;
        alignas(ElementType) char stack_memory[MAX_STACK_SIZE * sizeof(ElementType)];
        size_t stack_top = 0;
        
        ElementType* stack_data() { 
            return reinterpret_cast<ElementType*>(stack_memory); 
        }
        
        const ElementType* stack_data() const { 
            return reinterpret_cast<const ElementType*>(stack_memory); 
        }
        
    public:
        void push(const ElementType& value) {
            if (stack_top >= MAX_STACK_SIZE)
                throw std::runtime_error("Stack overflow");
            new(&stack_data()[stack_top++]) ElementType(value);
        }
        
        void push(ElementType&& value) {
            if (stack_top >= MAX_STACK_SIZE)
                throw std::runtime_error("Stack overflow");
            new(&stack_data()[stack_top++]) ElementType(std::move(value));
        }

        ElementType pop() {
            if (stack_top == 0)
                throw std::runtime_error("Trying to pop on an empty stack");
            ElementType value = std::move(stack_data()[stack_top - 1]);
            stack_data()[--stack_top].~ElementType();
            return value;
        }

        ElementType& operator[](size_t index) { return stack_data()[index]; }
        const ElementType& operator[](size_t index) const { return stack_data()[index]; }

        ElementType top() const {
            if (stack_top == 0)
                throw std::runtime_error("Stack is empty");
            return stack_data()[stack_top - 1];
        }

        bool empty() const { return stack_top == 0; }
        size_t size() const { return stack_top; }

        void clear() { 
            while (stack_top > 0) {
                stack_data()[--stack_top].~ElementType();
            }
        }
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
#ifdef DEBUG_CHECK_STACK
            if (stack.empty())
                throw std::runtime_error("Trying to pop on an empty stack");
#endif
            ElementType value = stack.top();
            stack.pop();

            return value;
        }

        ElementType peek(size_t distance = 0) const
        {
#ifdef DEBUG_CHECK_STACK
            if (distance >= stack.size())
                throw std::runtime_error("Trying to peek too far in the stack");
#endif
            return stack[stack.size() - 1 - distance];
        }

        uint16_t readUint16()
        {
#ifdef DEBUG_CHECK_STACK
            if (ip + 1 >= chunk.code.size())
            {
                throw std::runtime_error("Not enough bytes to read uint16.");
            }
#endif
            uint16_t value = (static_cast<uint16_t>(chunk.code[ip]) << 8);
            ip++;
            value |= static_cast<uint16_t>(chunk.code[ip]);
            ip++;

            return value;
        }

        uint32_t readUint32()
        {
#ifdef DEBUG_CHECK_STACK
            if (ip + 3 >= chunk.code.size())
            {
                throw std::runtime_error("Not enough bytes to read uint32.");
            }
#endif
            uint32_t value = (static_cast<uint32_t>(chunk.code[ip]) << 24);
            ip++;
            value |= (static_cast<uint32_t>(chunk.code[ip]) << 16);
            ip++;
            value |= (static_cast<uint32_t>(chunk.code[ip]) << 8);
            ip++;
            value |= static_cast<uint32_t>(chunk.code[ip]);
            ip++;

            return value;
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
#ifdef DEBUG_CHECK_STACK
            if (stack.size() < 2)
            {
                runtimeError("Stack underflow on binary operation.");
                return false;
            }
#endif
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

        // Test output buffer for __dprint (used in tests)
        std::string testOutput;

        // Bytecode optimization
        PassManager passManager;
        bool enableOptimizations = true;

        // Optimization control methods
        void enableBytecodeOptimization() {
            enableOptimizations = true;
            LOG_INFO("VM", "Bytecode optimization enabled");
        }

        void disableBytecodeOptimization() {
            enableOptimizations = false;
            LOG_INFO("VM", "Bytecode optimization disabled");
        }

        void enableOptimizationDebugging() {
            passManager.setDebugOutput(true);
        }

        void disableOptimizationDebugging() {
            passManager.setDebugOutput(false);
        }

        void listOptimizationPasses() const {
            passManager.listPasses();
        }

        void addOptimizationPass(std::unique_ptr<BytecodePass> pass) {
            passManager.addPass(std::move(pass));
        }
    };

}