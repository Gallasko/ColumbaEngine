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
#include <cassert>
#include <setjmp.h>

#include "ECS/uniqueid.h"

#ifdef DEBUG_RUNTIME_MEMORY
#include <iostream>
#endif

#define EMIT_RUNTIME_ERROR(msg) do {runtimeError((Strfy() << msg).getData()); return InterpretResult::RUNTIME_ERROR;} while(0);

namespace pg
{
    static constexpr size_t FRAMES_MAX = 64;

    // Forward declaration for VM
    struct VM;
    struct DecodedInstruction;
    struct DecodedChunk;

    // Function pointer type for operation handlers
    typedef void (*OpHandler)(VM* vm);
    typedef void (*OpDecodedHandler)(VM* vm, const DecodedInstruction& instr);

    // Operation information structure
    struct OpCodeInfo
    {
        OpHandler handler;
        OpDecodedHandler decodedHandler = nullptr;

        // NEW: Metadata for pre-decoding and batching optimization
        uint8_t flags;           // Instruction properties
        uint8_t operandBytes;    // Number of operand bytes (0-4) - inferred from getInstructionSize if 0
        int8_t stackEffect;      // Net stack change (-128 to +127) - not used yet

        // Flags for instruction properties
        static constexpr uint8_t PURE         = 0x01;  // No side effects
        static constexpr uint8_t CONST_TIME   = 0x02;  // Always same execution time
        static constexpr uint8_t NO_CALL      = 0x04;  // Doesn't call functions
        static constexpr uint8_t NO_BRANCH    = 0x08;  // Doesn't change control flow
        static constexpr uint8_t NO_MEMORY    = 0x10;  // Doesn't allocate/free memory
        static constexpr uint8_t BATCHABLE    = 0x20;  // Safe for batch execution
        static constexpr uint8_t LOCAL_ONLY   = 0x40;  // Only touches locals/stack

        // Common flag combinations
        static constexpr uint8_t PURE_BATCH   = PURE | BATCHABLE;  // Pure and batchable (most common)

        OpCodeInfo() : handler(nullptr), flags(0), operandBytes(0), stackEffect(0) {}
        OpCodeInfo(OpHandler h) : handler(h), flags(0), operandBytes(0), stackEffect(0) {}
        OpCodeInfo(OpHandler h, uint8_t f) : handler(h), flags(f), operandBytes(0), stackEffect(0) {}
        OpCodeInfo(OpHandler h, uint8_t f, OpDecodedHandler dh) : handler(h), decodedHandler(dh), flags(f), operandBytes(0), stackEffect(0) {}

        bool isPure() const { return (flags & PURE) != 0; }
        bool isBatchable() const { return (flags & BATCHABLE) != 0; }
        bool isConstTime() const { return (flags & CONST_TIME) != 0; }
        bool noBranch() const { return (flags & NO_BRANCH) != 0; }
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
        // Ultra-fast Value operations - single 64-bit MOV instruction.
        // The bounds/empty checks are debug-only: every hot opcode pushes
        // and pops, so the cost of an unconditional `if` was real. The
        // compiler is told the unlikely path is unreachable in release;
        // op_call validates stack headroom at frame entry instead.
        inline void push(Value value)  // Pass by value, not reference (64-bit fits in register)
        {
            assert(stack_top < MAX_STACK_SIZE and "Stack overflow");
            stack_values[stack_top++] = value;
        }

        inline Value pop()
        {
            assert(stack_top > 0 and "Trying to pop on an empty stack");
            return stack_values[--stack_top];
        }

        inline void changeTop(Value value)
        {
            assert(stack_top > 0 and "Trying to change top on an empty stack");
            stack_values[stack_top - 1] = value;
        }

        // Bulk truncate to a given depth. Caller is responsible for
        // releasing any refcounted Values in [newTop, stack_top) BEFORE
        // calling this.
        inline void truncateTo(size_t newTop)
        {
            assert(newTop <= stack_top);
            stack_top = newTop;
        }

        inline Value& operator[](size_t index) { return stack_values[index]; }
        inline const Value& operator[](size_t index) const { return stack_values[index]; }

