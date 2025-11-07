#include "vm.h"

#include <iostream>

#include "compiler.h"

#include "compiler_debug.h"

#include <chrono>

#include "pgconstant.h"

#include "chunk_serializer.h"

namespace pg
{
    bool isValueNumber(const Value& val, VM* vm)
    {
        if (IS_INT(val) || IS_DOUBLE(val))
            return true;

        // String objects might represent numbers - need pool access
        if (IS_STRING(val) && vm != nullptr) {
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
        if (IS_STRING(val) && vm != nullptr) {
            ElementType* obj = vm->pools.getString(val);
            return obj->isTrue();
        }

        if (IS_CLOSURE(val) || IS_FUNC(val) || IS_NAT_FUNC(val) ||
            IS_CLASS(val) || IS_INSTANCE(val) || IS_BOUND_METHOD(val))
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

    void op_add_ll(VM* vm);
    void op_subtract_ll(VM* vm);

    void op_subtract_lc(VM* vm);
    void op_subtract_cl(VM* vm);

    // Table operations
    void op_build_table(VM* vm);
    void op_get_index(VM* vm);
    void op_set_index(VM* vm);

    // Iterator operations
    void op_get_iterator(VM* vm);
    void op_iterator_next(VM* vm);

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
    }

    InterpretResult VM::interpret(const std::queue<Token>& tokens, bool compileOnly, const std::string& dumpByteCode)
    {
        // Todo change this
        // Reset the compiler state before compiling a new chunk
        Compiler compiler(this);

        std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();

        auto function = compiler.compile(tokens);

        if (function == 0x0)
            return InterpretResult::COMPILE_ERROR;

        push(function);  // function is already tracked from compiler

        auto closureValue = createClosure(asFunction(function));
        Closure *closure = asClosure(closureValue);
        pop();
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

        // Apply bytecode optimizations
        for (auto f : compiler.parser.allocatedFunction)
        {
            auto *func = asFunction(f);

            if (enableOptimizations and not func->chunk.code.empty())
            {
                begin = std::chrono::steady_clock::now();

                LOG_INFO("VM", "Applying bytecode optimizations");
                size_t originalSize = func->chunk.code.size();

                passManager.runAllPasses(func->chunk);

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
        }

        // Optionally dump bytecode to file
        if (dumpByteCode != "")
        {
            std::ofstream outFile(dumpByteCode, std::ios::binary);
            if (outFile.is_open())
            {
                LOG_INFO("VM", "Dumping bytecode to file: " << dumpByteCode);

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

    InterpretResult VM::interpretFromBytecodeFile(const std::string& filename)
    {
        currentFileName = filename;

        Chunk chunk;

        ChunkSerializer serializer;
        if (not serializer.deserializeFromFile(chunk, filename, this))
        {
            LOG_ERROR("VM", "Failed to load bytecode from file: " << filename);
            return InterpretResult::COMPILE_ERROR;
        }

        auto function = createFunction();  // Already tracked in createFunction
        ObjFunction* funcObj = asFunction(function);
        funcObj->chunk = chunk;

        // disassembleChunk(this, chunk, "<compiled chunk>");


        // Retain the function to prevent it from being freed when popped
        // The closure needs the function to stay alive
        retainValue(function);

        push(function);  // function is already tracked from createFunction

        auto closureValue = createClosure(funcObj);
        Closure *closure = asClosure(closureValue);
        pop();  // Pop function
        push(closureValue);  // closureValue is already tracked in createClosure

        call(closure, 0);

        try
        {
            // Freeze constant indices - all pool allocations up to this point are constants
            // Runtime allocations will have indices above these max values
            pools.freezeConstantIndices();

            return run();
        }
        catch(const std::exception& e)
        {
            LOG_ERROR("VM", e.what());

            return InterpretResult::RUNTIME_ERROR;
        }
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
                disassembleInstruction(this, currentFrame->closure->function->chunk, currentFrame->ip - chunkData - 1);
#endif
                // Todo add those behind a debug flag
                // if (operations[opcode].handler) {
                    // Dispatch to operation handler
                    operations[opcode].handler(this);
                // } else {
                    // runtimeError("Unknown opcode");
                    // return InterpretResult::RUNTIME_ERROR;
                // }

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

        Closure* method = asClosure(it->second);

        return callBound(method, argCount);
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
        // frame->slots = stack.data() + stack.size() - argCount - 1;

        // Point to first argument (skipping the function object at -argCount-1)

        // if (closure->function == FunctionType::TYPE_FUNCTION)
        // {
        //     frame->slots = stack.data() + stack.size() - argCount;
        // }
        // else
        // {
        //     frame->slots = stack.data() + stack.size() - argCount - 1;
        // }

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
        // Point to first argument (skipping the function object at -argCount-1)
        frame->slots = stack.data() + stack.size() - argCount - 1;

        return true;
    }

    void VM::deleteValue(const Value& value)
    {
        // Perform type-specific deletion
        if (IS_STRING(value))
        {
            pools.stringPool.release(asString(value));
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
            pools.instancePool.release(asInstance(value));
        }
        else if (IS_BOUND_METHOD(value))
        {
            auto *boundMethod = asBoundMethod(value);

            // Don't release receiver here - it will be released when the bound method is released
            // releaseAndDelete(boundMethod->receiver);

            pools.boundMethodPool.release(boundMethod);
        }
    }

    void VM::releaseAndDelete(const Value& value)
    {
        if (releaseValue(value))
        {
            deleteValue(value);
        }
    }

    Value VM::addValues(const Value& a, const Value& b)
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

        // Disallow functions
        if (IS_FUNC(a) or IS_FUNC(b))
            throw std::runtime_error("Cannot compare function Values");

        // Fall back to ElementType for other complex cases (strings, etc.)
        ElementType elemA = valueToElement(a);
        ElementType elemB = valueToElement(b);
        return trackNewValue(elementToValue(elemA + elemB));
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
        return trackNewValue(elementToValue(elemA - elemB));
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
        return trackNewValue(elementToValue(elemA * elemB));
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
        return trackNewValue(elementToValue(elemA / elemB));
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
        return trackNewValue(elementToValue(-elem));
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
        return trackNewValue(elementToValue(elemA == elemB));
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

        if (IS_BOOL(a) && IS_BOOL(b))
            return BOOL_VAL(AS_BOOL(a) != AS_BOOL(b));

        // Disallow functions
        if (IS_FUNC(a) or IS_FUNC(b))
            throw std::runtime_error("Cannot compare function Values");

        ElementType elemA = valueToElement(a);
        ElementType elemB = valueToElement(b);
        return trackNewValue(elementToValue(elemA != elemB));
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
        return trackNewValue(elementToValue(elemA > elemB));
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
        return trackNewValue(elementToValue(elemA >= elemB));
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
        return trackNewValue(elementToValue(elemA < elemB));
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
        return trackNewValue(elementToValue(elemA <= elemB));
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

        register_operation(static_cast<uint8_t>(OpCode::OP_AddLL), op_add_ll);
        register_operation(static_cast<uint8_t>(OpCode::OP_SubtractLL), op_subtract_ll);

        register_operation(static_cast<uint8_t>(OpCode::OP_SubtractLC), op_subtract_lc);
        register_operation(static_cast<uint8_t>(OpCode::OP_SubtractCL), op_subtract_cl);

        // Table operations
        register_operation(static_cast<uint8_t>(OpCode::OP_Build_Table), op_build_table);
        register_operation(static_cast<uint8_t>(OpCode::OP_Get_Index), op_get_index);
        register_operation(static_cast<uint8_t>(OpCode::OP_Set_Index), op_set_index);

        // Iterator operations
        register_operation(static_cast<uint8_t>(OpCode::OP_Get_Iterator), op_get_iterator);
        register_operation(static_cast<uint8_t>(OpCode::OP_Iterator_Next), op_iterator_next);

        // Module operations
        register_operation(static_cast<uint8_t>(OpCode::OP_Import), op_import);
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
        auto a = vm->pop(); \
        vm->push(vm->operation(a, b)); \
        /* Escape analysis: Only release heap objects, not primitives */ \
        if (requiresRefCount(a)) vm->releaseAndDelete(a); \
        if (requiresRefCount(b)) vm->releaseAndDelete(b); \
    }

    BINARY_OP_TEMPLATE(op_add, addValues)
    BINARY_OP_TEMPLATE(op_subtract, subtractValues)
    BINARY_OP_TEMPLATE(op_multiply, multiplyValues)
    BINARY_OP_TEMPLATE(op_divide, divideValues)

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

        Value oldValue = vm->currentFrame->slots[slot];
        // Escape analysis: Only release old value if it's a heap object
        if (requiresRefCount(oldValue)) {
            vm->releaseAndDelete(oldValue);
        }
        vm->currentFrame->slots[slot] = vm->peek(0);
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

        // Pop arguments based on function arity, then pop the callee
        int arity = vm->currentFrame->closure->function->arity;

        // Restore previous frame
        vm->currentFrame = &vm->frames[vm->frameCount - 1];
        vm->updateChunkCache();

        // Pop the arguments
        for (int i = 0; i < arity; i++)
        {
            auto v = vm->pop();
            vm->releaseAndDelete(v);
        }

        // Pop the callee (closure for regular calls, instance for bound methods)
        auto callee = vm->pop();
        vm->releaseAndDelete(callee);

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
        auto nameValue = vm->pop();  // variable name
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

        vm->push(vm->retainValue(it->second)); // Retain because stack becomes an owner
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
        auto value = vm->pop();
        auto nameValue = vm->pop();
        auto name = vm->valueToElement(nameValue);

        if (not name.isLitteral())
        {
            vm->releaseAndDelete(nameValue);
            vm->releaseAndDelete(value);
            vm->runtimeError("Global variable name must be a litteral.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }

        auto it = vm->globals.find(name.toString());
        if (it == vm->globals.end())
        {
            vm->releaseAndDelete(nameValue);
            vm->releaseAndDelete(value);
            vm->runtimeError("Undefined global variable '" + name.toString() + "'.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }

        vm->releaseAndDelete(it->second);
        it->second = vm->retainValue(value);
        vm->push(value);
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

        // // Remove the function object from the stack by shifting arguments up
        // for (int i = argCount - 1; i >= 0; i--)
        // {
        //     vm->stack[vm->stack.size() - argCount - 1 + i] = vm->stack[vm->stack.size() - argCount + i];
        // }

        // vm->stack.pop(); // Remove the duplicate top element

        // // Update the frame slots to point to the shifted arguments
        // vm->frames[vm->frameCount - 1].slots = vm->stack.data() + vm->stack.size() - argCount;

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

        // For testing: append to testOutput buffer instead of stdout
        ElementType elem = vm->valueToElement(value);
        vm->testOutput += elem.toString() + "\n";
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

        if (not isValueTrue(condition))
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
            vm->runtimeError("Operand after an unary (++) must be a number.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }

        // Old value is already on the stack (from OP_Get_Local before this opcode)
        // We just need to increment the variable in its slot
        auto newValue = vm->addValues(vm->stack[stackIndex], INT_VAL(1));
        vm->releaseAndDelete(vm->stack[stackIndex]);
        vm->stack[stackIndex] = newValue;

        vm->releaseAndDelete(slot);
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

        // Create a bound method
        auto bound = vm->createBoundMethod(vm->peek(0), vm->asClosure(methodValue));  // Already tracked

        auto instance = vm->pop(); // Remove the instance
        vm->releaseAndDelete(instance);
        vm->push(bound);

        return true;
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

        auto value = vm->pop(); // Value to set
        auto inst = vm->pop(); // Instance
        vm->releaseAndDelete(inst);

        // Todo maybe fix
        if (instance->fields.find(name.toString()) != instance->fields.end())
        {
            vm->releaseAndDelete(instance->fields[name.toString()]);
        }

        instance->fields[name.toString()] = vm->retainValue(value);
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

        // The class is below the method closure on the stack
        auto methodClosureValue = vm->pop();
        auto classValue = vm->peek();

        if (not IS_CLASS(classValue))
        {
            vm->runtimeError("Method definition must be on a class.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);

            return;
        }

        Klass* klass = vm->asClass(classValue);

        if (not IS_CLOSURE(methodClosureValue))
        {
            vm->runtimeError("Method must be a closure.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);

            return;
        }

        klass->methods[methodName] = methodClosureValue;
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
        auto [ptr, index] = pools.stringPool.allocateWithIndex(element);
        Value val = makeStringValue(static_cast<uint32_t>(index));
        return val;
    }

    Value VM::createClosure(ObjFunction* function)
    {
        auto [ptr, index] = pools.closurePool.allocateWithIndex(function);
        Value val = makeClosureValue(static_cast<uint32_t>(index));
        return val;
    }

    Value VM::createFunction()
    {
        auto [ptr, index] = pools.functionPool.allocateWithIndex();
        Value val = makeFunctionValue(static_cast<uint32_t>(index));
        return val;
    }

    Value VM::createUpvalue(Value* slot)
    {
        auto [ptr, index] = pools.upvaluePool.allocateWithIndex(slot);
        Value val = makeUpvalueValue(static_cast<uint32_t>(index));
        return val;
    }

    Value VM::createClass(const std::string& name)
    {
        auto [ptr, index] = pools.classPool.allocateWithIndex(name);
        Value val = makeClassValue(static_cast<uint32_t>(index));
        return val;
    }

    Value VM::createInstance(Klass* klass)
    {
        auto [ptr, index] = pools.instancePool.allocateWithIndex(klass);
        Value val = makeInstanceValue(static_cast<uint32_t>(index));
        return val;
    }

    Value VM::createBoundMethod(const Value& receiver, Closure* method)
    {
        auto [ptr, index] = pools.boundMethodPool.allocateWithIndex(receiver, method);
        Value val = makeBoundMethodValue(static_cast<uint32_t>(index));
        return val;
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
        else if (IS_STRING(value))
            return *asString(value);
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
        else if (IS_STRING(value))
        {
            ElementType* obj = asString(value);
            if (obj->type == ElementType::UnionType::INT)
                return obj->get<int>();
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
                keyStr = vm->asString(key)->toString();
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

    void op_get_index(VM* vm)
    {
        Value index = vm->pop();
        Value instance = vm->pop();

        if (!IS_INSTANCE(instance))
        {
            vm->releaseAndDelete(index);
            vm->releaseAndDelete(instance);
            vm->runtimeError("Can only index tables/instances");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }

        ObjInstance* inst = vm->asInstance(instance);

        // Convert index to string key
        std::string key;
        if (IS_INT(index))
        {
            key = std::to_string(AS_INT(index));
        }
        else if (IS_STRING(index))
        {
            key = vm->asString(index)->toString();
        }
        else
        {
            vm->releaseAndDelete(index);
            vm->releaseAndDelete(instance);
            vm->runtimeError("Index must be integer or string");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }

        vm->releaseAndDelete(index);
        vm->releaseAndDelete(instance);

        // Look up in fields map
        auto it = inst->fields.find(key);
        if (it == inst->fields.end())
        {
            vm->push(BOOL_VAL(false));  // Or NIL_VAL if you have it
        }
        else
        {
            vm->push(vm->retainValue(it->second));
        }
    }

    void op_set_index(VM* vm)
    {
        Value value = vm->pop();
        Value index = vm->pop();
        Value instance = vm->peek(0); // Keep instance on stack

        if (!IS_INSTANCE(instance))
        {
            vm->releaseAndDelete(value);
            vm->releaseAndDelete(index);
            vm->runtimeError("Can only index tables/instances");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }

        ObjInstance* inst = vm->asInstance(instance);

        // Convert index to string key
        std::string key;
        if (IS_INT(index))
        {
            key = std::to_string(AS_INT(index));
        }
        else if (IS_STRING(index))
        {
            key = vm->asString(index)->toString();
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

        // Release old value if it exists
        auto it = inst->fields.find(key);
        if (it != inst->fields.end())
        {
            vm->releaseAndDelete(it->second);
        }

        // Store in fields map
        inst->fields[key] = vm->retainValue(value);
        vm->releaseAndDelete(value);  // Release our reference (field now owns it)
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
        Value iteratorState = vm->pop();
        Value tableVal = vm->peek(0);  // Keep table on stack

        if (!IS_INT(iteratorState))
        {
            vm->releaseAndDelete(iteratorState);
            vm->runtimeError("Invalid iterator state");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }

        if (!IS_INSTANCE(tableVal))
        {
            vm->releaseAndDelete(iteratorState);
            vm->runtimeError("Can only iterate over tables");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }

        ObjInstance* table = vm->asInstance(tableVal);
        int64_t index = AS_INT(iteratorState);

        // Check if we've reached the end
        if (static_cast<size_t>(index) >= table->fields.size())
        {
            // End of iteration - push nil and update iterator
            vm->push(makeIntValue(index));  // Push updated iterator state
            vm->push(makeBoolValue(false)); // Push false to indicate end
            vm->releaseAndDelete(iteratorState);
            return;
        }

        // Get the key at the current index
        auto it = table->fields.begin();
        std::advance(it, index);
        std::string key = it->first;

        // Update iterator state (increment index)
        vm->push(makeIntValue(index + 1));

        // Push the key as a string
        Value keyVal = vm->createString(key);
        vm->push(keyVal);

        vm->releaseAndDelete(iteratorState);
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

        std::string moduleName = vm->asString(moduleNameValue)->toString();
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
                ElementType* strElem = importerVm.asString(globalPair.second);
                Value newStrVal = vm->createString(*strElem);
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

        // Placeholder: Module import not yet implemented
        // vm->runtimeError("Module import not yet implemented: '" + moduleName + "'");
        // vm->vm_return(InterpretResult::RUNTIME_ERROR);
    }
}