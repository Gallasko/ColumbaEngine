#include "vm.h"

#include <iostream>

#include "compiler.h"

#include "compiler_debug.h"

#include <chrono>

#include "pgconstant.h"

namespace pg
{
    // Static dispatch table definition
    OpCodeInfo VM::operations[256];

    // Forward declarations for operation handlers
    uint8_t* op_return(VM* vm, uint8_t* ip);
    uint8_t* op_constant(VM* vm, uint8_t* ip);
    uint8_t* op_long_constant(VM* vm, uint8_t* ip);
    uint8_t* op_add(VM* vm, uint8_t* ip);
    uint8_t* op_subtract(VM* vm, uint8_t* ip);
    uint8_t* op_multiply(VM* vm, uint8_t* ip);
    uint8_t* op_divide(VM* vm, uint8_t* ip);
    uint8_t* op_negate(VM* vm, uint8_t* ip);
    uint8_t* op_equal(VM* vm, uint8_t* ip);
    uint8_t* op_greater(VM* vm, uint8_t* ip);
    uint8_t* op_less(VM* vm, uint8_t* ip);
    uint8_t* op_true(VM* vm, uint8_t* ip);
    uint8_t* op_false(VM* vm, uint8_t* ip);
    uint8_t* op_pop(VM* vm, uint8_t* ip);
    uint8_t* op_get_local(VM* vm, uint8_t* ip);
    uint8_t* op_set_local(VM* vm, uint8_t* ip);
    uint8_t* op_get_global(VM* vm, uint8_t* ip);
    uint8_t* op_define_global(VM* vm, uint8_t* ip);
    uint8_t* op_set_global(VM* vm, uint8_t* ip);
    uint8_t* op_jump(VM* vm, uint8_t* ip);
    uint8_t* op_jump_if_false(VM* vm, uint8_t* ip);
    uint8_t* op_long_jump(VM* vm, uint8_t* ip);
    uint8_t* op_long_jump_if_false(VM* vm, uint8_t* ip);
    uint8_t* op_long_loop(VM* vm, uint8_t* ip);
    uint8_t* op_call(VM* vm, uint8_t* ip);
    uint8_t* op_closure(VM* vm, uint8_t* ip);
    uint8_t* op_get_upvalue(VM* vm, uint8_t* ip);
    uint8_t* op_set_upvalue(VM* vm, uint8_t* ip);
    uint8_t* op_close_upvalue(VM* vm, uint8_t* ip);
    uint8_t* op_debug_print(VM* vm, uint8_t* ip);
}

