#include "stdafx.h"

#include "vm.h"
#include "decoded_chunk.h"
#include "ecsserialization.h"

namespace pg
{
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

    void VM::parkIpForDecodedCall(const DecodedInstruction& callInstr)
    {
        currentFrame->ip = currentStartingIp + callInstr.bytecodeOffset + 1 + callInstr.operandBytes;
    }

    void VM::runLegacyAsDecoded(const DecodedInstruction& instr, OpHandler legacy)
    {
        currentFrame->ip = currentStartingIp + instr.bytecodeOffset + 1;
        const int frameCountBefore = frameCount;
        legacy(this);
        completeDecodedFrameSwitch(frameCountBefore);
    }

    bool VM::callValueDecoded(const Value& callee, int argCount, const DecodedInstruction& callInstr)
    {
        // Park caller's ip past this OP_Call BEFORE dispatching so a future
        // OP_Return can map ip back to the resume index in the caller's
        // decoded chunk. For native calls, no frame change occurs and the
        // parked ip is harmless (dispatcher tracks position via
        // instructionIndex, not ip).
        parkIpForDecodedCall(callInstr);

        const int frameCountBefore = frameCount;

        if (not callValue(callee, argCount))
            return false;

        completeDecodedFrameSwitch(frameCountBefore);
        return true;
    }

