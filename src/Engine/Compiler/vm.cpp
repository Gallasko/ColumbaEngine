#include "stdafx.h"

#include "vm.h"

#include <iostream>

#include "compiler.h"

#include "compiler_debug.h"

#include <chrono>

#include "pgconstant.h"

#include "chunk_serializer.h"

namespace pg
{
    UniqueIdGenerator VM::globalIdGenerator;

    bool isValueNumber(const Value& val, VM* vm)
    {
        if (IS_INT(val) or IS_DOUBLE(val))
            return true;

        // String objects might represent numbers - need pool access
        if (IS_STRING(val) and vm != nullptr)
        {
            ElementType* obj = vm->pools.getString(val);
            return obj->isNumber();
        }

        return false;
    }

    bool isValueTrue(const Value& val, VM* vm)
    {
        if (IS_BOOL(val))
            return AS_BOOL(val);

        if (IS_INT(val))
            return AS_INT(val) != 0;

        if (IS_DOUBLE(val))
        {
            const double a = AS_DOUBLE(val);
            const double b = 0.0;
            const double epsilon = 0.00001;

            return not (std::fabs(a - b) <= epsilon * std::max({1.0, std::fabs(a), std::fabs(b)}));
        }

        // String objects might represent booleans - need pool access
        if (IS_STRING(val) and vm != nullptr)
        {
            ElementType* obj = vm->pools.getString(val);
            return obj->isTrue();
        }

        if (IS_CLOSURE(val) or IS_FUNC(val) or IS_NAT_FUNC(val) or
            IS_CLASS(val) or IS_INSTANCE(val) or IS_BOUND_METHOD(val) or IS_VECTOR(val) or IS_CUSTOM_PTR(val))
        {
            return true; // Non-null objects are true
        }

        return false;
    }

    // Static dispatch table definition
    OpCodeInfo VM::operations[256];

    // Forward declarations for operation handlers
    void op_return(VM* vm);
    void op_constant(VM* vm);
    void op_long_constant(VM* vm);
    void op_add(VM* vm);
    void op_subtract(VM* vm);
    void op_multiply(VM* vm);
    void op_divide(VM* vm);
    void op_modulo(VM* vm);
    void op_negate(VM* vm);
    void op_equal(VM* vm);
    void op_not_equal(VM* vm);
    void op_greater(VM* vm);
    void op_greater_equal(VM* vm);
    void op_less(VM* vm);
    void op_less_equal(VM* vm);
    void op_true(VM* vm);
    void op_false(VM* vm);
    void op_not(VM* vm);
    void op_and(VM* vm);
    void op_or(VM* vm);
    void op_pop(VM* vm);
    void op_get_local(VM* vm);
    void op_set_local(VM* vm);
    void op_get_global(VM* vm);
    void op_define_global(VM* vm);
    void op_set_global(VM* vm);
    void op_jump(VM* vm);
    void op_jump_if_false(VM* vm);
    void op_loop(VM* vm);
    void op_long_jump(VM* vm);
    void op_long_jump_if_false(VM* vm);
    void op_long_loop(VM* vm);
    void op_call(VM* vm);
    void op_invoke(VM* vm);
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

    void op_pop_n(VM* vm);

    void op_define_constant_global(VM* vm);
    void op_get_constant_global(VM* vm);
    void op_set_constant_global(VM* vm);

    void op_add_ll(VM* vm);
    void op_subtract_ll(VM* vm);

    void op_subtract_lc(VM* vm);
    void op_subtract_cl(VM* vm);

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
}

namespace pg
{
    VM::VM()
    {
        // Initialize function pointer dispatch table
        register_builtin_operations();

        // Initialize built-in classes (like Table)
        initialize_builtin_classes();

        // Initialize single-character string cache for performance
        // Pre-allocate all 256 possible single-byte character strings
        for (int i = 0; i < 256; i++)
        {
            std::string singleChar(1, static_cast<char>(i));
            singleCharCache[i] = createString(singleChar);
        }
    }

    InterpretResult VM::interpret(const std::queue<Token>& tokens, bool compileOnly, const std::string& dumpByteCode)
    {
        // Record start time for pre-run profiling (everything before run())
        std::chrono::steady_clock::time_point interpretStart;
        if (profiler.isEnabled())
        {
            interpretStart = std::chrono::steady_clock::now();
        }

        if (tokens.empty())
        {
            LOG_WARNING("VM", "Empty tokens provided, nothing to execute !");
            return InterpretResult::OK;
        }

        // Todo change this
        // Reset the compiler state before compiling a new chunk
        Compiler compiler(this);

        std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();

        auto function = compiler.compile(tokens);

        if (function == 0x0)
            return InterpretResult::COMPILE_ERROR;

        // push(function);  // function is already tracked from compiler

        auto closureValue = createClosure(asFunction(function));
        Closure *closure = asClosure(closureValue);
        // pop();
        push(closureValue);  // closureValue is already tracked in createClosure
        call(closure, 0);

        std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();

#ifdef DEBUG_PROFILE_COMPILE
        std::cout << "Compilation took: "
                  << std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count()
                  << " ns"
                  << std::endl;
#endif
        // LOG_INFO("VM", "Compilation took " << elapsed_seconds.count() << "s");

        // Apply bytecode optimizations and store functions for profiling
        for (auto f : compiler.parser.allocatedFunction)
        {
            auto *func = asFunction(f);

            if (enableOptimizations and not func->chunk.code.empty())
            {
                begin = std::chrono::steady_clock::now();

                LOG_INFO("VM", "Applying bytecode optimizations");
                size_t originalSize = func->chunk.code.size();

                passManager.runAllPasses(this, func->chunk);

                size_t optimizedSize = func->chunk.code.size();

                if (optimizedSize != originalSize)
                {
                    LOG_INFO("VM", "Optimization changed bytecode size from " <<
                            originalSize << " to " << optimizedSize << " bytes");
                }

                end = std::chrono::steady_clock::now();

    #ifdef DEBUG_PROFILE_COMPILE
                std::cout << "Optimizations took: "
                        << std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count()
                        << " ns"
                        << std::endl;
    #endif
            }

            if (profiler.isEnabled())
            {
                // Store function for profiling reports
                compiledFunctions.push_back(func);
            }
        }

        // Optionally dump bytecode to file
        if (dumpByteCode != "")
        {
            std::ofstream outFile(dumpByteCode, std::ios::binary);
            if (outFile.is_open())
            {
                LOG_MILE("VM", "Dumping bytecode to file: " << dumpByteCode);

                ChunkSerializer serializer;
                if (not serializer.serialize(asFunction(function)->chunk, outFile, this))
                {
                    LOG_ERROR("VM", "Failed to dump bytecode to file: " << dumpByteCode);
                }

                outFile.close();
            }
            else
            {
                LOG_ERROR("VM", "Could not open file for bytecode dump: " << dumpByteCode);
            }
        }

        if (compileOnly)
        {
            return InterpretResult::OK;
        }

        try
        {
            // Freeze constant indices - all pool allocations up to this point are constants
            // Runtime allocations will have indices above these max values
            pools.freezeConstantIndices();

            // Record pre-run time if profiling is enabled (everything from start of interpret to here)
            if (profiler.isEnabled())
            {
                auto preRunEnd = std::chrono::steady_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(preRunEnd - interpretStart).count();
                profiler.recordPreRunTime(duration);
            }

            begin = std::chrono::steady_clock::now();
            auto result = run();
            end = std::chrono::steady_clock::now();

#ifdef DEBUG_PROFILE_COMPILE
            std::cout << "Execution took: "
                      << std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count()
                      << " ns"
                      << std::endl;
#endif
            return result;
        }
        catch(const std::exception& e)
        {
            LOG_ERROR("VM", e.what());

            return InterpretResult::RUNTIME_ERROR;
        }

    }

    // ============================================================================
    // Helper methods for bytecode execution
    // ============================================================================

    InterpretResult VM::executeChunk(ObjFunction* funcObj, int argCount)
    {
        // Create closure and set up call
        auto closureValue = createClosure(funcObj);
        Closure* closure = asClosure(closureValue);
        push(closureValue);

        call(closure, argCount);

        // Execute with error handling
        InterpretResult result;
        try
        {
            pools.freezeConstantIndices();

            // Note: preRunTime is not recorded here as it should be recorded
            // by the calling interpret method before calling executeChunk

            result = run();
        }
        catch(const std::exception& e)
        {
            LOG_ERROR("VM", "Execution error: " << e.what());
            result = InterpretResult::RUNTIME_ERROR;
        }

        return result;
    }

    void VM::cleanupFunction(ObjFunction* funcObj)
    {
        // Clean up: release the chunk constants before destroying the function
        // The constants array contains Values that point to heap objects (strings, etc.)
        // NOTE: We need to bypass the isConstant() check in releaseValue() because these
        // bytecode constants should be released when the function is destroyed
        for (const auto& constant : funcObj->chunk.constants)
        {
            if (requiresRefCount(constant))
            {
                // Manually decrement refcount and delete, bypassing isConstant() check
                uint32_t index = GET_INDEX(constant);
                auto& refCounts = pools.getRefCountVector(constant);

                if (index < refCounts.size() && refCounts[index] > 0)
                {
                    refCounts[index]--;
                    if (refCounts[index] == 0)
                    {
                        deleteValue(constant);
                    }
                }
            }
        }

        // Clean up: release the function to free the chunk's vectors
        pools.functionPool.release(funcObj);
    }

    // ============================================================================
    // Public interpret methods
    // ============================================================================

    InterpretResult VM::interpretFromBytecodeFile(const std::string& filename)
    {
        // Record start time for pre-run profiling
        std::chrono::steady_clock::time_point interpretStart;
        if (profiler.isEnabled())
        {
            interpretStart = std::chrono::steady_clock::now();
        }

        if (filename.empty())
        {
            LOG_WARNING("VM", "No file specified, nothing to execute !");
            return InterpretResult::OK;
        }

        currentFileName = filename;

        Chunk chunk;

        ChunkSerializer serializer;
        if (not serializer.deserializeFromFile(chunk, filename, this))
        {
            LOG_ERROR("VM", "Failed to load bytecode from file: " << filename);
            return InterpretResult::COMPILE_ERROR;
        }

        // Load all native modules that were imported during compilation
        for (const std::string& moduleName : chunk.importedModules)
        {
            if (not loadNativeModule(moduleName))
            {
                LOG_WARNING("VM", "Failed to load imported module '" << moduleName << "' from bytecode file");
            }
        }

        // Create function from deserialized chunk
        auto function = createFunction();
        ObjFunction* funcObj = asFunction(function);
        funcObj->chunk = chunk;

        // Record pre-run time if profiling is enabled
        if (profiler.isEnabled())
        {
            auto preRunEnd = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(preRunEnd - interpretStart).count();
            profiler.recordPreRunTime(duration);
        }

        // Execute and cleanup
        InterpretResult result = executeChunk(funcObj, 0);
        cleanupFunction(funcObj);

        return result;
    }

    InterpretResult VM::interpretFromCachedBytecode(const std::vector<char>& cachedBytecode, int argCount)
    {
        // Record start time for pre-run profiling
        std::chrono::steady_clock::time_point interpretStart;
        if (profiler.isEnabled())
        {
            interpretStart = std::chrono::steady_clock::now();
        }

        if (cachedBytecode.empty())
        {
            LOG_WARNING("VM", "Bytecode is empty, nothing to execute !");
            return InterpretResult::OK;
        }

        // Deserialize from cached memory (NO FILE I/O!)
        std::istringstream bytecodeStream(std::string(cachedBytecode.begin(), cachedBytecode.end()), std::ios::binary);

        Chunk chunk;
        if (not ChunkSerializer::deserialize(chunk, bytecodeStream, this))
        {
            LOG_ERROR("VM", "Failed to deserialize cached bytecode");
            return InterpretResult::COMPILE_ERROR;
        }

        // Load all native modules that were imported during compilation
        for (const std::string& moduleName : chunk.importedModules)
        {
            if (not loadNativeModule(moduleName))
            {
                LOG_WARNING("VM", "Failed to load imported module '" << moduleName << "' from cached bytecode");
            }
        }

        // Create function from deserialized chunk
        auto function = createFunction();
        ObjFunction* funcObj = asFunction(function);
        funcObj->chunk = chunk;

        // Record pre-run time if profiling is enabled
        if (profiler.isEnabled())
        {
            auto preRunEnd = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(preRunEnd - interpretStart).count();
            profiler.recordPreRunTime(duration);
        }

        InterpretResult result = executeChunk(funcObj, argCount);
        cleanupFunction(funcObj);
        return result;
    }

    InterpretResult VM::run()
    {
        // Set currentFrame to point to the topmost frame
        currentFrame = &frames[frameCount - 1];

        if (currentFrame->closure->function->chunk.code.empty())
            return InterpretResult::OK;

        // Cache chunk data pointer to avoid repeated vector::data() calls
        updateChunkCache();

        // Function pointer dispatch with longjmp
        if (setjmp(exit_jump) == 0)
        {
            while (true)
            {
                // Todo this is not needed with longjmp
                // if (currentFrame->ip >= chunkDataEnd)
                // {
                //     return InterpretResult::OK;
                // }

                // Calculate instruction offset before incrementing IP
                size_t instructionOffset = currentFrame->ip - chunkData;
                uint8_t opcode = *currentFrame->ip++;

#ifdef DEBUG_TRACE_EXECUTION
                std::cout << "          ";
                for (size_t i = 0; i < stack.size(); ++i)
                {
                    std::cout << "[";
                    printValue(this, stack[i]);
                    std::cout << "] ";
                }
                std::cout << std::endl;

                // Update frame IP for debug output
                // currentFrame->ip = ip;
                disassembleInstruction(this, currentFrame->closure->function->chunk, instructionOffset);
#endif

                // Only measure timing if profiling is actually enabled
                if (profiler.isEnabled())
                {
                    // IMPORTANT: Capture chunk pointer and function name BEFORE executing the operation
                    // because operations like OP_Call will change currentFrame
                    const void* chunkPtr = &currentFrame->closure->function->chunk;
                    const std::string& functionName = currentFrame->closure->function->name;
                    std::string opcodeName = opcodeToString(static_cast<OpCode>(opcode));

                    auto startTime = std::chrono::high_resolution_clock::now();

                    // Dispatch to operation handler
                    operations[opcode].handler(this);

                    auto endTime = std::chrono::high_resolution_clock::now();
                    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(endTime - startTime).count();

                    profiler.recordInstruction(chunkPtr, functionName, instructionOffset, opcode, opcodeName, duration);
                }
                else
                {
                    // Fast path: no profiling overhead
                    operations[opcode].handler(this);
                }

                // Update frame IP for potential frame switches
                // currentFrame->ip = ip;
            }
        }

        return exit_result;
    }

