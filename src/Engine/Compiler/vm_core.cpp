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

        auto dChunk = currentFrame->closure->function->decodedChunk;

        // Check if we have a pre-decoded chunk - use it for faster execution
        if (dChunk != nullptr)
        {
            return runDecoded(dChunk);
        }

        // Fallback: execute from bytecode (slower path)
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
                    const std::string& opcodeName = opcodeToString(static_cast<OpCode>(opcode));

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
        // Start at the beginning of decoded instructions
        // We need to map currentFrame->ip to instruction index
        size_t instructionIndex = 0;

        // If we're resuming mid-function, find the correct instruction index
        if (currentFrame->ip != currentFrame->closure->function->chunk.code.data())
        {
            size_t bytecodeOffset = currentFrame->ip - currentFrame->closure->function->chunk.code.data();
            instructionIndex = decoded->findInstructionIndex(bytecodeOffset);
        }

        auto startingIp = currentFrame->closure->function->chunk.code.data();

        // Execute with longjmp support
        if (setjmp(exit_jump) == 0)
        {
            while (instructionIndex < decoded->instructions.size())
            {
                const DecodedInstruction& instr = decoded->instructions[instructionIndex];

#ifdef DEBUG_TRACE_EXECUTION
                // Update currentFrame->ip for debug output
                currentFrame->ip = startingIp + instr.bytecodeOffset;

                std::cout << "          ";
                for (size_t i = 0; i < stack.size(); ++i)
                {
                    std::cout << "[";
                    printValue(this, stack[i]);
                    std::cout << "] ";
                }
                std::cout << std::endl;
                disassembleInstruction(this, currentFrame->closure->function->chunk, instr.bytecodeOffset);
#endif
                // Save the current frame before executing
                // (needed to detect frame changes from OP_Call/OP_Return)
                CallFrame* frameBeforeExecution = currentFrame;

                // Execute the pre-decoded instruction
                // The handler is already resolved, operands are already extracted
                if (profiler.isEnabled())
                {
                    const void* chunkPtr = &currentFrame->closure->function->chunk;
                    const std::string& functionName = currentFrame->closure->function->name;
                    const std::string& opcodeName = opcodeToString(static_cast<OpCode>(instr.originalOpcode));

                    auto startTime = std::chrono::high_resolution_clock::now();

                    // Execute handler - it will read operands from currentFrame->ip if needed
                    if (instr.decodedHandler)
                    {
                        instr.decodedHandler(this, instr);
                    }
                    else
                    {
                        // Update currentFrame->ip to point past the opcode to the operands
                        // This allows handlers that read operands via *ip++ to work correctly
                        currentFrame->ip = startingIp + instr.bytecodeOffset + 1;

                        instr.handler(this);
                    }

                    auto endTime = std::chrono::high_resolution_clock::now();
                    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(endTime - startTime).count();

                    profiler.recordInstruction(chunkPtr, functionName, instr.bytecodeOffset,
                                              instr.originalOpcode, opcodeName, duration);
                }
                else
                {
                    // Execute handler - it will read operands from currentFrame->ip if needed
                    if (instr.decodedHandler)
                    {
                        instr.decodedHandler(this, instr);
                    }
                    else
                    {
                        // Update currentFrame->ip to point past the opcode to the operands
                        // This allows handlers that read operands via *ip++ to work correctly
                        currentFrame->ip = startingIp + instr.bytecodeOffset + 1;

                        instr.handler(this);
                    }

                }

                // Fast path: opcodes flagged PURE (no side effects) can't change
                // IP or call/return, so the entire chain of opcode-equality checks
                // below is guaranteed to miss. Skipping it eliminates ~14 host
                // comparisons + 4 branches per dispatched opcode — the bulk of
                // the dispatcher's per-instruction overhead for tight numeric
                // loops where arithmetic/local-access ops dominate.
                if (instr.isPure())
                {
                    instructionIndex++;
                    continue;
                }

                // Handle control flow changes only for specific opcodes
                OpCode opcode = static_cast<OpCode>(instr.originalOpcode);

                // Check for jump instructions that modify IP
                if (opcode == OpCode::OP_Jump or opcode == OpCode::OP_Loop or
                    opcode == OpCode::OP_Long_Jump or opcode == OpCode::OP_Long_Loop)
                {
                    // IP was modified by jump - find the new instruction index
                    instructionIndex = instr.nextInstuctionIndex;
                    continue;
                }

                if (opcode == OpCode::OP_Long_Jump_If_False or opcode == OpCode::OP_Jump_If_False or
                    opcode == OpCode::OP_Long_Jump_If_False_Popping or opcode == OpCode::OP_Jump_If_False_Popping)
                {
                    // The conditional jump can only end in one of two places: fall through
                    // to the next sequential instruction, or jump to the precomputed target
                    // (already baked into instr.nextInstuctionIndex by resolveJumpTargets).
                    // Compare ip to the fall-through bytecode offset — no hash lookup needed.
                    const size_t fallThroughOffset = instr.bytecodeOffset + 1 + instr.operandBytes;
                    const size_t currentIpOffset   = currentFrame->ip - startingIp;
                    if (currentIpOffset == fallThroughOffset)
                        instructionIndex++;
                    else
                        instructionIndex = instr.nextInstuctionIndex;
                    continue;
                }

                // Check for instructions that can change frames
                // OP_Call, OP_Invoke: explicit function calls
                // OP_Get_Property, OP_Set_Property: may call __get/__set metamethods
                // OP_Get_Index, OP_Set_Index: may call __get/__set metamethods for indexing
                if (opcode == OpCode::OP_Call || opcode == OpCode::OP_Invoke ||
                    opcode == OpCode::OP_Get_Property || opcode == OpCode::OP_Set_Property ||
                    opcode == OpCode::OP_Get_Index || opcode == OpCode::OP_Set_Index)
                {
                    // Frame changed if currentFrame is different from what it was before execution
                    if (currentFrame != frameBeforeExecution)
                    {
                        // Reset starting ip pointer for new frame's function
                        startingIp = currentFrame->closure->function->chunk.code.data();
                        updateChunkCache();

                        // Switched to a different function - check if it has decoded chunk
                        if (currentFrame->closure->function->decodedChunk != nullptr)
                        {
                            decoded = currentFrame->closure->function->decodedChunk;
                            instructionIndex = 0;  // Start from beginning of new function

                            // If IP was set to middle of function, find the right index
                            if (currentFrame->ip != startingIp)
                            {
                                size_t bytecodeOffset = currentFrame->ip - startingIp;
                                instructionIndex = decoded->findInstructionIndex(bytecodeOffset);
                            }
                            continue;
                        }
                        else
                        {
                            // New function doesn't have decoded chunk, fall back to bytecode
                            return run();
                        }
                    }
                }

                // Check for return instruction that may restore previous frame
                if (opcode == OpCode::OP_Return)
                {
                    // After return, check if we've returned to a different frame
                    // op_return has already updated currentFrame to point to the caller
                    if (frameCount > 0 and currentFrame != frameBeforeExecution)
                    {
                        startingIp = currentFrame->closure->function->chunk.code.data();
                        updateChunkCache();

                        // Check if the caller has a decoded chunk
                        DecodedChunk* newDecoded = currentFrame->closure->function->decodedChunk;
                        if (newDecoded != nullptr)
                        {
                            decoded = newDecoded;

                            // Find where we are in the caller's decoded chunk
                            // The IP should be pointing right after the OP_Call instruction
                            size_t bytecodeOffset = currentFrame->ip - startingIp;
                            instructionIndex = decoded->findInstructionIndex(bytecodeOffset);
                            continue;
                        }
                        else
                        {
                            return run();
                        }
                    }
                    // If frameCount == 0, we've returned from the top-level script
                    // The longjmp in op_return will have already exited
                }

                // Normal sequential execution - just advance to next instruction
                instructionIndex++;
            }
        }

        return exit_result;
    }

    // Operation handler implementations
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
        if (requiresRefCount(value))
        {
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

        vm->push(constant);  // Primitives/constants just copied
    }

    void op_constant_decoded(VM* vm, const DecodedInstruction& instr)
    {
        // Use pre-resolved constant pointer (no index lookup needed!)
        vm->push(*instr.constantPtr);
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

    void op_long_constant_decoded(VM* vm, const DecodedInstruction& instr)
    {
        // Use pre-resolved constant pointer (no index lookup needed!)
        vm->push(*instr.constantPtr);
    }

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

    void op_get_local_decoded(VM* vm, const DecodedInstruction& instr)
    {
        // Read slot directly from pre-decoded instruction (no memory fetch!)
        uint8_t slot = instr.operands.byte;

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

    void op_set_local_decoded(VM* vm, const DecodedInstruction& instr)
    {
        // Read slot directly from pre-decoded instruction (no memory fetch!)
        uint8_t slot = instr.operands.byte;

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
    void op_set_local_pop(VM* vm)
    {
        uint8_t slot = *vm->currentFrame->ip++;

        Value newValue = vm->pop();
        Value oldValue = vm->currentFrame->slots[slot];

        vm->currentFrame->slots[slot] = newValue;

        if (requiresRefCount(oldValue))
        {
            vm->releaseAndDelete(oldValue);
        }
    }

    void op_set_local_pop_decoded(VM* vm, const DecodedInstruction& instr)
    {
        uint8_t slot = instr.operands.byte;

        Value newValue = vm->pop();
        Value oldValue = vm->currentFrame->slots[slot];

        vm->currentFrame->slots[slot] = newValue;

        if (requiresRefCount(oldValue))
        {
            vm->releaseAndDelete(oldValue);
        }
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

    void op_jump_if_false_popping(VM* vm)
    {
        uint16_t offset = (static_cast<uint16_t>(*vm->currentFrame->ip++) << 8);
        offset |= static_cast<uint16_t>(*vm->currentFrame->ip++);

        Value condition = vm->pop();

        if (not isValueTrue(condition, vm))  // Pass VM for proper string evaluation
        {
            vm->currentFrame->ip += offset;
        }

        if (requiresRefCount(condition))
        {
            vm->releaseAndDelete(condition);
        }
    }

    void op_long_jump_if_false(VM* vm)
    {
        uint32_t offset = (static_cast<uint32_t>(*vm->currentFrame->ip++) << 24);
        offset |= (static_cast<uint32_t>(*vm->currentFrame->ip++) << 16);
        offset |= (static_cast<uint32_t>(*vm->currentFrame->ip++) << 8);
        offset |= static_cast<uint32_t>(*vm->currentFrame->ip++);

        Value condition = vm->peek();

        if (not isValueTrue(condition))
        {
            vm->currentFrame->ip += offset;
        }
    }

    void op_long_jump_if_false_popping(VM* vm)
    {
        uint32_t offset = (static_cast<uint32_t>(*vm->currentFrame->ip++) << 24);
        offset |= (static_cast<uint32_t>(*vm->currentFrame->ip++) << 16);
        offset |= (static_cast<uint32_t>(*vm->currentFrame->ip++) << 8);
        offset |= static_cast<uint32_t>(*vm->currentFrame->ip++);

        Value condition = vm->pop();

        if (not isValueTrue(condition))
        {
            vm->currentFrame->ip += offset;
        }

        if (requiresRefCount(condition))
        {
            vm->releaseAndDelete(condition);
        }
    }

    void op_jump(VM* vm)
    {
        uint16_t offset = (static_cast<uint16_t>(*vm->currentFrame->ip++) << 8);
        offset |= static_cast<uint16_t>(*vm->currentFrame->ip++);

        vm->currentFrame->ip += offset;
    }

    void op_long_jump(VM* vm)
    {
        uint32_t offset = (static_cast<uint32_t>(*vm->currentFrame->ip++) << 24);
        offset |= (static_cast<uint32_t>(*vm->currentFrame->ip++) << 16);
        offset |= (static_cast<uint32_t>(*vm->currentFrame->ip++) << 8);
        offset |= static_cast<uint32_t>(*vm->currentFrame->ip++);

        vm->currentFrame->ip += offset;
    }

    void op_jump_decoded(VM* vm, const DecodedInstruction& instr)
    {
        vm->currentFrame->ip = vm->chunkData + instr.bytecodeOffset;
    }

    void op_loop(VM* vm)
    {
        uint16_t offset = (static_cast<uint16_t>(*vm->currentFrame->ip++) << 8);
        offset |= static_cast<uint16_t>(*vm->currentFrame->ip++);

        vm->currentFrame->ip -= offset;
    }

    void op_long_loop(VM* vm)
    {
        uint32_t offset = (static_cast<uint32_t>(*vm->currentFrame->ip++) << 24);
        offset |= (static_cast<uint32_t>(*vm->currentFrame->ip++) << 16);
        offset |= (static_cast<uint32_t>(*vm->currentFrame->ip++) << 8);
        offset |= static_cast<uint32_t>(*vm->currentFrame->ip++);

        vm->currentFrame->ip -= offset;
    }

    void op_loop_decoded(VM* vm, const DecodedInstruction& instr)
    {
        vm->currentFrame->ip = vm->chunkData - instr.bytecodeOffset;
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

    void op_short_int(VM* vm)
    {
        uint8_t value = *vm->currentFrame->ip++;

        vm->push(makeIntValue(value));
    }

    void op_short_int_decoded(VM* vm, const DecodedInstruction& instr)
    {
        // Operand already pre-extracted by the chunk decoder — skip the
        // currentFrame->ip increment that the non-decoded variant pays.
        vm->push(makeIntValue(instr.operands.byte));
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

    void op_define_constant_global_decoded(VM* vm, const DecodedInstruction& instr)
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