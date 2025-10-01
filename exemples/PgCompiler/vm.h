#pragma once

#include "chunk.h"

#include "compiler.h"

#include "Interpreter/lexer.h"

#include "logger.h"

#include "bytecode_pass.h"

#include "object.h"

#include <stack>
#include <functional>

// Todo add this as a flag in when compiling in debug
// #define DEBUG_TRACE_EXECUTION

// #define DEBUG_CHECK_STACK

#define DEBUG_PROFILE_COMPILE

#define EMIT_RUNTIME_ERROR(msg) do {runtimeError((Strfy() << msg).getData()); return InterpretResult::RUNTIME_ERROR;} while(0);

namespace pg
{
    static constexpr size_t FRAMES_MAX = 64;

    enum class InterpretResult
    {
        OK,
        COMPILE_ERROR,
        RUNTIME_ERROR
    };

    /**
     * @brief Tagged union value representation following Crafting Interpreters pattern
     *
     * Stores small values (int, bool) directly in the union for zero-allocation performance.
     * Complex values (strings, floats) use pointer overflow to ElementType on the heap.
     *
     * Memory layout: 16 bytes total (8-byte union + 4-byte type + 4-byte padding)
     */
    enum CompilerValueType
    {
        COMPILER_VAL_BOOL,
        COMPILER_VAL_INT,
        COMPILER_VAL_OBJ,     // Pointer overflow for complex values (strings, floats, etc.)
        COMPILER_VAL_FUNC
    };

    struct Value
    {
        CompilerValueType type;
        union {
            bool boolean;
            int64_t number;     // Use int64_t for wider range than int
            ElementType* obj;   // Heap-allocated for strings, floats, size_t, etc.
            ObjFunction* function;
        } as;
    };

    // Fast type checking macros (compile-time constant)
    #define IS_BOOL(value)    ((value).type == COMPILER_VAL_BOOL)
    #define IS_INT(value)     ((value).type == COMPILER_VAL_INT)
    #define IS_OBJ(value)     ((value).type == COMPILER_VAL_OBJ)
    #define IS_FUNC(value)    ((value).type == COMPILER_VAL_FUNC)

    // Fast value extraction macros (direct memory access)
    #define AS_BOOL(value)    ((value).as.boolean)
    #define AS_INT(value)     ((value).as.number)
    #define AS_OBJ(value)     ((value).as.obj)
    #define AS_FUNC(value)    ((value).as.function)

    struct CallFrame
    {
        ObjFunction *function;
        uint8_t *ip;
        Value *slots;
    };

    // Fast value creation functions (C++ compatible)
    inline Value makeBoolValue(bool value)
    {
        Value result;
        result.type = COMPILER_VAL_BOOL;
        result.as.boolean = value;
        return result;
    }

    inline Value makeIntValue(int64_t value)
    {
        Value result;
        result.type = COMPILER_VAL_INT;
        result.as.number = value;
        return result;
    }

    inline Value makeObjValue(ElementType* obj)
    {
        Value result;
        result.type = COMPILER_VAL_OBJ;
        result.as.obj = obj;
        return result;
    }

    inline Value makeFuncValue(ObjFunction* func)
    {
        Value result;
        result.type = COMPILER_VAL_FUNC;
        result.as.function = func;
        return result;
    }

    // Convenience macros
    #define BOOL_VAL(value)   makeBoolValue(value)
    #define INT_VAL(value)    makeIntValue(value)
    #define OBJ_VAL(object)   makeObjValue(object)
    #define FUNC_VAL(func)    makeFuncValue(func)

    // Convert ElementType to optimized Value (minimize heap allocation)
    inline Value elementToValue(const ElementType& element)
    {
        if (element.isBool())
            return BOOL_VAL(element.get<bool>());
        else if (element.type == ElementType::UnionType::INT)
        {
            int intVal = element.get<int>();
            return INT_VAL(static_cast<int64_t>(intVal));
        }
        else
        {
            // Strings, floats, complex types always use heap
            return OBJ_VAL(new ElementType(element));
        }
    }