    ObjUpvalue* VM::captureUpvalue(Value* local)
    {
        ObjUpvalue* prevUpvalue = nullptr;
        ObjUpvalue* upvalue = openUpvalues;

        while (upvalue != nullptr and upvalue->location > local)
        {
            prevUpvalue = upvalue;
            upvalue = upvalue->next;
        }

        if (upvalue != nullptr and upvalue->location == local)
        {
            return upvalue; // Existing upvalue found
        }

        auto upValueValue = createUpvalue(local);  // Already tracked in createUpvalue

        ObjUpvalue* newUpvalue = asUpvalue(upValueValue);
        newUpvalue->next = upvalue;

        if (prevUpvalue == nullptr)
        {
            openUpvalues = newUpvalue;
        }
        else
        {
            prevUpvalue->next = newUpvalue;
        }

        return newUpvalue;
    }

    void VM::closeUpvalues(Value* last)
    {
        while (openUpvalues != nullptr and openUpvalues->location >= last)
        {
            ObjUpvalue* upvalue = openUpvalues;
            upvalue->closed = retainValue(*upvalue->location);  // Retain! Upvalue owns it now
            upvalue->location = &upvalue->closed;
            openUpvalues = upvalue->next;
        }
    }

    bool VM::callValue(const Value& callee, int argCount)
    {
        if (IS_CLOSURE(callee))
        {
            return call(asClosure(callee), argCount);
        }
        else if (IS_NAT_FUNC(callee))
        {
            auto* native = asNativeFunc(callee);
            Value result = native->function(this, argCount, stack.data() + stack.size() - argCount);

            // Remove arguments from the stack
            for (int i = 0; i < argCount + 1; i++)
            {
                auto v = pop();
                releaseAndDelete(v);
            }

            push(result);
            return true;
        }
        else if (IS_CLASS(callee))
        {
            Klass* klass = asClass(callee);
            auto instanceValue = createInstance(klass);  // Already tracked in createInstance

            releaseAndDelete(stack[stack.size() - argCount - 1]);
            stack[stack.size() - argCount - 1] = instanceValue;

            // Call initializer if it exists
            if (klass->methods.find("init") != klass->methods.end())
            {
                auto initializer = klass->methods["init"];

                return callBound(asClosure(initializer), argCount);
            }
            else if (argCount != 0)
            {
                runtimeError((Strfy() << "Expected 0 arguments but got: " << argCount << ".").getData());

                return false;
            }

            return true;
        }
        else if (IS_BOUND_METHOD(callee))
        {
            ObjBoundMethod* boundMethod = asBoundMethod(callee);

            // Save the method closure before we delete the bound method
            Closure* method = boundMethod->method;

            // Retain the receiver since we're about to release the bound method
            Value receiver = retainValue(boundMethod->receiver);

            // Release the bound method since we're replacing it
            releaseAndDelete(stack[stack.size() - argCount - 1]);

            // Replace with the receiver
            stack[stack.size() - argCount - 1] = receiver;

            return callBound(method, argCount);
        }

        runtimeError("Can only call functions and classes");

        return false;
    }

    bool VM::callMethod(Klass* receiver, const std::string& methodName, int argCount)
    {
        auto it = receiver->methods.find(methodName);

        if (it == receiver->methods.end())
        {
            runtimeError((Strfy() << "Undefined method '" << methodName << "'." ).getData());

            return false;
        }

        Value methodValue = it->second;

        // Handle closures (script-defined methods)
        if (IS_CLOSURE(methodValue))
        {
            Closure* method = asClosure(methodValue);
            return callBound(method, argCount);
        }

        // Handle native functions (C++-defined methods)
        if (IS_NAT_FUNC(methodValue))
        {
            auto* native = asNativeFunc(methodValue);

            // For native methods, pass receiver as args[0], method arguments as args[1], args[2], etc.
            // Receiver is at stack[size - argCount - 1]
            // Increment argCount to include the receiver
            Value result = native->function(this, argCount + 1, stack.data() + stack.size() - argCount - 1);

            // Remove arguments and receiver from the stack
            for (int i = 0; i < argCount + 1; i++)
            {
                auto v = pop();
                releaseAndDelete(v);
            }

            push(result);
            return true;
        }

        runtimeError((Strfy() << "Method '" << methodName << "' is not a closure or native function." ).getData());
        return false;
    }

    bool VM::call(Closure* closure, int argCount)
    {
        if (argCount != closure->function->arity)
        {
            runtimeError((Strfy() << "Expected " << closure->function->arity << " arguments but got: " << argCount << ".").getData());

            return false;
        }

        if (frameCount == FRAMES_MAX)
        {
            runtimeError("Stack overflow");

            return false;
        }

        CallFrame *frame = &frames[frameCount++];

        frame->closure = closure;
        frame->ip = closure->function->chunk.code.data();
        frame->slots = stack.data() + stack.size() - argCount;

        // For regular function calls, the stack layout is:
        // [...caller...] [function] [arg1] [arg2] ...
        // slots points to arg1, and the function is at slots-1
        // When returning, we want to truncate to the function's position (remove function + args + locals)
        frame->stackBase = frame->slots - 1;

        return true;
    }

    bool VM::callBound(Closure* closure, int argCount)
    {
        if (argCount != closure->function->arity)
        {
            runtimeError((Strfy() << "Expected " << closure->function->arity << " arguments but got: " << argCount << ".").getData());

            return false;
        }

        if (frameCount == FRAMES_MAX)
        {
            runtimeError("Stack overflow");

            return false;
        }

        CallFrame *frame = &frames[frameCount++];

        frame->closure = closure;
        frame->ip = closure->function->chunk.code.data();
        frame->slots = stack.data() + stack.size() - argCount - 1;

        // For bound method calls, the stack layout is:
        // [...caller...] [receiver] ...
        // slots points to receiver
        // When returning, we want to truncate to the receiver's position (remove receiver + args + locals)
        frame->stackBase = frame->slots;

        return true;
    }

    void VM::deleteValue(const Value& value)
    {
        // Perform type-specific deletion
        if (IS_LONG_STRING(value))
        {
            // Remove from interned strings map before releasing
            ElementType* str = asStringPtr(value);
            std::string strContent = str->toString();
            pools.internedStrings.erase(strContent);
            pools.stringPool.release(str);
        }
        else if (IS_SMALL_STRING(value))
        {
            // Small strings are inline - no deletion needed
            return;
        }
        else if (IS_FUNC(value))
        {
            pools.functionPool.release(asFunction(value));
        }
        else if (IS_CLOSURE(value))
        {
            pools.closurePool.release(asClosure(value));
        }
        else if (IS_UPVALUE(value))
        {
            pools.upvaluePool.release(asUpvalue(value));
        }
        else if (IS_NAT_FUNC(value))
        {
            pools.nativeFuncPool.release(asNativeFunc(value));
        }
        else if (IS_CLASS(value))
        {
            auto *klass = asClass(value);

            for (auto& pair : klass->methods)
            {
                releaseAndDelete(pair.second);
            }

            pools.classPool.release(asClass(value));
        }
        else if (IS_INSTANCE(value))
        {
            for (auto& [ _ , field] : asInstance(value)->fields)
            {
                releaseAndDelete(field);
            }

            pools.instancePool.release(asInstance(value));
        }
        else if (IS_BOUND_METHOD(value))
        {
            auto *boundMethod = asBoundMethod(value);

            // Don't release receiver here - it will be released when the bound method is released
            // releaseAndDelete(boundMethod->receiver);

            pools.boundMethodPool.release(boundMethod);
        }
        else if (IS_VECTOR(value))
        {
            auto *vector = asVector(value);

            // Release all elements in the vector
            for (auto& element : vector->fields)
            {
                releaseAndDelete(element);
            }

            pools.vectorPool.release(vector);
        }
    }

    void VM::releaseAndDelete(const Value& value)
    {
        if (releaseValue(value))
        {
            deleteValue(value);
        }
    }

    Value VM::addValues(const Value a,const Value b)
    {
        // Fast path for integers
        if (IS_INT(a) and IS_INT(b))
            return INT_VAL(AS_INT(a) + AS_INT(b));

        // Fast path for floats
        if (IS_FLOAT(a) and IS_FLOAT(b))
            return FLOAT_VAL(AS_FLOAT(a) + AS_FLOAT(b));

        // Mixed int/float cases - promote to float
        if (IS_INT(a) and IS_FLOAT(b))
            return FLOAT_VAL(static_cast<double>(AS_INT(a)) + AS_FLOAT(b));

        if (IS_FLOAT(a) and IS_INT(b))
            return FLOAT_VAL(AS_FLOAT(a) + static_cast<double>(AS_INT(b)));

        // Fast path for string concatenation
        if (IS_STRING(a) && IS_STRING(b))
        {
            // Extract string content
            std::string strA = asString(a);
            std::string strB = asString(b);

            return createString(strA + strB);
        }

        // Disallow functions
        if (IS_FUNC(a) or IS_FUNC(b))
            throw std::runtime_error("Cannot compare function Values");

        // Fall back to ElementType for other complex cases (strings, etc.)
        ElementType elemA = valueToElement(a);
        ElementType elemB = valueToElement(b);
        return elementToValue(elemA + elemB);
    }

    Value VM::subtractValues(const Value& a, const Value& b)
    {
        // Fast path for integers
        if (IS_INT(a) and IS_INT(b))
            return INT_VAL(AS_INT(a) - AS_INT(b));

        // Fast path for floats
        if (IS_FLOAT(a) and IS_FLOAT(b))
            return FLOAT_VAL(AS_FLOAT(a) - AS_FLOAT(b));

        // Mixed int/float cases - promote to float
        if (IS_INT(a) and IS_FLOAT(b))
            return FLOAT_VAL(static_cast<double>(AS_INT(a)) - AS_FLOAT(b));

        if (IS_FLOAT(a) and IS_INT(b))
            return FLOAT_VAL(AS_FLOAT(a) - static_cast<double>(AS_INT(b)));

        // Disallow functions
        if (IS_FUNC(a) or IS_FUNC(b))
            throw std::runtime_error("Cannot compare function Values");

        // Fall back to ElementType for other complex cases
        ElementType elemA = valueToElement(a);
        ElementType elemB = valueToElement(b);
        return elementToValue(elemA - elemB);
    }

    Value VM::multiplyValues(const Value& a, const Value& b)
    {
        // Fast path for integers
        if (IS_INT(a) and IS_INT(b))
            return INT_VAL(AS_INT(a) * AS_INT(b));

        // Fast path for floats
        if (IS_FLOAT(a) and IS_FLOAT(b))
            return FLOAT_VAL(AS_FLOAT(a) * AS_FLOAT(b));

        // Mixed int/float cases - promote to float
        if (IS_INT(a) and IS_FLOAT(b))
            return FLOAT_VAL(static_cast<double>(AS_INT(a)) * AS_FLOAT(b));

        if (IS_FLOAT(a) and IS_INT(b))
            return FLOAT_VAL(AS_FLOAT(a) * static_cast<double>(AS_INT(b)));

        // Disallow functions
        if (IS_FUNC(a) or IS_FUNC(b))
            throw std::runtime_error("Cannot multiply function Values");

        // Fall back to ElementType for other complex cases
        ElementType elemA = valueToElement(a);
        ElementType elemB = valueToElement(b);
        return elementToValue(elemA * elemB);
    }

    Value VM::divideValues(const Value& a, const Value& b)
    {
        // Fast path for integers
        if (IS_INT(a) and IS_INT(b) and AS_INT(b) != 0)
            return INT_VAL(AS_INT(a) / AS_INT(b));

        // Fast path for floats
        if (IS_FLOAT(a) and IS_FLOAT(b) and areNotAlmostEqual(static_cast<float>(AS_FLOAT(b)), 0.0f))
            return FLOAT_VAL(AS_FLOAT(a) / AS_FLOAT(b));

        // Mixed int/float cases - promote to float
        if (IS_INT(a) and IS_FLOAT(b) and areNotAlmostEqual(static_cast<float>(AS_FLOAT(b)), 0.0f))
            return FLOAT_VAL(static_cast<double>(AS_INT(a)) / AS_FLOAT(b));

        if (IS_FLOAT(a) and IS_INT(b) and AS_INT(b) != 0)
            return FLOAT_VAL(AS_FLOAT(a) / static_cast<double>(AS_INT(b)));

        // Disallow functions
        if (IS_FUNC(a) or IS_FUNC(b))
            throw std::runtime_error("Cannot divide function Values");

        // Fall back to ElementType for other complex cases
        ElementType elemA = valueToElement(a);
        ElementType elemB = valueToElement(b);
        return elementToValue(elemA / elemB);
    }

    Value VM::moduloValues(const Value& a, const Value& b)
    {
        // Fast path for integers
        if (IS_INT(a) and IS_INT(b) and AS_INT(b) != 0)
            return INT_VAL(AS_INT(a) % AS_INT(b));

        // For floats, use fmod
        if (IS_FLOAT(a) and IS_FLOAT(b) and areNotAlmostEqual(static_cast<float>(AS_FLOAT(b)), 0.0f))
            return FLOAT_VAL(std::fmod(AS_FLOAT(a), AS_FLOAT(b)));

        // Mixed int/float cases - promote to float
        if (IS_INT(a) and IS_FLOAT(b) and areNotAlmostEqual(static_cast<float>(AS_FLOAT(b)), 0.0f))
            return FLOAT_VAL(std::fmod(static_cast<double>(AS_INT(a)), AS_FLOAT(b)));

        if (IS_FLOAT(a) and IS_INT(b) and AS_INT(b) != 0)
            return FLOAT_VAL(std::fmod(AS_FLOAT(a), static_cast<double>(AS_INT(b))));

        // Disallow functions
        if (IS_FUNC(a) or IS_FUNC(b))
            throw std::runtime_error("Cannot modulo function Values");

        throw std::runtime_error("Invalid types for modulo operation");
    }

