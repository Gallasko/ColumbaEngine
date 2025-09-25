#include "vm.h"

#include <iostream>

#include "compiler_debug.h"

#include <chrono>

namespace pg
{
    InterpretResult VM::interpret(const std::queue<Token>& tokens)
    {
        Chunk chunk;

        // Todo change this
        // Reset the compiler state before compiling a new chunk
        compiler.reset();

        std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();

        if (not compiler.compile(tokens, chunk))
            return InterpretResult::COMPILE_ERROR;

        std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();

#ifdef DEBUG_PROFILE_COMPILE
        std::cout << "Compilation took: "
                  << std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count()
                  << " ns"
                  << std::endl;
#endif
        // LOG_INFO("VM", "Compilation took " << elapsed_seconds.count() << "s");

        // Apply bytecode optimizations
        if (enableOptimizations && !chunk.code.empty())
        {
            begin = std::chrono::steady_clock::now();

            LOG_INFO("VM", "Applying bytecode optimizations");
            size_t originalSize = chunk.code.size();

            passManager.runAllPasses(chunk);

            size_t optimizedSize = chunk.code.size();
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

        this->chunk = chunk;
        ip = 0;

        try
        {
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
        if (chunk.code.empty())
            return InterpretResult::OK;

        for (;;)
        {
            if (ip >= chunk.code.size())
                return InterpretResult::OK;

#ifdef DEBUG_TRACE_EXECUTION
            std::cout << "          ";
            for (size_t i = 0; i < stack.size(); ++i)
            {
                std::cout << "[" << stack[i].toString() << "] ";
            }
            std::cout << std::endl;

            disassembleInstruction(chunk, ip);
#endif
            auto instruction = static_cast<OpCode>(chunk.code[ip++]);

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
                    ElementType elem = valueToElement(value);
                    std::cout << elem.toString() << std::endl;
                    freeValue(value);

                    return InterpretResult::OK;
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
                    freeValue(value);
                    break;
                }

                case OpCode::OP_Add:
                {
                    fastBinaryOp(addValues);
                    break;
                }

                case OpCode::OP_Subtract:
                {
                    fastBinaryOp(subtractValues);
                    break;
                }

                case OpCode::OP_Multiply:
                {
                    fastBinaryOp(multiplyValues);
                    break;
                }

                case OpCode::OP_Divide:
                {
                    fastBinaryOp(divideValues);
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
                    freeValue(value);
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
                    freeValue(a);
                    freeValue(b);
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
                    freeValue(a);
                    freeValue(b);
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
                        freeValue(a); \
                        freeValue(b); \
                        EMIT_RUNTIME_ERROR("Comparison operation failed: " << e.what()); \
                    } \
                    freeValue(a); \
                    freeValue(b); \
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
                    freeValue(value);

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
                        freeValue(nameValue);
                        freeValue(value);
                        EMIT_RUNTIME_ERROR("Global variable name must be a litteral.");
                    }

                    globals[name.toString()] = value;
                    freeValue(nameValue);
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
                        freeValue(nameValue);
                        EMIT_RUNTIME_ERROR("Global variable name must be a litteral.");
                    }

                    auto it = globals.find(name.toString());
                    if (it == globals.end())
                    {
                        freeValue(nameValue);
                        EMIT_RUNTIME_ERROR("Undefined global variable '" << name.toString() << "'.");
                    }

                    push(copyValue(it->second));
                    freeValue(nameValue);
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
                        freeValue(nameValue);
                        freeValue(value);
                        EMIT_RUNTIME_ERROR("Global variable name must be a litteral.");
                    }

                    auto it = globals.find(name.toString());
                    if (it == globals.end())
                    {
                        freeValue(nameValue);
                        freeValue(value);
                        EMIT_RUNTIME_ERROR("Undefined global variable '" << name.toString() << "'.");
                    }