    // Create a copy of a Value (for when we need to store the same value in multiple places)
    inline Value copyValue(const Value& value)
    {
        // Integers and booleans can be copied directly (no heap allocation)
        if (IS_INT(value) || IS_BOOL(value))
            return value;
        // Functions are pointers, just copy the pointer
        else if (IS_FUNC(value))
            return value;
            // For heap objects, create a new copy
        else
            return OBJ_VAL(new ElementType(*AS_OBJ(value)));
    }

    inline int getValueAsInt(const Value& value)
    {
        if (IS_INT(value))
            return static_cast<int>(AS_INT(value));
        else if (IS_BOOL(value))
            return AS_BOOL(value) ? 1 : 0;
        else if (IS_OBJ(value) && AS_OBJ(value)->type == ElementType::UnionType::INT)
            return AS_OBJ(value)->get<int>();

        throw std::runtime_error("Value is not an integer");
    }

    // Convert Value back to ElementType when needed
    inline ElementType valueToElement(const Value& value)
    {
        switch (value.type)
        {
            case COMPILER_VAL_BOOL:
                return ElementType(AS_BOOL(value));
            case COMPILER_VAL_INT:
                return ElementType(static_cast<int>(AS_INT(value)));
            case COMPILER_VAL_OBJ:
                return *AS_OBJ(value);
            case COMPILER_VAL_FUNC:
                throw std::runtime_error("Cannot convert function Value to ElementType");
        }

        return ElementType(); // Should never reach
    }

    // Fast arithmetic operations on Value types
    inline Value addValues(const Value& a, const Value& b)
    {
        // Fast path for integers
        if (IS_INT(a) and IS_INT(b))
            return INT_VAL(AS_INT(a) + AS_INT(b));

        // Handle mixed int/float cases without ElementType conversion
        if (IS_INT(a) and IS_OBJ(b) and AS_OBJ(b)->isNumber())
        {
            // int + float -> convert int to float and return float result
            float floatA = static_cast<float>(AS_INT(a));
            float floatB = (*AS_OBJ(b)).get<float>();
            return OBJ_VAL(new ElementType(floatA + floatB));
        }

        if (IS_OBJ(a) and AS_OBJ(a)->isNumber() and IS_INT(b))
        {
            // float + int -> convert int to float and return float result
            float floatA = (*AS_OBJ(a)).get<float>();
            float floatB = static_cast<float>(AS_INT(b));
            return OBJ_VAL(new ElementType(floatA + floatB));
        }

        // Disallow functions
        if (IS_FUNC(a) or IS_FUNC(b))
            throw std::runtime_error("Cannot add function Values");

        // Fall back to ElementType for other complex cases (strings, etc.)
        ElementType elemA = valueToElement(a);
        ElementType elemB = valueToElement(b);
        return elementToValue(elemA + elemB);
    }

    inline Value subtractValues(const Value& a, const Value& b)
    {
        // Fast path for integers
        if (IS_INT(a) and IS_INT(b))
            return INT_VAL(AS_INT(a) - AS_INT(b));

        // Handle mixed int/float cases without ElementType conversion
        if (IS_INT(a) and IS_OBJ(b) and AS_OBJ(b)->isNumber())
        {
            float floatA = static_cast<float>(AS_INT(a));
            float floatB = (*AS_OBJ(b)).get<float>();
            return OBJ_VAL(new ElementType(floatA - floatB));
        }

        if (IS_OBJ(a) and AS_OBJ(a)->isNumber() and IS_INT(b))
        {
            float floatA = (*AS_OBJ(a)).get<float>();
            float floatB = static_cast<float>(AS_INT(b));
            return OBJ_VAL(new ElementType(floatA - floatB));
        }

        // Disallow functions
        if (IS_FUNC(a) or IS_FUNC(b))
            throw std::runtime_error("Cannot add function Values");

        // Fall back to ElementType for other complex cases
        ElementType elemA = valueToElement(a);
        ElementType elemB = valueToElement(b);
        return elementToValue(elemA - elemB);
    }

