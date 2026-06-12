#include "stdafx.h"

#include "vm.h"

#include <iostream>

#include "compiler.h"

#include "compiler_debug.h"

#include <chrono>

#include "pgconstant.h"

#include "chunk_serializer.h"

#include "decoded_chunk.h"

namespace pg
{
    UniqueIdGenerator VM::globalIdGenerator;

    bool isValueNumber(const Value& val, VM*)
    {
        if (IS_INT(val) or IS_DOUBLE(val))
            return true;

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
            return true;
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
}

namespace pg
{
    VM::VM()
    {
        // Initialize function pointer dispatch table
        register_builtin_operations();

        // Initialize built-in classes (like Table)
        initialize_builtin_classes();

        // Reserve base pool size on startup
        pools.reserve();

        // Initialize single-character string cache for performance
        // Use small string values (inline, no heap allocation, no refcounting)
        for (int i = 0; i < 256; i++)
        {
            char singleChar = static_cast<char>(i);
            singleCharCache[i] = makeSmallStringValue(&singleChar, 1);
        }
    }

    VM::~VM()
    {
        // Free all Values stored in globals before destruction
        for (auto& pair : globals)
        {
            releaseAndDelete(pair.second);
        }

        // Clean up any remaining Values on the stack
        while (not stack.empty())
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

        // Clear the custom pointer pool
        // Todo add a flag when registering a custom pointer type to indicate if VM should free them
        // for (auto& [typeId, ptrList] : pools.customPointerPool)
        // {
        //     for (void* ptr : ptrList)
        //     {
        //         // User is responsible for freeing custom pointers if needed
        //         // Here we just clear the pool
        //     }
        // }

        pools.customPointerPool.clear();
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

            // Pre-decode chunk for faster execution
            if (not func->chunk.code.empty())
            {
                begin = std::chrono::steady_clock::now();

                LOG_INFO("VM", "Pre-decoding chunk for function: " << func->name);
                ChunkDecoder decoder;
                func->decodedChunk = decoder.decode(func->chunk, this);

                end = std::chrono::steady_clock::now();

#ifdef DEBUG_PROFILE_COMPILE
                std::cout << "Chunk decoding took: "
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

#ifdef DEBUG_PROFILE_COMPILE
            begin = std::chrono::steady_clock::now();
#endif

            auto result = run();

#ifdef DEBUG_PROFILE_COMPILE
            end = std::chrono::steady_clock::now();


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

            std::cout << "          ";
            for (size_t i = 0; i < stack.size(); ++i)
            {
                std::cout << "[";
                printValue(this, stack[i]);
                std::cout << "] ";
            }
            std::cout << std::endl;

            return InterpretResult::RUNTIME_ERROR;
        }

    }

    // ============================================================================
    // Helper methods for bytecode execution
    // ============================================================================

    // Pre-decode a function and every function nested in its constants —
    // the dispatcher only knows the decoded path, so every function
    // reachable through OP_Closure must carry a DecodedChunk before it can
    // be called.
    static void predecodeFunctionTree(ObjFunction* funcObj, VM* vm)
    {
        if (funcObj->decodedChunk == nullptr and not funcObj->chunk.code.empty())
        {
            ChunkDecoder decoder;
            funcObj->decodedChunk = decoder.decode(funcObj->chunk, vm);
        }

        for (const auto& constant : funcObj->chunk.constants)
        {
            if (IS_FUNC(constant))
            {
                ObjFunction* nested = vm->asFunction(constant);
                if (nested != nullptr and nested->decodedChunk == nullptr)
                {
                    predecodeFunctionTree(nested, vm);
                }
            }
        }
    }

    InterpretResult VM::executeChunk(ObjFunction* funcObj, int argCount)
    {
        // interpret() pre-decodes during compilation; the bytecode-file
        // entry points reach executeChunk with a fresh chunk (and fresh
        // nested-function constants) that haven't been decoded yet.
        predecodeFunctionTree(funcObj, this);

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

        // Every chunk reachable here has been pre-decoded — interpret() does
        // it during compilation, executeChunk() does it before dispatch.
        // The legacy bytecode interpreter loop has been removed; this is now
        // just an entry-point shim onto the decoded dispatcher.
        return runDecoded(currentFrame->closure->function->decodedChunk);
    }