                    // Free the old value that was stored
                    freeValue(it->second);
                    it->second = copyValue(value);  // Store a copy in globals
                    push(value);  // Push the original to stack
                    freeValue(nameValue);
                    break;
                }

                case OpCode::OP_Get_Local:
                {
#ifdef DEBUG_CHECK_STACK
                    if (stack.empty())
                    {
                        EMIT_RUNTIME_ERROR("Stack underflow for local variable access.");
                    }
#endif
                    auto slot = pop(); // Get the slot index from stack
                    if (not isValueNumber(slot))
                    {
                        freeValue(slot);
                        EMIT_RUNTIME_ERROR("Local variable slot must be a number.");
                    }

                    int index = getValueAsInt(slot);
                    if (index < 0 || index >= static_cast<int>(stack.size()))
                    {
                        freeValue(slot);
                        EMIT_RUNTIME_ERROR("Local variable index out of bounds.");
                    }

                    push(stack[index]);
                    freeValue(slot);
                    break;
                }

                case OpCode::OP_Set_Local:
                {
#ifdef DEBUG_CHECK_STACK
                    if (stack.size() < 2)
                    {
                        EMIT_RUNTIME_ERROR("Not enough values on stack for local assignment.");
                    }
#endif
                    auto value = pop(); // New value
                    auto slot = pop();  // Slot index

                    if (not isValueNumber(slot))
                    {
                        freeValue(slot);
                        freeValue(value);
                        EMIT_RUNTIME_ERROR("Local variable slot must be a number.");
                    }

                    int index = getValueAsInt(slot);
                    if (index < 0 || index >= static_cast<int>(stack.size()))
                    {
                        freeValue(slot);
                        freeValue(value);
                        EMIT_RUNTIME_ERROR("Local variable index out of bounds.");
                    }

                    // Free the old value that was in the stack slot
                    freeValue(stack[index]);
                    stack[index] = value;
                    push(value); // Assignment expression returns the value
                    freeValue(slot);
                    break;
                }

                case OpCode::OP_Jump_If_False:
                {
#ifdef DEBUG_CHECK_STACK
                    if (ip + 2 >= chunk.code.size())
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
                        ip += jumpOffset;
                        if (ip > chunk.code.size())
                        {
                            EMIT_RUNTIME_ERROR("Jump offset out of bounds.");
                        }
                    }

                    break;
                }

                case OpCode::OP_Long_Jump_If_False:
                {
#ifdef DEBUG_CHECK_STACK
                    if (ip + 4 >= chunk.code.size())
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
                        ip += jumpOffset;
                        if (ip > chunk.code.size())
                        {
                            EMIT_RUNTIME_ERROR("Jump offset out of bounds.");
                        }
                    }

                    break;
                }

                case OpCode::OP_Jump:
                {
#ifdef DEBUG_CHECK_STACK
                    if (ip + 2 >= chunk.code.size())
                    {
                        EMIT_RUNTIME_ERROR("Not enough bytes to read jump offset.");
                    }
#endif
                    uint16_t jumpOffset = readUint16();

                    ip += jumpOffset;
                    if (ip > chunk.code.size())
                    {
                        EMIT_RUNTIME_ERROR("Jump offset out of bounds.");
                    }

                    break;
                }

                case OpCode::OP_Long_Jump:
                {
#ifdef DEBUG_CHECK_STACK
                    if (ip + 4 >= chunk.code.size())
                    {
                        EMIT_RUNTIME_ERROR("Not enough bytes to read long jump offset.");
                    }
#endif
                    uint32_t jumpOffset = readUint32();

                    ip += jumpOffset;
                    if (ip > chunk.code.size())
                    {
                        EMIT_RUNTIME_ERROR("Jump offset out of bounds.");
                    }

                    break;
                }

                case OpCode::OP_Loop:
                {
#ifdef DEBUG_CHECK_STACK
                    if (ip + 2 >= chunk.code.size())
                    {
                        EMIT_RUNTIME_ERROR("Not enough bytes to read loop offset.");
                    }
#endif
                    uint16_t loopOffset = readUint16();
#ifdef DEBUG_CHECK_STACK
                    if (loopOffset > ip)
                    {
                        EMIT_RUNTIME_ERROR("Loop offset out of bounds.");
                    }
#endif
                    ip -= loopOffset;
                    break;
                }

                case OpCode::OP_Long_Loop:
                {
#ifdef DEBUG_CHECK_STACK
                    if (ip + 4 >= chunk.code.size())
                    {
                        EMIT_RUNTIME_ERROR("Not enough bytes to read long loop offset.");
                    }
#endif
                    uint32_t loopOffset = readUint32();
#ifdef DEBUG_CHECK_STACK
                    if (loopOffset > ip)
                    {
                        EMIT_RUNTIME_ERROR("Loop offset out of bounds.");
                    }
#endif
                    ip -= loopOffset;
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

                    // For testing: append to testOutput buffer instead of stdout
                    ElementType elem = valueToElement(value);
                    testOutput += elem.toString() + "\n";
                    freeValue(value);

                    break;
                }

                case OpCode::OP_Post_Incr_Global:
                {
#ifdef DEBUG_CHECK_STACK
                    if (stack.size() < 2)
                    {
                        EMIT_RUNTIME_ERROR("Stack underflow on post-increment.");
                    }
#endif
                    auto nameValue = pop();  // variable name
                    auto oldValueFromStack = pop(); // old value (we'll discard this)
                    auto name = valueToElement(nameValue);

                    if (not name.isLitteral())
                    {
                        freeValue(nameValue);
                        freeValue(oldValueFromStack);
                        EMIT_RUNTIME_ERROR("Global variable name must be a litteral.");
                    }

                    auto it = globals.find(name.toString());
                    if (it == globals.end())
                    {
                        freeValue(nameValue);
                        freeValue(oldValueFromStack);
                        EMIT_RUNTIME_ERROR("Undefined global variable '" << name.toString() << "'.");
                    }

                    if (not isValueNumber(it->second))
                    {
                        freeValue(nameValue);
                        freeValue(oldValueFromStack);
                        EMIT_RUNTIME_ERROR("Operand after an unary (++) must be a number.");
                    }

                    auto oldValue = copyValue(it->second);
                    auto newValue = addValues(it->second, INT_VAL(1));
                    freeValue(it->second);
                    it->second = newValue;

                    push(oldValue); // Post-increment returns the old value
                    freeValue(nameValue);
                    freeValue(oldValueFromStack);
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
                        freeValue(nameValue);
                        EMIT_RUNTIME_ERROR("Global variable name must be a litteral.");
                    }

                    auto it = globals.find(name.toString());
                    if (it == globals.end())
                    {
                        freeValue(nameValue);
                        EMIT_RUNTIME_ERROR("Undefined global variable '" << name.toString() << "'.");
                    }

                    if (not isValueNumber(it->second))
                    {
                        freeValue(nameValue);
                        EMIT_RUNTIME_ERROR("Operand after an unary (++) must be a number.");
                    }

                    auto newValue = addValues(it->second, INT_VAL(1));
                    freeValue(it->second);
                    it->second = newValue;

                    push(copyValue(newValue)); // Pre-increment returns the new value
                    freeValue(nameValue);
                    break;
                }

                case OpCode::OP_Post_Decr_Global:
                {
#ifdef DEBUG_CHECK_STACK
                    if (stack.size() < 2)
                    {
                        EMIT_RUNTIME_ERROR("Stack underflow on post-decrement.");
                    }
#endif
                    auto nameValue = pop();  // variable name
                    auto oldValueFromStack = pop(); // old value (we'll discard this)
                    auto name = valueToElement(nameValue);

                    if (not name.isLitteral())
                    {
                        freeValue(nameValue);
                        freeValue(oldValueFromStack);
                        EMIT_RUNTIME_ERROR("Global variable name must be a litteral.");
                    }

                    auto it = globals.find(name.toString());
                    if (it == globals.end())
                    {
                        freeValue(nameValue);
                        freeValue(oldValueFromStack);
                        EMIT_RUNTIME_ERROR("Undefined global variable '" << name.toString() << "'.");
                    }

                    if (not isValueNumber(it->second))
                    {
                        freeValue(nameValue);
                        freeValue(oldValueFromStack);
                        EMIT_RUNTIME_ERROR("Operand after an unary (--) must be a number.");
                    }

                    auto oldValue = copyValue(it->second);
                    auto newValue = subtractValues(it->second, INT_VAL(1));
                    freeValue(it->second);
                    it->second = newValue;

                    push(oldValue); // Post-decrement returns the old value
                    freeValue(nameValue);
                    freeValue(oldValueFromStack);
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
                        freeValue(nameValue);
                        EMIT_RUNTIME_ERROR("Global variable name must be a litteral.");
                    }

                    auto it = globals.find(name.toString());
                    if (it == globals.end())
                    {
                        freeValue(nameValue);
                        EMIT_RUNTIME_ERROR("Undefined global variable '" << name.toString() << "'.");
                    }

                    if (not isValueNumber(it->second))
                    {
                        freeValue(nameValue);
                        EMIT_RUNTIME_ERROR("Operand after an unary (--) must be a number.");
                    }

                    auto newValue = subtractValues(it->second, INT_VAL(1));
                    freeValue(it->second);
                    it->second = newValue;

                    push(copyValue(newValue)); // Pre-decrement returns the new value
                    freeValue(nameValue);
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
                        freeValue(slot);
                        EMIT_RUNTIME_ERROR("Local variable slot must be a number.");
                    }

                    int index = getValueAsInt(slot);
                    if (index < 0 || index >= static_cast<int>(stack.size()))
                    {
                        freeValue(slot);
                        EMIT_RUNTIME_ERROR("Local variable index out of bounds.");
                    }

                    if (not isValueNumber(stack[index]))
                    {
                        freeValue(slot);
                        EMIT_RUNTIME_ERROR("Operand after an unary (++) must be a number.");
                    }

                    auto oldValue = copyValue(stack[index]);
                    auto newValue = addValues(stack[index], INT_VAL(1));
                    freeValue(stack[index]);
                    stack[index] = newValue;

                    push(oldValue); // Post-increment returns the old value
                    freeValue(slot);
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
                        freeValue(slot);
                        EMIT_RUNTIME_ERROR("Local variable slot must be a number.");
                    }

                    int index = getValueAsInt(slot);
                    if (index < 0 || index >= static_cast<int>(stack.size()))
                    {
                        freeValue(slot);
                        EMIT_RUNTIME_ERROR("Local variable index out of bounds.");
                    }

                    if (not isValueNumber(stack[index]))
                    {
                        freeValue(slot);
                        EMIT_RUNTIME_ERROR("Operand after an unary (++) must be a number.");
                    }

                    auto newValue = addValues(stack[index], INT_VAL(1));
                    freeValue(stack[index]);
                    stack[index] = newValue;

                    push(copyValue(newValue)); // Pre-increment returns the new value
                    freeValue(slot);
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
                        freeValue(slot);
                        EMIT_RUNTIME_ERROR("Local variable slot must be a number.");
                    }

                    int index = getValueAsInt(slot);
                    if (index < 0 || index >= static_cast<int>(stack.size()))
                    {
                        freeValue(slot);
                        EMIT_RUNTIME_ERROR("Local variable index out of bounds.");
                    }

                    if (not isValueNumber(stack[index]))
                    {
                        freeValue(slot);
                        EMIT_RUNTIME_ERROR("Operand after an unary (--) must be a number.");
                    }

                    auto oldValue = copyValue(stack[index]);
                    auto newValue = subtractValues(stack[index], INT_VAL(1));
                    freeValue(stack[index]);
                    stack[index] = newValue;

                    push(oldValue); // Post-decrement returns the old value
                    freeValue(slot);
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
                        freeValue(slot);
                        EMIT_RUNTIME_ERROR("Local variable slot must be a number.");
                    }

                    int index = getValueAsInt(slot);
                    if (index < 0 || index >= static_cast<int>(stack.size()))
                    {
                        freeValue(slot);
                        EMIT_RUNTIME_ERROR("Local variable index out of bounds.");
                    }

                    if (not isValueNumber(stack[index]))
                    {
                        freeValue(slot);
                        EMIT_RUNTIME_ERROR("Operand after an unary (--) must be a number.");
                    }

                    auto newValue = subtractValues(stack[index], INT_VAL(1));
                    freeValue(stack[index]);
                    stack[index] = newValue;

                    push(copyValue(newValue)); // Pre-decrement returns the new value
                    freeValue(slot);
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
        uint8_t constantIndex = chunk.code[ip++];