    inline Value multiplyValues(const Value& a, const Value& b)
    {
        if (IS_INT(a) and IS_INT(b))
            return INT_VAL(AS_INT(a) * AS_INT(b));

        // Disallow functions
        if (IS_FUNC(a) or IS_FUNC(b))
            throw std::runtime_error("Cannot add function Values");

        ElementType elemA = valueToElement(a);
        ElementType elemB = valueToElement(b);
        return elementToValue(elemA * elemB);
    }

    inline Value divideValues(const Value& a, const Value& b)
    {
        if (IS_INT(a) and IS_INT(b) and AS_INT(b) != 0)
            return INT_VAL(AS_INT(a) / AS_INT(b));

        // Disallow functions
        if (IS_FUNC(a) or IS_FUNC(b))
            throw std::runtime_error("Cannot add function Values");

        ElementType elemA = valueToElement(a);
        ElementType elemB = valueToElement(b);
        return elementToValue(elemA / elemB);
    }

    inline Value negateValue(const Value& val)
    {
        if (IS_INT(val))
            return INT_VAL(-AS_INT(val));

        // Disallow functions
        if (IS_FUNC(val))
            throw std::runtime_error("Cannot add function Values");

        ElementType elem = valueToElement(val);
        return elementToValue(-elem);
    }

    inline bool isValueNumber(const Value& val)
    {
        if (IS_INT(val))
            return true;

        if (IS_OBJ(val))
            return AS_OBJ(val)->isNumber();

        return false;
    }

    inline bool isValueTrue(const Value& val)
    {
        if (IS_BOOL(val))
            return AS_BOOL(val);

        if (IS_INT(val))
            return AS_INT(val) != 0;

        if (IS_OBJ(val))
            return AS_OBJ(val)->isTrue();

        return false;
    }

    // Fast comparison operations
    inline Value equalsValues(const Value& a, const Value& b)
    {
        if (IS_INT(a) and IS_INT(b))
            return BOOL_VAL(AS_INT(a) == AS_INT(b));

        if (IS_BOOL(a) and IS_BOOL(b))
            return BOOL_VAL(AS_BOOL(a) == AS_BOOL(b));

        // Disallow functions
        if (IS_FUNC(a) or IS_FUNC(b))
            throw std::runtime_error("Cannot add function Values");

        ElementType elemA = valueToElement(a);
        ElementType elemB = valueToElement(b);
        return elementToValue(elemA == elemB);
    }

    inline Value notEqualsValues(const Value& a, const Value& b)
    {
        if (IS_INT(a) and IS_INT(b))
            return BOOL_VAL(AS_INT(a) != AS_INT(b));

        if (IS_BOOL(a) && IS_BOOL(b))
            return BOOL_VAL(AS_BOOL(a) != AS_BOOL(b));

        // Disallow functions
        if (IS_FUNC(a) or IS_FUNC(b))
            throw std::runtime_error("Cannot add function Values");

        ElementType elemA = valueToElement(a);
        ElementType elemB = valueToElement(b);
        return elementToValue(elemA != elemB);
    }

    inline Value greaterValues(const Value& a, const Value& b)
    {
        if (IS_INT(a) and IS_INT(b))
            return BOOL_VAL(AS_INT(a) > AS_INT(b));

        // Disallow functions
        if (IS_FUNC(a) or IS_FUNC(b))
            throw std::runtime_error("Cannot add function Values");

        ElementType elemA = valueToElement(a);
        ElementType elemB = valueToElement(b);
        return elementToValue(elemA > elemB);
    }

    inline Value greaterEqualValues(const Value& a, const Value& b)
    {
        if (IS_INT(a) and IS_INT(b))
            return BOOL_VAL(AS_INT(a) >= AS_INT(b));

        // Disallow functions
        if (IS_FUNC(a) or IS_FUNC(b))
            throw std::runtime_error("Cannot add function Values");

        ElementType elemA = valueToElement(a);
        ElementType elemB = valueToElement(b);
        return elementToValue(elemA >= elemB);
    }