    // ============================================================================
    // DECODED CHUNK EXECUTION - Fast path with pre-decoded instructions
    // ============================================================================
    // This is the optimized execution path that runs from pre-decoded chunks.
    // Benefits:
    // - No instruction fetch overhead (operands pre-extracted)
    // - No opcode decode (handler pre-resolved)
    // - No operand extraction (already done at load time)
    // - Direct execution from decoded instruction array
    // ============================================================================

    InterpretResult VM::runDecoded(DecodedChunk *decoded)
    {
        // Branch once on the profiler flag so the hot loop below is
        // instantiated without any profiling code when it is off (-p not set).
        if (profiler.isEnabled())
            return runDecodedImpl<true>(decoded);

        return runDecodedImpl<false>(decoded);
    }

    template <bool ProfileEnabled>
    InterpretResult VM::runDecodedImpl(DecodedChunk *decoded)
    {
        if (decoded == nullptr or decoded->instructions.empty())
            return InterpretResult::OK;

        // The instruction pointer lives in a register: each handler returns
        // the next instruction to execute (`&instr + 1` to fall through, a
        // pre-resolved jumpTargetPtr to jump, the callee's first instruction
        // on a frame push, the frame's callerResume on return). No per-
        // instruction VM-memory traffic for sequencing.
        const DecodedInstruction* instr = decoded->instructions.data();

        // If we're resuming mid-function (nested executeChunk entered from a
        // native while a frame is partially executed), map ip back to the
        // decoded instruction.
        uint8_t* startingIp = currentFrame->closure->function->chunk.code.data();
        if (currentFrame->ip != startingIp)
        {
            instr = decoded->instructions.data()
                  + decoded->findInstructionIndex(currentFrame->ip - startingIp);
        }

        // A handler that wants to stop execution (final OP_Return, runtime
        // error, synthetic halt) calls vm_return(result) and returns
        // nullptr; everything else returns the next instruction. The null
        // test is a perfectly-predicted branch — no setjmp/longjmp, so
        // nested runDecoded calls (natives re-entering the VM) each unwind
        // their own loop cleanly.
        exit_result = InterpretResult::OK;

        while (instr)
        {
#ifdef DEBUG_TRACE_EXECUTION
            // Update currentFrame->ip for debug output
            const Chunk& traceChunk = currentFrame->closure->function->chunk;
            currentFrame->ip = const_cast<uint8_t*>(traceChunk.code.data()) + instr->bytecodeOffset;

            std::cout << "          ";
            for (size_t i = 0; i < stack.size(); ++i)
            {
                std::cout << "[";
                printValue(this, stack[i]);
                std::cout << "] ";
            }
            std::cout << std::endl;
            // The synthetic halt's bytecodeOffset is one past the end.
            if (instr->bytecodeOffset < traceChunk.code.size())
                disassembleInstruction(this, traceChunk, instr->bytecodeOffset);
#endif
            if constexpr (ProfileEnabled)
            {
                // Snapshot before dispatch: the handler may switch frames
                // and the call itself overwrites instr.
                const void* chunkPtr = &currentFrame->closure->function->chunk;
                const std::string& functionName = currentFrame->closure->function->name;
                const uint8_t opcode = instr->originalOpcode;
                const size_t  offset = instr->bytecodeOffset;
                const std::string& opcodeName = opcodeToString(static_cast<OpCode>(opcode));

                auto startTime = std::chrono::high_resolution_clock::now();
                instr = instr->decodedHandler(this, *instr);
                auto endTime = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(endTime - startTime).count();

                profiler.recordInstruction(chunkPtr, functionName, offset,
                                           opcode, opcodeName, duration);
            }
            else
            {
                instr = instr->decodedHandler(this, *instr);
            }
        }

        return exit_result;
    }

    // Operation handler implementations
    const DecodedInstruction* op_constant_decoded(VM* vm, const DecodedInstruction& instr)
    {
        // Use pre-resolved constant pointer (no index lookup needed!)
        vm->push(*instr.constantPtr);
        return &instr + 1;
    }

    const DecodedInstruction* op_long_constant_decoded(VM* vm, const DecodedInstruction& instr)
    {
        // Use pre-resolved constant pointer (no index lookup needed!)
        vm->push(*instr.constantPtr);
        return &instr + 1;
    }