    Value VM::negateValue(const Value& val)
    {
        // Fast path for integers
        if (IS_INT(val))
            return INT_VAL(-AS_INT(val));

        // Fast path for floats
        if (IS_FLOAT(val))
            return FLOAT_VAL(-AS_FLOAT(val));

        // Disallow functions
        if (IS_FUNC(val))
            throw std::runtime_error("Cannot negate function Values");

        ElementType elem = valueToElement(val);
        return elementToValue(-elem);
    }

    Value VM::equalsValues(const Value& a, const Value& b)
    {
        if (IS_INT(a) and IS_INT(b))
            return BOOL_VAL(AS_INT(a) == AS_INT(b));

        if (IS_FLOAT(a) and IS_FLOAT(b))
            return BOOL_VAL(areAlmostEqual(static_cast<float>(AS_FLOAT(a)), static_cast<float>(AS_FLOAT(b))));

        if (IS_INT(a) and IS_FLOAT(b))
            return BOOL_VAL(areAlmostEqual(static_cast<float>(AS_INT(a)), static_cast<float>(AS_FLOAT(b))));

        if (IS_FLOAT(a) and IS_INT(b))
            return BOOL_VAL(areAlmostEqual(static_cast<float>(AS_FLOAT(a)), static_cast<float>(AS_INT(b))));

        if (IS_BOOL(a) and IS_BOOL(b))
            return BOOL_VAL(AS_BOOL(a) == AS_BOOL(b));

        // Disallow functions
        if (IS_FUNC(a) or IS_FUNC(b))
            throw std::runtime_error("Cannot compare function Values");

        ElementType elemA = valueToElement(a);
        ElementType elemB = valueToElement(b);
        return elementToValue(elemA == elemB);
    }

    Value VM::notEqualsValues(const Value& a, const Value& b)
    {
        if (IS_INT(a) and IS_INT(b))
            return BOOL_VAL(AS_INT(a) != AS_INT(b));

        if (IS_FLOAT(a) and IS_FLOAT(b))
            return BOOL_VAL(areNotAlmostEqual(static_cast<float>(AS_FLOAT(a)), static_cast<float>(AS_FLOAT(b))));

        if (IS_INT(a) and IS_FLOAT(b))
            return BOOL_VAL(areNotAlmostEqual(static_cast<float>(AS_INT(a)), static_cast<float>(AS_FLOAT(b))));

        if (IS_FLOAT(a) and IS_INT(b))
            return BOOL_VAL(areNotAlmostEqual(static_cast<float>(AS_FLOAT(a)), static_cast<float>(AS_INT(b))));

        if (IS_BOOL(a) and IS_BOOL(b))
            return BOOL_VAL(AS_BOOL(a) != AS_BOOL(b));

        // Disallow functions
        if (IS_FUNC(a) or IS_FUNC(b))
            throw std::runtime_error("Cannot compare function Values");

        ElementType elemA = valueToElement(a);
        ElementType elemB = valueToElement(b);
        return elementToValue(elemA != elemB);
    }

    Value VM::greaterValues(const Value& a, const Value& b)
    {
        if (IS_INT(a) and IS_INT(b))
            return BOOL_VAL(AS_INT(a) > AS_INT(b));

        if (IS_FLOAT(a) and IS_FLOAT(b))
            return BOOL_VAL(AS_FLOAT(a) > AS_FLOAT(b));

        if (IS_INT(a) and IS_FLOAT(b))
            return BOOL_VAL(static_cast<double>(AS_INT(a)) > AS_FLOAT(b));

        if (IS_FLOAT(a) and IS_INT(b))
            return BOOL_VAL(AS_FLOAT(a) > static_cast<double>(AS_INT(b)));

        // Disallow functions
        if (IS_FUNC(a) or IS_FUNC(b))
            throw std::runtime_error("Cannot compare function Values");

        ElementType elemA = valueToElement(a);
        ElementType elemB = valueToElement(b);
        return elementToValue(elemA > elemB);
    }

    Value VM::greaterEqualValues(const Value& a, const Value& b)
    {
        if (IS_INT(a) and IS_INT(b))
            return BOOL_VAL(AS_INT(a) >= AS_INT(b));

        if (IS_FLOAT(a) and IS_FLOAT(b))
            return BOOL_VAL(AS_FLOAT(a) >= AS_FLOAT(b));

        if (IS_INT(a) and IS_FLOAT(b))
            return BOOL_VAL(static_cast<double>(AS_INT(a)) >= AS_FLOAT(b));

        if (IS_FLOAT(a) and IS_INT(b))
            return BOOL_VAL(AS_FLOAT(a) >= static_cast<double>(AS_INT(b)));

        // Disallow functions
        if (IS_FUNC(a) or IS_FUNC(b))
            throw std::runtime_error("Cannot compare function Values");

        ElementType elemA = valueToElement(a);
        ElementType elemB = valueToElement(b);
        return elementToValue(elemA >= elemB);
    }

    Value VM::lessValues(const Value& a, const Value& b)
    {
        if (IS_INT(a) and IS_INT(b))
            return BOOL_VAL(AS_INT(a) < AS_INT(b));

        if (IS_FLOAT(a) and IS_FLOAT(b))
            return BOOL_VAL(AS_FLOAT(a) < AS_FLOAT(b));

        if (IS_INT(a) and IS_FLOAT(b))
            return BOOL_VAL(static_cast<double>(AS_INT(a)) < AS_FLOAT(b));

        if (IS_FLOAT(a) and IS_INT(b))
            return BOOL_VAL(AS_FLOAT(a) < static_cast<double>(AS_INT(b)));

        // Disallow functions
        if (IS_FUNC(a) or IS_FUNC(b))
            throw std::runtime_error("Cannot compare function Values");

        ElementType elemA = valueToElement(a);
        ElementType elemB = valueToElement(b);
        return elementToValue(elemA < elemB);
    }

    Value VM::lessEqualValues(const Value& a, const Value& b)
    {
        if (IS_INT(a) and IS_INT(b))
            return BOOL_VAL(AS_INT(a) <= AS_INT(b));

        if (IS_FLOAT(a) and IS_FLOAT(b))
            return BOOL_VAL(AS_FLOAT(a) <= AS_FLOAT(b));

        if (IS_INT(a) and IS_FLOAT(b))
            return BOOL_VAL(static_cast<double>(AS_INT(a)) <= AS_FLOAT(b));

        if (IS_FLOAT(a) and IS_INT(b))
            return BOOL_VAL(AS_FLOAT(a) <= static_cast<double>(AS_INT(b)));

        // Disallow functions
        if (IS_FUNC(a) or IS_FUNC(b))
            throw std::runtime_error("Cannot compare function Values");

        ElementType elemA = valueToElement(a);
        ElementType elemB = valueToElement(b);
        return elementToValue(elemA <= elemB);
    }

    // Function pointer dispatch implementation
    void VM::vm_return(InterpretResult result) {
        exit_result = result;
        longjmp(exit_jump, 1);
    }

    void VM::register_operation(uint8_t opcode, OpHandler handler) {
        operations[opcode] = OpCodeInfo(handler);
    }

    void VM::register_builtin_operations() {
        register_operation(static_cast<uint8_t>(OpCode::OP_Return), op_return);
        register_operation(static_cast<uint8_t>(OpCode::OP_Constant), op_constant);
        register_operation(static_cast<uint8_t>(OpCode::OP_LongConstant), op_long_constant);
        register_operation(static_cast<uint8_t>(OpCode::OP_Add), op_add);
        register_operation(static_cast<uint8_t>(OpCode::OP_Subtract), op_subtract);
        register_operation(static_cast<uint8_t>(OpCode::OP_Multiply), op_multiply);
        register_operation(static_cast<uint8_t>(OpCode::OP_Divide), op_divide);
        register_operation(static_cast<uint8_t>(OpCode::OP_Modulo), op_modulo);
        register_operation(static_cast<uint8_t>(OpCode::OP_Negate), op_negate);
        register_operation(static_cast<uint8_t>(OpCode::OP_Equal), op_equal);
        register_operation(static_cast<uint8_t>(OpCode::OP_NotEqual), op_not_equal);
        register_operation(static_cast<uint8_t>(OpCode::OP_Greater), op_greater);
        register_operation(static_cast<uint8_t>(OpCode::OP_GreaterEqual), op_greater_equal);
        register_operation(static_cast<uint8_t>(OpCode::OP_Less), op_less);
        register_operation(static_cast<uint8_t>(OpCode::OP_LessEqual), op_less_equal);
        register_operation(static_cast<uint8_t>(OpCode::OP_True), op_true);
        register_operation(static_cast<uint8_t>(OpCode::OP_False), op_false);
        register_operation(static_cast<uint8_t>(OpCode::OP_Not), op_not);
        register_operation(static_cast<uint8_t>(OpCode::OP_And), op_and);
        register_operation(static_cast<uint8_t>(OpCode::OP_Or), op_or);
        register_operation(static_cast<uint8_t>(OpCode::OP_Pop), op_pop);
        register_operation(static_cast<uint8_t>(OpCode::OP_Get_Local), op_get_local);
        register_operation(static_cast<uint8_t>(OpCode::OP_Set_Local), op_set_local);
        register_operation(static_cast<uint8_t>(OpCode::OP_Get_Global), op_get_global);
        register_operation(static_cast<uint8_t>(OpCode::OP_Define_Global), op_define_global);
        register_operation(static_cast<uint8_t>(OpCode::OP_Set_Global), op_set_global);
        register_operation(static_cast<uint8_t>(OpCode::OP_Jump), op_jump);
        register_operation(static_cast<uint8_t>(OpCode::OP_Jump_If_False), op_jump_if_false);
        register_operation(static_cast<uint8_t>(OpCode::OP_Loop), op_loop);
        register_operation(static_cast<uint8_t>(OpCode::OP_Long_Jump), op_long_jump);
        register_operation(static_cast<uint8_t>(OpCode::OP_Long_Jump_If_False), op_long_jump_if_false);
        register_operation(static_cast<uint8_t>(OpCode::OP_Long_Loop), op_long_loop);
        register_operation(static_cast<uint8_t>(OpCode::OP_Call), op_call);
        register_operation(static_cast<uint8_t>(OpCode::OP_Invoke), op_invoke);
        register_operation(static_cast<uint8_t>(OpCode::OP_Closure), op_closure);
        register_operation(static_cast<uint8_t>(OpCode::OP_Get_Upvalue), op_get_upvalue);
        register_operation(static_cast<uint8_t>(OpCode::OP_Set_Upvalue), op_set_upvalue);
        register_operation(static_cast<uint8_t>(OpCode::OP_Close_Upvalue), op_close_upvalue);
        register_operation(static_cast<uint8_t>(OpCode::OP_Debug_Print), op_debug_print);
        register_operation(static_cast<uint8_t>(OpCode::OP_Post_Incr_Global), op_post_incr_global);
        register_operation(static_cast<uint8_t>(OpCode::OP_Incr_Global), op_incr_global);
        register_operation(static_cast<uint8_t>(OpCode::OP_Post_Decr_Global), op_post_decr_global);
        register_operation(static_cast<uint8_t>(OpCode::OP_Decr_Global), op_decr_global);
        register_operation(static_cast<uint8_t>(OpCode::OP_Post_Incr_Local), op_post_incr_local);
        register_operation(static_cast<uint8_t>(OpCode::OP_Incr_Local), op_incr_local);
        register_operation(static_cast<uint8_t>(OpCode::OP_Post_Decr_Local), op_post_decr_local);
        register_operation(static_cast<uint8_t>(OpCode::OP_Decr_Local), op_decr_local);
        register_operation(static_cast<uint8_t>(OpCode::OP_Class), op_class);
        register_operation(static_cast<uint8_t>(OpCode::OP_Get_Property), op_get_property);
        register_operation(static_cast<uint8_t>(OpCode::OP_Set_Property), op_set_property);
        register_operation(static_cast<uint8_t>(OpCode::OP_Method), op_method);
        register_operation(static_cast<uint8_t>(OpCode::OP_Short_Int), op_short_int);

        register_operation(static_cast<uint8_t>(OpCode::OP_PopN), op_pop_n);

        register_operation(static_cast<uint8_t>(OpCode::OP_Define_Constant_Global), op_define_constant_global);
        register_operation(static_cast<uint8_t>(OpCode::OP_Get_Constant_Global), op_get_constant_global);
        register_operation(static_cast<uint8_t>(OpCode::OP_Set_Constant_Global), op_set_constant_global);

        register_operation(static_cast<uint8_t>(OpCode::OP_AddLL), op_add_ll);
        register_operation(static_cast<uint8_t>(OpCode::OP_SubtractLL), op_subtract_ll);

        register_operation(static_cast<uint8_t>(OpCode::OP_SubtractLC), op_subtract_lc);
        register_operation(static_cast<uint8_t>(OpCode::OP_SubtractCL), op_subtract_cl);

        // Table and vector operations
        register_operation(static_cast<uint8_t>(OpCode::OP_Build_Table), op_build_table);
        register_operation(static_cast<uint8_t>(OpCode::OP_Build_Vector), op_build_vector);
        register_operation(static_cast<uint8_t>(OpCode::OP_Get_Index), op_get_index);
        register_operation(static_cast<uint8_t>(OpCode::OP_Set_Index), op_set_index);

        // Iterator operations
        register_operation(static_cast<uint8_t>(OpCode::OP_Get_Iterator), op_get_iterator);
        register_operation(static_cast<uint8_t>(OpCode::OP_Iterator_Next), op_iterator_next);
        register_operation(static_cast<uint8_t>(OpCode::OP_Table_Size), op_table_size);
        register_operation(static_cast<uint8_t>(OpCode::OP_Table_At), op_table_at);

        // Module operations
        register_operation(static_cast<uint8_t>(OpCode::OP_Import), op_import);

        register_operation(static_cast<uint8_t>(OpCode::OP_Define_Global_Non_Popping), op_define_global_non_popping);
    }