    inline Value lessValues(const Value& a, const Value& b)
    {
        if (IS_INT(a) and IS_INT(b))
            return BOOL_VAL(AS_INT(a) < AS_INT(b));

        // Disallow functions
        if (IS_FUNC(a) and IS_FUNC(b))
            throw std::runtime_error("Cannot add function Values");

        ElementType elemA = valueToElement(a);
        ElementType elemB = valueToElement(b);
        return elementToValue(elemA < elemB);
    }

    inline Value lessEqualValues(const Value& a, const Value& b)
    {
        if (IS_INT(a) and IS_INT(b))
            return BOOL_VAL(AS_INT(a) <= AS_INT(b));

        // Disallow functions
        if (IS_FUNC(a) or IS_FUNC(b))
            throw std::runtime_error("Cannot add function Values");

        ElementType elemA = valueToElement(a);
        ElementType elemB = valueToElement(b);
        return elementToValue(elemA <= elemB);
    }

    // Memory management for heap-allocated objects
    inline void freeValue(Value& value)
    {
        if (IS_OBJ(value) and AS_OBJ(value) != nullptr)
        {
            delete AS_OBJ(value);
            value.as.obj = nullptr;
        }
    }

    class IndexableStack
    {
    private:
        static constexpr size_t MAX_STACK_SIZE = FRAMES_MAX * 4096; // Callstack * 4K elements max
        alignas(Value) char stack_memory[MAX_STACK_SIZE * sizeof(Value)];
        size_t stack_top = 0;

        Value* stack_data()
        {
            return reinterpret_cast<Value*>(stack_memory);
        }

        const Value* stack_data() const
        {
            return reinterpret_cast<const Value*>(stack_memory);
        }

    public:
        // Direct Value operations (fast path)
        void push(const Value& value)
        {
            if (stack_top >= MAX_STACK_SIZE)
                throw std::runtime_error("Stack overflow");

            stack_data()[stack_top++] = value;  // Simple assignment, no constructor
        }

        // Legacy ElementType support (converts to Value)
        void push(const ElementType& element)
        {
            push(elementToValue(element));
        }

        void push(ElementType&& element)
        {
            push(elementToValue(element));
        }

        Value pop()
        {
            if (stack_top == 0)
                throw std::runtime_error("Trying to pop on an empty stack");

            Value value = stack_data()[stack_top - 1];
            stack_top--;  // No destructor needed for POD-like Value

            return value;
        }

        // For legacy compatibility - returns ElementType
        ElementType popElement()
        {
            Value val = pop();
            ElementType result = valueToElement(val);
            freeValue(val);  // Clean up any heap allocation

            return result;
        }

        Value& operator[](size_t index) { return stack_data()[index]; }
        const Value& operator[](size_t index) const { return stack_data()[index]; }

        Value top() const
        {
            if (stack_top == 0)
                throw std::runtime_error("Stack is empty");

            return stack_data()[stack_top - 1];
        }

        // For legacy compatibility
        ElementType topElement() const
        {
            return valueToElement(top());
        }

        bool empty() const { return stack_top == 0; }
        size_t size() const { return stack_top; }
        
        // Get pointer to stack data for frame slots
        Value* data() { return stack_data(); }
        const Value* data() const { return stack_data(); }

        void clear()
        {
            // Clean up any heap-allocated objects
            while (stack_top > 0)
            {
                freeValue(stack_data()[--stack_top]);
            }
        }
    };

    struct VM
    {
        // Destructor to properly clean up globals map and stack
        ~VM()
        {
            // Free all Values stored in globals before destruction
            for (auto& pair : globals)
            {
                freeValue(pair.second);
            }
            // Clean up any remaining Values on the stack
            stack.clear();
        }

