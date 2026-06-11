#include "stdafx.h"

#include "vm.h"
#include "decoded_chunk.h"

namespace pg
{

    Value VM::addValues(const Value a, const Value b)
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
        if (IS_STRING(a) and IS_STRING(b))
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

    void op_true(VM* vm)
    {
        vm->push(BOOL_VAL(true));
    }

    void op_false(VM* vm)
    {
        vm->push(BOOL_VAL(false));
    }

    #define BINARY_OP_TEMPLATE(op_name, operation) \
    void op_name(VM* vm) \
    { \
        auto b = vm->pop(); \
        auto a = vm->peek(); \
        vm->changeTop(vm->operation(a, b)); \
        /* Escape analysis: Only release heap objects, not primitives */ \
        if (requiresRefCount(b)) vm->releaseAndDelete(b); \
    } \
    void op_name##_decoded(VM* vm, const DecodedInstruction&) \
    { \
        /* Same body — the win is the dispatcher skipping the */ \
        /* `currentFrame->ip = bytecodeOffset + 1` store per opcode. */ \
        auto b = vm->pop(); \
        auto a = vm->peek(); \
        vm->changeTop(vm->operation(a, b)); \
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

    void op_add_ll(VM* vm)
    {
        uint8_t local1 = *vm->currentFrame->ip++;

        auto value1 = vm->currentFrame->slots[local1];

        uint8_t local2 = *vm->currentFrame->ip++;

        auto value2 = vm->currentFrame->slots[local2];

        auto result = vm->addValues(value1, value2);

        vm->push(result);
    }

    void op_add_ll_decoded(VM* vm, const DecodedInstruction& instr)
    {
        // Both local slot indices were pre-extracted at decode time. The
        // non-decoded variant does two `*ip++` reads plus a dispatcher
        // `currentFrame->ip` write per opcode — that all goes away here.
        auto value1 = vm->currentFrame->slots[instr.operands.indexed.byte1];
        auto value2 = vm->currentFrame->slots[instr.operands.indexed.byte2];
        vm->push(vm->addValues(value1, value2));
    }

    // OP_LessEqualLL: peephole fusion of OP_Get_Local A + OP_Get_Local B + OP_LessEqual.
    // Reads two locals directly and pushes the comparison result. No popping the
    // pushed local copies, no addValues-style boxing — straight slot read + compare.
    void op_less_equal_ll(VM* vm)
    {
        uint8_t local1 = *vm->currentFrame->ip++;
        uint8_t local2 = *vm->currentFrame->ip++;
        auto v1 = vm->currentFrame->slots[local1];
        auto v2 = vm->currentFrame->slots[local2];
        vm->push(vm->lessEqualValues(v1, v2));
    }

    void op_less_equal_ll_decoded(VM* vm, const DecodedInstruction& instr)
    {
        auto v1 = vm->currentFrame->slots[instr.operands.indexed.byte1];
        auto v2 = vm->currentFrame->slots[instr.operands.indexed.byte2];
        vm->push(vm->lessEqualValues(v1, v2));
    }

    // OP_LessLL: same shape as OP_LessEqualLL but using lessValues. Used by
    // loop conditions of the form `while (x < y)` where both are locals.
    void op_less_ll(VM* vm)
    {
        uint8_t local1 = *vm->currentFrame->ip++;
        uint8_t local2 = *vm->currentFrame->ip++;
        auto v1 = vm->currentFrame->slots[local1];
        auto v2 = vm->currentFrame->slots[local2];
        vm->push(vm->lessValues(v1, v2));
    }

    void op_less_ll_decoded(VM* vm, const DecodedInstruction& instr)
    {
        auto v1 = vm->currentFrame->slots[instr.operands.indexed.byte1];
        auto v2 = vm->currentFrame->slots[instr.operands.indexed.byte2];
        vm->push(vm->lessValues(v1, v2));
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

    // ===================================================================
    // Register-based operations (no push/pop, direct slot manipulation)
    // ===================================================================

    void op_load_constant_r(VM* vm)
    {
        uint8_t destSlot = *vm->currentFrame->ip++;
        uint8_t constIndex = *vm->currentFrame->ip++;

        Value constant = vm->currentFrame->closure->function->chunk.constants[constIndex];

        // Direct write to stack slot (register) - no push!
        vm->currentFrame->slots[destSlot] = constant;
    }

    void op_move_r(VM* vm)
    {
        uint8_t destSlot = *vm->currentFrame->ip++;
        uint8_t srcSlot = *vm->currentFrame->ip++;

        // Direct register-to-register move - no push/pop!
        vm->currentFrame->slots[destSlot] = vm->currentFrame->slots[srcSlot];
    }

    void op_add_rrr(VM* vm)
    {
        uint8_t destSlot = *vm->currentFrame->ip++;
        uint8_t src1Slot = *vm->currentFrame->ip++;
        uint8_t src2Slot = *vm->currentFrame->ip++;

        Value a = vm->currentFrame->slots[src1Slot];
        Value b = vm->currentFrame->slots[src2Slot];

        // Direct computation and storage - no push/pop!
        vm->currentFrame->slots[destSlot] = vm->addValues(a, b);
    }

    void op_less_rr(VM* vm)
    {
        uint8_t src1Slot = *vm->currentFrame->ip++;
        uint8_t src2Slot = *vm->currentFrame->ip++;

        Value a = vm->currentFrame->slots[src1Slot];
        Value b = vm->currentFrame->slots[src2Slot];

        // Comparison result pushed to stack for use by jump instructions
        vm->push(vm->lessValues(a, b));
    }

    void op_incr_r(VM* vm)
    {
        uint8_t slot = *vm->currentFrame->ip++;

        Value val = vm->currentFrame->slots[slot];

        // Fast path for integers
        if (IS_INT(val))
        {
            vm->currentFrame->slots[slot] = INT_VAL(AS_INT(val) + 1);
        }
        else if (IS_FLOAT(val))
        {
            vm->currentFrame->slots[slot] = FLOAT_VAL(AS_FLOAT(val) + 1.0);
        }
        else
        {
            // Fall back to add operation
            vm->currentFrame->slots[slot] = vm->addValues(val, INT_VAL(1));
        }
    }

    void op_less_rrr(VM* vm)
    {
        uint8_t destSlot = *vm->currentFrame->ip++;
        uint8_t src1Slot = *vm->currentFrame->ip++;
        uint8_t src2Slot = *vm->currentFrame->ip++;

        Value a = vm->currentFrame->slots[src1Slot];
        Value b = vm->currentFrame->slots[src2Slot];

        // Direct computation and storage - no push!
        vm->currentFrame->slots[destSlot] = vm->lessValues(a, b);
    }

    void op_jump_if_false_r(VM* vm)
    {
        uint8_t slot = *vm->currentFrame->ip++;
        uint16_t offset = (*vm->currentFrame->ip++) << 8;
        offset |= *vm->currentFrame->ip++;

        Value condition = vm->currentFrame->slots[slot];

        // Jump if false (doesn't pop, just reads from register)
        if (!isValueTrue(condition, vm))
        {
            vm->currentFrame->ip += offset;
        }
    }

    // ---------------------------------------------------------------------
    // Thin decoded wrappers that delegate to the existing legacy handlers
    // via runLegacyAsDecoded. The legacy bodies still read operands via
    // *ip++; the wrapper positions ip just past the opcode first, runs the
    // body, then commits any frame switch (no-op when no frame was pushed).
    // ---------------------------------------------------------------------
    #define DECODED_VIA_LEGACY(legacy_name) \
        void legacy_name##_decoded(VM* vm, const DecodedInstruction& instr) \
        { vm->runLegacyAsDecoded(instr, legacy_name); }

    DECODED_VIA_LEGACY(op_negate)
    DECODED_VIA_LEGACY(op_not)
    DECODED_VIA_LEGACY(op_and)
    DECODED_VIA_LEGACY(op_or)
    DECODED_VIA_LEGACY(op_true)
    DECODED_VIA_LEGACY(op_false)

    DECODED_VIA_LEGACY(op_post_incr_global)
    DECODED_VIA_LEGACY(op_incr_global)
    DECODED_VIA_LEGACY(op_post_decr_global)
    DECODED_VIA_LEGACY(op_decr_global)
    DECODED_VIA_LEGACY(op_post_incr_local)
    DECODED_VIA_LEGACY(op_incr_local)
    DECODED_VIA_LEGACY(op_post_decr_local)
    DECODED_VIA_LEGACY(op_decr_local)

    DECODED_VIA_LEGACY(op_subtract_ll)
    DECODED_VIA_LEGACY(op_subtract_lc)
    DECODED_VIA_LEGACY(op_subtract_cl)

    DECODED_VIA_LEGACY(op_load_constant_r)
    DECODED_VIA_LEGACY(op_move_r)
    DECODED_VIA_LEGACY(op_add_rrr)
    DECODED_VIA_LEGACY(op_less_rr)
    DECODED_VIA_LEGACY(op_incr_r)
    DECODED_VIA_LEGACY(op_less_rrr)
    DECODED_VIA_LEGACY(op_jump_if_false_r)

    #undef DECODED_VIA_LEGACY
}