    void VM::initialize_builtin_classes()
    {
        // Create the built-in Table class
        Value tableClass = createClass("__Table");
        globals["__Table"] = retainValue(tableClass);
    }

    // Operation handler implementations
    void op_true(VM* vm)
    {
        vm->push(BOOL_VAL(true));
    }

    void op_false(VM* vm)
    {
        vm->push(BOOL_VAL(false));
    }

    void op_pop(VM* vm)
    {
#ifdef DEBUG_CHECK_STACK
        if (stack.empty())
        {
            EMIT_RUNTIME_ERROR("Nothing to pop from the stack.");
        }
#endif
        auto value = vm->pop();
        // Escape analysis: Only release heap objects, not primitives
        if (requiresRefCount(value)) {
            vm->releaseAndDelete(value);
        }
    }

    void op_constant(VM* vm)
    {
        uint8_t constantIndex = *vm->currentFrame->ip++;

#ifdef DEBUG_CHECK_STACK
        if (constantIndex >= vm->currentFrame->closure->function->chunk.constants.size())
        {
            vm->runtimeError("Constant index out of bounds");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }
#endif
        Value constant = vm->currentFrame->closure->function->chunk.constants[constantIndex];
        // Escape analysis: Constants live in bytecode chunk, don't need refcounting
        // Only retain if it's a heap object that could be GC'd
        if (requiresRefCount(constant)) {
            vm->push(vm->retainValue(constant));
        } else {
            vm->push(constant);  // Primitives/constants just copied
        }
    }

    void op_long_constant(VM* vm)
    {
        uint32_t constantIndex = (static_cast<uint32_t>(*vm->currentFrame->ip++) << 16);
        constantIndex |= (static_cast<uint32_t>(*vm->currentFrame->ip++) << 8);
        constantIndex |= static_cast<uint32_t>(*vm->currentFrame->ip++);

#ifdef DEBUG_CHECK_STACK
        if (constantIndex >= vm->currentFrame->closure->function->chunk.constants.size()) {
            vm->runtimeError("Long constant index out of bounds.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }
#endif

        Value constant = vm->currentFrame->closure->function->chunk.constants[constantIndex];
        // Escape analysis: Constants live in bytecode chunk, don't need refcounting
        if (requiresRefCount(constant)) {
            vm->push(vm->retainValue(constant));
        } else {
            vm->push(constant);  // Primitives/constants just copied
        }
    }

#define BINARY_OP_TEMPLATE(op_name, operation) \
    void op_name(VM* vm) \
    { \
        auto b = vm->pop(); \
        auto a = vm->peek(); \
        vm->changeTop(vm->operation(a, b)); \
        /* Escape analysis: Only release heap objects, not primitives */ \
        if (requiresRefCount(b)) vm->releaseAndDelete(b); \
    }

    BINARY_OP_TEMPLATE(op_add, addValues)
    BINARY_OP_TEMPLATE(op_subtract, subtractValues)
    BINARY_OP_TEMPLATE(op_multiply, multiplyValues)
    BINARY_OP_TEMPLATE(op_divide, divideValues)
    BINARY_OP_TEMPLATE(op_modulo, moduloValues)

    BINARY_OP_TEMPLATE(op_equal, equalsValues)
    BINARY_OP_TEMPLATE(op_greater, greaterValues)
    BINARY_OP_TEMPLATE(op_less, lessValues)

    // Additional comparison operations using BINARY_OP_TEMPLATE pattern
    BINARY_OP_TEMPLATE(op_not_equal, notEqualsValues)
    BINARY_OP_TEMPLATE(op_greater_equal, greaterEqualValues)
    BINARY_OP_TEMPLATE(op_less_equal, lessEqualValues)

#undef BINARY_OP_TEMPLATE

    void op_get_local(VM* vm)
    {
        uint8_t slot = *vm->currentFrame->ip++;

        if (slot >= 255)
        {
            vm->runtimeError("Local variable slot out of range");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);

            return;
        }

        Value value = vm->currentFrame->slots[slot];
        // Escape analysis: Only retain heap objects, not primitives or stack values
        if (requiresRefCount(value)) {
            vm->push(vm->retainValue(value));
        } else {
            vm->push(value);  // Primitives just copied by value
        }
    }

    void op_set_local(VM* vm)
    {
        uint8_t slot = *vm->currentFrame->ip++;

        if (slot >= 255)
        {
            vm->runtimeError("Local variable slot out of range");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);

            return;
        }

        Value newValue = vm->peek(0);
        Value oldValue = vm->currentFrame->slots[slot];

        // Retain the new value if it's a heap object (slot now owns a reference)
        if (requiresRefCount(newValue)) {
            vm->currentFrame->slots[slot] = vm->retainValue(newValue);
        } else {
            vm->currentFrame->slots[slot] = newValue;
        }

        // Release old value if it's a heap object (after assignment to avoid use-after-free if old == new)
        if (requiresRefCount(oldValue)) {
            vm->releaseAndDelete(oldValue);
        }

        // Value oldValue = vm->currentFrame->slots[slot];
        // // Escape analysis: Only release old value if it's a heap object
        // if (requiresRefCount(oldValue)) {
        //     vm->releaseAndDelete(oldValue);
        // }

        // vm->currentFrame->slots[slot] = vm->peek(0);
    }

    void op_long_jump_if_false(VM* vm)
    {
        uint32_t offset = (static_cast<uint32_t>(*vm->currentFrame->ip++) << 24);
        offset |= (static_cast<uint32_t>(*vm->currentFrame->ip++) << 16);
        offset |= (static_cast<uint32_t>(*vm->currentFrame->ip++) << 8);
        offset |= static_cast<uint32_t>(*vm->currentFrame->ip++);

        Value condition = vm->peek();

        if (!isValueTrue(condition))
        {
            vm->currentFrame->ip += offset;
        }
    }

    void op_long_loop(VM* vm)
    {
        uint32_t offset = (static_cast<uint32_t>(*vm->currentFrame->ip++) << 24);
        offset |= (static_cast<uint32_t>(*vm->currentFrame->ip++) << 16);
        offset |= (static_cast<uint32_t>(*vm->currentFrame->ip++) << 8);
        offset |= static_cast<uint32_t>(*vm->currentFrame->ip++);

        vm->currentFrame->ip -= offset;
    }