#ifdef DEBUG_CHECK_STACK
        if (constantIndex >= chunk.constants.size())
        {
            throw std::runtime_error("Constant index out of bounds.");
        }
#endif

        return elementToValue(chunk.constants[constantIndex]);
    }

    Value VM::readLongConstant()
    {
#ifdef DEBUG_CHECK_STACK
        if (ip + 2 >= chunk.code.size())
        {
            throw std::runtime_error("Not enough bytes to read long constant index.");
        }
#endif

        uint32_t constantIndex = (static_cast<uint32_t>(chunk.code[ip]) << 16);
        ip++;

        constantIndex |= (static_cast<uint32_t>(chunk.code[ip]) << 8);
        ip++;

        constantIndex |= static_cast<uint32_t>(chunk.code[ip]);
        ip++;

#ifdef DEBUG_CHECK_STACK
        if (constantIndex >= chunk.constants.size())
        {
            throw std::runtime_error("Long constant index out of bounds.");
        }
#endif

        return elementToValue(chunk.constants[constantIndex]);
    }

    void VM::binaryOp(std::function<Value(Value, Value)> op)
    {
#ifdef DEBUG_CHECK_STACK
        if (stack.size() < 2)
        {
            runtimeError((Strfy() << "Stack underflow on binary operation.").getData());
            return;
        }
#endif

        auto val1 = peek(0);
        auto val2 = peek(1);

        // Type check using Value operations
        bool val1IsNumber = isValueNumber(val1);
        bool val2IsNumber = isValueNumber(val2);
        bool val1IsLiteral = IS_OBJ(val1) && AS_OBJ(val1)->isLitteral();
        bool val2IsLiteral = IS_OBJ(val2) && AS_OBJ(val2)->isLitteral();

        if (not (val1IsNumber and val2IsNumber) and not (val1IsLiteral and val2IsLiteral))
        {
            ElementType e1 = valueToElement(val1);
            ElementType e2 = valueToElement(val2);
            runtimeError((Strfy() << "Operands after a binary operator should be the same type: " << e1.getTypeString() << " and " << e2.getTypeString()).getData());
            return;
        }

        auto b = pop();
        auto a = pop();

        push(op(a, b));
        freeValue(a);
        freeValue(b);
    }

    void VM::fastBinaryOp(Value (*op)(const Value&, const Value&))
    {
#ifdef DEBUG_CHECK_STACK
        if (stack.size() < 2)
        {
            runtimeError((Strfy() << "Stack underflow on binary operation.").getData());
            return;
        }
#endif

        auto val1 = peek(0);
        auto val2 = peek(1);

        // Fast path for integers
        if (IS_INT(val1) && IS_INT(val2)) {
            auto b = pop();
            auto a = pop();
            push(op(a, b));
            freeValue(a);
            freeValue(b);
            return;
        }

        // Type check for complex cases
        bool val1IsNumber = isValueNumber(val1);
        bool val2IsNumber = isValueNumber(val2);
        bool val1IsLiteral = IS_OBJ(val1) && AS_OBJ(val1)->isLitteral();
        bool val2IsLiteral = IS_OBJ(val2) && AS_OBJ(val2)->isLitteral();

        if (not (val1IsNumber and val2IsNumber) and not (val1IsLiteral and val2IsLiteral))
        {
            ElementType e1 = valueToElement(val1);
            ElementType e2 = valueToElement(val2);
            runtimeError((Strfy() << "Operands after a binary operator should be the same type: " << e1.getTypeString() << " and " << e2.getTypeString()).getData());
            return;
        }

        auto b = pop();
        auto a = pop();
        push(op(a, b));
        freeValue(a);
        freeValue(b);
    }
}