    void VM::completeDecodedFrameSwitch(int frameCountBefore)
    {
        // Native calls (and class constructors with no init) push no frame —
        // execution stays in the current decoded chunk, so leave dispatcher
        // state alone.
        if (frameCount == frameCountBefore)
            return;

        // A new frame was pushed. Switch dispatcher state to the callee.
        currentFrame = &frames[frameCount - 1];
        updateChunkCache();

        currentStartingIp = currentFrame->closure->function->chunk.code.data();

        DecodedChunk* newDecoded = currentFrame->closure->function->decodedChunk;
        if (newDecoded != nullptr)
        {
            currentDecoded = newDecoded;
            nextInstructionIndex = 0;

            // If the callee starts mid-function (unusual, but possible for
            // resumed frames), map the ip back to the matching decoded index.
            if (currentFrame->ip != currentStartingIp)
            {
                size_t offset = currentFrame->ip - currentStartingIp;
                nextInstructionIndex = newDecoded->findInstructionIndex(offset);
            }
        }
        else
        {
            wantsLegacyFallback = true;
        }
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
            std::string* str = asStringPtr(value);
            pools.internedStrings.erase(*str);
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
            for (auto& field : asInstance(value)->fieldValues)
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

    // Function pointer dispatch implementation
    void VM::vm_return(InterpretResult result)
    {
        exit_result = result;
        longjmp(exit_jump, 1);
    }

    void VM::register_operation(uint8_t opcode, OpHandler handler)
    {
        operations[opcode] = OpCodeInfo(handler);
    }

    void VM::register_operation(uint8_t opcode, OpHandler handler, uint8_t flags)
    {
        operations[opcode] = OpCodeInfo(handler, flags);
    }

    void VM::register_operation(uint8_t opcode, OpHandler handler, OpDecodedHandler decodedHandler, uint8_t flags)
    {
        operations[opcode] = OpCodeInfo(handler, flags, decodedHandler);
    }

    void VM::register_builtin_operations()
    {
        // Control flow (default flags: 0 - no batching, has control flow)
        register_operation(static_cast<uint8_t>(OpCode::OP_Return), op_return, op_return_decoded, 0);
        // Pure & batchable: Constants
        register_operation(static_cast<uint8_t>(OpCode::OP_Constant), op_constant, op_constant_decoded, OpCodeInfo::PURE_BATCH);
        register_operation(static_cast<uint8_t>(OpCode::OP_LongConstant), op_long_constant, op_long_constant_decoded, OpCodeInfo::PURE_BATCH);
        register_operation(static_cast<uint8_t>(OpCode::OP_Short_Int), op_short_int, op_short_int_decoded, OpCodeInfo::PURE_BATCH);

        // Pure & batchable: Arithmetic
        register_operation(static_cast<uint8_t>(OpCode::OP_Add), op_add, op_add_decoded, OpCodeInfo::PURE_BATCH);
        register_operation(static_cast<uint8_t>(OpCode::OP_Subtract), op_subtract, op_subtract_decoded, OpCodeInfo::PURE_BATCH);
        register_operation(static_cast<uint8_t>(OpCode::OP_Multiply), op_multiply, op_multiply_decoded, OpCodeInfo::PURE_BATCH);
        register_operation(static_cast<uint8_t>(OpCode::OP_Divide), op_divide, op_divide_decoded, OpCodeInfo::PURE_BATCH);
        register_operation(static_cast<uint8_t>(OpCode::OP_Modulo), op_modulo, op_modulo_decoded, OpCodeInfo::PURE_BATCH);
        register_operation(static_cast<uint8_t>(OpCode::OP_Negate), op_negate, op_negate_decoded, OpCodeInfo::PURE_BATCH);

        // Pure & batchable: Optimized arithmetic
        register_operation(static_cast<uint8_t>(OpCode::OP_AddLL), op_add_ll, op_add_ll_decoded, OpCodeInfo::PURE_BATCH | OpCodeInfo::LOCAL_ONLY);
        register_operation(static_cast<uint8_t>(OpCode::OP_SubtractLL), op_subtract_ll, op_subtract_ll_decoded, OpCodeInfo::PURE_BATCH | OpCodeInfo::LOCAL_ONLY);
        register_operation(static_cast<uint8_t>(OpCode::OP_SubtractLC), op_subtract_lc, op_subtract_lc_decoded, OpCodeInfo::PURE_BATCH);
        register_operation(static_cast<uint8_t>(OpCode::OP_SubtractCL), op_subtract_cl, op_subtract_cl_decoded, OpCodeInfo::PURE_BATCH);
        // Peephole fusions emitted by ComparisonLocalIndexingPass / SetLocalPopFusionPass
        register_operation(static_cast<uint8_t>(OpCode::OP_LessEqualLL), op_less_equal_ll, op_less_equal_ll_decoded, OpCodeInfo::PURE_BATCH | OpCodeInfo::LOCAL_ONLY);
        register_operation(static_cast<uint8_t>(OpCode::OP_LessLL), op_less_ll, op_less_ll_decoded, OpCodeInfo::PURE_BATCH | OpCodeInfo::LOCAL_ONLY);
        register_operation(static_cast<uint8_t>(OpCode::OP_Set_Local_Pop), op_set_local_pop, op_set_local_pop_decoded, OpCodeInfo::PURE_BATCH | OpCodeInfo::LOCAL_ONLY);

        // Pure & batchable: Comparisons
        register_operation(static_cast<uint8_t>(OpCode::OP_Equal), op_equal, op_equal_decoded, OpCodeInfo::PURE_BATCH);
        register_operation(static_cast<uint8_t>(OpCode::OP_NotEqual), op_not_equal, op_not_equal_decoded, OpCodeInfo::PURE_BATCH);
        register_operation(static_cast<uint8_t>(OpCode::OP_Greater), op_greater, op_greater_decoded, OpCodeInfo::PURE_BATCH);
        register_operation(static_cast<uint8_t>(OpCode::OP_GreaterEqual), op_greater_equal, op_greater_equal_decoded, OpCodeInfo::PURE_BATCH);
        register_operation(static_cast<uint8_t>(OpCode::OP_Less), op_less, op_less_decoded, OpCodeInfo::PURE_BATCH);
        register_operation(static_cast<uint8_t>(OpCode::OP_LessEqual), op_less_equal, op_less_equal_decoded, OpCodeInfo::PURE_BATCH);

        // Pure & batchable: Boolean ops
        register_operation(static_cast<uint8_t>(OpCode::OP_True), op_true, op_true_decoded, OpCodeInfo::PURE_BATCH);
        register_operation(static_cast<uint8_t>(OpCode::OP_False), op_false, op_false_decoded, OpCodeInfo::PURE_BATCH);
        register_operation(static_cast<uint8_t>(OpCode::OP_Not), op_not, op_not_decoded, OpCodeInfo::PURE_BATCH);
        register_operation(static_cast<uint8_t>(OpCode::OP_And), op_and, op_and_decoded, OpCodeInfo::PURE_BATCH);
        register_operation(static_cast<uint8_t>(OpCode::OP_Or), op_or, op_or_decoded, OpCodeInfo::PURE_BATCH);

        // Pure & batchable: Stack ops
        register_operation(static_cast<uint8_t>(OpCode::OP_Pop), op_pop, op_pop_decoded, OpCodeInfo::PURE_BATCH);
        register_operation(static_cast<uint8_t>(OpCode::OP_PopN), op_pop_n, op_pop_n_decoded, OpCodeInfo::PURE_BATCH);

        // Pure & batchable: Locals
        register_operation(static_cast<uint8_t>(OpCode::OP_Get_Local), op_get_local, op_get_local_decoded, OpCodeInfo::PURE_BATCH | OpCodeInfo::LOCAL_ONLY);
        register_operation(static_cast<uint8_t>(OpCode::OP_Set_Local), op_set_local, op_set_local_decoded, OpCodeInfo::BATCHABLE | OpCodeInfo::LOCAL_ONLY);

        // Global variables (default: no flags)
        register_operation(static_cast<uint8_t>(OpCode::OP_Get_Global), op_get_global, op_get_global_decoded, 0);
        register_operation(static_cast<uint8_t>(OpCode::OP_Define_Global), op_define_global, op_define_global_decoded, 0);
        register_operation(static_cast<uint8_t>(OpCode::OP_Set_Global), op_set_global, op_set_global_decoded, 0);
        register_operation(static_cast<uint8_t>(OpCode::OP_Define_Global_Non_Popping), op_define_global_non_popping, op_define_global_non_popping_decoded, 0);
        register_operation(static_cast<uint8_t>(OpCode::OP_Define_Constant_Global), op_define_constant_global, op_define_constant_global_decoded);
        register_operation(static_cast<uint8_t>(OpCode::OP_Get_Constant_Global), op_get_constant_global, op_get_constant_global_decoded, 0);
        register_operation(static_cast<uint8_t>(OpCode::OP_Set_Constant_Global), op_set_constant_global, op_set_constant_global_decoded, 0);

        // Control flow (default: no flags)
        register_operation(static_cast<uint8_t>(OpCode::OP_Jump), op_jump, op_jump_decoded);
        register_operation(static_cast<uint8_t>(OpCode::OP_Long_Jump), op_long_jump, op_jump_decoded);
        register_operation(static_cast<uint8_t>(OpCode::OP_Loop), op_loop, op_loop_decoded);
        register_operation(static_cast<uint8_t>(OpCode::OP_Long_Loop), op_long_loop, op_loop_decoded);
        register_operation(static_cast<uint8_t>(OpCode::OP_Jump_If_False), op_jump_if_false, op_jump_if_false_decoded, 0);
        register_operation(static_cast<uint8_t>(OpCode::OP_Jump_If_False_Popping), op_jump_if_false_popping, op_jump_if_false_popping_decoded, 0);
        register_operation(static_cast<uint8_t>(OpCode::OP_Long_Jump_If_False), op_long_jump_if_false, op_long_jump_if_false_decoded, 0);
        register_operation(static_cast<uint8_t>(OpCode::OP_Long_Jump_If_False_Popping), op_long_jump_if_false_popping, op_long_jump_if_false_popping_decoded, 0);
        register_operation(static_cast<uint8_t>(OpCode::OP_Call), op_call, op_call_decoded, 0);

        // Function operations (default: no flags)
        register_operation(static_cast<uint8_t>(OpCode::OP_Invoke), op_invoke, op_invoke_decoded, 0);
        register_operation(static_cast<uint8_t>(OpCode::OP_Closure), op_closure, op_closure_decoded, 0);
        register_operation(static_cast<uint8_t>(OpCode::OP_Get_Upvalue), op_get_upvalue, op_get_upvalue_decoded, 0);
        register_operation(static_cast<uint8_t>(OpCode::OP_Set_Upvalue), op_set_upvalue, op_set_upvalue_decoded, 0);
        register_operation(static_cast<uint8_t>(OpCode::OP_Close_Upvalue), op_close_upvalue, op_close_upvalue_decoded, 0);

        // Class operations (default: no flags)
        register_operation(static_cast<uint8_t>(OpCode::OP_Class), op_class, op_class_decoded, 0);
        register_operation(static_cast<uint8_t>(OpCode::OP_Get_Property), op_get_property, op_get_property_decoded, 0);
        register_operation(static_cast<uint8_t>(OpCode::OP_Set_Property), op_set_property, op_set_property_decoded, 0);
        register_operation(static_cast<uint8_t>(OpCode::OP_Method), op_method, op_method_decoded, 0);

        // Increment/decrement operations (default: no flags)
        register_operation(static_cast<uint8_t>(OpCode::OP_Post_Incr_Global), op_post_incr_global, op_post_incr_global_decoded, 0);
        register_operation(static_cast<uint8_t>(OpCode::OP_Incr_Global), op_incr_global, op_incr_global_decoded, 0);
        register_operation(static_cast<uint8_t>(OpCode::OP_Post_Decr_Global), op_post_decr_global, op_post_decr_global_decoded, 0);
        register_operation(static_cast<uint8_t>(OpCode::OP_Decr_Global), op_decr_global, op_decr_global_decoded, 0);
        register_operation(static_cast<uint8_t>(OpCode::OP_Post_Incr_Local), op_post_incr_local, op_post_incr_local_decoded, 0);
        register_operation(static_cast<uint8_t>(OpCode::OP_Incr_Local), op_incr_local, op_incr_local_decoded, 0);
        register_operation(static_cast<uint8_t>(OpCode::OP_Post_Decr_Local), op_post_decr_local, op_post_decr_local_decoded, 0);
        register_operation(static_cast<uint8_t>(OpCode::OP_Decr_Local), op_decr_local, op_decr_local_decoded, 0);

        // Debug operations (default: no flags)
        register_operation(static_cast<uint8_t>(OpCode::OP_Debug_Print), op_debug_print, op_debug_print_decoded, 0);

        // Table and vector operations
        register_operation(static_cast<uint8_t>(OpCode::OP_Build_Table), op_build_table, op_build_table_decoded, 0);
        register_operation(static_cast<uint8_t>(OpCode::OP_Build_Vector), op_build_vector, op_build_vector_decoded, 0);
        register_operation(static_cast<uint8_t>(OpCode::OP_Get_Index), op_get_index, op_get_index_decoded, 0);
        register_operation(static_cast<uint8_t>(OpCode::OP_Set_Index), op_set_index, op_set_index_decoded, 0);

        // Iterator operations
        register_operation(static_cast<uint8_t>(OpCode::OP_Get_Iterator), op_get_iterator, op_get_iterator_decoded, 0);
        register_operation(static_cast<uint8_t>(OpCode::OP_Iterator_Next), op_iterator_next, op_iterator_next_decoded, 0);
        register_operation(static_cast<uint8_t>(OpCode::OP_Table_Size), op_table_size, op_table_size_decoded, 0);
        register_operation(static_cast<uint8_t>(OpCode::OP_Table_At), op_table_at, op_table_at_decoded, 0);

        // Module operations
        register_operation(static_cast<uint8_t>(OpCode::OP_Import), op_import, op_import_decoded, 0);


        // Register-based operations
        register_operation(static_cast<uint8_t>(OpCode::OP_Load_Constant_R), op_load_constant_r, op_load_constant_r_decoded, 0);
        register_operation(static_cast<uint8_t>(OpCode::OP_Move_R), op_move_r, op_move_r_decoded, 0);
        register_operation(static_cast<uint8_t>(OpCode::OP_Add_RRR), op_add_rrr, op_add_rrr_decoded, 0);
        register_operation(static_cast<uint8_t>(OpCode::OP_Less_RR), op_less_rr, op_less_rr_decoded, 0);
        register_operation(static_cast<uint8_t>(OpCode::OP_Incr_R), op_incr_r, op_incr_r_decoded, 0);
        register_operation(static_cast<uint8_t>(OpCode::OP_Less_RRR), op_less_rrr, op_less_rrr_decoded, 0);
        register_operation(static_cast<uint8_t>(OpCode::OP_Jump_If_False_R), op_jump_if_false_r, op_jump_if_false_r_decoded, 0);
    }

    void VM::initialize_builtin_classes()
    {
        // Create the built-in Table class
        Value tableClass = createClass("__Table");
        globals["__Table"] = retainValue(tableClass);

        // Register ComponentProxy class for zero-copy component access
        ComponentProxy::registerWithVM(this);
    }

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
            return makeSmallStringValue(stringContent.data(), static_cast<uint8_t>(stringContent.length()));
        }