    void op_return(VM* vm)
    {
#ifdef DEBUG_CHECK_STACK
        if (vm->stack.empty())
        {
            vm->runtimeError("Nothing in the stack for return.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);

            return;
        }
#endif
        auto value = vm->pop();

        // Save callee's stackBase (where caller's stack ended) before we dec frame count
        Value* calleeStackBase = vm->currentFrame->stackBase;

        vm->closeUpvalues(vm->currentFrame->slots);
        vm->frameCount--;

        if (vm->frameCount == 0)
        {
            vm->releaseAndDelete(value);

            auto finalValue = vm->pop();

            vm->releaseAndDelete(finalValue);
            vm->vm_return(InterpretResult::OK);

            return;
        }

        // Restore previous frame
        vm->currentFrame = &vm->frames[vm->frameCount - 1];
        vm->updateChunkCache();

        // Truncate stack to the callee's stackBase position
        // This removes the function/receiver + args + locals
        size_t stackTruncatePosition = calleeStackBase - vm->stack.data();

        while (vm->stack.size() > stackTruncatePosition)
        {
            auto v = vm->pop();
            vm->releaseAndDelete(v);
        }

        vm->push(value);
    }

    void op_negate(VM* vm)
    {
        if (not isValueNumber(vm->peek(0)))
        {
            vm->runtimeError("Operand after an unary (-) must be a number.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }

        auto val = vm->pop();
        vm->push(vm->negateValue(val));
        vm->releaseAndDelete(val);
    }

    void op_get_global(VM* vm)
    {
#ifdef DEBUG_CHECK_STACK
        if (vm->stack.empty())
        {
            vm->runtimeError("Not enough values on stack for variable retrieval.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }
#endif
        auto nameValue = vm->peek();  // variable name

        if (not IS_STRING(nameValue))
        {
            vm->releaseAndDelete(nameValue);
            vm->pop(); // Remove name from stack
            vm->runtimeError("Global variable name must be a litteral.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }

        auto name = vm->asString(nameValue);

        auto it = vm->globals.find(name);
        if (it == vm->globals.end())
        {
            vm->releaseAndDelete(nameValue);
            vm->pop(); // Remove name from stack
            vm->runtimeError("Undefined global variable '" + name + "'.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }

        vm->changeTop(vm->retainValue(it->second)); // Retain because stack becomes an owner
        vm->releaseAndDelete(nameValue);
    }

    void op_set_global(VM* vm)
    {
#ifdef DEBUG_CHECK_STACK
        if (vm->stack.size() < 2)
        {
            vm->runtimeError("Not enough values on stack for variable assignment.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }
#endif
        auto nameValue = vm->pop();
        auto value = vm->peek();
        auto name = vm->valueToElement(nameValue);

        if (not name.isLitteral())
        {
            vm->releaseAndDelete(nameValue);
            vm->runtimeError("Global variable name must be a litteral.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }

        auto it = vm->globals.find(name.toString());
        if (it == vm->globals.end())
        {
            vm->releaseAndDelete(nameValue);
            vm->runtimeError("Undefined global variable '" + name.toString() + "'.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }

        vm->releaseAndDelete(it->second);
        it->second = vm->retainValue(value);
        vm->releaseAndDelete(nameValue);
    }

    void op_define_global(VM* vm)
    {
#ifdef DEBUG_CHECK_STACK
        if (vm->stack.size() < 2)
        {
            vm->runtimeError("Not enough values on stack for variable definition.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }
#endif
        auto nameValue = vm->pop();  // variable name
        auto name = vm->valueToElement(nameValue);
        auto value = vm->pop(); // variable value

        if (not name.isLitteral())
        {
            vm->releaseAndDelete(nameValue);
            vm->releaseAndDelete(value);
            vm->runtimeError("Global variable name must be a litteral.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }

        vm->globals[name.toString()] = vm->retainValue(value);
        vm->releaseAndDelete(nameValue);
        vm->releaseAndDelete(value);
    }

    void op_define_global_non_popping(VM* vm)
    {
#ifdef DEBUG_CHECK_STACK
        if (vm->stack.size() < 2)
        {
            vm->runtimeError("Not enough values on stack for variable definition.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }
#endif
        auto nameValue = vm->pop();  // variable name
        auto name = vm->valueToElement(nameValue);
        auto value = vm->peek(); // variable value

        if (not name.isLitteral())
        {
            vm->releaseAndDelete(nameValue);
            vm->runtimeError("Global variable name must be a litteral.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }

        vm->globals[name.toString()] = vm->retainValue(value);
        vm->releaseAndDelete(nameValue);
    }

    void op_long_jump(VM* vm)
    {
        uint32_t offset = (static_cast<uint32_t>(*vm->currentFrame->ip++) << 24);
        offset |= (static_cast<uint32_t>(*vm->currentFrame->ip++) << 16);
        offset |= (static_cast<uint32_t>(*vm->currentFrame->ip++) << 8);
        offset |= static_cast<uint32_t>(*vm->currentFrame->ip++);

        vm->currentFrame->ip += offset;
    }

    void op_call(VM* vm)
    {
        int argCount = *vm->currentFrame->ip++;

        // Get the function object (at position argCount from top)
        Value function = vm->peek(argCount);

        if (not vm->callValue(function, argCount))
        {
            vm->runtimeError("Cannot call function");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);

            return;
        }

        // Function has been called, frame is set up with stackBase pointing to where to truncate on return
        vm->currentFrame = &vm->frames[vm->frameCount - 1];
        vm->updateChunkCache(); // Update cached chunk data for new frame
    }

    bool invoke(VM* vm, const std::string& name, uint8_t argCount)
    {
        auto receiverValue = vm->peek(argCount);

        if (not IS_INSTANCE(receiverValue))
        {
            return false;
        }

        ObjInstance* instance = vm->asInstance(receiverValue);

        // Check for field first
        auto fieldIt = instance->fields.find(name);
        if (fieldIt != instance->fields.end())
        {
            Value fieldValue = fieldIt->second;
            vm->releaseAndDelete(vm->stack[vm->stack.size() - argCount - 1]); // Remove receiver
            vm->stack[vm->stack.size() - argCount - 1] = vm->retainValue(fieldValue); // Replace it with the field value
            return vm->callValue(fieldValue, argCount);
        }

        // Then check for method
        return vm->callMethod(instance->klass, name, argCount);
    }

    void op_invoke(VM* vm)
    {
        uint8_t methodIndex = *vm->currentFrame->ip++;

        auto methodValue = vm->currentFrame->closure->function->chunk.constants[methodIndex];
        auto methodValueName = vm->valueToElement(methodValue);

        uint8_t argCount = *vm->currentFrame->ip++;

        if (not methodValueName.isLitteral())
        {
            vm->runtimeError("Invoke operand must be a method name string.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);

            return;
        }

        auto methodName = methodValueName.toString();

        if (not invoke(vm, methodName, argCount))
        {
            vm->runtimeError("Method '" + methodName + "' not found.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);

            return;
        }

        vm->currentFrame = &vm->frames[vm->frameCount - 1];
        vm->updateChunkCache();
    }

    void op_closure(VM* vm)
    {
        uint8_t constantIndex = *vm->currentFrame->ip++;

        auto functionValue = vm->currentFrame->closure->function->chunk.constants[constantIndex];

        if (not IS_FUNC(functionValue))
        {
            vm->runtimeError("Closure operand must be a function.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);

            return;
        }

        ObjFunction* function = vm->asFunction(functionValue);
        auto closure = vm->createClosure(function);  // Already tracked in createClosure
        vm->push(closure);

        for (int i = 0; i < function->upvalueCount; i++)
        {
            uint8_t isLocal = *vm->currentFrame->ip++;
            uint8_t index = *vm->currentFrame->ip++;
            if (isLocal)
            {
                vm->asClosure(closure)->upvalues[i] = vm->captureUpvalue(vm->currentFrame->slots + index);
            }
            else
            {
                vm->asClosure(closure)->upvalues[i] = vm->currentFrame->closure->upvalues[index];
            }
        }
    }

    void op_debug_print(VM* vm)
    {
#ifdef DEBUG_CHECK_STACK
        if (vm->stack.empty())
        {
            vm->runtimeError("Nothing to print from the stack.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }
#endif
        auto value = vm->pop();

        if (IS_FUNC(value))
        {
            ObjFunction* func = vm->asFunction(value);
            if (func != nullptr)
            {
                vm->testOutput += "<" + func->name + ">\n";
            }
            else
            {
                vm->testOutput += "<script>\n";
            }
            vm->releaseAndDelete(value);
            return;
        }

        if (IS_CLASS(value))
        {
            Klass* klass = vm->asClass(value);
            if (klass != nullptr)
            {
                vm->testOutput += "<class " + klass->name + ">\n";
            }
            else
            {
                vm->testOutput += "<null class>\n";
            }
            vm->releaseAndDelete(value);
            return;
        }

        if (IS_INSTANCE(value))
        {
            ObjInstance* instance = vm->asInstance(value);

            if (instance != nullptr && instance->klass != nullptr)
            {
                vm->testOutput += "<instance of " + instance->klass->name + ">\n";
            }
            else
            {
                vm->testOutput += "<null instance>\n";
            }

            vm->releaseAndDelete(value);
            return;
        }

        if (IS_BOUND_METHOD(value))
        {
            ObjBoundMethod* boundMethod = vm->asBoundMethod(value);

            if (boundMethod != nullptr && boundMethod->method != nullptr && boundMethod->method->function != nullptr)
            {
                vm->testOutput += "<bound method " + boundMethod->method->function->name + ">\n";
            }
            else
            {
                vm->testOutput += "<null bound method>\n";
            }

            vm->releaseAndDelete(value);
            return;
        }

        if (IS_CLOSURE(value))
        {
            Closure* closure = vm->asClosure(value);

            if (closure != nullptr && closure->function != nullptr)
            {
                vm->testOutput += "<closure " + closure->function->name + ">\n";
            }
            else
            {
                vm->testOutput += "<null closure>\n";
            }

            vm->releaseAndDelete(value);
            return;
        }

        if (IS_VECTOR(value))
        {
            ObjVector* vector = vm->asVector(value);

            if (vector != nullptr)
            {
                vm->testOutput += "<vector size=" + std::to_string(vector->fields.size()) + ">\n";
            }
            else
            {
                vm->testOutput += "<null vector>\n";
            }

            vm->releaseAndDelete(value);
            return;
        }

        // For testing: append to testOutput buffer instead of stdout
        // Handle integers directly to avoid 32-bit truncation in ElementType
        if (IS_INT(value))
        {
            int64_t val = AS_INT(value);
            vm->testOutput += std::to_string(val) + "\n";
        }
        else
        {
            ElementType elem = vm->valueToElement(value);
            vm->testOutput += elem.toString() + "\n";
        }
        vm->releaseAndDelete(value);
    }

    void op_not(VM* vm)
    {
        if (not IS_BOOL(vm->peek(0)))
        {
            vm->runtimeError("Operand after an unary (!) must be a boolean.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }

        auto value = vm->pop();
        vm->push(BOOL_VAL(not isValueTrue(value)));
        vm->releaseAndDelete(value);
    }

    void op_and(VM* vm)
    {
        vm->checkBooleanBinaryOp();
        auto b = vm->pop();
        auto a = vm->pop();

        bool resultA = isValueTrue(a);
        bool resultB = isValueTrue(b);
        vm->push(BOOL_VAL(resultA and resultB));
        vm->releaseAndDelete(a);
        vm->releaseAndDelete(b);
    }

    void op_or(VM* vm)
    {
        vm->checkBooleanBinaryOp();
        auto b = vm->pop();
        auto a = vm->pop();

        bool resultA = isValueTrue(a);
        bool resultB = isValueTrue(b);
        vm->push(BOOL_VAL(resultA or resultB));
        vm->releaseAndDelete(a);
        vm->releaseAndDelete(b);
    }

    void op_jump_if_false(VM* vm)
    {
        uint16_t offset = (static_cast<uint16_t>(*vm->currentFrame->ip++) << 8);
        offset |= static_cast<uint16_t>(*vm->currentFrame->ip++);

        Value condition = vm->peek();

        if (not isValueTrue(condition, vm))  // Pass VM for proper string evaluation
        {
            vm->currentFrame->ip += offset;
        }
    }

    void op_jump(VM* vm)
    {
        uint16_t offset = (static_cast<uint16_t>(*vm->currentFrame->ip++) << 8);
        offset |= static_cast<uint16_t>(*vm->currentFrame->ip++);

        vm->currentFrame->ip += offset;
    }

    void op_loop(VM* vm)
    {
        uint16_t offset = (static_cast<uint16_t>(*vm->currentFrame->ip++) << 8);
        offset |= static_cast<uint16_t>(*vm->currentFrame->ip++);

        vm->currentFrame->ip -= offset;
    }

    void op_get_upvalue(VM* vm)
    {
        uint8_t slot = *vm->currentFrame->ip++;

        if (slot >= vm->currentFrame->closure->function->upvalueCount)
        {
            vm->runtimeError("Upvalue index out of bounds.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }

        ObjUpvalue* upvalue = vm->currentFrame->closure->upvalues[slot];

        // Check if upvalue is closed (location points to &closed)
        if (upvalue->location == &upvalue->closed)
        {
            // For closed upvalues, just copy the value - don't retain it
            // The value is owned by the upvalue object itself
            vm->push(*upvalue->location);
        } else
        {
            // For open upvalues, retain the value since it's on the stack
            vm->push(vm->retainValue(*upvalue->location));
        }
    }

    void op_set_upvalue(VM* vm)
    {
        uint8_t slot = *vm->currentFrame->ip++;
#ifdef DEBUG_CHECK_STACK
        if (vm->stack.empty())
        {
            vm->runtimeError("Not enough values on stack for upvalue assignment.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }
#endif
        ObjUpvalue* upvalue = vm->currentFrame->closure->upvalues[slot];
        upvalue->location = &vm->stack[vm->stack.size() - 1 - 0];
    }

    void op_close_upvalue(VM* vm)
    {
#ifdef DEBUG_CHECK_STACK
        if (vm->stack.empty())
        {
            vm->runtimeError("Stack underflow on closing upvalue.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }
#endif
        vm->closeUpvalues(&vm->stack[vm->stack.size() - 1]);
        auto value = vm->pop();
        vm->releaseAndDelete(value);
    }

    void op_post_incr_global(VM* vm)
    {
#ifdef DEBUG_CHECK_STACK
        if (vm->stack.size() < 1)
        {
            vm->runtimeError("Stack underflow on post-increment.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }
#endif
        auto nameValue = vm->pop();
        auto name = vm->valueToElement(nameValue);

        if (not name.isLitteral())
        {
            vm->releaseAndDelete(nameValue);
            vm->runtimeError("Global variable name must be a litteral.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }

        auto it = vm->globals.find(name.toString());
        if (it == vm->globals.end())
        {
            vm->releaseAndDelete(nameValue);
            vm->runtimeError("Undefined global variable '" + name.toString() + "'.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }

        if (not isValueNumber(it->second))
        {
            vm->releaseAndDelete(nameValue);
            vm->runtimeError("Operand after an unary (++) must be a number.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }

        auto newValue = vm->addValues(it->second, INT_VAL(1));
        vm->releaseAndDelete(it->second);
        it->second = vm->retainValue(newValue);

        vm->releaseAndDelete(nameValue);
    }

    void op_incr_global(VM* vm)
    {
#ifdef DEBUG_CHECK_STACK
        if (vm->stack.empty())
        {
            vm->runtimeError("Stack underflow on increment.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }
#endif
        auto nameValue = vm->pop();
        auto name = vm->valueToElement(nameValue);

        if (not name.isLitteral())
        {
            vm->releaseAndDelete(nameValue);
            vm->runtimeError("Global variable name must be a litteral.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }

        auto it = vm->globals.find(name.toString());
        if (it == vm->globals.end())
        {
            vm->releaseAndDelete(nameValue);
            vm->runtimeError("Undefined global variable '" + name.toString() + "'.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }

        if (not isValueNumber(it->second))
        {
            vm->releaseAndDelete(nameValue);
            vm->runtimeError("Operand after an unary (++) must be a number.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }

        auto newValue = vm->addValues(it->second, INT_VAL(1));
        vm->releaseAndDelete(it->second);
        it->second = vm->retainValue(newValue);

        vm->push(vm->retainValue(newValue));
        vm->releaseAndDelete(nameValue);
    }

    void op_post_decr_global(VM* vm)
    {
#ifdef DEBUG_CHECK_STACK
        if (vm->stack.size() < 1)
        {
            vm->runtimeError("Stack underflow on post-decrement.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }
#endif
        auto nameValue = vm->pop();
        auto name = vm->valueToElement(nameValue);

        if (not name.isLitteral())
        {
            vm->releaseAndDelete(nameValue);
            vm->runtimeError("Global variable name must be a litteral.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }

        auto it = vm->globals.find(name.toString());
        if (it == vm->globals.end())
        {
            vm->releaseAndDelete(nameValue);
            vm->runtimeError("Undefined global variable '" + name.toString() + "'.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }

        if (not isValueNumber(it->second))
        {
            vm->releaseAndDelete(nameValue);
            vm->runtimeError("Operand after an unary (--) must be a number.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }

        auto newValue = vm->subtractValues(it->second, INT_VAL(1));
        vm->releaseAndDelete(it->second);
        it->second = vm->retainValue(newValue);

        vm->releaseAndDelete(nameValue);
    }

    void op_decr_global(VM* vm)
    {
#ifdef DEBUG_CHECK_STACK
        if (vm->stack.empty())
        {
            vm->runtimeError("Stack underflow on decrement.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }
#endif
        auto nameValue = vm->pop();
        auto name = vm->valueToElement(nameValue);

        if (not name.isLitteral())
        {
            vm->releaseAndDelete(nameValue);
            vm->runtimeError("Global variable name must be a litteral.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }

        auto it = vm->globals.find(name.toString());
        if (it == vm->globals.end())
        {
            vm->releaseAndDelete(nameValue);
            vm->runtimeError("Undefined global variable '" + name.toString() + "'.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }

        if (not isValueNumber(it->second))
        {
            vm->releaseAndDelete(nameValue);
            vm->runtimeError("Operand after an unary (--) must be a number.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }

        auto newValue = vm->subtractValues(it->second, INT_VAL(1));
        vm->releaseAndDelete(it->second);
        it->second = vm->retainValue(newValue);

        vm->push(vm->retainValue(newValue));
        vm->releaseAndDelete(nameValue);
    }

    void op_post_incr_local(VM* vm)
    {
#ifdef DEBUG_CHECK_STACK
        if (vm->stack.size() < 1)
        {
            vm->runtimeError("Not enough values on stack for local post-increment.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }
#endif
        auto slot = vm->pop();

        if (not isValueNumber(slot))
        {
            vm->releaseAndDelete(slot);
            vm->runtimeError("Local variable slot must be a number.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }

        int index = vm->getValueAsInt(slot);
        vm->releaseAndDelete(slot);

        if (index < 0)
        {
            vm->runtimeError("Local variable index cannot be negative.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }

        // Calculate the absolute stack index from the frame-relative index
        size_t stackIndex = (vm->currentFrame->slots - vm->stack.data()) + index;

        if (not isValueNumber(vm->stack[stackIndex]))
        {
            vm->runtimeError("Operand after an unary (++) must be a number.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }

        auto& val = vm->stack[stackIndex];

        // Old value is already on the stack (from OP_Get_Local before this opcode)
        // We just need to increment the variable in its slot
        auto newValue = vm->addValues(val, INT_VAL(1));
        vm->releaseAndDelete(val);
        val = newValue;
    }

    void op_incr_local(VM* vm)
    {
#ifdef DEBUG_CHECK_STACK
        if (vm->stack.size() < 1)
        {
            vm->runtimeError("Not enough values on stack for local increment.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }
#endif
        auto slot = vm->pop();

        if (not isValueNumber(slot))
        {
            vm->releaseAndDelete(slot);
            vm->runtimeError("Local variable slot must be a number.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }

        int index = vm->getValueAsInt(slot);
        if (index < 0)
        {
            vm->releaseAndDelete(slot);
            vm->runtimeError("Local variable index cannot be negative.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }

        if (not isValueNumber(vm->currentFrame->slots[index]))
        {
            vm->releaseAndDelete(slot);
            vm->runtimeError("Operand after an unary (++) must be a number.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }

        // Calculate the absolute stack index from the frame-relative index
        size_t stackIndex = (vm->currentFrame->slots - vm->stack.data()) + index;

        auto newValue = vm->addValues(vm->stack[stackIndex], INT_VAL(1));
        vm->releaseAndDelete(vm->stack[stackIndex]);
        vm->stack[stackIndex] = newValue;

        vm->push(vm->retainValue(newValue));
        vm->releaseAndDelete(slot);
    }

    void op_post_decr_local(VM* vm)
    {
#ifdef DEBUG_CHECK_STACK
        if (vm->stack.size() < 1)
        {
            vm->runtimeError("Not enough values on stack for local post-decrement.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }
#endif
        auto slot = vm->pop();

        if (not isValueNumber(slot))
        {
            vm->releaseAndDelete(slot);
            vm->runtimeError("Local variable slot must be a number.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }

        int index = vm->getValueAsInt(slot);
        if (index < 0)
        {
            vm->releaseAndDelete(slot);
            vm->runtimeError("Local variable index cannot be negative.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }

        // Calculate the absolute stack index from the frame-relative index
        size_t stackIndex = (vm->currentFrame->slots - vm->stack.data()) + index;

        if (not isValueNumber(vm->stack[stackIndex]))
        {
            vm->releaseAndDelete(slot);
            vm->runtimeError("Operand after an unary (--) must be a number.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }

        // Old value is already on the stack (from OP_Get_Local before this opcode)
        // We just need to decrement the variable in its slot
        auto newValue = vm->subtractValues(vm->stack[stackIndex], INT_VAL(1));
        vm->releaseAndDelete(vm->stack[stackIndex]);
        vm->stack[stackIndex] = newValue;

        vm->releaseAndDelete(slot);
    }

    void op_decr_local(VM* vm)
    {
#ifdef DEBUG_CHECK_STACK
        if (vm->stack.size() < 1)
        {
            vm->runtimeError("Not enough values on stack for local decrement.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }
#endif
        auto slot = vm->pop();

        if (not isValueNumber(slot))
        {
            vm->releaseAndDelete(slot);
            vm->runtimeError("Local variable slot must be a number.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }

        int index = vm->getValueAsInt(slot);
        if (index < 0)
        {
            vm->releaseAndDelete(slot);
            vm->runtimeError("Local variable index cannot be negative.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }

        if (not isValueNumber(vm->currentFrame->slots[index]))
        {
            vm->releaseAndDelete(slot);
            vm->runtimeError("Operand after an unary (--) must be a number.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }

        // Calculate the absolute stack index from the frame-relative index
        size_t stackIndex = (vm->currentFrame->slots - vm->stack.data()) + index;

        auto newValue = vm->subtractValues(vm->stack[stackIndex], INT_VAL(1));
        vm->releaseAndDelete(vm->stack[stackIndex]);
        vm->stack[stackIndex] = newValue;

        vm->push(vm->retainValue(newValue));
        vm->releaseAndDelete(slot);
    }

    void op_class(VM* vm)
    {
        uint8_t constantIndex = *vm->currentFrame->ip++;

        auto classNameValue = vm->currentFrame->closure->function->chunk.constants[constantIndex];

        ElementType classNameElem = vm->valueToElement(classNameValue);
        if (not classNameElem.isLitteral())
        {
            vm->runtimeError("Class name must be a litteral.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);

            return;
        }

        std::string className = classNameElem.toString();
        auto newClass = vm->createClass(className);  // Already tracked in createClass
        vm->push(newClass);
    }

    bool bindMethod(VM* vm, Klass* klass, const std::string& name)
    {
        auto methodIt = klass->methods.find(name);
        if (methodIt == klass->methods.end())
        {
            return false;
        }

        auto methodValue = methodIt->second;

        // Handle closures (script-defined methods) - create bound method
        if (IS_CLOSURE(methodValue))
        {
            auto bound = vm->createBoundMethod(vm->peek(0), vm->asClosure(methodValue));  // Already tracked

            auto instance = vm->pop(); // Remove the instance
            vm->releaseAndDelete(instance);
            vm->push(bound);

            return true;
        }

        // Handle native functions (C++-defined methods) - return native function directly
        // Native functions will receive the receiver as the first argument when called
        if (IS_NAT_FUNC(methodValue))
        {
            auto instance = vm->pop(); // Remove the instance
            vm->releaseAndDelete(instance);
            vm->push(vm->retainValue(methodValue));  // Push the native function directly

            return true;
        }

        return false;
    }

    void op_get_property(VM* vm)
    {
        if (not IS_INSTANCE(vm->peek(0)))
        {
            vm->runtimeError("Only instances have properties.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);

            return;
        }

        auto* instance = vm->asInstance(vm->peek(0));

        uint8_t constantIndex = *vm->currentFrame->ip++;
        auto nameValue = vm->currentFrame->closure->function->chunk.constants[constantIndex];
        auto name = vm->valueToElement(nameValue);

        if (not name.isLitteral())
        {
            vm->runtimeError("Property name must be a litteral.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);

            return;
        }

        auto nameStr = name.toString();

        // Try to find a field first
        if (instance->fields.find(nameStr) != instance->fields.end())
        {
            auto inst = vm->pop(); // Remove the instance from the stack
            vm->releaseAndDelete(inst);
            vm->push(vm->retainValue(instance->fields[nameStr]));

            return;
        }

        // Check for __get metamethod in class methods OR instance fields
        Value getMethod;
        bool hasGetMethod = false;
        bool isDynamicGet = false;

        // First check class methods
        auto getMetaIt = instance->klass->methods.find("__get");
        if (getMetaIt != instance->klass->methods.end())
        {
            getMethod = getMetaIt->second;
            hasGetMethod = true;
        }
        else
        {
            // Also check instance fields for __get (for dynamic metamethods)
            auto fieldIt = instance->fields.find("__get");
            if (fieldIt != instance->fields.end())
            {
                getMethod = fieldIt->second;
                hasGetMethod = true;
                isDynamicGet = true;
            }
        }

        if (hasGetMethod and (IS_CLOSURE(getMethod) or IS_NAT_FUNC(getMethod)))
        {
            // Call __get(instance, propertyName)
            if (IS_CLOSURE(getMethod))
            {
                // Push property name as argument
                vm->push(nameValue);

                if (isDynamicGet)
                {
                    // Call the __get method as a closure with stack [inst, val]
                    if (not vm->call(vm->asClosure(getMethod), 2))
                    {
                        vm->runtimeError("Cannot call __get metamethod.");
                        vm->vm_return(InterpretResult::RUNTIME_ERROR);
                        return;
                    }

                }
                else
                {
                    // Call the __get method as a bound method (instance is already on stack)
                    if (not vm->callBound(vm->asClosure(getMethod), 1))
                    {
                        vm->runtimeError("Cannot call __get metamethod.");
                        vm->vm_return(InterpretResult::RUNTIME_ERROR);
                        return;
                    }
                }

                // No need to pop the values as callValue handles that

                // Function has been called, frame is set up with stackBase pointing to where to truncate on return
                vm->currentFrame = &vm->frames[vm->frameCount - 1];
                vm->updateChunkCache(); // Update cached chunk data for new frame
                return;
            }
            else if (IS_NAT_FUNC(getMethod))
            {
                auto* native = vm->asNativeFunc(getMethod);

                // Call native __get(instance, propertyName)
                // Stack: [instance] -> args[0]=instance, args[1]=propertyName
                vm->push(nameValue);  // Push property name
                Value result = native->function(vm, 2, vm->stack.data() + vm->stack.size() - 2);

                // Remove arguments from stack
                auto propName = vm->pop();
                auto inst = vm->pop();
                vm->releaseAndDelete(propName);
                vm->releaseAndDelete(inst);

                vm->push(result);
                return;
            }
        }

        // Try to find a method in the class
        if (not bindMethod(vm, instance->klass, nameStr))
        {
            vm->runtimeError("Undefined property '" + nameStr + "'.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
        }
    }

    void op_set_property(VM *vm)
    {
        if (not IS_INSTANCE(vm->peek(1)))
        {
            vm->runtimeError("Only instances have fields.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);

            return;
        }

        auto* instance = vm->asInstance(vm->peek(1));

        uint8_t constantIndex = *vm->currentFrame->ip++;
        auto nameValue = vm->currentFrame->closure->function->chunk.constants[constantIndex];
        auto name = vm->valueToElement(nameValue);

        if (not name.isLitteral())
        {
            vm->runtimeError("Field name must be a litteral.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);

            return;
        }

        auto nameStr = name.toString();

        // Check for __set metamethod in class methods OR instance fields
        Value setMethod;
        bool hasSetMethod = false;

        // First check class methods
        auto setMetaIt = instance->klass->methods.find("__set");
        if (setMetaIt != instance->klass->methods.end())
        {
            setMethod = setMetaIt->second;
            hasSetMethod = true;
        }
        else
        {
            // Also check instance fields for __set (for dynamic metamethods)
            auto fieldIt = instance->fields.find("__set");
            if (fieldIt != instance->fields.end())
            {
                setMethod = fieldIt->second;
                hasSetMethod = true;
            }
        }

        if (hasSetMethod)
        {
            // Call __set(instance, propertyName, value)

            if (IS_CLOSURE(setMethod))
            {
                // Stack is currently: [instance, value]
                // We need: [instance, propertyName, value]
                Value value = vm->pop();     // Pop value
                // instance is still on stack

                vm->push(nameValue);  // Push property name
                vm->push(value);      // Push value

                // Call __set(instance, propertyName, value)
                if (vm->callValue(setMethod, 2))
                {
                    // __set was called successfully, result is on stack
                    return;
                }
                else
                {
                    vm->vm_return(InterpretResult::RUNTIME_ERROR);
                    return;
                }
            }
            else if (IS_NAT_FUNC(setMethod))
            {
                auto* native = vm->asNativeFunc(setMethod);

                // Stack: [instance, value]
                // Call native __set(instance, propertyName, value)
                Value value = vm->peek(0);  // Get value (keep on stack)

                vm->push(nameValue);  // Push property name
                vm->push(value);      // Push value again

                // Now stack: [instance, value, propertyName, value]
                // Call with args[0]=instance, args[1]=propertyName, args[2]=value
                Value result = native->function(vm, 3, vm->stack.data() + vm->stack.size() - 4);

                // Clean up stack: remove [value, propertyName, value]
                vm->pop(); // value (duplicate)
                vm->pop(); // propertyName
                vm->pop(); // value (original)
                auto inst = vm->pop(); // instance
                vm->releaseAndDelete(inst);

                vm->push(result); // Push result (usually the value that was set)
                return;
            }
        }

        // No __set metamethod, do normal field assignment
        auto value = vm->pop(); // Value to set
        auto inst = vm->pop(); // Instance
        vm->releaseAndDelete(inst);

        // Todo maybe fix
        if (instance->fields.find(nameStr) != instance->fields.end())
        {
            vm->releaseAndDelete(instance->fields[nameStr]);
        }

        instance->fields[nameStr] = vm->retainValue(value);
        vm->push(value);
    }

    void op_method(VM* vm)
    {
        uint8_t constantIndex = *vm->currentFrame->ip++;

        auto methodNameValue = vm->currentFrame->closure->function->chunk.constants[constantIndex];
        ElementType methodNameElem = vm->valueToElement(methodNameValue);
        if (not methodNameElem.isLitteral())
        {
            vm->runtimeError("Method name must be a litteral.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);

            return;
        }

        std::string methodName = methodNameElem.toString();

        // The class is below the method closure/native function on the stack
        auto methodValue = vm->pop();
        auto classValue = vm->peek();

        if (not IS_CLASS(classValue))
        {
            vm->runtimeError("Method definition must be on a class.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);

            return;
        }

        Klass* klass = vm->asClass(classValue);

        // Methods can be either closures (script-defined) or native functions (C++-defined)
        if (not IS_CLOSURE(methodValue) and not IS_NAT_FUNC(methodValue))
        {
            vm->runtimeError("Method must be a closure or native function.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);

            return;
        }

        klass->methods[methodName] = methodValue;
    }

    void op_short_int(VM* vm)
    {
        uint8_t value = *vm->currentFrame->ip++;

        vm->push(makeIntValue(value));
    }

    void op_pop_n(VM* vm)
    {
        uint8_t count = *vm->currentFrame->ip++;

        for (int i = 0; i < count; i++)
        {
            auto value = vm->pop();

            if (requiresRefCount(value)) {
                vm->releaseAndDelete(value);
            }
        }
    }

    void op_define_constant_global(VM* vm)
    {
        uint8_t constant1 = *vm->currentFrame->ip++;

        auto value1 = vm->currentFrame->closure->function->chunk.constants[constant1];

        uint8_t constant2 = *vm->currentFrame->ip++;

        auto value2 = vm->currentFrame->closure->function->chunk.constants[constant2];
        auto name = vm->valueToElement(value2);

        if (not name.isLitteral())
        {
            vm->runtimeError("Global variable name must be a litteral.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }

        vm->globals[name.toString()] = vm->retainValue(value1);
    }

    void op_get_constant_global(VM* vm)
    {
        uint8_t constant = *vm->currentFrame->ip++;
        auto value = vm->currentFrame->closure->function->chunk.constants[constant];
        auto name = vm->valueToElement(value);

        if (not name.isLitteral())
        {
            vm->runtimeError("Global variable name must be a litteral.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }

        auto it = vm->globals.find(name.toString());
        if (it == vm->globals.end())
        {
            vm->runtimeError("Undefined global variable '" + name.toString() + "'.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }

        vm->push(vm->retainValue(it->second)); // Retain because stack becomes an owner
    }

    void op_set_constant_global(VM* vm)
    {
        uint8_t constant1 = *vm->currentFrame->ip++;

        auto value1 = vm->currentFrame->closure->function->chunk.constants[constant1];

        uint8_t constant2 = *vm->currentFrame->ip++;

        auto value2 = vm->currentFrame->closure->function->chunk.constants[constant2];
        auto name = vm->valueToElement(value2);

        if (not name.isLitteral())
        {
            vm->runtimeError("Global variable name must be a litteral.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }

        auto it = vm->globals.find(name.toString());
        if (it == vm->globals.end())
        {
            vm->runtimeError("Undefined global variable '" + name.toString() + "'.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }

        vm->releaseAndDelete(it->second);
        it->second = vm->retainValue(value1);
        vm->push(vm->retainValue(value1));
    }

    void op_add_ll(VM* vm)
    {
        uint8_t local1 = *vm->currentFrame->ip++;

        auto value1 = vm->currentFrame->slots[local1];

        uint8_t local2 = *vm->currentFrame->ip++;

        auto value2 = vm->currentFrame->slots[local2];

        auto result = vm->addValues(value1, value2);

        vm->push(result);
    }

    void op_subtract_ll(VM* vm)
    {
        uint8_t local1 = *vm->currentFrame->ip++;

        auto value1 = vm->currentFrame->slots[local1];

        uint8_t local2 = *vm->currentFrame->ip++;

        auto value2 = vm->currentFrame->slots[local2];

        auto result = vm->subtractValues(value1, value2);

        vm->push(result);
    }

    void op_subtract_lc(VM* vm)
    {
        uint8_t local1 = *vm->currentFrame->ip++;

        auto value1 = vm->currentFrame->slots[local1];

        uint8_t constantIndex = *vm->currentFrame->ip++;

        auto value2 = vm->currentFrame->closure->function->chunk.constants[constantIndex];

        auto result = vm->subtractValues(value1, value2);

        vm->push(result);
    }

    void op_subtract_cl(VM* vm)
    {
        uint8_t constantIndex = *vm->currentFrame->ip++;

        auto value1 = vm->currentFrame->closure->function->chunk.constants[constantIndex];

        uint8_t local2 = *vm->currentFrame->ip++;

        auto value2 = vm->currentFrame->slots[local2];

        auto result = vm->subtractValues(value1, value2);

        vm->push(result);
    }

    // ========================================================================
    // VM Helper Method Implementations
    // ========================================================================

    Value VM::createString(const ElementType& element)
    {
        // Convert ElementType to string for lookup (assuming ElementType has toString() or similar)
        std::string stringContent = element.toString();

        return createString(stringContent);
    }

    Value VM::createString(const std::string& stringContent)
    {
        // Small string optimization: inline strings with 5 or fewer characters
        if (stringContent.length() <= 5)
        {
            return makeSmallStringValue(stringContent.c_str(), static_cast<uint8_t>(stringContent.length()));
        }

        // // Check if string already exists in the intern map
        auto it = pools.internedStrings.find(stringContent);
        if (it != pools.internedStrings.end())
        {
            // String already exists, reuse it
            return retainValue(makeStringValue(it->second));
        }

        // String doesn't exist, create new one
        // IMPORTANT: Always store as STRING type, not whatever type the input ElementType had
        ElementType stringElement(stringContent);
        auto [ptr, index] = pools.stringPool.allocateWithIndex(stringElement);
        Value val = makeStringValue(static_cast<uint32_t>(index));

        // Add to intern map for future reuse
        pools.internedStrings[stringContent] = static_cast<uint32_t>(index);

        return trackNewValue(val);
    }

    Value VM::createClosure(ObjFunction* function)
    {
        auto [ptr, index] = pools.closurePool.allocateWithIndex(function);
        Value val = makeClosureValue(static_cast<uint32_t>(index));
        return trackNewValue(val);
    }

    Value VM::createFunction()
    {
        auto [ptr, index] = pools.functionPool.allocateWithIndex();
        Value val = makeFunctionValue(static_cast<uint32_t>(index));
        return trackNewValue(val);
    }

    Value VM::createUpvalue(Value* slot)
    {
        auto [ptr, index] = pools.upvaluePool.allocateWithIndex(slot);
        Value val = makeUpvalueValue(static_cast<uint32_t>(index));
        return trackNewValue(val);
    }

    Value VM::createClass(const std::string& name)
    {
        auto [ptr, index] = pools.classPool.allocateWithIndex(name);
        Value val = makeClassValue(static_cast<uint32_t>(index));
        return trackNewValue(val);
    }

    Value VM::createInstance(Klass* klass)
    {
        auto [ptr, index] = pools.instancePool.allocateWithIndex(klass);
        Value val = makeInstanceValue(static_cast<uint32_t>(index));
        return trackNewValue(val);
    }

    Value VM::createBoundMethod(const Value& receiver, Closure* method)
    {
        auto [ptr, index] = pools.boundMethodPool.allocateWithIndex(receiver, method);
        Value val = makeBoundMethodValue(static_cast<uint32_t>(index));
        return trackNewValue(val);
    }

    Value VM::createVector()
    {
        auto [ptr, index] = pools.vectorPool.allocateWithIndex();
        Value val = makeVectorValue(static_cast<uint32_t>(index));
        return trackNewValue(val);
    }

    Value VM::elementToValue(const ElementType& element)
    {
        if (element.isBool())
            return makeBoolValue(element.get<bool>());
        else if (element.type == ElementType::UnionType::INT)
        {
            int intVal = element.get<int>();
            return makeIntValue(static_cast<int64_t>(intVal));
        }
        else if (element.type == ElementType::UnionType::FLOAT)
        {
            float floatVal = element.get<float>();
            return makeDoubleValue(static_cast<double>(floatVal));
        }
        else if (element.type == ElementType::UnionType::DOUBLE)
        {
            double doubleVal = element.get<double>();
            return makeDoubleValue(doubleVal);
        }
        else
        {
            // Strings and complex types go to string pool
            return createString(element);
        }
    }

    ElementType VM::valueToElement(const Value& value)
    {
        if (IS_BOOL(value))
            return ElementType(AS_BOOL(value));
        else if (IS_INT(value))
            return ElementType(static_cast<int>(AS_INT(value)));
        else if (IS_DOUBLE(value))
            return ElementType(AS_DOUBLE(value));
        else if (IS_SMALL_STRING(value))
            return ElementType(AS_SMALL_STRING(value));
        else if (IS_LONG_STRING(value))
            return *asStringPtr(value);
        else
            throw std::runtime_error("Cannot convert Value to ElementType - unsupported type");
    }

    Value VM::copyValue(const Value& value)
    {
        // Primitives and doubles can be copied directly (no heap allocation)
        if (IS_INT(value) || IS_BOOL(value) || IS_DOUBLE(value))
            return value;

        // For heap objects with reference counting, just retain and return
        // (we use reference counting, not deep copying)
        if (requiresRefCount(value))
        {
            return retainValue(value);
        }

        return value;
    }

    int VM::getValueAsInt(const Value& value)
    {
        if (IS_INT(value))
            return static_cast<int>(AS_INT(value));
        else if (IS_DOUBLE(value))
            return static_cast<int>(AS_DOUBLE(value));
        else if (IS_BOOL(value))
            return AS_BOOL(value) ? 1 : 0;
        else if (IS_LONG_STRING(value))
        {
            ElementType* obj = asStringPtr(value);
            if (obj->type == ElementType::UnionType::INT)
                return obj->get<int>();
        }
        else if (IS_SMALL_STRING(value))
        {
            // Small strings don't have ElementType backing, can't extract int
            // Fall through to error
        }

        throw std::runtime_error("Value is not an integer");
    }

    // ========================================================================
    // Table Operations
    // ========================================================================

    void op_build_table(VM* vm)
    {
        uint8_t pairCount = *vm->currentFrame->ip++;

        // Get the built-in Table class
        auto it = vm->globals.find("__Table");
        if (it == vm->globals.end())
        {
            vm->runtimeError("Table class not found - was initializeTableClass() called?");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }

        Value tableClassVal = it->second;
        if (!IS_CLASS(tableClassVal))
        {
            vm->runtimeError("Table is not a class");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }

        Klass* tableClass = vm->asClass(tableClassVal);

        // Create new instance of Table
        Value instanceVal = vm->createInstance(tableClass);
        ObjInstance* table = vm->asInstance(instanceVal);

        // Pop pairCount key-value pairs from stack (in reverse)
        std::vector<std::pair<std::string, Value>> pairs;
        pairs.reserve(pairCount);

        for (int i = 0; i < pairCount; i++)
        {
            Value key = vm->pop();
            Value value = vm->pop();

            // Convert key to string
            std::string keyStr;
            if (IS_STRING(key))
            {
                keyStr = vm->asString(key);
            }
            else if (IS_INT(key))
            {
                keyStr = std::to_string(AS_INT(key));
            }
            else
            {
                vm->releaseAndDelete(key);
                vm->releaseAndDelete(value);
                vm->runtimeError("Table key must be string or integer");
                vm->vm_return(InterpretResult::RUNTIME_ERROR);
                return;
            }

            vm->releaseAndDelete(key);  // We've converted it to string, release original
            pairs.push_back({keyStr, value});
        }

        // Insert pairs in correct order (we popped in reverse)
        for (auto it = pairs.rbegin(); it != pairs.rend(); ++it)
        {
            table->fields[it->first] = vm->retainValue(it->second);
            vm->releaseAndDelete(it->second);  // Release our temporary reference
        }

        // Push the table instance we created
        vm->push(instanceVal);
    }

    void op_build_vector(VM* vm)
    {
        uint8_t pairCount = *vm->currentFrame->ip++;

        // Create new vector
        Value vectorVal = vm->createVector();
        ObjVector* vector = vm->asVector(vectorVal);

        // Pop pairCount key-value pairs from stack (in reverse order)
        // Stack layout: [value, index, value, index, ...]
        std::vector<std::pair<int64_t, Value>> pairs;
        pairs.reserve(pairCount);

        for (int i = 0; i < pairCount; i++)
        {
            Value index = vm->pop();
            Value value = vm->pop();

            // Index must be an integer
            if (!IS_INT(index))
            {
                vm->releaseAndDelete(index);
                vm->releaseAndDelete(value);
                // Clean up the vector we created before returning
                vm->releaseAndDelete(vectorVal);
                vm->runtimeError("Vector index must be an integer");
                vm->vm_return(InterpretResult::RUNTIME_ERROR);
                return;
            }

            int64_t indexInt = AS_INT(index);
            vm->releaseAndDelete(index);  // We've extracted the int, release the value
            pairs.push_back({indexInt, value});
        }

        // Sort pairs by index to ensure correct order
        std::sort(pairs.begin(), pairs.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });

        // Insert values in sorted order
        for (const auto& pair : pairs)
        {
            // Ensure vector is large enough
            while (vector->fields.size() <= static_cast<size_t>(pair.first))
            {
                vector->fields.push_back(makeIntValue(0));  // Fill with zeros
            }

            vector->fields[pair.first] = vm->retainValue(pair.second);
            vm->releaseAndDelete(pair.second);  // Release our temporary reference
        }

        // Push the vector we created
        vm->push(vectorVal);
    }

    void op_get_index(VM* vm)
    {
        Value index = vm->pop();
        Value target = vm->pop();

        // Handle vector indexing
        if (IS_VECTOR(target))
        {
            if (!IS_INT(index))
            {
                vm->releaseAndDelete(index);
                vm->releaseAndDelete(target);
                vm->runtimeError("Vector index must be an integer");
                vm->vm_return(InterpretResult::RUNTIME_ERROR);
                return;
            }

            ObjVector* vec = vm->asVector(target);
            int idx = AS_INT(index);

            // Handle negative indices (Python-style)
            if (idx < 0)
            {
                idx = static_cast<int>(vec->fields.size()) + idx;
            }

            if (idx < 0 || idx >= static_cast<int>(vec->fields.size()))
            {
                vm->releaseAndDelete(target);
                vm->runtimeError("Vector index out of bounds");
                vm->vm_return(InterpretResult::RUNTIME_ERROR);
                return;
            }

            vm->releaseAndDelete(target);

            // Return the value at the index
            vm->push(vm->retainValue(vec->fields[idx]));
            return;
        }

        // Handle string indexing (both long and small strings)
        if (IS_STRING(target))
        {
            if (!IS_INT(index))
            {
                vm->releaseAndDelete(index);
                if (IS_LONG_STRING(target)) vm->releaseAndDelete(target);
                vm->runtimeError("String index must be an integer");
                vm->vm_return(InterpretResult::RUNTIME_ERROR);
                return;
            }

            std::string str = vm->asString(target);

            int idx = AS_INT(index);

            // Handle negative indices (Python-style)
            if (idx < 0)
            {
                idx = static_cast<int>(str.length()) + idx;
            }

            if (idx < 0 || idx >= static_cast<int>(str.length()))
            {
                if (IS_LONG_STRING(target)) vm->releaseAndDelete(target);
                vm->runtimeError("String index out of bounds");
                vm->vm_return(InterpretResult::RUNTIME_ERROR);
                return;
            }

            if (IS_LONG_STRING(target)) vm->releaseAndDelete(target);

            // Return single character as a string using cached value
            unsigned char ch = static_cast<unsigned char>(str[idx]);
            vm->push(vm->retainValue(vm->singleCharCache[ch]));
            return;
        }

        // Handle table/instance indexing
        if (!IS_INSTANCE(target))
        {
            vm->releaseAndDelete(index);
            vm->releaseAndDelete(target);
            vm->runtimeError("Can only index vectors, strings, or tables/instances");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }

        ObjInstance* inst = vm->asInstance(target);

        // Convert index to string key
        std::string key;
        if (IS_INT(index))
        {
            key = std::to_string(AS_INT(index));
        }
        else if (IS_STRING(index))
        {
            key = vm->asString(index);
        }
        else
        {
            vm->releaseAndDelete(index);
            vm->releaseAndDelete(target);
            vm->runtimeError("Index must be integer or string");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }

        // Look up in fields map first
        auto it = inst->fields.find(key);
        if (it != inst->fields.end())
        {
            vm->releaseAndDelete(index);
            vm->releaseAndDelete(target);
            vm->push(vm->retainValue(it->second));
            return;
        }

        // Check for __get metamethod if property not found in fields
        if (inst->klass)
        {
            auto getMetaIt = inst->klass->methods.find("__get");
            if (getMetaIt != inst->klass->methods.end())
            {
                Value getMethod = getMetaIt->second;

                if (IS_NAT_FUNC(getMethod))
                {
                    auto* native = vm->asNativeFunc(getMethod);

                    // Call __get(instance, key)
                    // Stack setup: args[0]=instance, args[1]=key
                    Value keyValue = vm->createString(ElementType{key});

                    vm->push(target);   // Push instance
                    vm->push(keyValue); // Push key as string

                    Value result = native->function(vm, 2, vm->stack.data() + vm->stack.size() - 2);

                    // Clean up stack
                    vm->pop(); // keyValue
                    vm->pop(); // target
                    vm->releaseAndDelete(keyValue);
                    vm->releaseAndDelete(index);
                    vm->releaseAndDelete(target);

                    vm->push(result);
                    return;
                }
            }
        }

        // Property not found and no __get metamethod
        vm->releaseAndDelete(index);
        vm->releaseAndDelete(target);
        vm->push(BOOL_VAL(false));  // Or NIL_VAL if you have it
    }

    void op_set_index(VM* vm)
    {
        Value value = vm->pop();
        Value index = vm->pop();
        Value target = vm->peek(0); // Keep target on stack

        if (!IS_INSTANCE(target) && !IS_STRING(target) && !IS_VECTOR(target))
        {
            vm->releaseAndDelete(value);
            vm->releaseAndDelete(index);
            vm->runtimeError("Can only index vectors, tables/instances, or strings");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }

        // Handle vector indexing
        if (IS_VECTOR(target))
        {
            if (!IS_INT(index))
            {
                vm->releaseAndDelete(value);
                vm->releaseAndDelete(index);
                vm->runtimeError("Vector index must be an integer");
                vm->vm_return(InterpretResult::RUNTIME_ERROR);
                return;
            }

            ObjVector* vec = vm->asVector(target);
            int idx = AS_INT(index);

            // Handle negative indices (Python-style)
            if (idx < 0)
            {
                idx = static_cast<int>(vec->fields.size()) + idx;
            }

            // Allow setting at the very end to push back
            if (idx == static_cast<int>(vec->fields.size()))
            {
                vm->releaseAndDelete(index);
                vec->fields.push_back(vm->retainValue(value));
                vm->releaseAndDelete(value);
                return;
            }

            if (idx < 0 || idx > static_cast<int>(vec->fields.size()))
            {
                vm->releaseAndDelete(value);
                vm->releaseAndDelete(index);
                vm->runtimeError("Vector index out of bounds");
                vm->vm_return(InterpretResult::RUNTIME_ERROR);
                return;
            }

            vm->releaseAndDelete(index);

            // Release old value at this index
            vm->releaseAndDelete(vec->fields[idx]);

            // Store new value
            vec->fields[idx] = vm->retainValue(value);
            vm->releaseAndDelete(value);  // Release our reference (vector now owns it)
            return;
        }

        // Handle instance/table indexing (most common case)
        if (IS_INSTANCE(target))
        {
            ObjInstance* inst = vm->asInstance(target);

            // Convert index to string key
            std::string key;
            if (IS_INT(index))
            {
                key = std::to_string(AS_INT(index));
            }
            else if (IS_STRING(index))
            {
                key = vm->asString(index);
            }
            else
            {
                vm->releaseAndDelete(value);
                vm->releaseAndDelete(index);
                vm->runtimeError("Index must be integer or string");
                vm->vm_return(InterpretResult::RUNTIME_ERROR);
                return;
            }

            vm->releaseAndDelete(index);

            // Check for __set metamethod first
            if (inst->klass)
            {
                auto setMetaIt = inst->klass->methods.find("__set");
                if (setMetaIt != inst->klass->methods.end())
                {
                    Value setMethod = setMetaIt->second;

                    if (IS_NAT_FUNC(setMethod))
                    {
                        auto* native = vm->asNativeFunc(setMethod);

                        // Call __set(instance, key, value)
                        // Stack setup: args[0]=instance, args[1]=key, args[2]=value
                        Value keyValue = vm->createString(key);

                        vm->push(target);    // Push instance (target still on stack at peek(0))
                        vm->push(keyValue);  // Push key as string
                        vm->push(value);     // Push value

                        native->function(vm, 3, vm->stack.data() + vm->stack.size() - 3);

                        // Clean up stack
                        vm->pop(); // value
                        vm->pop(); // keyValue
                        vm->pop(); // target (still on stack at peek(0), don't release twice)
                        vm->releaseAndDelete(keyValue);
                        vm->releaseAndDelete(value);

                        // Note: target stays on stack (peek(0)) as per op_set_index contract
                        return;
                    }
                }
            }

            // No __set metamethod, do normal field assignment
            // Release old value if it exists
            auto it = inst->fields.find(key);
            if (it != inst->fields.end())
            {
                vm->releaseAndDelete(it->second);
            }

            // Store in fields map
            inst->fields[key] = vm->retainValue(value);
            vm->releaseAndDelete(value);  // Release our reference (field now owns it)
            return;
        }

        // Handle string indexing
        if (IS_STRING(target))
        {
            if (!IS_INT(index))
            {
                vm->releaseAndDelete(value);
                vm->releaseAndDelete(index);
                vm->runtimeError("String index must be integer");
                vm->vm_return(InterpretResult::RUNTIME_ERROR);
                return;
            }

            if (!IS_STRING(value))
            {
                vm->releaseAndDelete(value);
                vm->releaseAndDelete(index);
                vm->runtimeError("Can only assign string to string index");
                vm->vm_return(InterpretResult::RUNTIME_ERROR);
                return;
            }

            std::string str = vm->asString(target);
            int idx = AS_INT(index);

            // Handle negative indices (Python-style)
            if (idx < 0)
            {
                idx = static_cast<int>(str.length()) + idx;
            }

            // Allow appending at the end (idx == str.length())
            if (idx < 0 || idx > static_cast<int>(str.length()))
            {
                vm->releaseAndDelete(value);
                vm->releaseAndDelete(index);
                vm->runtimeError("String index out of range");
                vm->vm_return(InterpretResult::RUNTIME_ERROR);
                return;
            }

            std::string valueString = vm->asString(value);

            // Can only set a single character
            if (valueString.length() != 1)
            {
                vm->releaseAndDelete(value);
                vm->releaseAndDelete(index);
                vm->runtimeError("Can only assign single character to string index");
                vm->vm_return(InterpretResult::RUNTIME_ERROR);
                return;
            }

            // If appending at the end, append the character
            if (idx == static_cast<int>(str.length()))
            {
                str += valueString[0];
            }
            else
            {
                // Modify the string at the index
                str[idx] = valueString[0];
            }

            // Create a new string with the modified content and replace on stack
            vm->pop(); // Remove old string
            vm->push(vm->createString(str));

            vm->releaseAndDelete(value);
            vm->releaseAndDelete(index);
            return;
        }
    }

    // ========================================================================
    // Iterator Operations
    // ========================================================================

    void op_get_iterator(VM* vm)
    {
        Value tableVal = vm->peek(0);  // Don't pop, we keep table on stack

        if (!IS_INSTANCE(tableVal))
        {
            vm->runtimeError("Can only iterate over tables");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }

        // Create an integer to track iteration index (we'll iterate by index in fields map)
        // Push 0 as the initial iterator state
        vm->push(makeIntValue(0));
    }

    void op_iterator_next(VM* vm)
    {
        // Stack layout: [table, iterator_state]
        // We need to:
        // 1. Check if iterator is past the end
        // 2. If not, push the current key
        // 3. Increment the iterator
        // Final stack: [table, iterator_state+1, key] or [table, iterator_state, false]

        Value iteratorState = vm->peek(0);  // Peek, don't pop yet
        Value tableVal = vm->peek(1);        // Table is one below

        if (!IS_INT(iteratorState))
        {
            vm->runtimeError("Invalid iterator state");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }

        if (!IS_INSTANCE(tableVal))
        {
            vm->runtimeError("Can only iterate over tables");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }

        ObjInstance* table = vm->asInstance(tableVal);
        int64_t index = AS_INT(iteratorState);

        // Check if we've reached the end
        if (static_cast<size_t>(index) >= table->fields.size())
        {
            // End of iteration - push false to indicate done
            // Stack remains: [table, iterator_state, false]
            vm->push(makeBoolValue(false));
            return;
        }

        // Get the key at the current index
        auto it = table->fields.begin();
        std::advance(it, index);
        std::string key = it->first;

        // Pop old iterator state
        vm->pop();  // Remove old iterator state
        vm->releaseAndDelete(iteratorState);

        // Push incremented iterator state
        vm->push(makeIntValue(index + 1));

        // Push the key as a string
        Value keyVal = vm->createString(key);
        vm->push(keyVal);

        // Final stack: [table, iterator_state+1, key]
    }

    void op_table_size(VM* vm)
    {
        // Stack: [table or vector]
        Value tableVal = vm->peek(0);

        if (IS_VECTOR(tableVal))
        {
            ObjVector* vector = vm->asVector(tableVal);

            // Pop the vector
            vm->pop();
            vm->releaseAndDelete(tableVal);

            // Push the size as an integer
            vm->push(makeIntValue(static_cast<int64_t>(vector->fields.size())));
            return;
        }

        if (!IS_INSTANCE(tableVal))
        {
            vm->runtimeError("Can only get size of tables or vectors");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }

        ObjInstance* table = vm->asInstance(tableVal);

        // Pop the table
        vm->pop();
        vm->releaseAndDelete(tableVal);

        // Push the size as an integer
        vm->push(makeIntValue(static_cast<int64_t>(table->fields.size())));
    }

    void op_table_at(VM* vm)
    {
        // Stack: [table or vector, index]
        Value indexVal = vm->pop();
        Value tableVal = vm->pop();

        // Handle vector access (direct indexed access, returns value at index)
        if (IS_VECTOR(tableVal))
        {
            if (!IS_INT(indexVal))
            {
                vm->releaseAndDelete(indexVal);
                vm->releaseAndDelete(tableVal);
                vm->runtimeError("Vector index must be an integer");
                vm->vm_return(InterpretResult::RUNTIME_ERROR);
                return;
            }

            ObjVector* vector = vm->asVector(tableVal);
            int64_t index = AS_INT(indexVal);

            // Check bounds
            if (index < 0 || static_cast<size_t>(index) >= vector->fields.size())
            {
                vm->releaseAndDelete(indexVal);
                vm->releaseAndDelete(tableVal);
                vm->runtimeError("Vector index out of bounds");
                vm->vm_return(InterpretResult::RUNTIME_ERROR);
                return;
            }

            // Get the value directly
            Value value = vector->fields[index];

            vm->releaseAndDelete(indexVal);
            vm->releaseAndDelete(tableVal);

            // For vectors, return the value directly (not the index)
            // This is different from tables where we return the key
            vm->push(vm->retainValue(value));
            return;
        }

        // Handle table access (returns key at index position in map)
        if (!IS_INSTANCE(tableVal))
        {
            vm->releaseAndDelete(indexVal);
            vm->releaseAndDelete(tableVal);
            vm->runtimeError("Can only get keys from tables or vectors");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }

        if (!IS_INT(indexVal))
        {
            vm->releaseAndDelete(indexVal);
            vm->releaseAndDelete(tableVal);
            vm->runtimeError("Table index must be an integer");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }

        ObjInstance* table = vm->asInstance(tableVal);
        int64_t index = AS_INT(indexVal);

        // Check bounds
        if (index < 0 || static_cast<size_t>(index) >= table->fields.size())
        {
            vm->releaseAndDelete(indexVal);
            vm->releaseAndDelete(tableVal);
            vm->runtimeError("Table index out of bounds");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }

        // Get the key at the specified index
        auto it = table->fields.begin();
        std::advance(it, index);
        std::string key = it->first;

        vm->releaseAndDelete(indexVal);
        vm->releaseAndDelete(tableVal);

        // Push the key as a string
        Value keyVal = vm->createString(key);
        vm->push(keyVal);
    }

    // ========================================================================
    // Module Operations
    // ========================================================================

    void op_import(VM* vm)
    {
#ifdef DEBUG_CHECK_STACK
        if (vm->stack.empty())
        {
            vm->runtimeError("Stack underflow on import.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }
#endif
        // Pop the module name from the stack
        auto moduleNameValue = vm->pop();

        // Convert to string
        if (not IS_STRING(moduleNameValue))
        {
            vm->releaseAndDelete(moduleNameValue);
            vm->runtimeError("Import module name must be a string.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }

        std::string moduleName = vm->asString(moduleNameValue);
        vm->releaseAndDelete(moduleNameValue);

        // TODO: Implement the full import logic:
        // 1. Check for moduleName.pgc in cache
        // 2. If not found, check for moduleName.pg in folder paths
        // 3. If found, compile and cache
        // 4. If not found, check for native module
        // 5. Load module exports into global scope

        // For now, just log that we're importing
        LOG_INFO("VM", "Import requested for module: " << moduleName);

        VM importerVm;
        importerVm.interpretFromFile(moduleName + ".pg", false, moduleName + ".pgc");

        for (const auto& globalPair : importerVm.globals)
        {
            // Skip special globals
            if (globalPair.first == "__Table")
            {
                continue;
            }
            else if (IS_INT(globalPair.second) or IS_BOOL(globalPair.second) or IS_DOUBLE(globalPair.second))
            {
                LOG_INFO("VM", "Importing primitive global: " << globalPair.first);
                // Primitives can be copied directly
                vm->globals[globalPair.first] = globalPair.second;
            }
            else if (IS_FUNC(globalPair.second))
            {
                LOG_INFO("VM", "Importing function global: " << globalPair.first);
                // Functions need to be converted to closures in the current VM
                ObjFunction* func = importerVm.asFunction(globalPair.second);
                Value closureVal = vm->createClosure(func);
                vm->globals[globalPair.first] = closureVal;
            }
            else if (IS_CLOSURE(globalPair.second))
            {
                LOG_INFO("VM", "Importing closure global: " << globalPair.first);
                // Closures need to be recreated in the current VM
                Closure* closure = importerVm.asClosure(globalPair.second);
                for (size_t i = 0; i < closure->upvalues.size(); i++)
                {
                    vm->createUpvalue(closure->upvalues[i]->location);
                }
                Value newClosureVal = vm->createClosure(closure->function);
                vm->globals[globalPair.first] = newClosureVal;
            }
            else if (IS_STRING(globalPair.second))
            {
                LOG_INFO("VM", "Importing string global: " << globalPair.first);
                // Strings need to be recreated in the current VM
                std::string str = importerVm.asString(globalPair.second);
                Value newStrVal = vm->createString(str);
                vm->globals[globalPair.first] = newStrVal;
            }
            else
            {
                LOG_INFO("VM", "Importing global '" << globalPair.first << "' of unsupported type: " << valueTypeName(globalPair.second));
            }

            LOG_INFO("VM", "Importing global: " << globalPair.first);
        }

        for (auto& global : vm->globals)
        {
            LOG_INFO("VM", "Post-import global: " << global.first);
        }
    }

    void VM::printAllFunctionsBytecodeWithPerformance()
    {
        if (!profiler.isEnabled())
        {
            LOG_WARNING("VM_Profiling", "Profiling is not enabled. Call enableProfiling() before running the VM.");

            return;
        }

        if (compiledFunctions.empty())
        {
            std::cout << "No functions have been compiled yet." << std::endl;
            return;
        }

        std::cout << "\n========================================" << std::endl;
        std::cout << "Bytecode with Performance - All Functions" << std::endl;
        std::cout << "========================================\n" << std::endl;

        for (const auto* func : compiledFunctions)
        {
            std::string funcName = func->name.empty() ? "<script>" : func->name;
            profiler.printBytecodeWithPerformance(funcName);
        }
    }
}