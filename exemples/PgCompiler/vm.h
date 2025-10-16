#pragma once

#include "chunk.h"

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
            // Note: No cleanup here - caller is responsible for managing Value lifecycle

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
            Value val = elementToValue(value);
            // Track if it's a heap object
            if (IS_OBJ(val) || IS_FUNC(val) || IS_CLOSURE(val) || IS_UPVALUE(val) || IS_NAT_FUNC(val)) {
                val = trackNewValue(val);
            }
            stack.push(val);
        }

        Value pop()
        {
#ifdef DEBUG_CHECK_STACK
            if (stack.empty())
                throw std::runtime_error("Trying to pop on an empty stack");
#endif
            return stack.pop();
        }

        const Value& peek(size_t distance = 0) const
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
            if (checkIpAgainstStack(1))
            {
                throw std::runtime_error("Not enough bytes to read uint16.");
            }
#endif
            uint16_t value = (static_cast<uint16_t>(readByte()) << 8);
            value |= static_cast<uint16_t>(readByte());

            return value;
        }

        uint32_t readUint32()
        {
#ifdef DEBUG_CHECK_STACK
            if (checkIpAgainstStack(3))
            {
                throw std::runtime_error("Not enough bytes to read uint32.");
            }
#endif
            uint32_t value = (static_cast<uint32_t>(readByte()) << 24);
            value |= (static_cast<uint32_t>(readByte()) << 16);
            value |= (static_cast<uint32_t>(readByte()) << 8);
            value |= static_cast<uint32_t>(readByte());

            return value;
        }

        inline uint8_t readByte()
        {
            return *(currentFrame->ip++);
        }

        inline bool checkIpAgainstStack(uint8_t ahead)
        {
            return static_cast<size_t>(currentFrame->ip - chunkData) + ahead >= currentFrame->closure->function->chunk.code.size();
        }

        // Update cached chunk data pointer when switching functions
        inline void updateChunkCache()
        {
            if (currentFrame && currentFrame->closure) {
                chunkData = currentFrame->closure->function->chunk.code.data();
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

        bool call(Closure* closure, int argCount);

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

        /* The stack of the VM */
        IndexableStack stack;

        std::unordered_map<std::string, Value> globals;

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
    };

    // Inline implementations for critical performance functions
    inline Value VM::retainValue(const Value& value)
    {
        // Fast path for primitives - no function call overhead
        if (IS_INT(value) || IS_BOOL(value))
            return value;

        // Extract pointer from Value based on type
        void* ptr = nullptr;
        switch(value.type)
        {
            case COMPILER_VAL_OBJ:     ptr = value.as.obj; break;
            case COMPILER_VAL_FUNC:    ptr = value.as.function; break;
            case COMPILER_VAL_CLOSURE: ptr = value.as.closure; break;
            case COMPILER_VAL_UPVALUE: ptr = value.as.upvalue; break;
            case COMPILER_VAL_NATIVE:  ptr = value.as.nativeFunc; break;
            default: return value; // Already handled above, but safety
        }

        if (ptr != nullptr) {
            refCounts[ptr]++;
        }
        return value;
    }

    inline bool VM::releaseValue(const Value& value)
    {
        // Fast path for primitives - no cleanup needed
        if (IS_INT(value) || IS_BOOL(value))
            return false;

        // Extract pointer from Value based on type
        void* ptr = nullptr;
        switch(value.type)
        {
            case COMPILER_VAL_OBJ:     ptr = value.as.obj; break;
            case COMPILER_VAL_FUNC:    ptr = value.as.function; break;
            case COMPILER_VAL_CLOSURE: ptr = value.as.closure; break;
            case COMPILER_VAL_UPVALUE: ptr = value.as.upvalue; break;
            case COMPILER_VAL_NATIVE:  ptr = value.as.nativeFunc; break;
            default: return false; // Already handled above
        }

        if (ptr != nullptr) {
            auto it = refCounts.find(ptr);
            if (it != refCounts.end()) {
                it->second--;
                if (it->second <= 0) {
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
        if (IS_INT(value) || IS_BOOL(value))
            return value;

        // For newly created objects, start with refcount=1
        void* ptr = nullptr;
        switch(value.type) {
            case COMPILER_VAL_OBJ:     ptr = value.as.obj; break;
            case COMPILER_VAL_FUNC:    ptr = value.as.function; break;
            case COMPILER_VAL_CLOSURE: ptr = value.as.closure; break;
            case COMPILER_VAL_UPVALUE: ptr = value.as.upvalue; break;
            case COMPILER_VAL_NATIVE:  ptr = value.as.nativeFunc; break;
            default: return value; // Already handled above
        }

        if (ptr != nullptr) {
            refCounts[ptr] = 1; // Start with refcount=1
        }
        return value;
    }
}