    const DecodedInstruction* op_get_local_decoded(VM* vm, const DecodedInstruction& instr)
    {
        // slot is uint8_t so it's already in 0..255. Well-formed bytecode from
        // the compiler doesn't emit 255; assert in debug, no branch in release.
        uint8_t slot = instr.operands.byte;
        assert(slot < 255 and "Local variable slot out of range");

        // retainValue early-returns for primitives (no refcount needed), so
        // unconditionally calling it avoids a duplicate requiresRefCount check
        // on the heap path while keeping the int/double path branch-equivalent.
        vm->push(vm->retainValue(vm->currentFrame->slots[slot]));
        return &instr + 1;
    }

    const DecodedInstruction* op_set_local_decoded(VM* vm, const DecodedInstruction& instr)
    {
        uint8_t slot = instr.operands.byte;
        assert(slot < 255 and "Local variable slot out of range");

        Value newValue = vm->peek(0);
        Value oldValue = vm->currentFrame->slots[slot];

        // retainValue is inline and early-returns for primitives, so the
        // unconditional call costs the same on the int path as the explicit
        // branch would, while saving a duplicate requiresRefCount call on
        // the heap path.
        vm->currentFrame->slots[slot] = vm->retainValue(newValue);

        if (requiresRefCount(oldValue))
        {
            vm->releaseAndDelete(oldValue);
        }
        return &instr + 1;
    }

    // OP_Set_Local_Pop: peephole fusion of OP_Set_Local + OP_Pop, which the
    // compiler emits for every assignment statement. Set_Local leaves the value
    // on the stack (so `var x = expr` can return it); the trailing Pop drops
    // it. Fusing into one op removes a full dispatch per assignment.
    //
    // Semantic difference vs Set_Local: instead of peek + leave-on-stack, we
    // pop. The slot's old value still needs to be released; the popped new
    // value's refcount was already incremented when it was pushed (and the
    // slot now owns that reference), so no retain is needed — we just move it.
    const DecodedInstruction* op_set_local_pop_decoded(VM* vm, const DecodedInstruction& instr)
    {
        uint8_t slot = instr.operands.byte;

        Value newValue = vm->pop();
        Value oldValue = vm->currentFrame->slots[slot];

        vm->currentFrame->slots[slot] = newValue;

        if (requiresRefCount(oldValue))
        {
            vm->releaseAndDelete(oldValue);
        }
        return &instr + 1;
    }

    // op_return_decoded: tears down the returning frame and, on a non-final
    // return, hands the dispatch loop the caller's resume instruction
    // (captured at frame-push time).
    const DecodedInstruction* op_return_decoded(VM* vm, const DecodedInstruction&)
    {
#ifdef DEBUG_CHECK_STACK
        if (vm->stack.empty())
        {
            vm->runtimeError("Nothing in the stack for return.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);

            return nullptr;
        }
#endif
        // Snapshot the resume instruction from the callee frame BEFORE the
        // frame teardown below decrements frameCount and restores
        // currentFrame to the caller.
        const DecodedInstruction* resume = vm->currentFrame->callerResume;

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

            return nullptr;
        }

        // Restore previous frame
        vm->currentFrame = &vm->frames[vm->frameCount - 1];

        // Truncate stack to the callee's stackBase position
        // This removes the function/receiver + args + locals.
        // Walk top-down, releasing each slot (releaseAndDelete is a no-op
        // for primitives), then drop stack_top in one store. Saves the
        // per-slot bounds check / size() reload that the old while-loop
        // ran for every truncated value.
        const size_t stackTruncatePosition = calleeStackBase - vm->stack.data();
        Value*       slot                  = vm->stack.data() + vm->stack.size();
        Value* const end                   = vm->stack.data() + stackTruncatePosition;
        while (slot > end)
        {
            --slot;
            vm->releaseAndDelete(*slot);
        }

        vm->stack.truncateTo(stackTruncatePosition);

        vm->push(value);

        // If we reach here, frameCount > 0 (the frameCount == 0 path above
        // records the result and exits the dispatch loop). currentFrame is
        // already the caller; resume it at the captured instruction.
        return resume;
    }

