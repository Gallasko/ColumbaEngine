#pragma once

#include "chunk.h"

#include "Interpreter/lexer.h"

#include "logger.h"

#include "bytecode_pass.h"

#include "object.h"

#include <stack>
#include <functional>

#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <setjmp.h>

// Todo add this as a flag in when compiling in debug
// #define DEBUG_TRACE_EXECUTION

// #define DEBUG_CHECK_STACK

// #define DEBUG_RUNTIME_MEMORY

#ifdef DEBUG_RUNTIME_MEMORY
#include <iostream>
#endif

#define DEBUG_PROFILE_COMPILE

#define EMIT_RUNTIME_ERROR(msg) do {runtimeError((Strfy() << msg).getData()); return InterpretResult::RUNTIME_ERROR;} while(0);

namespace pg
{
    static constexpr size_t FRAMES_MAX = 64;

    // Forward declaration for VM
    struct VM;

    // Function pointer type for operation handlers
    typedef void (*OpHandler)(VM* vm);

    // Operation information structure
    struct OpCodeInfo {
        OpHandler handler;
        const char* name;
        uint8_t operand_count;

        OpCodeInfo() : handler(nullptr), name("UNKNOWN"), operand_count(0) {}
        OpCodeInfo(OpHandler h, const char* n, uint8_t count = 0)
            : handler(h), name(n), operand_count(count) {}
    };

    inline bool isValueNumber(const Value& val)
    {
        if (IS_INT(val) || IS_FLOAT(val))
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

        if (IS_FLOAT(val))
        {
            const float a = static_cast<float>(AS_FLOAT(val));
            const float b = 0.0f;
            const float epsilon = 0.00001f;

            return not (std::fabs(a - b) <= epsilon * std::max({1.0f, std::fabs(a), std::fabs(b)}));
        }

        if (IS_OBJ(val))
            return AS_OBJ(val)->isTrue();

        return false;
    }

    // Memory management for heap-allocated objects
    inline void freeValue(Value& value)
    {
        if (IS_OBJ(value) and AS_OBJ(value) != nullptr)
        {
            delete AS_OBJ(value);
            value.as.obj = nullptr;
        }
        else if (IS_NAT_FUNC(value) and AS_NAT_FUNC(value) != nullptr)
        {
            delete AS_NAT_FUNC(value);
            value.as.nativeFunc = nullptr;
        }
    }

    class IndexableStack
    {
    private:
        static constexpr size_t MAX_STACK_SIZE = FRAMES_MAX * 4096; // Callstack * 4K elements max
        alignas(Value) char stack_memory[MAX_STACK_SIZE * sizeof(Value)];
        Value* stack_values = reinterpret_cast<Value*>(stack_memory); // Cached pointer for fast access
        size_t stack_top = 0;

    public:
        // Direct Value operations (fast path)
        inline void push(const Value& value)
        {
            if (stack_top >= MAX_STACK_SIZE)
                throw std::runtime_error("Stack overflow");

            stack_values[stack_top++] = value;  // Direct access, no function call
        }

        inline void push(const Value&& value)
        {
            if (stack_top >= MAX_STACK_SIZE)
                throw std::runtime_error("Stack overflow");

            stack_values[stack_top++] = value;  // Direct access, no function call
        }

        // Legacy ElementType support (converts to Value)
        inline void push(const ElementType& element)
        {
            push(elementToValue(element));
        }

        inline void push(ElementType&& element)
        {
            push(elementToValue(element));
        }

        inline Value pop()
        {
            if (stack_top == 0)
                throw std::runtime_error("Trying to pop on an empty stack");

            Value value = stack_values[stack_top - 1];
            stack_top--;  // No destructor needed for POD-like Value

            return value;
        }

        // For legacy compatibility - returns ElementType
        inline ElementType popElement()
        {
            Value val = pop();
            ElementType result = valueToElement(val);
            // Note: No cleanup here - caller is responsible for managing Value lifecycle

            return result;
        }

        inline Value& operator[](size_t index) { return stack_values[index]; }
        inline const Value& operator[](size_t index) const { return stack_values[index]; }

        inline Value top() const
        {
            if (stack_top == 0)
                throw std::runtime_error("Stack is empty");

            return stack_values[stack_top - 1];
        }

        // For legacy compatibility
        inline ElementType topElement() const
        {
            return valueToElement(top());
        }

        inline bool empty() const { return stack_top == 0; }
        inline size_t size() const { return stack_top; }

        // Get pointer to stack data for frame slots
        inline Value* data() { return stack_values; }
        inline const Value* data() const { return reinterpret_cast<const Value*>(stack_memory); }