namespace pg
{
    InterpretResult VM::interpret(const std::queue<Token>& tokens)
    {
        // Todo change this
        // Reset the compiler state before compiling a new chunk
        Compiler compiler;

        std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();

        auto *function = compiler.compile(tokens);

        if (not function)
            return InterpretResult::COMPILE_ERROR;

        push(trackNewValue(FUNC_VAL(function)));
        Closure *closure = new Closure(function);
        pop();
        push(trackNewValue(CLOSURE_VAL(closure)));
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
//         if (enableOptimizations && !function->chunk.code.empty())
//         {
//             begin = std::chrono::steady_clock::now();

//             LOG_INFO("VM", "Applying bytecode optimizations");
//             size_t originalSize = function->chunk.code.size();

//             passManager.runAllPasses(function->chunk);

//             size_t optimizedSize = function->chunk.code.size();
//             if (optimizedSize != originalSize)
//             {
//                 LOG_INFO("VM", "Optimization changed bytecode size from " <<
//                          originalSize << " to " << optimizedSize << " bytes");
//             }

//             end = std::chrono::steady_clock::now();

// #ifdef DEBUG_PROFILE_COMPILE
//             std::cout << "Optimizations took: "
//                       << std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count()
//                       << " ns"
//                       << std::endl;
// #endif
//         }

        try
        {
            // Initialize function pointer dispatch table
            register_builtin_operations();

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

    InterpretResult VM::run()
    {
        // Set currentFrame to point to the topmost frame
        currentFrame = &frames[frameCount - 1];

        if (currentFrame->closure->function->chunk.code.empty())
            return InterpretResult::OK;

        // Cache chunk data pointer to avoid repeated vector::data() calls
        updateChunkCache();

        // Function pointer dispatch with longjmp
        if (setjmp(exit_jump) == 0) {
            uint8_t* ip = currentFrame->ip;

            while (true) {
                if (not ip or ip >= chunkDataEnd) {
                    return InterpretResult::OK;
                }

#ifdef DEBUG_TRACE_EXECUTION
                std::cout << "          ";
                for (size_t i = 0; i < stack.size(); ++i)
                {
                    std::cout << "[";
                    printValue(stack[i]);
                    std::cout << "] ";
                }
                std::cout << std::endl;

                // Update frame IP for debug output
                currentFrame->ip = ip;
                disassembleInstruction(currentFrame->closure->function->chunk, ip - chunkData);
#endif

                uint8_t opcode = *ip++;

                // Dispatch to operation handler
                if (operations[opcode].handler) {
                    ip = operations[opcode].handler(this, ip);
                } else {
                    runtimeError("Unknown opcode");
                    return InterpretResult::RUNTIME_ERROR;
                }

                // Update frame IP for potential frame switches
                // currentFrame->ip = ip;
            }
        }

        return exit_result;
    }

    // Old switch statement (will be removed after verification)
    InterpretResult VM::run_old()
    {
        // Set currentFrame to point to the topmost frame
        currentFrame = &frames[frameCount - 1];

        if (currentFrame->closure->function->chunk.code.empty())
            return InterpretResult::OK;

        // Cache chunk data pointer to avoid repeated vector::data() calls
        updateChunkCache();


        for (;;)
        {
            if (checkIpForLoopExit())
            {
                return InterpretResult::OK;
            }

#ifdef DEBUG_TRACE_EXECUTION
            std::cout << "          ";
            for (size_t i = 0; i < stack.size(); ++i)
            {
                std::cout << "[";
                printValue(stack[i]);
                std::cout << "] ";
            }
            std::cout << std::endl;

            // No sync needed - using frame IP directly
            disassembleInstruction(currentFrame->closure->function->chunk, currentFrame->ip - currentFrame->closure->function->chunk.code.data());
#endif
            uint8_t opcode_byte = readByte();
            auto instruction = static_cast<OpCode>(opcode_byte);


            switch (instruction)
            {
                case OpCode::OP_Return:
                {
#ifdef DEBUG_CHECK_STACK
                    if (stack.empty())
                    {
                        EMIT_RUNTIME_ERROR("Nothing in the stack for return.");
                    }
#endif
                    auto value = pop();
                    closeUpvalues(currentFrame->slots);
                    frameCount--;

                    if (frameCount == 0)
                    {
                        releaseAndDelete(value);

                        auto finalValue = pop();
                        releaseAndDelete(finalValue);

                        return InterpretResult::OK;
                    }

                    // Update current frame first, then clean up stack
                    CallFrame* returningFrame = currentFrame;
                    currentFrame = &frames[frameCount - 1];
                    updateChunkCache(); // Update cached chunk data for returning frame

                    while (stack.data() + stack.size() > returningFrame->slots)
                    {
                        auto v = stack.pop();
                        releaseAndDelete(v);
                    }

                    push(retainValue(value));
                    releaseAndDelete(value);
                    break;
                }

                case OpCode::OP_Constant:
                {
                    auto constant = readConstant();
                    push(constant);
                    break;
                }

                case OpCode::OP_LongConstant:
                {
                    auto constant = readLongConstant();
                    push(constant);
                    break;
                }

                case OpCode::OP_Negate:
                {
                    if (not isValueNumber(peek(0)))
                    {
                        EMIT_RUNTIME_ERROR("Operand after an unary (-) must be a number.");
                    }

                    auto value = pop();
                    push(negateValue(value));
                    releaseAndDelete(value);
                    break;
                }

                case OpCode::OP_Add:
                {
                    auto b = pop();
                    auto a = pop();
                    push(addValues(a, b));
                    releaseAndDelete(a);
                    releaseAndDelete(b);
                    break;
                }

                case OpCode::OP_Subtract:
                {
                    auto b = pop();
                    auto a = pop();
                    push(subtractValues(a, b));
                    releaseAndDelete(a);
                    releaseAndDelete(b);
                    break;
                }

                case OpCode::OP_Multiply:
                {
                    auto b = pop();
                    auto a = pop();
                    push(multiplyValues(a, b));
                    releaseAndDelete(a);
                    releaseAndDelete(b);
                    break;
                }

                case OpCode::OP_Divide:
                {
                    auto b = pop();
                    auto a = pop();
                    push(divideValues(a, b));
                    releaseAndDelete(a);
                    releaseAndDelete(b);
                    break;
                }

                case OpCode::OP_True:
                {
                    push(BOOL_VAL(true));
                    break;
                }

                case OpCode::OP_False:
                {
                    push(BOOL_VAL(false));
                    break;
                }

                case OpCode::OP_Not:
                {
                    if (not IS_BOOL(peek(0)))
                    {
                        EMIT_RUNTIME_ERROR("Operand after an unary (!) must be a boolean.");
                    }

                    auto value = pop();
                    push(BOOL_VAL(not isValueTrue(value)));
                    releaseAndDelete(value);
                    break;
                }

                case OpCode::OP_And:
                {
                    checkBooleanBinaryOp();
                    auto b = pop();
                    auto a = pop();

                    bool resultA = isValueTrue(a);
                    bool resultB = isValueTrue(b);
                    push(BOOL_VAL(resultA and resultB));
                    releaseAndDelete(a);
                    releaseAndDelete(b);
                    break;
                }

                case OpCode::OP_Or:
                {
                    checkBooleanBinaryOp();
                    auto b = pop();
                    auto a = pop();

                    bool resultA = isValueTrue(a);
                    bool resultB = isValueTrue(b);
                    push(BOOL_VAL(resultA or resultB));
                    releaseAndDelete(a);
                    releaseAndDelete(b);
                    break;
                }

// Macro to generate comparison operation cases with exception handling
// Usage: COMPARISON_OP(==) generates a complete case block for equality comparison
// Handles stack operations, type checking, and runtime error management
#define COMPARISON_OP(op) \
                { \
                    checkBooleanBinaryOp(); \
                    auto b = pop(); \
                    auto a = pop(); \
                    try { \
                        push(op(a, b)); \
                    } catch (const std::exception& e) { \
                        releaseAndDelete(a); \
                        releaseAndDelete(b); \
                        EMIT_RUNTIME_ERROR("Comparison operation failed: " << e.what()); \
                    } \
                    releaseAndDelete(a); \
                    releaseAndDelete(b); \
                    break; \
                }

                case OpCode::OP_Equal:
                    COMPARISON_OP(equalsValues)

                case OpCode::OP_NotEqual:
                    COMPARISON_OP(notEqualsValues)

                case OpCode::OP_Greater:
                    COMPARISON_OP(greaterValues)

                case OpCode::OP_GreaterEqual:
                    COMPARISON_OP(greaterEqualValues)

                case OpCode::OP_Less:
                    COMPARISON_OP(lessValues)

                case OpCode::OP_LessEqual:
                    COMPARISON_OP(lessEqualValues)

                case OpCode::OP_Pop:
                {
#ifdef DEBUG_CHECK_STACK
                    if (stack.empty())
                    {
                        EMIT_RUNTIME_ERROR("Nothing to pop from the stack.");
                    }
#endif
                    auto value = pop();
                    releaseAndDelete(value);

                    break;
                }

                case OpCode::OP_Define_Global:
                {
#ifdef DEBUG_CHECK_STACK
                    if (stack.size() < 2)
                    {
                        EMIT_RUNTIME_ERROR("Not enough values on stack for variable definition.");
                    }
#endif
                    auto nameValue = pop();  // variable name
                    auto name = valueToElement(nameValue);
                    auto value = pop(); // variable value

                    if (not name.isLitteral())
                    {
                        releaseAndDelete(nameValue);
                        releaseAndDelete(value);
                        EMIT_RUNTIME_ERROR("Global variable name must be a litteral.");
                    }

                    globals[name.toString()] = retainValue(value);
                    releaseAndDelete(nameValue);
                    releaseAndDelete(value); // Release the local reference since we retained for globals
                    break;
                }

                case OpCode::OP_Get_Global:
                {
#ifdef DEBUG_CHECK_STACK
                    if (stack.empty())
                    {
                        EMIT_RUNTIME_ERROR("Not enough values on stack for variable retrieval.");
                    }
#endif
                    auto nameValue = pop();  // variable name
                    auto name = valueToElement(nameValue);

                    if (not name.isLitteral())
                    {
                        releaseAndDelete(nameValue);
                        EMIT_RUNTIME_ERROR("Global variable name must be a litteral.");
                    }

                    auto it = globals.find(name.toString());
                    if (it == globals.end())
                    {
                        releaseAndDelete(nameValue);
                        EMIT_RUNTIME_ERROR("Undefined global variable '" << name.toString() << "'.");
                    }

                    push(it->second); // Don't retain - global already holds reference
                    releaseAndDelete(nameValue);
                    break;
                }

                case OpCode::OP_Set_Global:
                {
#ifdef DEBUG_CHECK_STACK
                    if (stack.size() < 2)
                    {
                        EMIT_RUNTIME_ERROR("Not enough values on stack for variable assignment.");
                    }
#endif
                    auto value = pop(); // new variable value
                    auto nameValue = pop();  // variable name
                    auto name = valueToElement(nameValue);

                    if (not name.isLitteral())
                    {
                        releaseAndDelete(nameValue);
                        releaseAndDelete(value);
                        EMIT_RUNTIME_ERROR("Global variable name must be a litteral.");
                    }

                    auto it = globals.find(name.toString());
                    if (it == globals.end())
                    {
                        releaseAndDelete(nameValue);
                        releaseAndDelete(value);
                        EMIT_RUNTIME_ERROR("Undefined global variable '" << name.toString() << "'.");
                    }

                    // Free the old value that was stored
                    releaseAndDelete(it->second);
                    it->second = retainValue(value);  // Store retained value in globals
                    push(value);  // Push the original to stack
                    releaseAndDelete(nameValue);
                    break;
                }

                case OpCode::OP_Get_Local:
                {
                    uint8_t slot = readByte(); // Get the slot index as immediate operand

                    if (slot >= 255)
                    {
                        EMIT_RUNTIME_ERROR("Local variable slot index out of bounds.");
                    }

                    push(retainValue(currentFrame->slots[slot]));
                    break;
                }

                case OpCode::OP_Set_Local:
                {
#ifdef DEBUG_CHECK_STACK
                    if (stack.empty())
                    {
                        EMIT_RUNTIME_ERROR("Not enough values on stack for local assignment.");
                    }
#endif
                    uint8_t slot = readByte(); // Get the slot index as immediate operand
                    auto value = pop(); // New value

                    if (slot >= 255)
                    {
                        releaseAndDelete(value);
                        EMIT_RUNTIME_ERROR("Local variable slot index out of bounds.");
                    }

                    // Free the old value that was in the frame slot
                    releaseAndDelete(currentFrame->slots[slot]);
                    currentFrame->slots[slot] = retainValue(value);
                    push(value); // Assignment expression returns the value
                    break;
                }

                case OpCode::OP_Jump_If_False:
                {
#ifdef DEBUG_CHECK_STACK
                    if (checkIpAgainstStack(2))
                    {
                        EMIT_RUNTIME_ERROR("Not enough bytes to read jump offset.");
                    }
#endif
                    uint16_t jumpOffset = readUint16();
#ifdef DEBUG_CHECK_STACK
                    if (stack.empty())
                    {
                        EMIT_RUNTIME_ERROR("Stack underflow on conditional jump.");
                    }
#endif
                    auto condition = peek();

                    if (not isValueTrue(condition))
                    {
                        currentFrame->ip += jumpOffset;
#ifdef DEBUG_CHECK_STACK
                        if (checkIpAgainstStack(1))
                        {
                            EMIT_RUNTIME_ERROR("Jump offset out of bounds.");
                        }
#endif
                    }

                    break;
                }

                case OpCode::OP_Long_Jump_If_False:
                {
#ifdef DEBUG_CHECK_STACK
                    if (checkIpAgainstStack(4))
                    {
                        EMIT_RUNTIME_ERROR("Not enough bytes to read long jump offset.");
                    }
#endif
                    uint32_t jumpOffset = readUint32();
#ifdef DEBUG_CHECK_STACK
                    if (stack.empty())
                    {
                        EMIT_RUNTIME_ERROR("Stack underflow on conditional jump.");
                    }
#endif
                    auto condition = peek();

                    if (not isValueTrue(condition))
                    {
                        currentFrame->ip += jumpOffset;
#ifdef DEBUG_CHECK_STACK
                        if (checkIpAgainstStack(1))
                        {
                            EMIT_RUNTIME_ERROR("Jump offset out of bounds.");
                        }
#endif
                    }

                    break;
                }

                case OpCode::OP_Jump:
                {
#ifdef DEBUG_CHECK_STACK
                    if (checkIpAgainstStack(2))
                    {
                        EMIT_RUNTIME_ERROR("Not enough bytes to read jump offset.");
                    }
#endif
                    uint16_t jumpOffset = readUint16();

                    currentFrame->ip += jumpOffset;
#ifdef DEBUG_CHECK_STACK
                    if (checkIpAgainstStack(1))
                    {
                        EMIT_RUNTIME_ERROR("Jump offset out of bounds.");
                    }
#endif

                    break;
                }

                case OpCode::OP_Long_Jump:
                {
#ifdef DEBUG_CHECK_STACK
                    if (checkIpAgainstStack(4))
                    {
                        EMIT_RUNTIME_ERROR("Not enough bytes to read long jump offset.");
                    }
#endif
                    uint32_t jumpOffset = readUint32();

                    currentFrame->ip += jumpOffset;
#ifdef DEBUG_CHECK_STACK
                    if (checkIpAgainstStack(1))
                    {
                        EMIT_RUNTIME_ERROR("Jump offset out of bounds.");
                    }
#endif

                    break;
                }

                case OpCode::OP_Loop:
                {
#ifdef DEBUG_CHECK_STACK
                    if (checkIpAgainstStack(2))
                    {
                        EMIT_RUNTIME_ERROR("Not enough bytes to read loop offset.");
                    }
#endif
                    uint16_t loopOffset = readUint16();
#ifdef DEBUG_CHECK_STACK
                    // No sync needed - using frame IP directly
                    if (loopOffset > (currentFrame->ip - currentFrame->closure->function->chunk.code.data()))
                    {
                        EMIT_RUNTIME_ERROR("Loop offset out of bounds.");
                    }
#endif
                    currentFrame->ip -= loopOffset;
                    break;
                }

                case OpCode::OP_Long_Loop:
                {
#ifdef DEBUG_CHECK_STACK
                    if (checkIpAgainstStack(4))
                    {
                        EMIT_RUNTIME_ERROR("Not enough bytes to read long loop offset.");
                    }
#endif
                    uint32_t loopOffset = readUint32();
#ifdef DEBUG_CHECK_STACK
                    // No sync needed - using frame IP directly
                    if (loopOffset > (currentFrame->ip - currentFrame->closure->function->chunk.code.data()))
                    {
                        EMIT_RUNTIME_ERROR("Loop offset out of bounds.");
                    }
#endif
                    currentFrame->ip -= loopOffset;
                    break;
                }

                case OpCode::OP_Debug_Print:
                {
#ifdef DEBUG_CHECK_STACK
                    if (stack.empty())
                    {
                        EMIT_RUNTIME_ERROR("Nothing to print from the stack.");
                    }
#endif
                    auto value = pop();

                    if (IS_FUNC(value))
                    {
                        ObjFunction* func = AS_FUNC(value);
                        if (func != nullptr)
                        {
                            testOutput += "<" + func->name + "> \n";
                        }
                        else
                        {
                            testOutput += "<script> \n";
                        }
                        releaseAndDelete(value);
                        break;
                    }

                    // For testing: append to testOutput buffer instead of stdout
                    ElementType elem = valueToElement(value);
                    testOutput += elem.toString() + "\n";
                    releaseAndDelete(value);

                    break;
                }

                case OpCode::OP_Post_Incr_Global:
                {
#ifdef DEBUG_CHECK_STACK
                    if (stack.size() < 1)
                    {
                        EMIT_RUNTIME_ERROR("Stack underflow on post-increment.");
                    }
#endif
                    auto nameValue = pop();  // variable name
                    auto name = valueToElement(nameValue);

                    if (not name.isLitteral())
                    {
                        releaseAndDelete(nameValue);
                        EMIT_RUNTIME_ERROR("Global variable name must be a litteral.");
                    }

                    auto it = globals.find(name.toString());
                    if (it == globals.end())
                    {
                        releaseAndDelete(nameValue);
                        EMIT_RUNTIME_ERROR("Undefined global variable '" << name.toString() << "'.");
                    }

                    if (not isValueNumber(it->second))
                    {
                        releaseAndDelete(nameValue);
                        EMIT_RUNTIME_ERROR("Operand after an unary (++) must be a number.");
                    }

                    auto newValue = addValues(it->second, INT_VAL(1));
                    releaseAndDelete(it->second);
                    it->second = newValue;

                    releaseAndDelete(nameValue);
                    break;
                }

                case OpCode::OP_Incr_Global:
                {
#ifdef DEBUG_CHECK_STACK
                    if (stack.empty())
                    {
                        EMIT_RUNTIME_ERROR("Stack underflow on increment.");
                    }
#endif
                    auto nameValue = pop();  // variable name
                    auto name = valueToElement(nameValue);

                    if (not name.isLitteral())
                    {
                        releaseAndDelete(nameValue);
                        EMIT_RUNTIME_ERROR("Global variable name must be a litteral.");
                    }

                    auto it = globals.find(name.toString());
                    if (it == globals.end())
                    {
                        releaseAndDelete(nameValue);
                        EMIT_RUNTIME_ERROR("Undefined global variable '" << name.toString() << "'.");
                    }

                    if (not isValueNumber(it->second))
                    {
                        releaseAndDelete(nameValue);
                        EMIT_RUNTIME_ERROR("Operand after an unary (++) must be a number.");
                    }

                    auto newValue = addValues(it->second, INT_VAL(1));
                    releaseAndDelete(it->second);
                    it->second = newValue;

                    push(retainValue(newValue)); // Pre-increment returns the new value
                    releaseAndDelete(nameValue);
                    break;
                }

                case OpCode::OP_Post_Decr_Global:
                {
#ifdef DEBUG_CHECK_STACK
                    if (stack.size() < 1)
                    {
                        EMIT_RUNTIME_ERROR("Stack underflow on post-decrement.");
                    }
#endif
                    auto nameValue = pop();  // variable name
                    auto name = valueToElement(nameValue);

                    if (not name.isLitteral())
                    {
                        releaseAndDelete(nameValue);
                        EMIT_RUNTIME_ERROR("Global variable name must be a litteral.");
                    }

                    auto it = globals.find(name.toString());
                    if (it == globals.end())
                    {
                        releaseAndDelete(nameValue);
                        EMIT_RUNTIME_ERROR("Undefined global variable '" << name.toString() << "'.");
                    }

                    if (not isValueNumber(it->second))
                    {
                        releaseAndDelete(nameValue);
                        EMIT_RUNTIME_ERROR("Operand after an unary (--) must be a number.");
                    }

                    auto newValue = subtractValues(it->second, INT_VAL(1));
                    releaseAndDelete(it->second);
                    it->second = newValue;

                    releaseAndDelete(nameValue);
                    break;
                }

                case OpCode::OP_Decr_Global:
                {
#ifdef DEBUG_CHECK_STACK
                    if (stack.empty())
                    {
                        EMIT_RUNTIME_ERROR("Stack underflow on decrement.");
                    }
#endif
                    auto nameValue = pop();  // variable name
                    auto name = valueToElement(nameValue);

                    if (not name.isLitteral())
                    {
                        releaseAndDelete(nameValue);
                        EMIT_RUNTIME_ERROR("Global variable name must be a litteral.");
                    }

                    auto it = globals.find(name.toString());
                    if (it == globals.end())
                    {
                        releaseAndDelete(nameValue);
                        EMIT_RUNTIME_ERROR("Undefined global variable '" << name.toString() << "'.");
                    }

                    if (not isValueNumber(it->second))
                    {
                        releaseAndDelete(nameValue);
                        EMIT_RUNTIME_ERROR("Operand after an unary (--) must be a number.");
                    }

                    auto newValue = subtractValues(it->second, INT_VAL(1));
                    releaseAndDelete(it->second);
                    it->second = newValue;

                    push(retainValue(newValue)); // Pre-decrement returns the new value
                    releaseAndDelete(nameValue);
                    break;
                }

                case OpCode::OP_Post_Incr_Local:
                {
#ifdef DEBUG_CHECK_STACK
                    if (stack.size() < 1)
                    {
                        EMIT_RUNTIME_ERROR("Not enough values on stack for local post-increment.");
                    }
#endif
                    auto slot = pop(); // Get the slot index from stack

                    if (not isValueNumber(slot))
                    {
                        releaseAndDelete(slot);
                        EMIT_RUNTIME_ERROR("Local variable slot must be a number.");
                    }

                    int index = getValueAsInt(slot);
                    if (index < 0)
                    {
                        releaseAndDelete(slot);
                        EMIT_RUNTIME_ERROR("Local variable index cannot be negative.");
                    }

                    if (not isValueNumber(currentFrame->slots[index]))
                    {
                        releaseAndDelete(slot);
                        EMIT_RUNTIME_ERROR("Operand after an unary (++) must be a number.");
                    }

                    auto oldValue = retainValue(currentFrame->slots[index]);
                    auto newValue = addValues(currentFrame->slots[index], INT_VAL(1));
                    releaseAndDelete(currentFrame->slots[index]);
                    currentFrame->slots[index] = newValue;

                    push(oldValue); // Post-increment returns the old value
                    releaseAndDelete(slot);
                    break;
                }

                case OpCode::OP_Incr_Local:
                {
#ifdef DEBUG_CHECK_STACK
                    if (stack.size() < 1)
                    {
                        EMIT_RUNTIME_ERROR("Not enough values on stack for local increment.");
                    }
#endif
                    auto slot = pop(); // Get the slot index from stack

                    if (not isValueNumber(slot))
                    {
                        releaseAndDelete(slot);
                        EMIT_RUNTIME_ERROR("Local variable slot must be a number.");
                    }

                    int index = getValueAsInt(slot);
                    if (index < 0)
                    {
                        releaseAndDelete(slot);
                        EMIT_RUNTIME_ERROR("Local variable index cannot be negative.");
                    }

                    if (not isValueNumber(currentFrame->slots[index]))
                    {
                        releaseAndDelete(slot);
                        EMIT_RUNTIME_ERROR("Operand after an unary (++) must be a number.");
                    }

                    auto newValue = addValues(currentFrame->slots[index], INT_VAL(1));
                    releaseAndDelete(currentFrame->slots[index]);
                    currentFrame->slots[index] = newValue;

                    push(retainValue(newValue)); // Pre-increment returns the new value
                    releaseAndDelete(slot);
                    break;
                }

                case OpCode::OP_Post_Decr_Local:
                {
#ifdef DEBUG_CHECK_STACK
                    if (stack.size() < 1)
                    {
                        EMIT_RUNTIME_ERROR("Not enough values on stack for local post-decrement.");
                    }
#endif
                    auto slot = pop(); // Get the slot index from stack

                    if (not isValueNumber(slot))
                    {
                        releaseAndDelete(slot);
                        EMIT_RUNTIME_ERROR("Local variable slot must be a number.");
                    }

                    int index = getValueAsInt(slot);
                    if (index < 0)
                    {
                        releaseAndDelete(slot);
                        EMIT_RUNTIME_ERROR("Local variable index cannot be negative.");
                    }

                    if (not isValueNumber(currentFrame->slots[index]))
                    {
                        releaseAndDelete(slot);
                        EMIT_RUNTIME_ERROR("Operand after an unary (--) must be a number.");
                    }

                    auto oldValue = retainValue(currentFrame->slots[index]);
                    auto newValue = subtractValues(currentFrame->slots[index], INT_VAL(1));
                    releaseAndDelete(currentFrame->slots[index]);
                    currentFrame->slots[index] = newValue;

                    push(oldValue); // Post-decrement returns the old value
                    releaseAndDelete(slot);
                    break;
                }

                case OpCode::OP_Decr_Local:
                {
#ifdef DEBUG_CHECK_STACK
                    if (stack.size() < 1)
                    {
                        EMIT_RUNTIME_ERROR("Not enough values on stack for local decrement.");
                    }
#endif
                    auto slot = pop(); // Get the slot index from stack

                    if (not isValueNumber(slot))
                    {
                        releaseAndDelete(slot);
                        EMIT_RUNTIME_ERROR("Local variable slot must be a number.");
                    }

                    int index = getValueAsInt(slot);
                    if (index < 0)
                    {
                        releaseAndDelete(slot);
                        EMIT_RUNTIME_ERROR("Local variable index cannot be negative.");
                    }

                    if (not isValueNumber(currentFrame->slots[index]))
                    {
                        releaseAndDelete(slot);
                        EMIT_RUNTIME_ERROR("Operand after an unary (--) must be a number.");
                    }

                    auto newValue = subtractValues(currentFrame->slots[index], INT_VAL(1));
                    releaseAndDelete(currentFrame->slots[index]);
                    currentFrame->slots[index] = newValue;

                    push(retainValue(newValue)); // Pre-decrement returns the new value
                    releaseAndDelete(slot);
                    break;
                }

                case OpCode::OP_Call:
                {
                    int argCount = readByte();

                    // Get the function object (at position argCount from top)
                    Value function = peek(argCount);

                    if (not callValue(function, argCount))
                    {
                        EMIT_RUNTIME_ERROR("Cannot call funtion");
                    }

                    // Remove the function object from the stack by shifting arguments up
                    for (int i = argCount - 1; i >= 0; i--)
                    {
                        stack[stack.size() - argCount - 1 + i] = stack[stack.size() - argCount + i];
                    }
                    stack.pop(); // Remove the duplicate top element

                    // Update the frame slots to point to the shifted arguments
                    frames[frameCount - 1].slots = stack.data() + stack.size() - argCount;

                    currentFrame = &frames[frameCount - 1];
                    updateChunkCache(); // Update cached chunk data for new frame
                    break;
                }

                case OpCode::OP_Closure:
                {
                    auto functionValue = readConstant();
                    if (not IS_FUNC(functionValue))
                    {
                        EMIT_RUNTIME_ERROR("Closure operand must be a function.");
                    }

                    ObjFunction* function = AS_FUNC(functionValue);
                    auto closure = new Closure(function);
                    push(trackNewValue(CLOSURE_VAL(closure)));

                    for (int i = 0; i < function->upvalueCount; i++)
                    {
                        uint8_t isLocal = readByte();
                        uint8_t index = readByte();
                        if (isLocal)
                        {
                            closure->upvalues[i] = captureUpvalue(currentFrame->slots + index);
                        }
                        else
                        {
                            closure->upvalues[i] = currentFrame->closure->upvalues[index];
                        }
                    }
                    break;
                }

                case OpCode::OP_Get_Upvalue:
                {
                    uint8_t slot = readByte(); // Get the upvalue index as immediate operand

                    if (slot >= currentFrame->closure->function->upvalueCount)
                    {
                        EMIT_RUNTIME_ERROR("Upvalue index out of bounds.");
                    }

                    ObjUpvalue* upvalue = currentFrame->closure->upvalues[slot];
                    push(retainValue(*upvalue->location));
                    break;
                }

                case OpCode::OP_Set_Upvalue:
                {
                    uint8_t slot = readByte(); // Get the upvalue index as immediate operand
#ifdef DEBUG_CHECK_STACK
                    if (stack.empty())
                    {
                        EMIT_RUNTIME_ERROR("Not enough values on stack for upvalue assignment.");
                    }
#endif
                    ObjUpvalue* upvalue = currentFrame->closure->upvalues[slot];
                    upvalue->location = &stack[stack.size() - 1 - 0];
                    break;
                }

                case OpCode::OP_Close_Upvalue:
                {
#ifdef DEBUG_CHECK_STACK
                    if (stack.empty())
                    {
                        EMIT_RUNTIME_ERROR("Stack underflow on closing upvalue.");
                    }
#endif
                    closeUpvalues(&stack[stack.size() - 1]);
                    pop();
                    break;
                }

                default:
                    std::cout << "Unknown opcode " << static_cast<int>(instruction) << std::endl;
                    return InterpretResult::RUNTIME_ERROR;
            }
        }

        return InterpretResult::OK;
    }

    Value VM::readConstant()
    {
        uint8_t constantIndex = readByte();

#ifdef DEBUG_CHECK_STACK
        if (constantIndex >= currentFrame->closure->function->chunk.constants.size())
        {
            throw std::runtime_error("Constant index out of bounds.");
        }
#endif

        Value constant = currentFrame->closure->function->chunk.constants[constantIndex];
        // Constants are immutable and never reference-counted
        // Just return them as-is, they'll be cleaned up when the chunk is destroyed
        return constant;
    }

    Value VM::readLongConstant()
    {
#ifdef DEBUG_CHECK_STACK
        if (checkIpAgainstStack(2))
        {
            throw std::runtime_error("Not enough bytes to read long constant index.");
        }
#endif

        uint32_t constantIndex = (static_cast<uint32_t>(readByte()) << 16);
        constantIndex |= (static_cast<uint32_t>(readByte()) << 8);
        constantIndex |= static_cast<uint32_t>(readByte());

#ifdef DEBUG_CHECK_STACK
        if (constantIndex >= currentFrame->closure->function->chunk.constants.size())
        {
            throw std::runtime_error("Long constant index out of bounds.");
        }
#endif

        Value constant = currentFrame->closure->function->chunk.constants[constantIndex];
        // Constants are immutable and never reference-counted
        // Just return them as-is, they'll be cleaned up when the chunk is destroyed
        return constant;
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

        ObjUpvalue* newUpvalue = new ObjUpvalue(local);
        trackNewValue(UPVALUE_VAL(newUpvalue));
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
            upvalue->closed = retainValue(*upvalue->location);
            upvalue->location = &upvalue->closed;
            openUpvalues = upvalue->next;
        }
    }

    bool VM::callValue(const Value& callee, int argCount)
    {
        switch (callee.type)
        {
            case CompilerValueType::COMPILER_VAL_CLOSURE:
                return call(AS_CLOSURE(callee), argCount);

            case CompilerValueType::COMPILER_VAL_NATIVE:
            {
                auto* native = AS_NAT_FUNC(callee);
                Value result = native->function(argCount, stack.data() + stack.size() - argCount);

                // Remove arguments from the stack
                for (int i = 0; i < argCount; i++)
                {
                    auto v = pop();
                    releaseAndDelete(v);
                }

                push(result);
                return true;
            }

            default:
                break;
        }

        runtimeError("Can only call functions and classes");

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

        return true;
    }


    int VM::getValueRefCount(const Value& value) const
    {
        void* ptr = nullptr;
        switch(value.type) {
            case COMPILER_VAL_OBJ:     ptr = value.as.obj; break;
            case COMPILER_VAL_FUNC:    ptr = value.as.function; break;
            case COMPILER_VAL_CLOSURE: ptr = value.as.closure; break;
            case COMPILER_VAL_UPVALUE: ptr = value.as.upvalue; break;
            case COMPILER_VAL_NATIVE:  ptr = value.as.nativeFunc; break;
            default: return 0; // Primitives
        }

        if (ptr != nullptr) {
            auto it = refCounts.find(ptr);
            if (it != refCounts.end()) {
                return it->second;
            }
        }
        return 0;
    }

    void VM::deleteValue(const Value& value)
    {
        // Perform type-specific deletion
        switch(value.type) {
            case COMPILER_VAL_OBJ:
                if (value.as.obj) delete value.as.obj;
                break;
            case COMPILER_VAL_FUNC:
                if (value.as.function) delete value.as.function;
                break;
            case COMPILER_VAL_CLOSURE:
                if (value.as.closure) delete value.as.closure;
                break;
            case COMPILER_VAL_UPVALUE:
                if (value.as.upvalue) delete value.as.upvalue;
                break;
            case COMPILER_VAL_NATIVE:
                if (value.as.nativeFunc) delete value.as.nativeFunc;
                break;
            default:
                // Primitives don't need deletion
                break;
        }
    }


    void VM::releaseAndDelete(const Value& value)
    {
        if (releaseValue(value)) {
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

        // Handle legacy object-based floats (for compatibility)
        if (IS_INT(a) and IS_OBJ(b) and AS_OBJ(b)->isNumber())
        {
            double floatA = static_cast<double>(AS_INT(a));
            double floatB = (*AS_OBJ(b)).get<float>();
            return FLOAT_VAL(floatA + floatB);
        }

        if (IS_OBJ(a) and AS_OBJ(a)->isNumber() and IS_INT(b))
        {
            double floatA = (*AS_OBJ(a)).get<float>();
            double floatB = static_cast<double>(AS_INT(b));
            return FLOAT_VAL(floatA + floatB);
        }

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

        // Handle legacy object-based floats (for compatibility)
        if (IS_INT(a) and IS_OBJ(b) and AS_OBJ(b)->isNumber())
        {
            double floatA = static_cast<double>(AS_INT(a));
            double floatB = (*AS_OBJ(b)).get<float>();
            return FLOAT_VAL(floatA - floatB);
        }

        if (IS_OBJ(a) and AS_OBJ(a)->isNumber() and IS_INT(b))
        {
            double floatA = (*AS_OBJ(a)).get<float>();
            double floatB = static_cast<double>(AS_INT(b));
            return FLOAT_VAL(floatA - floatB);
        }

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

        // Handle legacy object-based floats (for compatibility)
        if (IS_INT(a) and IS_OBJ(b) and AS_OBJ(b)->isNumber())
        {
            double floatA = static_cast<double>(AS_INT(a));
            double floatB = (*AS_OBJ(b)).get<float>();
            return FLOAT_VAL(floatA * floatB);
        }

        if (IS_OBJ(a) and AS_OBJ(a)->isNumber() and IS_INT(b))
        {
            double floatA = (*AS_OBJ(a)).get<float>();
            double floatB = static_cast<double>(AS_INT(b));
            return FLOAT_VAL(floatA * floatB);
        }

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

        // Handle legacy object-based floats (for compatibility)
        if (IS_INT(a) and IS_OBJ(b) and AS_OBJ(b)->isNumber())
        {
            double floatA = static_cast<double>(AS_INT(a));
            double floatB = (*AS_OBJ(b)).get<float>();
            if (areNotAlmostEqual(static_cast<float>(floatB), 0.0f))
                return FLOAT_VAL(floatA / floatB);
        }

        if (IS_OBJ(a) and AS_OBJ(a)->isNumber() and IS_INT(b))
        {
            double floatA = (*AS_OBJ(a)).get<float>();
            double floatB = static_cast<double>(AS_INT(b));
            if (areNotAlmostEqual(static_cast<float>(floatB), 0.0f))
                return FLOAT_VAL(floatA / floatB);
        }

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

    void VM::register_operation(uint8_t opcode, OpHandler handler, const char* name, uint8_t operand_count) {
        operations[opcode] = OpCodeInfo(handler, name, operand_count);
    }

    void VM::register_builtin_operations() {
        register_operation(static_cast<uint8_t>(OpCode::OP_Return), op_return, "RETURN");
        register_operation(static_cast<uint8_t>(OpCode::OP_Constant), op_constant, "CONSTANT", 1);
        register_operation(static_cast<uint8_t>(OpCode::OP_LongConstant), op_long_constant, "LONGCONSTANT", 3);
        register_operation(static_cast<uint8_t>(OpCode::OP_Add), op_add, "ADD");
        register_operation(static_cast<uint8_t>(OpCode::OP_Subtract), op_subtract, "SUBTRACT");
        register_operation(static_cast<uint8_t>(OpCode::OP_Multiply), op_multiply, "MULTIPLY");
        register_operation(static_cast<uint8_t>(OpCode::OP_Divide), op_divide, "DIVIDE");
        register_operation(static_cast<uint8_t>(OpCode::OP_Negate), op_negate, "NEGATE");
        register_operation(static_cast<uint8_t>(OpCode::OP_Equal), op_equal, "EQUAL");
        register_operation(static_cast<uint8_t>(OpCode::OP_Greater), op_greater, "GREATER");
        register_operation(static_cast<uint8_t>(OpCode::OP_Less), op_less, "LESS");
        register_operation(static_cast<uint8_t>(OpCode::OP_True), op_true, "TRUE");
        register_operation(static_cast<uint8_t>(OpCode::OP_False), op_false, "FALSE");
        register_operation(static_cast<uint8_t>(OpCode::OP_Pop), op_pop, "POP");
        register_operation(static_cast<uint8_t>(OpCode::OP_Get_Local), op_get_local, "GET_LOCAL", 1);
        register_operation(static_cast<uint8_t>(OpCode::OP_Set_Local), op_set_local, "SET_LOCAL", 1);
        register_operation(static_cast<uint8_t>(OpCode::OP_Get_Global), op_get_global, "GET_GLOBAL");
        register_operation(static_cast<uint8_t>(OpCode::OP_Define_Global), op_define_global, "DEFINE_GLOBAL");
        register_operation(static_cast<uint8_t>(OpCode::OP_Set_Global), op_set_global, "SET_GLOBAL");
        register_operation(static_cast<uint8_t>(OpCode::OP_Jump), op_jump, "JUMP", 2);
        register_operation(static_cast<uint8_t>(OpCode::OP_Jump_If_False), op_jump_if_false, "JUMP_IF_FALSE", 2);
        register_operation(static_cast<uint8_t>(OpCode::OP_Long_Jump), op_long_jump, "LONG_JUMP", 4);
        register_operation(static_cast<uint8_t>(OpCode::OP_Long_Jump_If_False), op_long_jump_if_false, "LONG_JUMP_IF_FALSE", 4);
        register_operation(static_cast<uint8_t>(OpCode::OP_Long_Loop), op_long_loop, "LONG_LOOP", 4);
        register_operation(static_cast<uint8_t>(OpCode::OP_Call), op_call, "CALL", 1);
        register_operation(static_cast<uint8_t>(OpCode::OP_Closure), op_closure, "CLOSURE", 1);
        register_operation(static_cast<uint8_t>(OpCode::OP_Get_Upvalue), op_get_upvalue, "GET_UPVALUE", 1);
        register_operation(static_cast<uint8_t>(OpCode::OP_Set_Upvalue), op_set_upvalue, "SET_UPVALUE", 1);
        register_operation(static_cast<uint8_t>(OpCode::OP_Close_Upvalue), op_close_upvalue, "CLOSE_UPVALUE");
        register_operation(static_cast<uint8_t>(OpCode::OP_Debug_Print), op_debug_print, "DEBUG_PRINT");
    }

    // Operation handler implementations
    uint8_t* op_true(VM* vm, uint8_t* ip) {
        vm->push(BOOL_VAL(true));
        return ip;
    }

    uint8_t* op_false(VM* vm, uint8_t* ip) {
        vm->push(BOOL_VAL(false));
        return ip;
    }

    uint8_t* op_pop(VM* vm, uint8_t* ip) {
        auto value = vm->pop();
        vm->releaseAndDelete(value);
        return ip;
    }

    uint8_t* op_constant(VM* vm, uint8_t* ip)
    {
        uint8_t constantIndex = *ip++;

#ifdef DEBUG_CHECK_STACK
        if (constantIndex >= vm->currentFrame->closure->function->chunk.constants.size())
        {
            vm->runtimeError("Constant index out of bounds");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return nullptr;
        }
#endif
        Value constant = vm->currentFrame->closure->function->chunk.constants[constantIndex];
        vm->push(constant);
        return ip;
    }

    uint8_t* op_add(VM* vm, uint8_t* ip) {
        auto b = vm->pop();
        auto a = vm->pop();
        vm->push(vm->addValues(a, b));
        vm->releaseAndDelete(a);
        vm->releaseAndDelete(b);
        return ip;
    }

    uint8_t* op_subtract(VM* vm, uint8_t* ip) {
        auto b = vm->pop();
        auto a = vm->pop();
        vm->push(vm->subtractValues(a, b));
        vm->releaseAndDelete(a);
        vm->releaseAndDelete(b);
        return ip;
    }

    uint8_t* op_multiply(VM* vm, uint8_t* ip) {
        auto b = vm->pop();
        auto a = vm->pop();
        vm->push(vm->multiplyValues(a, b));
        vm->releaseAndDelete(a);
        vm->releaseAndDelete(b);
        return ip;
    }

    uint8_t* op_divide(VM* vm, uint8_t* ip) {
        auto b = vm->pop();
        auto a = vm->pop();
        vm->push(vm->divideValues(a, b));
        vm->releaseAndDelete(a);
        vm->releaseAndDelete(b);
        return ip;
    }

    uint8_t* op_equal(VM* vm, uint8_t* ip) {
        auto b = vm->pop();
        auto a = vm->pop();
        vm->push(vm->equalsValues(a, b));
        vm->releaseAndDelete(a);
        vm->releaseAndDelete(b);
        return ip;
    }

    uint8_t* op_less(VM* vm, uint8_t* ip) {
        auto b = vm->pop();
        auto a = vm->pop();
        vm->push(vm->lessValues(a, b));
        vm->releaseAndDelete(a);
        vm->releaseAndDelete(b);
        return ip;
    }

    uint8_t* op_get_local(VM* vm, uint8_t* ip) {
        uint8_t slot = *ip++;
        if (slot >= 255) {
            vm->runtimeError("Local variable slot out of range");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return nullptr;
        }
        vm->push(vm->currentFrame->slots[slot]);
        return ip;
    }

    uint8_t* op_set_local(VM* vm, uint8_t* ip) {
        uint8_t slot = *ip++;
        auto value = vm->pop();
        if (slot >= 255) {
            vm->runtimeError("Local variable slot out of range");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return nullptr;
        }
        vm->currentFrame->slots[slot] = value;
        vm->push(value);
        return ip;
    }

    uint8_t* op_long_jump_if_false(VM* vm, uint8_t* ip) {
        uint32_t offset = (static_cast<uint32_t>(*ip++) << 24);
        offset |= (static_cast<uint32_t>(*ip++) << 16);
        offset |= (static_cast<uint32_t>(*ip++) << 8);
        offset |= static_cast<uint32_t>(*ip++);

        Value condition = vm->pop();
        if (!isValueTrue(condition)) {
            ip += offset;
        }
        vm->releaseAndDelete(condition);
        return ip;
    }

    uint8_t* op_long_loop(VM* vm, uint8_t* ip) {
        uint32_t offset = (static_cast<uint32_t>(*ip++) << 24);
        offset |= (static_cast<uint32_t>(*ip++) << 16);
        offset |= (static_cast<uint32_t>(*ip++) << 8);
        offset |= static_cast<uint32_t>(*ip++);

        return ip - offset;
    }

    uint8_t* op_return(VM* vm, uint8_t* ip) {
#ifdef DEBUG_CHECK_STACK
        if (vm->stack.empty()) {
            vm->runtimeError("Nothing in the stack for return.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return nullptr;
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

            return nullptr;
        }

        // Restore previous frame
        CallFrame* returningFrame = vm->currentFrame;

        vm->currentFrame = &vm->frames[vm->frameCount - 1];
        vm->updateChunkCache();
        // vm->push(value);

        while (vm->stack.data() + vm->stack.size() > returningFrame->slots)
        {
            auto v = vm->stack.pop();
            vm->releaseAndDelete(v);
        }

        vm->push(vm->retainValue(value));
        vm->releaseAndDelete(value);

        return vm->currentFrame->ip;
    }

    // Placeholder implementations for missing operations (will implement as needed)
    uint8_t* op_long_constant(VM* vm, uint8_t* ip) {
        uint32_t constantIndex = (static_cast<uint32_t>(*ip++) << 16);
        constantIndex |= (static_cast<uint32_t>(*ip++) << 8);
        constantIndex |= static_cast<uint32_t>(*ip++);

#ifdef DEBUG_CHECK_STACK
        if (constantIndex >= vm->currentFrame->closure->function->chunk.constants.size()) {
            vm->runtimeError("Long constant index out of bounds.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return nullptr;
        }
#endif

        Value constant = vm->currentFrame->closure->function->chunk.constants[constantIndex];
        vm->push(constant);
        return ip;
    }

    uint8_t* op_negate(VM* vm, uint8_t* ip) {
        auto val = vm->pop();
        vm->push(vm->negateValue(val));
        vm->releaseAndDelete(val);
        return ip;
    }

    uint8_t* op_greater(VM* vm, uint8_t* ip) {
        auto b = vm->pop();
        auto a = vm->pop();
        vm->push(vm->greaterValues(a, b));
        vm->releaseAndDelete(a);
        vm->releaseAndDelete(b);
        return ip;
    }

    uint8_t* op_get_global(VM* vm, uint8_t* ip) {
#ifdef DEBUG_CHECK_STACK
        if (vm->stack.empty()) {
            vm->runtimeError("Not enough values on stack for variable retrieval.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return nullptr;
        }
#endif
        auto nameValue = vm->pop();  // variable name
        auto name = valueToElement(nameValue);

        if (not name.isLitteral()) {
            vm->releaseAndDelete(nameValue);
            vm->runtimeError("Global variable name must be a litteral.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return nullptr;
        }

        auto it = vm->globals.find(name.toString());
        if (it == vm->globals.end()) {
            vm->releaseAndDelete(nameValue);
            vm->runtimeError("Undefined global variable '" + name.toString() + "'.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return nullptr;
        }

        vm->push(it->second); // Don't retain - global already holds reference
        vm->releaseAndDelete(nameValue);
        return ip;
    }

    uint8_t* op_define_global(VM* vm, uint8_t* ip) {
#ifdef DEBUG_CHECK_STACK
        if (vm->stack.size() < 2) {
            vm->runtimeError("Not enough values on stack for variable definition.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return nullptr;
        }
#endif
        auto nameValue = vm->pop();  // variable name
        auto name = valueToElement(nameValue);
        auto value = vm->pop(); // variable value

        if (not name.isLitteral()) {
            vm->releaseAndDelete(nameValue);
            vm->releaseAndDelete(value);
            vm->runtimeError("Global variable name must be a litteral.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return nullptr;
        }

        vm->globals[name.toString()] = vm->retainValue(value);
        vm->releaseAndDelete(nameValue);
        vm->releaseAndDelete(value);
        return ip;
    }

    uint8_t* op_set_global(VM* vm, uint8_t* ip) {
        vm->runtimeError("OP_Set_Global not implemented yet");
        vm->vm_return(InterpretResult::RUNTIME_ERROR);
        return nullptr;
    }

    uint8_t* op_jump(VM* vm, uint8_t* ip) {
        vm->runtimeError("OP_Jump not implemented yet");
        vm->vm_return(InterpretResult::RUNTIME_ERROR);
        return nullptr;
    }

    uint8_t* op_jump_if_false(VM* vm, uint8_t* ip) {
        vm->runtimeError("OP_Jump_If_False not implemented yet");
        vm->vm_return(InterpretResult::RUNTIME_ERROR);
        return nullptr;
    }

    uint8_t* op_long_jump(VM* vm, uint8_t* ip) {
        uint32_t offset = (static_cast<uint32_t>(*ip++) << 24);
        offset |= (static_cast<uint32_t>(*ip++) << 16);
        offset |= (static_cast<uint32_t>(*ip++) << 8);
        offset |= static_cast<uint32_t>(*ip++);

        return ip + offset;
    }

    uint8_t* op_call(VM* vm, uint8_t* ip) {
        int argCount = *ip++;

        // Get the function object (at position argCount from top)
        Value function = vm->peek(argCount);

        if (not vm->callValue(function, argCount)) {
            vm->runtimeError("Cannot call function");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return nullptr;
        }

        // Remove the function object from the stack by shifting arguments up
        for (int i = argCount - 1; i >= 0; i--) {
            vm->stack[vm->stack.size() - argCount - 1 + i] = vm->stack[vm->stack.size() - argCount + i];
        }
        vm->stack.pop(); // Remove the duplicate top element

        // Update the frame slots to point to the shifted arguments
        vm->frames[vm->frameCount - 1].slots = vm->stack.data() + vm->stack.size() - argCount;

        vm->currentFrame = &vm->frames[vm->frameCount - 1];
        vm->updateChunkCache(); // Update cached chunk data for new frame
        return vm->currentFrame->ip;
    }

    uint8_t* op_closure(VM* vm, uint8_t* ip) {
        uint8_t constantIndex = *ip++;
        auto functionValue = vm->currentFrame->closure->function->chunk.constants[constantIndex];
        if (not IS_FUNC(functionValue)) {
            vm->runtimeError("Closure operand must be a function.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return nullptr;
        }

        ObjFunction* function = AS_FUNC(functionValue);
        auto closure = new Closure(function);
        vm->push(vm->trackNewValue(CLOSURE_VAL(closure)));

        for (int i = 0; i < function->upvalueCount; i++) {
            uint8_t isLocal = *ip++;
            uint8_t index = *ip++;
            if (isLocal) {
                closure->upvalues[i] = vm->captureUpvalue(vm->currentFrame->slots + index);
            } else {
                closure->upvalues[i] = vm->currentFrame->closure->upvalues[index];
            }
        }
        return ip;
    }

    uint8_t* op_get_upvalue(VM* vm, uint8_t* ip) {
        vm->runtimeError("OP_Get_Upvalue not implemented yet");
        vm->vm_return(InterpretResult::RUNTIME_ERROR);
        return nullptr;
    }

    uint8_t* op_set_upvalue(VM* vm, uint8_t* ip) {
        vm->runtimeError("OP_Set_Upvalue not implemented yet");
        vm->vm_return(InterpretResult::RUNTIME_ERROR);
        return nullptr;
    }

    uint8_t* op_close_upvalue(VM* vm, uint8_t* ip) {
        vm->runtimeError("OP_Close_Upvalue not implemented yet");
        vm->vm_return(InterpretResult::RUNTIME_ERROR);
        return nullptr;
    }

    uint8_t* op_debug_print(VM* vm, uint8_t* ip) {
#ifdef DEBUG_CHECK_STACK
        if (vm->stack.empty()) {
            vm->runtimeError("Nothing to print from the stack.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return nullptr;
        }
#endif
        auto value = vm->pop();

        if (IS_FUNC(value)) {
            ObjFunction* func = AS_FUNC(value);
            if (func != nullptr) {
                vm->testOutput += "<" + func->name + "> \n";
            } else {
                vm->testOutput += "<script> \n";
            }
            vm->releaseAndDelete(value);
            return ip;
        }

        // For testing: append to testOutput buffer instead of stdout
        ElementType elem = valueToElement(value);
        vm->testOutput += elem.toString() + "\n";
        vm->releaseAndDelete(value);
        return ip;
    }

}