        inline Value top() const
        {
            assert(stack_top > 0 and "Stack is empty");
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

    // Forward declarations for operation handlers
    void op_return(VM* vm);
    void op_return_decoded(VM* vm, const DecodedInstruction& instr);
    void op_constant(VM* vm);
    void op_constant_decoded(VM* vm, const DecodedInstruction& instr);
    void op_long_constant(VM* vm);
    void op_long_constant_decoded(VM* vm, const DecodedInstruction& instr);
    void op_add(VM* vm);
    void op_add_decoded(VM* vm, const DecodedInstruction& instr);
    void op_subtract(VM* vm);
    void op_subtract_decoded(VM* vm, const DecodedInstruction& instr);
    void op_multiply(VM* vm);
    void op_multiply_decoded(VM* vm, const DecodedInstruction& instr);
    void op_divide(VM* vm);
    void op_divide_decoded(VM* vm, const DecodedInstruction& instr);
    void op_modulo(VM* vm);
    void op_modulo_decoded(VM* vm, const DecodedInstruction& instr);
    void op_negate(VM* vm);
    void op_equal(VM* vm);
    void op_equal_decoded(VM* vm, const DecodedInstruction& instr);
    void op_not_equal(VM* vm);
    void op_not_equal_decoded(VM* vm, const DecodedInstruction& instr);
    void op_greater(VM* vm);
    void op_greater_decoded(VM* vm, const DecodedInstruction& instr);
    void op_greater_equal(VM* vm);
    void op_greater_equal_decoded(VM* vm, const DecodedInstruction& instr);
    void op_less(VM* vm);
    void op_less_decoded(VM* vm, const DecodedInstruction& instr);
    void op_less_equal(VM* vm);
    void op_less_equal_decoded(VM* vm, const DecodedInstruction& instr);
    void op_true(VM* vm);
    void op_false(VM* vm);
    void op_not(VM* vm);
    void op_and(VM* vm);
    void op_or(VM* vm);
    void op_pop(VM* vm);
    void op_get_local(VM* vm);
    void op_get_local_decoded(VM* vm, const DecodedInstruction& instr);
    void op_set_local(VM* vm);
    void op_set_local_decoded(VM* vm, const DecodedInstruction& instr);
    void op_get_global(VM* vm);
    void op_define_global(VM* vm);
    void op_set_global(VM* vm);
    void op_jump_if_false(VM* vm);
    void op_long_jump_if_false(VM* vm);
    void op_jump_if_false_popping(VM* vm);
    void op_long_jump_if_false_popping(VM* vm);
    void op_jump_if_false_decoded(VM* vm, const DecodedInstruction& instr);
    void op_jump_if_false_popping_decoded(VM* vm, const DecodedInstruction& instr);
    void op_long_jump_if_false_decoded(VM* vm, const DecodedInstruction& instr);
    void op_long_jump_if_false_popping_decoded(VM* vm, const DecodedInstruction& instr);
    void op_jump(VM* vm);
    void op_long_jump(VM* vm);
    void op_jump_decoded(VM* vm, const DecodedInstruction& instr);
    void op_loop(VM* vm);
    void op_long_loop(VM* vm);
    void op_loop_decoded(VM* vm, const DecodedInstruction& instr);
    void op_call(VM* vm);
    void op_call_decoded(VM* vm, const DecodedInstruction& instr);
    void op_invoke(VM* vm);
    void op_invoke_decoded(VM* vm, const DecodedInstruction& instr);
    void op_closure(VM* vm);
    void op_get_upvalue(VM* vm);
    void op_set_upvalue(VM* vm);
    void op_close_upvalue(VM* vm);
    void op_debug_print(VM* vm);
    void op_post_incr_global(VM* vm);
    void op_incr_global(VM* vm);
    void op_post_decr_global(VM* vm);
    void op_decr_global(VM* vm);
    void op_post_incr_local(VM* vm);
    void op_incr_local(VM* vm);
    void op_post_decr_local(VM* vm);
    void op_decr_local(VM* vm);
    void op_class(VM* vm);
    void op_get_property(VM* vm);
    void op_set_property(VM* vm);
    void op_method(VM* vm);
    void op_short_int(VM* vm);
    void op_short_int_decoded(VM* vm, const DecodedInstruction& instr);

    void op_pop_n(VM* vm);

    void op_define_constant_global(VM* vm);
    void op_define_constant_global_decoded(VM* vm, const DecodedInstruction& instr);
    void op_get_constant_global(VM* vm);
    void op_set_constant_global(VM* vm);

    void op_add_ll(VM* vm);
    void op_add_ll_decoded(VM* vm, const DecodedInstruction& instr);
    void op_subtract_ll(VM* vm);

    void op_subtract_lc(VM* vm);
    void op_subtract_cl(VM* vm);

    void op_less_equal_ll(VM* vm);
    void op_less_equal_ll_decoded(VM* vm, const DecodedInstruction& instr);
    void op_less_ll(VM* vm);
    void op_less_ll_decoded(VM* vm, const DecodedInstruction& instr);
    void op_set_local_pop(VM* vm);
    void op_set_local_pop_decoded(VM* vm, const DecodedInstruction& instr);

    // Table operations
    void op_build_vector(VM* vm);
    void op_build_table(VM* vm);
    void op_get_index(VM* vm);
    void op_set_index(VM* vm);

    // Iterator operations
    void op_get_iterator(VM* vm);
    void op_iterator_next(VM* vm);
    void op_table_size(VM* vm);
    void op_table_at(VM* vm);

    void op_define_global_non_popping(VM *vm);

    // Module operations
    void op_import(VM* vm);

    // Register-based operations
    void op_load_constant_r(VM* vm);
    void op_move_r(VM* vm);
    void op_add_rrr(VM* vm);
    void op_less_rr(VM* vm);
    void op_incr_r(VM* vm);
    void op_less_rrr(VM* vm);
    void op_jump_if_false_r(VM* vm);

    struct VM
    {
        VM();

        // Destructor to properly clean up globals map and stack
        ~VM();

        void reset()
        {
            // Free all Values stored in globals before destruction
            for (auto& pair : globals)
            {
                releaseAndDelete(pair.second);
            }

            globals.clear();

            // Clean up any remaining Values on the stack
            while (not stack.empty())
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
        InterpretResult runDecoded(DecodedChunk *decoded);  // Execute from pre-decoded chunks (faster)

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
            if (currentFrame and currentFrame->closure)
            {
                auto& chunk = currentFrame->closure->function->chunk.code;
                chunkData = chunk.data();
                chunkDataEnd = chunk.data() + chunk.size();
            }
        }

        inline void resetStack()
        {
            // Use VM's reference counting instead of IndexableStack's clear()
            while (not stack.empty())
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

        // Variant for the decoded execution path: parks caller's ip past
        // the supplied call instruction so a future OP_Return can map back
        // to the resume index. If a new frame is pushed (closure /
        // bound-method / class-init call), updates currentFrame,
        // currentStartingIp, currentDecoded, and nextInstructionIndex so the
        // runDecoded dispatcher can keep going without the post-dispatch
        // opcode ladder. Native calls leave decoded state untouched. Sets
        // wantsLegacyFallback if the called frame has no decoded chunk.
        bool callValueDecoded(const Value& callee, int argCount, const DecodedInstruction& callInstr);

        // Park caller's ip past a triggering instruction (OP_Call, OP_Invoke,
        // OP_Get/Set_Property, OP_Get/Set_Index) on the decoded path. After
        // running the call, completeDecodedFrameSwitch() retargets dispatcher
        // state when a new frame was pushed.
        void parkIpForDecodedCall(const DecodedInstruction& callInstr);

        // If a new frame was pushed since frameCountBefore (closure /
        // bound-method / class-init invocation), retarget currentDecoded /
        // currentStartingIp / nextInstructionIndex to the callee. Sets
        // wantsLegacyFallback if the callee lacks a decoded chunk. Native
        // calls (no frame change) leave state untouched.
        void completeDecodedFrameSwitch(int frameCountBefore);

        bool callMethod(Klass* receiver, const std::string& methodName, int argCount);

        bool call(Closure* closure, int argCount);
        bool callBound(Closure* closure, int argCount);

        // Reference counting methods
        inline const Value& retainValue(const Value& value);   // Returns the value after retaining
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

        // Convenience method for release + delete. Inline: the call sites are
        // hot (op_set_local, BINARY_OP_TEMPLATE, etc.) and the primitive path
        // costs one releaseValue check (which itself early-returns on
        // !requiresRefCount). deleteValue is the only non-inline body and
        // only fires when refcount drops to zero, so leaving it out-of-line
        // keeps the cold path from bloating the dispatcher.
        inline void releaseAndDelete(const Value& value)
        {
            if (releaseValue(value))
                deleteValue(value);
        }

        // ====================================================================
        // Pool Access Helpers - Convenient wrappers for vm->pools.getXXX()
        // ====================================================================

        // Get heap objects from pools (returns pointer to actual object)
        inline std::string* asStringPtr(Value v)      { return pools.getString(v); }
        inline Closure* asClosure(Value v)            { return pools.getClosure(v); }
        inline ObjFunction* asFunction(Value v)       { return pools.getFunction(v); }
        inline ObjUpvalue* asUpvalue(Value v)         { return pools.getUpvalue(v); }
        inline Klass* asClass(Value v)                { return pools.getClass(v); }
        inline NativeFunction* asNativeFunc(Value v)  { return pools.getNativeFunc(v); }
        inline ObjInstance* asInstance(Value v)       { return pools.getInstance(v); }
        inline ObjBoundMethod* asBoundMethod(Value v) { return pools.getBoundMethod(v); }
        inline ObjVector* asVector(Value v)           { return pools.getVector(v); }

        template <typename Type>
        inline Type* asCustomPtr(Value v)
        {
            return static_cast<Type*>(pools.getCustomPointer(v));
        }

        // Helper to get string content from long, small, or interned strings
        inline std::string asString(Value v)
        {
            if (IS_SMALL_STRING(v))
                return AS_SMALL_STRING(v);
            else if (IS_INTERNED_STRING(v))
            {
                uint32_t index = AS_INTERNED_STRING_INDEX(v);
                return currentFrame->closure->function->chunk.constantStrings[index];
            }
            else
                return *asStringPtr(v);
        }

        // Overload for explicit chunk specification
        inline std::string asString(Value v, const Chunk& chunk)
        {
            if (IS_SMALL_STRING(v))
                return AS_SMALL_STRING(v);
            else if (IS_INTERNED_STRING(v))
            {
                // Get the string from the specified chunk's constantStrings
                uint32_t index = AS_INTERNED_STRING_INDEX(v);
                return chunk.constantStrings[index];
            }
            else
                return *asStringPtr(v);
        }

        // Create new heap objects and return tracked Values
        Value createString(const ElementType& element);
        Value createString(const std::string& stringContent);
        Value createString(const char* cstr) { return createString(std::string(cstr)); }
        Value createClosure(ObjFunction* function);
        Value createFunction();
        Value createUpvalue(Value* slot);
        Value createClass(const std::string& name);
        Value createInstance(Klass* klass);
        Value createBoundMethod(const Value& receiver, Closure* method);
        Value createVector();

        template <typename Type>
        Value createCustomPtr(Type* ptr)
        {
            const auto& id = getTypeId<Type>();

            auto& pool = pools.customPointerPool[id];
            pool.push_back(static_cast<void*>(ptr));

            uint32_t index = static_cast<uint32_t>(pool.size() - 1);

            Value val = makeCustomPtrValue(id, index);

            return val;
        }

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
        Value moduloValues(const Value& a, const Value& b);
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

        // Dispatcher state for runDecoded. Decoded handlers may mutate these
        // to direct control flow without going through the post-dispatch
        // opcode ladder. Owned by the active runDecoded frame.
        DecodedChunk *currentDecoded        = nullptr;
        uint8_t      *currentStartingIp     = nullptr;
        size_t        nextInstructionIndex  = 0;
        bool          wantsLegacyFallback   = false;

        /* The stack of the VM */
        IndexableStack stack;

        std::unordered_map<std::string, Value> globals;

        // Native module registry (per VM instance)
        struct NativeModuleData
        {
            std::map<std::string, NativeFn> functions;
            std::map<std::string, ElementType> variables;
            std::function<void(VM*)> init;
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

        inline void printBytecodeWithPerformance(const std::string& functionName) const
        {
            profiler.printBytecodeWithPerformance(functionName);
        }

        void printAllFunctionsBytecodeWithPerformance();

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

        /**
         * @brief Add a native method to a class
         *
         * This helper function adds a C++ native function as a method to a class.
         * The native function receives the receiver instance as args[0].
         *
         * @param classValue The class Value to add the method to
         * @param methodName The name of the method
         * @param function The native function implementation
         *
         * @example
         * Value myClass = vm->createClass("MyClass");
         * vm->addNativeMethod(myClass, "greet", [](VM* vm, int argCount, Value* args) -> Value {
         *     // args[0] is the receiver (instance)
         *     // args[1..n] are the method arguments
         *     ObjInstance* instance = vm->asInstance(args[0]);
         *
         *     if (argCount > 1) {
         *         std::string name = vm->asString(args[1]);
         *         std::cout << "Hello, " << name << "!" << std::endl;
         *     }
         *
         *     return makeIntValue(-1); // Return value
         * });
         *
         * // Usage in script:
         * // var obj = MyClass();
         * // obj.greet("World");  // argCount=2: args[0]=obj, args[1]="World"
         */
        void addNativeMethod(Value classValue, const std::string& methodName, NativeFn function)
        {
            if (not IS_CLASS(classValue))
            {
                throw std::runtime_error("addNativeMethod: first argument must be a class");
            }

            Klass* klass = asClass(classValue);
            Value nativeFunc = createNativeFunction(function);
            klass->methods[methodName] = nativeFunc;
        }

        // Native module system - per VM instance
        template<typename T>
        void addNativeModule(const std::string& moduleName, const T& moduleData)
        {
            static_assert(std::is_base_of<NativeModule, T>::value, "Module must derive from NativeModule");

            NativeModuleData data;
            data.functions = moduleData.exportedFunctions;
            data.variables = moduleData.exportedVariables;
            // Capture the full derived type, not just the base class
            data.init = [moduleData](VM* vm) { moduleData.init(vm); };
            nativeModules[moduleName] = data;

            // Note: Module is registered but not loaded into global scope
            // It will be loaded when explicitly imported via: import moduleName
        }

        bool loadNativeModule(const std::string& moduleName)
        {
            auto it = nativeModules.find(moduleName);
            if (it == nativeModules.end())
            {
                LOG_WARNING("VM", "Native module '" << moduleName << "' not found");
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

            it->second.init(this);

            return true;
        }

        template <typename Type>
        _unique_id getGlobalGenericId() const noexcept
        {
            static const _unique_id id = globalIdGenerator.generateId();
            return id;
        }

        template <typename Type>
        _unique_id getTypeId() const noexcept
        {
            auto globalId = getGlobalGenericId<Type>();

            auto it = idMap.find(globalId);

            // Todo add a variable to keep track of the running state of the ECS
            if (it == idMap.end())
            {
                LOG_MILE("ID", "Generating a new id (in compiler) for " << typeid(Type).name());

                return idMap[globalId] = idGenerator.generateId();
            }

            return it->second;

            // This can't work as the static make this id the same through all the different object
            // static const _unique_id id = idGenerator.generateId();
            // return id;
        }

        static UniqueIdGenerator globalIdGenerator;
        mutable UniqueIdGenerator idGenerator;
        mutable std::unordered_map<_unique_id, _unique_id> idMap;

        // Test helper: Set up VM with a specific chunk for testing
        void setupTestChunk(const Chunk& chunk)
        {
            // Create a temporary function object for testing
            auto* testFunction = new ObjFunction();
            testFunction->chunk = chunk;
            testFunction->name = "test";
            testFunction->arity = 0;

            // Create a closure for the function
            Value closureValue = createClosure(testFunction);
            Closure* closure = asClosure(closureValue);

            // Push the closure onto the stack (this is what the VM normally does)
            push(closureValue);

            // Set up the first frame
            frameCount = 1;
            frames[0].closure = closure;
            frames[0].ip = testFunction->chunk.code.data();
            // slots should point PAST the closure (where local variables start)
            frames[0].slots = stack.data() + 1;  // Skip the closure at stack[0]
            frames[0].stackBase = stack.data();
            currentFrame = &frames[0];

            // Update chunk data cache
            updateChunkCache();
        }

        // Helper methods for interpreting bytecode
        InterpretResult executeChunk(ObjFunction* funcObj, int argCount);
        void cleanupFunction(ObjFunction* funcObj);

        // Function pointer dispatch methods
        void vm_return(InterpretResult result);
        static void register_builtin_operations();
        static void register_operation(uint8_t opcode, OpHandler handler);
        static void register_operation(uint8_t opcode, OpHandler handler, uint8_t flags);
        static void register_operation(uint8_t opcode, OpHandler handler, OpDecodedHandler decodedHandler, uint8_t flags = 0);
        void initialize_builtin_classes();

        std::string currentFileName;

        std::unordered_map<std::string, NativeFn> registeredNativeFunctions;

        // Track all compiled functions for profiling
        std::vector<ObjFunction*> compiledFunctions;
    };

    // Inline implementations for critical performance functions
    inline const Value& VM::retainValue(const Value& v)
    {
        // Fast path: primitives and doubles don't need refcounting
        if (not requiresRefCount(v))
            return v;

        // Constants are never ref-counted (they live forever in the constant table)
        if (pools.isConstant(v))
            return v;

        // Increment refcount in appropriate pool vector
        uint32_t index = GET_INDEX(v);
        auto& refCounts = pools.getRefCountVector(v);

        if (index < refCounts.size())
        {
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
        if (not requiresRefCount(v))
            return false;

        // Constants are never released (they live forever in the constant table)
        if (pools.isConstant(v))
            return false;

        // Decrement refcount in appropriate pool vector
        uint32_t index = GET_INDEX(v);
        auto& refCounts = pools.getRefCountVector(v);

        if (index >= refCounts.size() or refCounts[index] == 0)
            return false;

        refCounts[index]--;

        if (refCounts[index] == 0)
        {
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
        if (not requiresRefCount(v))
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