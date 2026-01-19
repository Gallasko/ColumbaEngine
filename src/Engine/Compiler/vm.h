#pragma once

#include "chunk.h"

#include "Interpreter/lexer.h"

#include "logger.h"

#include "bytecode_pass.h"

#include "value_nanbox.h"  // NaN-boxed value representation
#include "vmpools.h"       // Pool-based memory management
#include "object.h"
#include "native_module.h"
#include "vm_profiler.h"   // Bytecode profiling

#include <stack>
#include <functional>
#include <map>
#include <unordered_map>

#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <setjmp.h>

#ifdef DEBUG_RUNTIME_MEMORY
#include <iostream>
#endif

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

        OpCodeInfo() : handler(nullptr) {}
        OpCodeInfo(OpHandler h) : handler(h) {}
    };

    // Forward declare VM for helper functions
    struct VM;

    bool isValueNumber(const Value& val, VM* vm = nullptr);

    bool isValueTrue(const Value& val, VM* vm = nullptr);

    class IndexableStack
    {
    private:
        static constexpr size_t MAX_STACK_SIZE = FRAMES_MAX * 4096; // Callstack * 4K elements max
        // Now using 8-byte values directly (2.1 MB vs 4.2 MB before!)
        alignas(uint64_t) uint64_t stack_values[MAX_STACK_SIZE];
        size_t stack_top = 0;

    public:
        // Ultra-fast Value operations - single 64-bit MOV instruction
        inline void push(Value value)  // Pass by value, not reference (64-bit fits in register)
        {
            if (stack_top >= MAX_STACK_SIZE)
                throw std::runtime_error("Stack overflow");

            stack_values[stack_top++] = value;
        }

        inline Value pop()
        {
            if (stack_top == 0)
                throw std::runtime_error("Trying to pop on an empty stack");

            return stack_values[--stack_top];  // Single instruction!
        }

        inline void changeTop(Value value)
        {
            if (stack_top == 0)
                throw std::runtime_error("Trying to change top on an empty stack");

            stack_values[stack_top - 1] = value;
        }

        inline Value& operator[](size_t index) { return stack_values[index]; }
        inline const Value& operator[](size_t index) const { return stack_values[index]; }

        inline Value top() const
        {
            if (stack_top == 0)
                throw std::runtime_error("Stack is empty");

            return stack_values[stack_top - 1];
        }

        inline bool empty() const { return stack_top == 0; }
        inline size_t size() const { return stack_top; }

        // Get pointer to stack data for frame slots
        inline Value* data() { return stack_values; }
        inline const Value* data() const { return stack_values; }

        void clear()
        {
            // Clean up any heap-allocated objects
            while (stack_top > 0)
            {
                pop();
            }
        }
    };

    struct VM
    {
        VM();

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

            // Destroy all remaining objects in pools (including constants)
            // This is necessary to properly cleanup objects with complex destructors like ElementType
            pools.stringPool.destroyAll();
            pools.closurePool.destroyAll();
            pools.functionPool.destroyAll();
            pools.upvaluePool.destroyAll();
            pools.classPool.destroyAll();
            pools.nativeFuncPool.destroyAll();
            pools.instancePool.destroyAll();
            pools.boundMethodPool.destroyAll();
            pools.vectorPool.destroyAll();

            // Clear the interned strings map
            pools.internedStrings.clear();
        }

        void reset()
        {
            // Free all Values stored in globals before destruction
            for (auto& pair : globals)
            {
                releaseAndDelete(pair.second);
            }

            globals.clear();

            // Clean up any remaining Values on the stack
            while (!stack.empty())
            {
                auto value = stack.pop();
                releaseAndDelete(value);
            }

            // Clear call frames to avoid dangling pointers to freed chunks
            frameCount = 0;
            currentFrame = nullptr;

            // Initialize function pointer dispatch table
            register_builtin_operations();

            // Initialize built-in classes (like Table)
            initialize_builtin_classes();

            for (auto [name, fun] : registeredNativeFunctions)
            {
                defineNative(name, fun);
            }

            // Note: Native modules are registered but not auto-loaded into global scope
            // They will be loaded when explicitly imported via: import moduleName
        }

        InterpretResult interpretFromText(const std::string& source, bool compileOnly = false, const std::string& dumpByteCode = "")
        {
            currentFileName = "text_source";

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

            return interpret(tokens, compileOnly, dumpByteCode);
        }

        InterpretResult interpretFromFile(const std::string& filename, bool compileOnly = false, const std::string& dumpByteCode = "")
        {
            currentFileName = filename;

            Lexer lexer;

            try
            {
                lexer.readFromFile(filename);
            }
            catch (const std::exception& e)
            {
                LOG_ERROR("VM", e.what());
                return InterpretResult::COMPILE_ERROR;
            }

            auto tokens = lexer.getTokens();

            return interpret(tokens, compileOnly, dumpByteCode);
        }

        InterpretResult interpret(const std::queue<Token>& tokens, bool compileOnly = false, const std::string& dumpByteCode = "");

        InterpretResult interpretFromBytecodeFile(const std::string& filename);

        InterpretResult interpretFromCachedBytecode(const std::vector<char>& cachedBytecode, int argCount = 0);

        InterpretResult run();

        // Core Value operations for performance
        inline void push(Value value)  // Pass by value (64-bit in register)
        {
            stack.push(value);
        }

        inline void push(const ElementType& value)
        {
            Value val = elementToValue(value);
            // Track if it's a heap object
            if (requiresRefCount(val)) {
                val = trackNewValue(val);
            }
            stack.push(val);
        }

        inline void changeTop(Value value)
        {
            Value val = stack.top();

            if (requiresRefCount(val))
            {
                // Release old top value
                releaseAndDelete(val);
            }

            stack.changeTop(value);
        }

        inline Value pop()
        {
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

        size_t getTotalTrackedObjects() const
        {
            return pools.stringPool.getNbElements() +
                   pools.closurePool.getNbElements() +
                   pools.functionPool.getNbElements() +
                   pools.upvaluePool.getNbElements() +
                   pools.classPool.getNbElements() +
                   pools.nativeFuncPool.getNbElements() +
                   pools.instancePool.getNbElements() +
                   pools.boundMethodPool.getNbElements();
        }

        // Convenience method for release + delete
        void releaseAndDelete(const Value& value);

        // ====================================================================
        // Pool Access Helpers - Convenient wrappers for vm->pools.getXXX()
        // ====================================================================

        // Get heap objects from pools (returns pointer to actual object)
        inline ElementType* asStringPtr(Value v)      { return pools.getString(v); }
        inline Closure* asClosure(Value v)            { return pools.getClosure(v); }
        inline ObjFunction* asFunction(Value v)       { return pools.getFunction(v); }
        inline ObjUpvalue* asUpvalue(Value v)         { return pools.getUpvalue(v); }
        inline Klass* asClass(Value v)                { return pools.getClass(v); }
        inline NativeFunction* asNativeFunc(Value v)  { return pools.getNativeFunc(v); }
        inline ObjInstance* asInstance(Value v)       { return pools.getInstance(v); }
        inline ObjBoundMethod* asBoundMethod(Value v) { return pools.getBoundMethod(v); }
        inline ObjVector* asVector(Value v)           { return pools.getVector(v); }

        // Helper to get string content from either long or small strings
        inline std::string asString(Value v) {
            return IS_SMALL_STRING(v) ? AS_SMALL_STRING(v) : asStringPtr(v)->toString();
        }

        // Create new heap objects and return tracked Values
        Value createString(const ElementType& element);
        Value createClosure(ObjFunction* function);
        Value createFunction();
        Value createUpvalue(Value* slot);
        Value createClass(const std::string& name);
        Value createInstance(Klass* klass);
        Value createBoundMethod(const Value& receiver, Closure* method);
        Value createVector();

        // Convert between Value and ElementType
        Value elementToValue(const ElementType& element);
        ElementType valueToElement(const Value& value);

        // Value utilities
        Value copyValue(const Value& value);
        int getValueAsInt(const Value& value);

        // Arithmetic operations with proper reference tracking
        Value addValues(const Value a, const Value b);
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

        // Native module registry (per VM instance)
        struct NativeModuleData {
            std::map<std::string, NativeFn> functions;
            std::map<std::string, ElementType> variables;
        };

        std::unordered_map<std::string, NativeModuleData> nativeModules;

        // Function pointer dispatch system
        static OpCodeInfo operations[256];
        jmp_buf exit_jump;
        InterpretResult exit_result;

        // Pool-based memory management (replaces old pointer-based refCounts)
        VMPools pools;

        // Test output buffer for __dprint (used in tests)
        std::string testOutput;

        // Bytecode optimization
        PassManager passManager;
        bool enableOptimizations = true;

        // Bytecode profiling
        VMProfiler profiler;

        // Single-character string cache (optimization for string indexing)
        Value singleCharCache[256];

        ObjUpvalue* openUpvalues = nullptr;

        // Optimization control methods
        inline void enableBytecodeOptimization()
        {
            enableOptimizations = true;
        }

        inline void disableBytecodeOptimization()
        {
            enableOptimizations = false;
        }

        inline void enableOptimizationDebugging()
        {
            passManager.setDebugOutput(true);
        }

        inline void disableOptimizationDebugging()
        {
            passManager.setDebugOutput(false);
        }

        // Profiler control methods
        inline void enableProfiling()
        {
            profiler.setEnabled(true);
            profiler.reset();
            LOG_INFO("VM", "Bytecode profiling enabled");
        }

        inline void disableProfiling()
        {
            profiler.setEnabled(false);
            LOG_INFO("VM", "Bytecode profiling disabled");
        }

        inline void resetProfiling()
        {
            profiler.reset();
        }

        inline void printProfilingReport(bool sortByTime = true)
        {
            profiler.printReport(sortByTime);
        }

        inline void printProfilingBytecodeReport()
        {
            profiler.printBytecodeReport();
        }

        inline void listOptimizationPasses() const
        {
            passManager.listPasses();
        }

        inline void addOptimizationPass(std::unique_ptr<BytecodePass> pass)
        {
            passManager.addPass(std::move(pass));
        }

        inline void registerNative(const std::string& name, NativeFn function)
        {
            registeredNativeFunctions[name] = function;

            defineNative(name, function);
        }

        /**
         * @brief Create a native function Value from a lambda without registering it globally
         *
         * This is a helper function that encapsulates the boilerplate of:
         * 1. Allocating from the native function pool with correct index
         * 2. Setting the function pointer
         * 3. Creating the NaN-boxed Value
         * 4. Tracking the value for reference counting
         *
         * Use this when you want to add native functions directly to table fields
         * without polluting the global namespace.
         *
         * @param function The native function lambda
         * @return Value The tracked native function Value ready to be added to a table
         *
         * @example
         * NativeFn myLambda = [component](VM*, int argCount, Value* args) -> Value { ... };
         * Value funcValue = vm->createNativeFunction(myLambda);
         * table->fields["myMethod"] = funcValue;
         */
        Value createNativeFunction(NativeFn function)
        {
            auto [nativeFunc, index] = pools.nativeFuncPool.allocateWithIndex();
            nativeFunc->function = function;

            Value val = makeNativeFuncValue(static_cast<uint32_t>(index));

            return trackNewValue(val);
        }

        void defineNative(const std::string& name, NativeFn function)
        {
            // Skip if already defined in globals
            if (globals.find(name) != globals.end())
            {
                return;
            }

            globals[name] = createNativeFunction(function);
        }

        // Native module system - per VM instance
        void addNativeModule(const std::string& moduleName, const NativeModule& moduleData)
        {
            NativeModuleData data;
            data.functions = moduleData.exportedFunctions;
            data.variables = moduleData.exportedVariables;
            nativeModules[moduleName] = data;

            // Note: Module is registered but not loaded into global scope
            // It will be loaded when explicitly imported via: import moduleName
        }

        bool loadNativeModule(const std::string& moduleName)
        {
            auto it = nativeModules.find(moduleName);
            if (it == nativeModules.end())
            {
                return false;
            }

            // Define all native functions from the module into this VM's globals
            for (const auto& [name, func] : it->second.functions)
            {
                defineNative(name, func);
            }

            // Define all native variables from the module into this VM's globals
            for (const auto& [name, value] : it->second.variables)
            {
                // Skip if already defined in globals
                if (globals.find(name) == globals.end())
                {
                    globals[name] = elementToValue(value);
                }
            }

            return true;
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

        // Helper methods for interpreting bytecode
        InterpretResult executeChunk(ObjFunction* funcObj, int argCount);
        void cleanupFunction(ObjFunction* funcObj);

        // Function pointer dispatch methods
        void vm_return(InterpretResult result);
        static void register_builtin_operations();
        static void register_operation(uint8_t opcode, OpHandler handler);
        void initialize_builtin_classes();

        std::string currentFileName;

        std::unordered_map<std::string, NativeFn> registeredNativeFunctions;
    };

    // Inline implementations for critical performance functions
    inline Value VM::retainValue(const Value& v)
    {
        // Fast path: primitives and doubles don't need refcounting
        if (!requiresRefCount(v))
            return v;

        // Constants are never ref-counted (they live forever in the constant table)
        if (pools.isConstant(v))
            return v;

        // Increment refcount in appropriate pool vector
        uint32_t index = GET_INDEX(v);
        auto& refCounts = pools.getRefCountVector(v);

        if (index < refCounts.size()) {
            refCounts[index]++;

#ifdef DEBUG_RUNTIME_MEMORY
            std::cout << "Retained " << valueTypeName(v) << "[" << index << "], count: " << refCounts[index] << std::endl;
#endif
        }

        return v;
    }

    inline bool VM::releaseValue(const Value& v)
    {
        // Fast path: primitives and doubles don't need cleanup
        if (!requiresRefCount(v))
            return false;

        // Constants are never released (they live forever in the constant table)
        if (pools.isConstant(v))
            return false;

        // Decrement refcount in appropriate pool vector
        uint32_t index = GET_INDEX(v);
        auto& refCounts = pools.getRefCountVector(v);

        if (index >= refCounts.size() || refCounts[index] == 0)
            return false;

        refCounts[index]--;

        if (refCounts[index] == 0) {
#ifdef DEBUG_RUNTIME_MEMORY
            std::cout << "Releasing " << valueTypeName(v) << "[" << index << "]" << std::endl;
#endif
            return true; // Should delete - refcount reached zero
        }

        return false; // Don't delete - still has references
    }

    inline Value VM::trackNewValue(const Value& v)
    {
        // Fast path: primitives and doubles don't need tracking
        if (!requiresRefCount(v))
            return v;

        // For newly created objects, start with refcount=1
        uint32_t index = GET_INDEX(v);
        pools.ensureRefCountCapacity(v, index);
        pools.getRefCountVector(v)[index] = 1;

#ifdef DEBUG_RUNTIME_MEMORY
        std::cout << "Tracking new " << valueTypeName(v) << "[" << index << "]" << std::endl;
#endif

        return v;
    }
}