        void clear()
        {
            // Clean up any heap-allocated objects
            while (stack_top > 0)
            {
                freeValue(stack_values[--stack_top]);
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
                releaseAndDelete(pair.second);
            }
            // Clean up any remaining Values on the stack
            while (!stack.empty())
            {
                auto value = stack.pop();
                releaseAndDelete(value);
            }
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

        // Core Value operations for performance
        inline void push(const Value& value)
        {
            stack.push(value);
        }

        inline void push(const ElementType& value)
        {
            Value val = elementToValue(value);
            // Track if it's a heap object
            if (IS_OBJ(val) || IS_FUNC(val) || IS_CLOSURE(val) || IS_UPVALUE(val) || IS_NAT_FUNC(val)) {
                val = trackNewValue(val);
            }
            stack.push(val);
        }

        inline Value pop()
        {
#ifdef DEBUG_CHECK_STACK
            if (stack.empty())
                throw std::runtime_error("Trying to pop on an empty stack");
#endif
            return stack.pop();
        }

        inline const Value& peek(size_t distance = 0) const
        {
#ifdef DEBUG_CHECK_STACK
            if (distance >= stack.size())
                throw std::runtime_error("Trying to peek too far in the stack");
#endif
            return stack[stack.size() - 1 - distance];
        }

        // Update cached chunk data pointer when switching functions
        inline void updateChunkCache()
        {
            if (currentFrame && currentFrame->closure)
            {
                auto& chunk = currentFrame->closure->function->chunk.code;
                chunkData = chunk.data();
                chunkDataEnd = chunk.data() + chunk.size();
            }
        }

        inline void resetStack()
        {
            // Use VM's reference counting instead of IndexableStack's clear()
            while (!stack.empty())
            {
                auto value = stack.pop();
                releaseAndDelete(value);
            }
        }

        void runtimeError(const std::string& message)
        {
            LOG_ERROR("VM", message);

            // No sync needed - using frame IP directly

            for (int i = frameCount - 1; i >= 0; i--)
            {
                CallFrame *frame = &frames[i];
                ObjFunction *function = frame->closure->function;
                size_t instruction = frame->ip - function->chunk.code.data() - 1;

                LOG_ERROR("VM", "[line " << function->chunk.lines[instruction] << "] in " << function->name);
            }

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

        ObjUpvalue* captureUpvalue(Value* local);

        void closeUpvalues(Value* last);

        bool callValue(const Value& callee, int argCount);
        bool callMethod(Klass* receiver, const std::string& methodName, int argCount);

        bool call(Closure* closure, int argCount);
        bool callBound(Closure* closure, int argCount);

        // Reference counting methods
        inline Value retainValue(const Value& value);   // Returns the value after retaining
        inline bool releaseValue(const Value& value);   // Returns true if should delete
        void deleteValue(const Value& value);    // Actually delete the object
        inline Value trackNewValue(const Value& value); // Track newly created object with refcount=1
        int getValueRefCount(const Value& value) const;
        size_t getTotalTrackedObjects() const { return refCounts.size(); }

        // Convenience method for release + delete
        void releaseAndDelete(const Value& value);

        // Arithmetic operations with proper reference tracking
        Value addValues(const Value& a, const Value& b);
        Value subtractValues(const Value& a, const Value& b);
        Value multiplyValues(const Value& a, const Value& b);
        Value divideValues(const Value& a, const Value& b);
        Value negateValue(const Value& val);

        // Comparison operations with proper reference tracking
        Value equalsValues(const Value& a, const Value& b);
        Value notEqualsValues(const Value& a, const Value& b);
        Value greaterValues(const Value& a, const Value& b);
        Value greaterEqualValues(const Value& a, const Value& b);
        Value lessValues(const Value& a, const Value& b);
        Value lessEqualValues(const Value& a, const Value& b);

        CallFrame frames[FRAMES_MAX];

        CallFrame *currentFrame = nullptr;

        int frameCount = 0;

        // Cache chunk data pointer to avoid repeated vector::data() calls
        uint8_t *chunkData = nullptr;
        uint8_t *chunkDataEnd = nullptr; // Cached end pointer for fast loop exit check

        /* The stack of the VM */
        IndexableStack stack;

        std::unordered_map<std::string, Value> globals;

        // Function pointer dispatch system
        static OpCodeInfo operations[256];
        jmp_buf exit_jump;
        InterpretResult exit_result;

        // Reference counting for heap-allocated objects
        std::unordered_map<void*, int> refCounts;

        // Test output buffer for __dprint (used in tests)
        std::string testOutput;

        // Bytecode optimization
        PassManager passManager;
        bool enableOptimizations = true;

        ObjUpvalue* openUpvalues = nullptr;

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

        void defineNative(const std::string& name, NativeFn function)
        {
            auto* nativeFunc = new NativeFunction();
            nativeFunc->function = function;

            Value val;
            val.type = COMPILER_VAL_NATIVE;
            val.as.nativeFunc = nativeFunc;

            globals[name] = trackNewValue(val);
        }

        // Test helper: Set up VM with a specific chunk for testing
        void setupTestChunk(const Chunk& chunk)
        {
            // Create a temporary function object for testing
            if (frameCount > 0 && frames[0].closure->function != nullptr)
            {
                delete frames[0].closure->function;
            }

            auto* testFunction = new ObjFunction();
            testFunction->chunk = chunk;
            testFunction->name = "test";
            testFunction->arity = 0;

            frameCount = 1;
            frames[0].closure->function = testFunction;
            frames[0].ip = testFunction->chunk.code.data();
            frames[0].slots = stack.data();  // For tests, start at beginning
            currentFrame = &frames[0];
        }

        // Function pointer dispatch methods
        void vm_return(InterpretResult result);
        static void register_builtin_operations();
        static void register_operation(uint8_t opcode, OpHandler handler, const char* name, uint8_t operand_count = 0);
    };

    // Inline implementations for critical performance functions
    inline Value VM::retainValue(const Value& value)
    {
        // Fast path for primitives - no function call overhead
        if (IS_INT(value) || IS_BOOL(value) || IS_FLOAT(value))
            return value;

        // Extract pointer from Value based on type
        void* ptr = nullptr;
        switch(value.type)
        {
            case COMPILER_VAL_OBJ:          ptr = value.as.obj; break;
            case COMPILER_VAL_FUNC:         ptr = value.as.function; break;
            case COMPILER_VAL_CLOSURE:      ptr = value.as.closure; break;
            case COMPILER_VAL_UPVALUE:      ptr = value.as.upvalue; break;
            case COMPILER_VAL_NATIVE:       ptr = value.as.nativeFunc; break;
            case COMPILER_VAL_CLASS:        ptr = value.as.klass; break;
            case COMPILER_VAL_INSTANCE:     ptr = value.as.instance; break;
            case COMPILER_VAL_BOUND_METHOD: ptr = value.as.boundMethod; break;
            default: return value; // Already handled above, but safety
        }

        if (ptr != nullptr)
        {
            // Only increment refcount if this value is already being tracked
            auto it = refCounts.find(ptr);
            if (it != refCounts.end())
            {
                it->second++;

#ifdef DEBUG_RUNTIME_MEMORY
                std::cout << "Retained " << ptr << ", nb: " << it->second << std::endl;
#endif
            }
        }

        return value;
    }

    inline bool VM::releaseValue(const Value& value)
    {
        // Fast path for primitives - no cleanup needed
        if (IS_INT(value) || IS_BOOL(value) || IS_FLOAT(value))
            return false;

        // Extract pointer from Value based on type
        void* ptr = nullptr;
        switch(value.type)
        {
            case COMPILER_VAL_OBJ:          ptr = value.as.obj; break;
            case COMPILER_VAL_FUNC:         ptr = value.as.function; break;
            case COMPILER_VAL_CLOSURE:      ptr = value.as.closure; break;
            case COMPILER_VAL_UPVALUE:      ptr = value.as.upvalue; break;
            case COMPILER_VAL_NATIVE:       ptr = value.as.nativeFunc; break;
            case COMPILER_VAL_CLASS:        ptr = value.as.klass; break;
            case COMPILER_VAL_INSTANCE:     ptr = value.as.instance; break;
            case COMPILER_VAL_BOUND_METHOD: ptr = value.as.boundMethod; break;
            default: return false; // Already handled above
        }

        if (ptr != nullptr)
        {
            auto it = refCounts.find(ptr);
            if (it != refCounts.end())
            {
                it->second--;

                if (it->second <= 0)
                {
#ifdef DEBUG_RUNTIME_MEMORY
                    std::cout << "Releasing object of type " << static_cast<int>(value.type) << " at " << ptr << std::endl;
#endif
                    refCounts.erase(it);
                    return true; // Should delete
                }
            }
        }

        return false; // Don't delete
    }

    inline Value VM::trackNewValue(const Value& value)
    {
        // Fast path for primitives - no tracking needed
        if (IS_INT(value) || IS_BOOL(value) || IS_FLOAT(value))
            return value;

        // For newly created objects, start with refcount=1
        void* ptr = nullptr;
        switch(value.type)
        {
            case COMPILER_VAL_OBJ:          ptr = value.as.obj; break;
            case COMPILER_VAL_FUNC:         ptr = value.as.function; break;
            case COMPILER_VAL_CLOSURE:      ptr = value.as.closure; break;
            case COMPILER_VAL_UPVALUE:      ptr = value.as.upvalue; break;
            case COMPILER_VAL_NATIVE:       ptr = value.as.nativeFunc; break;
            case COMPILER_VAL_CLASS:        ptr = value.as.klass; break;
            case COMPILER_VAL_INSTANCE:     ptr = value.as.instance; break;
            case COMPILER_VAL_BOUND_METHOD: ptr = value.as.boundMethod; break;
            default: return value; // Already handled above
        }

#ifdef DEBUG_RUNTIME_MEMORY
        std::cout << "Tracking new object of type " << static_cast<int>(value.type) << " at " << ptr << std::endl;
#endif

        if (ptr != nullptr)
        {
            refCounts[ptr] = 1; // Start with refcount=1
        }

        return value;
    }
}