        // // Check if string already exists in the intern map
        auto it = pools.internedStrings.find(stringContent);
        if (it != pools.internedStrings.end())
        {
            // String already exists, reuse it
            return retainValue(makeStringValue(it->second));
        }

        // String doesn't exist, create new one
        auto [ptr, index] = pools.stringPool.allocateWithIndex(stringContent);
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
        else if (element.type == UnionType::INT)
        {
            int intVal = element.get<int>();
            return makeIntValue(static_cast<int64_t>(intVal));
        }
        else if (element.type == UnionType::FLOAT)
        {
            float floatVal = element.get<float>();
            return makeDoubleValue(static_cast<double>(floatVal));
        }
        else if (element.type == UnionType::DOUBLE)
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
        else if (IS_INTERNED_STRING(value))
        {
            // Materialize interned string from current chunk's constantStrings
            if (!currentFrame || !currentFrame->closure || !currentFrame->closure->function)
            {
                throw std::runtime_error("Cannot convert interned string to ElementType: no active execution frame");
            }
            uint32_t index = AS_INTERNED_STRING_INDEX(value);
            const auto& constantStrings = currentFrame->closure->function->chunk.constantStrings;
            if (index >= constantStrings.size())
            {
                throw std::runtime_error("Interned string index " + std::to_string(index) +
                                       " out of bounds (size: " + std::to_string(constantStrings.size()) + ")");
            }
            return ElementType(constantStrings[index]);
        }
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
            // Should be an error or need a conversion with explicit toString
        }
        else if (IS_SMALL_STRING(value))
        {
            // Small strings don't have ElementType backing, can't extract int
            // Fall through to error
        }

        throw std::runtime_error("Value is not an integer");
    }

}