        InterpretResult interpretFromText(const std::string& source)
        {
            Lexer lexer;

            try
            {
                lexer.readFromText(source);
            }
            catch (const std::exception& e)
            {
                LOG_ERROR("VM", e.what());
                return InterpretResult::COMPILE_ERROR;
            }

            auto tokens = lexer.getTokens();

            return interpret(tokens);
        }

        InterpretResult interpret(const std::queue<Token>& tokens);

        InterpretResult run();

        Value readConstant();
        Value readLongConstant();

        void binaryOp(std::function<Value(Value, Value)> op);
        void fastBinaryOp(Value (*op)(const Value&, const Value&));

        // Core Value operations for performance
        inline void push(const Value& value)
        {
            stack.push(value);
        }

        inline void push(const ElementType& value)
        {
            stack.push(value);  // Automatically converts to Value
        }

        Value pop()
        {
#ifdef DEBUG_CHECK_STACK
            if (stack.empty())
                throw std::runtime_error("Trying to pop on an empty stack");
#endif
            return stack.pop();
        }

        Value peek(size_t distance = 0) const
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
            if (*currentFrame->ip + 1 >= currentFrame->function->chunk.code.size())
            {
                throw std::runtime_error("Not enough bytes to read uint16.");
            }
#endif
            uint16_t value = (static_cast<uint16_t>(currentFrame->function->chunk.code[*currentFrame->ip]) << 8);
            advanceIp();
            value |= static_cast<uint16_t>(currentFrame->function->chunk.code[*currentFrame->ip]);
            advanceIp();

            return value;
        }

        uint32_t readUint32()
        {
#ifdef DEBUG_CHECK_STACK
            if (*currentFrame->ip + 3 >= currentFrame->function->chunk.code.size())
            {
                throw std::runtime_error("Not enough bytes to read uint32.");
            }
#endif
            uint32_t value = (static_cast<uint32_t>(currentFrame->function->chunk.code[*currentFrame->ip]) << 24);
            advanceIp();
            value |= (static_cast<uint32_t>(currentFrame->function->chunk.code[*currentFrame->ip]) << 16);
            advanceIp();
            value |= (static_cast<uint32_t>(currentFrame->function->chunk.code[*currentFrame->ip]) << 8);
            advanceIp();
            value |= static_cast<uint32_t>(currentFrame->function->chunk.code[*currentFrame->ip]);
            advanceIp();

            return value;
        }

        inline uint8_t advanceIp()
        {
            return ((*currentFrame->ip)++);
        }

        inline bool checkIpAgainstStack(uint8_t ahead)
        {
            return *currentFrame->ip + ahead >= currentFrame->function->chunk.code.size();
        }

        inline void resetStack()
        {
            stack.clear();
        }

        void runtimeError(const std::string& message)
        {
            LOG_ERROR("VM", "[line " << currentFrame->function->chunk.lines[*currentFrame->ip - 1] << "] in script");
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

        CallFrame frames[FRAMES_MAX];

        CallFrame *currentFrame = nullptr;

        int frameCount = 0;

        /* The stack of the VM */
        IndexableStack stack;

        std::unordered_map<std::string, Value> globals;

        // Test output buffer for __dprint (used in tests)
        std::string testOutput;

        // Bytecode optimization
        PassManager passManager;
        bool enableOptimizations = true;

        // Optimization control methods
        void enableBytecodeOptimization()
        {
            enableOptimizations = true;
            LOG_INFO("VM", "Bytecode optimization enabled");
        }

        void disableBytecodeOptimization()
        {
            enableOptimizations = false;
            LOG_INFO("VM", "Bytecode optimization disabled");
        }

        void enableOptimizationDebugging()
        {
            passManager.setDebugOutput(true);
        }

        void disableOptimizationDebugging()
        {
            passManager.setDebugOutput(false);
        }

        void listOptimizationPasses() const
        {
            passManager.listPasses();
        }

        void addOptimizationPass(std::unique_ptr<BytecodePass> pass)
        {
            passManager.addPass(std::move(pass));
        }
    };
}