    // op_call_decoded: self-managing decoded variant of OP_Call. Reads
    // argCount directly from the pre-decoded instruction (no *ip++).
    // Native calls fall through; closure/bound calls switch to the callee's
    // decoded instructions via completeDecodedFrameSwitch.
    const DecodedInstruction* op_call_decoded(VM* vm, const DecodedInstruction& instr)
    {
        int argCount = instr.operands.byte;

        Value function = vm->peek(argCount);

        // Resume point for op_return_decoded, captured into the new frame
        // by call()/callBound() if one is pushed.
        vm->pendingCallResume = &instr + 1;

        const int frameCountBefore = vm->frameCount;

        if (not vm->callValue(function, argCount))
        {
            vm->runtimeError("Cannot call function");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return nullptr;
        }

        return vm->completeDecodedFrameSwitch(frameCountBefore, &instr + 1);
    }

    const DecodedInstruction* op_debug_print_decoded(VM* vm, const DecodedInstruction& instr)
    {
#ifdef DEBUG_CHECK_STACK
        if (vm->stack.empty())
        {
            vm->runtimeError("Nothing to print from the stack.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return nullptr;
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
            return &instr + 1;
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
            return &instr + 1;
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
            return &instr + 1;
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
            return &instr + 1;
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
            return &instr + 1;
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
            return &instr + 1;
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
        return &instr + 1;
    }

    // Decoded jump-if-false variants. The false-branch target pointer is
    // baked into instr.jumpTargetPtr by resolveJumpTargets; the true branch
    // falls through to the next instruction.

    const DecodedInstruction* op_jump_if_false_decoded(VM* vm, const DecodedInstruction& instr)
    {
        if (not isValueTrue(vm->peek(), vm))
            return instr.jumpTargetPtr;

        return &instr + 1;
    }

    const DecodedInstruction* op_jump_if_false_popping_decoded(VM* vm, const DecodedInstruction& instr)
    {
        Value condition = vm->pop();

        const DecodedInstruction* next =
            isValueTrue(condition, vm) ? &instr + 1 : instr.jumpTargetPtr;

        if (requiresRefCount(condition))
            vm->releaseAndDelete(condition);

        return next;
    }

    const DecodedInstruction* op_long_jump_if_false_decoded(VM* vm, const DecodedInstruction& instr)
    {
        if (not isValueTrue(vm->peek()))
            return instr.jumpTargetPtr;

        return &instr + 1;
    }

    const DecodedInstruction* op_long_jump_if_false_popping_decoded(VM* vm, const DecodedInstruction& instr)
    {
        Value condition = vm->pop();

        const DecodedInstruction* next =
            isValueTrue(condition) ? &instr + 1 : instr.jumpTargetPtr;

        if (requiresRefCount(condition))
            vm->releaseAndDelete(condition);

        return next;
    }

    const DecodedInstruction* op_jump_decoded(VM*, const DecodedInstruction& instr)
    {
        // Pre-resolved target pointer baked in by resolveJumpTargets.
        return instr.jumpTargetPtr;
    }

    const DecodedInstruction* op_loop_decoded(VM*, const DecodedInstruction& instr)
    {
        // Pre-resolved target pointer baked in by resolveJumpTargets.
        return instr.jumpTargetPtr;
    }

    const DecodedInstruction* op_short_int_decoded(VM* vm, const DecodedInstruction& instr)
    {
        // Operand already pre-extracted by the chunk decoder.
        vm->push(makeIntValue(instr.operands.byte));
        return &instr + 1;
    }

    const DecodedInstruction* op_define_constant_global_decoded(VM* vm, const DecodedInstruction& instr)
    {
        // Read operands directly from pre-decoded instruction (no memory fetch!)
        uint8_t constant1 = instr.operands.indexed.byte1;
        uint8_t constant2 = instr.operands.indexed.byte2;

        auto value1 = vm->currentFrame->closure->function->chunk.constants[constant1];
        auto value2 = vm->currentFrame->closure->function->chunk.constants[constant2];

        auto name = vm->valueToElement(value2);

        if (not name.isLitteral())
        {
            vm->runtimeError("Global variable name must be a litteral.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return nullptr;
        }

        vm->globals[name.toString()] = vm->retainValue(value1);
        return &instr + 1;
    }

    // ========================================================================
    // Module Operations
    // ========================================================================

    // op_import builds an external VM and replays interpret — no per-
    // instruction operands to extract.
    const DecodedInstruction* op_import_decoded(VM* vm, const DecodedInstruction& instr)
    {
#ifdef DEBUG_CHECK_STACK
        if (vm->stack.empty())
        {
            vm->runtimeError("Stack underflow on import.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return nullptr;
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
            return nullptr;
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
        return &instr + 1;
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

    // ---------------------------------------------------------------------
    // Proper decoded handlers for the previously-wrapped vm_core.cpp ops.
    // Operands come from the pre-extracted DecodedInstruction rather than
    // *ip++; bodies otherwise mirror their legacy counterparts.
    // ---------------------------------------------------------------------

    // Zero-operand ops: body identical to the legacy handler.

    const DecodedInstruction* op_pop_decoded(VM* vm, const DecodedInstruction& instr)
    {
#ifdef DEBUG_CHECK_STACK
        if (vm->stack.empty())
        {
            vm->runtimeError("Nothing to pop from the stack.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return nullptr;
        }
#endif
        auto value = vm->pop();
        if (requiresRefCount(value))
        {
            vm->releaseAndDelete(value);
        }
        return &instr + 1;
    }

    const DecodedInstruction* op_close_upvalue_decoded(VM* vm, const DecodedInstruction& instr)
    {
#ifdef DEBUG_CHECK_STACK
        if (vm->stack.empty())
        {
            vm->runtimeError("Stack underflow on closing upvalue.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return nullptr;
        }
#endif
        vm->closeUpvalues(&vm->stack[vm->stack.size() - 1]);
        auto value = vm->pop();
        vm->releaseAndDelete(value);
        return &instr + 1;
    }

    const DecodedInstruction* op_get_global_decoded(VM* vm, const DecodedInstruction& instr)
    {
#ifdef DEBUG_CHECK_STACK
        if (vm->stack.empty())
        {
            vm->runtimeError("Not enough values on stack for variable retrieval.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return nullptr;
        }
#endif
        auto nameValue = vm->peek();

        if (not IS_STRING(nameValue))
        {
            vm->releaseAndDelete(nameValue);
            vm->pop();
            vm->runtimeError("Global variable name must be a litteral.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return nullptr;
        }

        auto name = vm->asString(nameValue);

        auto it = vm->globals.find(name);
        if (it == vm->globals.end())
        {
            vm->releaseAndDelete(nameValue);
            vm->pop();
            vm->runtimeError("Undefined global variable '" + name + "'.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return nullptr;
        }

        vm->changeTop(vm->retainValue(it->second));
        vm->releaseAndDelete(nameValue);
        return &instr + 1;
    }

    const DecodedInstruction* op_set_global_decoded(VM* vm, const DecodedInstruction& instr)
    {
#ifdef DEBUG_CHECK_STACK
        if (vm->stack.size() < 2)
        {
            vm->runtimeError("Not enough values on stack for variable assignment.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return nullptr;
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
            return nullptr;
        }

        auto it = vm->globals.find(name.toString());
        if (it == vm->globals.end())
        {
            vm->releaseAndDelete(nameValue);
            vm->runtimeError("Undefined global variable '" + name.toString() + "'.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return nullptr;
        }

        vm->releaseAndDelete(it->second);
        it->second = vm->retainValue(value);
        vm->releaseAndDelete(nameValue);
        return &instr + 1;
    }

    const DecodedInstruction* op_define_global_decoded(VM* vm, const DecodedInstruction& instr)
    {
#ifdef DEBUG_CHECK_STACK
        if (vm->stack.size() < 2)
        {
            vm->runtimeError("Not enough values on stack for variable definition.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return nullptr;
        }
#endif
        auto nameValue = vm->pop();
        auto name = vm->valueToElement(nameValue);
        auto value = vm->pop();

        if (not name.isLitteral())
        {
            vm->releaseAndDelete(nameValue);
            vm->releaseAndDelete(value);
            vm->runtimeError("Global variable name must be a litteral.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return nullptr;
        }

        vm->globals[name.toString()] = vm->retainValue(value);
        vm->releaseAndDelete(nameValue);
        vm->releaseAndDelete(value);
        return &instr + 1;
    }

    const DecodedInstruction* op_define_global_non_popping_decoded(VM* vm, const DecodedInstruction& instr)
    {
#ifdef DEBUG_CHECK_STACK
        if (vm->stack.size() < 2)
        {
            vm->runtimeError("Not enough values on stack for variable definition.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return nullptr;
        }
#endif
        auto nameValue = vm->pop();
        auto name = vm->valueToElement(nameValue);
        auto value = vm->peek();

        if (not name.isLitteral())
        {
            vm->releaseAndDelete(nameValue);
            vm->runtimeError("Global variable name must be a litteral.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return nullptr;
        }

        vm->globals[name.toString()] = vm->retainValue(value);
        vm->releaseAndDelete(nameValue);
        return &instr + 1;
    }

    // One-byte-operand ops: operand was reading via *ip++ before; now it's
    // pre-extracted on instr.operands.byte.

    const DecodedInstruction* op_pop_n_decoded(VM* vm, const DecodedInstruction& instr)
    {
        uint8_t count = instr.operands.byte;

        for (int i = 0; i < count; i++)
        {
            auto value = vm->pop();
            if (requiresRefCount(value))
            {
                vm->releaseAndDelete(value);
            }
        }
        return &instr + 1;
    }

    const DecodedInstruction* op_get_upvalue_decoded(VM* vm, const DecodedInstruction& instr)
    {
        uint8_t slot = instr.operands.byte;

        if (slot >= vm->currentFrame->closure->function->upvalueCount)
        {
            vm->runtimeError("Upvalue index out of bounds.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return nullptr;
        }

        ObjUpvalue* upvalue = vm->currentFrame->closure->upvalues[slot];

        if (upvalue->location == &upvalue->closed)
        {
            vm->push(*upvalue->location);
        }
        else
        {
            vm->push(vm->retainValue(*upvalue->location));
        }
        return &instr + 1;
    }

    const DecodedInstruction* op_set_upvalue_decoded(VM* vm, const DecodedInstruction& instr)
    {
        uint8_t slot = instr.operands.byte;
#ifdef DEBUG_CHECK_STACK
        if (vm->stack.empty())
        {
            vm->runtimeError("Not enough values on stack for upvalue assignment.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return nullptr;
        }
#endif
        ObjUpvalue* upvalue = vm->currentFrame->closure->upvalues[slot];
        upvalue->location = &vm->stack[vm->stack.size() - 1];
        return &instr + 1;
    }

    const DecodedInstruction* op_get_constant_global_decoded(VM* vm, const DecodedInstruction& instr)
    {
        uint8_t constant = instr.operands.byte;
        auto value = vm->currentFrame->closure->function->chunk.constants[constant];
        auto name = vm->valueToElement(value);

        if (not name.isLitteral())
        {
            vm->runtimeError("Global variable name must be a litteral.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return nullptr;
        }

        auto it = vm->globals.find(name.toString());
        if (it == vm->globals.end())
        {
            vm->runtimeError("Undefined global variable '" + name.toString() + "'.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return nullptr;
        }

        vm->push(vm->retainValue(it->second));
        return &instr + 1;
    }

    // Two-byte-operand op: operands.indexed.byte1/byte2 hold the two
    // constant indices the legacy version read via two *ip++ reads.

    const DecodedInstruction* op_set_constant_global_decoded(VM* vm, const DecodedInstruction& instr)
    {
        uint8_t constant1 = instr.operands.indexed.byte1;
        uint8_t constant2 = instr.operands.indexed.byte2;

        auto value1 = vm->currentFrame->closure->function->chunk.constants[constant1];
        auto value2 = vm->currentFrame->closure->function->chunk.constants[constant2];
        auto name = vm->valueToElement(value2);

        if (not name.isLitteral())
        {
            vm->runtimeError("Global variable name must be a litteral.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return nullptr;
        }

        auto it = vm->globals.find(name.toString());
        if (it == vm->globals.end())
        {
            vm->runtimeError("Undefined global variable '" + name.toString() + "'.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return nullptr;
        }

        vm->releaseAndDelete(it->second);
        it->second = vm->retainValue(value1);
        vm->push(vm->retainValue(value1));
        return &instr + 1;
    }
}