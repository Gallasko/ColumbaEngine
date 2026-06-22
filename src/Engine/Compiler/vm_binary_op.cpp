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

    const DecodedInstruction* op_true_decoded(VM* vm, const DecodedInstruction& instr)
    {
        vm->push(BOOL_VAL(true));
        return &instr + 1;
    }

    const DecodedInstruction* op_false_decoded(VM* vm, const DecodedInstruction& instr)
    {
        vm->push(BOOL_VAL(false));
        return &instr + 1;
    }

    #define BINARY_OP_TEMPLATE(op_name, operation) \
    const DecodedInstruction* op_name##_decoded(VM* vm, const DecodedInstruction& instr) \
    { \
        auto b = vm->pop(); \
        auto a = vm->peek(); \
        vm->changeTop(vm->operation(a, b)); \
        /* Escape analysis: Only release heap objects, not primitives */ \
        if (requiresRefCount(b)) vm->releaseAndDelete(b); \
        return &instr + 1; \
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

    const DecodedInstruction* op_negate_decoded(VM* vm, const DecodedInstruction& instr)
    {
        if (not isValueNumber(vm->peek(0)))
        {
            vm->runtimeError("Operand after an unary (-) must be a number.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return nullptr;
        }

        auto val = vm->pop();
        vm->push(vm->negateValue(val));
        vm->releaseAndDelete(val);
        return &instr + 1;
    }


    const DecodedInstruction* op_not_decoded(VM* vm, const DecodedInstruction& instr)
    {
        if (not IS_BOOL(vm->peek(0)))
        {
            vm->runtimeError("Operand after an unary (!) must be a boolean.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return nullptr;
        }

        auto value = vm->pop();
        vm->push(BOOL_VAL(not isValueTrue(value)));
        vm->releaseAndDelete(value);
        return &instr + 1;
    }

    const DecodedInstruction* op_and_decoded(VM* vm, const DecodedInstruction& instr)
    {
        vm->checkBooleanBinaryOp();
        auto b = vm->pop();
        auto a = vm->pop();

        bool resultA = isValueTrue(a);
        bool resultB = isValueTrue(b);
        vm->push(BOOL_VAL(resultA and resultB));
        vm->releaseAndDelete(a);
        vm->releaseAndDelete(b);
        return &instr + 1;
    }

    const DecodedInstruction* op_or_decoded(VM* vm, const DecodedInstruction& instr)
    {
        vm->checkBooleanBinaryOp();
        auto b = vm->pop();
        auto a = vm->pop();

        bool resultA = isValueTrue(a);
        bool resultB = isValueTrue(b);
        vm->push(BOOL_VAL(resultA or resultB));
        vm->releaseAndDelete(a);
        vm->releaseAndDelete(b);
        return &instr + 1;
    }


    const DecodedInstruction* op_post_incr_global_decoded(VM* vm, const DecodedInstruction& instr)
    {
#ifdef DEBUG_CHECK_STACK
        if (vm->stack.size() < 1)
        {
            vm->runtimeError("Stack underflow on post-increment.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return nullptr;
        }
#endif
        auto nameValue = vm->pop();
        auto name = vm->valueToElement(nameValue);

        if (not name.isLitteral())
        {
            vm->releaseAndDelete(nameValue);
            return vm->raiseError("Global variable name must be a litteral.");
        }

        VM::GlobalCell* cell = vm->findGlobalCell(name.toString());
        if (cell == nullptr or not cell->defined)
        {
            vm->releaseAndDelete(nameValue);
            return vm->raiseError("Undefined global variable '" + name.toString() + "'.");
        }

        if (not isValueNumber(cell->value))
        {
            vm->releaseAndDelete(nameValue);
            return vm->raiseError("Operand after an unary (++) must be a number.");
        }

        auto newValue = vm->addValues(cell->value, INT_VAL(1));
        vm->releaseAndDelete(cell->value);
        cell->value = vm->retainValue(newValue);

        vm->releaseAndDelete(nameValue);
        return &instr + 1;
    }

    const DecodedInstruction* op_incr_global_decoded(VM* vm, const DecodedInstruction& instr)
    {
#ifdef DEBUG_CHECK_STACK
        if (vm->stack.empty())
        {
            vm->runtimeError("Stack underflow on increment.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return nullptr;
        }
#endif
        auto nameValue = vm->pop();
        auto name = vm->valueToElement(nameValue);

        if (not name.isLitteral())
        {
            vm->releaseAndDelete(nameValue);
            return vm->raiseError("Global variable name must be a litteral.");
        }

        VM::GlobalCell* cell = vm->findGlobalCell(name.toString());
        if (cell == nullptr or not cell->defined)
        {
            vm->releaseAndDelete(nameValue);
            return vm->raiseError("Undefined global variable '" + name.toString() + "'.");
        }

        if (not isValueNumber(cell->value))
        {
            vm->releaseAndDelete(nameValue);
            return vm->raiseError("Operand after an unary (++) must be a number.");
        }

        auto newValue = vm->addValues(cell->value, INT_VAL(1));
        vm->releaseAndDelete(cell->value);
        cell->value = vm->retainValue(newValue);

        vm->push(vm->retainValue(newValue));
        vm->releaseAndDelete(nameValue);
        return &instr + 1;
    }

    const DecodedInstruction* op_post_decr_global_decoded(VM* vm, const DecodedInstruction& instr)
    {
#ifdef DEBUG_CHECK_STACK
        if (vm->stack.size() < 1)
        {
            vm->runtimeError("Stack underflow on post-decrement.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return nullptr;
        }
#endif
        auto nameValue = vm->pop();
        auto name = vm->valueToElement(nameValue);

        if (not name.isLitteral())
        {
            vm->releaseAndDelete(nameValue);
            return vm->raiseError("Global variable name must be a litteral.");
        }

        VM::GlobalCell* cell = vm->findGlobalCell(name.toString());
        if (cell == nullptr or not cell->defined)
        {
            vm->releaseAndDelete(nameValue);
            return vm->raiseError("Undefined global variable '" + name.toString() + "'.");
        }

        if (not isValueNumber(cell->value))
        {
            vm->releaseAndDelete(nameValue);
            return vm->raiseError("Operand after an unary (--) must be a number.");
        }

        auto newValue = vm->subtractValues(cell->value, INT_VAL(1));
        vm->releaseAndDelete(cell->value);
        cell->value = vm->retainValue(newValue);

        vm->releaseAndDelete(nameValue);
        return &instr + 1;
    }

    const DecodedInstruction* op_decr_global_decoded(VM* vm, const DecodedInstruction& instr)
    {
#ifdef DEBUG_CHECK_STACK
        if (vm->stack.empty())
        {
            vm->runtimeError("Stack underflow on decrement.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return nullptr;
        }
#endif
        auto nameValue = vm->pop();
        auto name = vm->valueToElement(nameValue);

        if (not name.isLitteral())
        {
            vm->releaseAndDelete(nameValue);
            return vm->raiseError("Global variable name must be a litteral.");
        }

        VM::GlobalCell* cell = vm->findGlobalCell(name.toString());
        if (cell == nullptr or not cell->defined)
        {
            vm->releaseAndDelete(nameValue);
            return vm->raiseError("Undefined global variable '" + name.toString() + "'.");
        }

        if (not isValueNumber(cell->value))
        {
            vm->releaseAndDelete(nameValue);
            return vm->raiseError("Operand after an unary (--) must be a number.");
        }

        auto newValue = vm->subtractValues(cell->value, INT_VAL(1));
        vm->releaseAndDelete(cell->value);
        cell->value = vm->retainValue(newValue);

        vm->push(vm->retainValue(newValue));
        vm->releaseAndDelete(nameValue);
        return &instr + 1;
    }

    const DecodedInstruction* op_post_incr_local_decoded(VM* vm, const DecodedInstruction& instr)
    {
#ifdef DEBUG_CHECK_STACK
        if (vm->stack.size() < 1)
        {
            vm->runtimeError("Not enough values on stack for local post-increment.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return nullptr;
        }
#endif
        auto slot = vm->pop();

        if (not isValueNumber(slot))
        {
            vm->releaseAndDelete(slot);
            vm->runtimeError("Local variable slot must be a number.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return nullptr;
        }

        int index = vm->getValueAsInt(slot);
        vm->releaseAndDelete(slot);

        if (index < 0)
        {
            vm->runtimeError("Local variable index cannot be negative.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return nullptr;
        }

        // Calculate the absolute stack index from the frame-relative index
        size_t stackIndex = (vm->currentFrame->slots - vm->stack.data()) + index;

        if (not isValueNumber(vm->stack[stackIndex]))
        {
            vm->runtimeError("Operand after an unary (++) must be a number.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return nullptr;
        }

        auto& val = vm->stack[stackIndex];

        // Old value is already on the stack (from OP_Get_Local before this opcode)
        // We just need to increment the variable in its slot
        auto newValue = vm->addValues(val, INT_VAL(1));
        vm->releaseAndDelete(val);
        val = newValue;
        return &instr + 1;
    }

    const DecodedInstruction* op_incr_local_decoded(VM* vm, const DecodedInstruction& instr)
    {
#ifdef DEBUG_CHECK_STACK
        if (vm->stack.size() < 1)
        {
            vm->runtimeError("Not enough values on stack for local increment.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return nullptr;
        }
#endif
        auto slot = vm->pop();

        if (not isValueNumber(slot))
        {
            vm->releaseAndDelete(slot);
            vm->runtimeError("Local variable slot must be a number.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return nullptr;
        }

        int index = vm->getValueAsInt(slot);
        if (index < 0)
        {
            vm->releaseAndDelete(slot);
            vm->runtimeError("Local variable index cannot be negative.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return nullptr;
        }

        if (not isValueNumber(vm->currentFrame->slots[index]))
        {
            vm->releaseAndDelete(slot);
            vm->runtimeError("Operand after an unary (++) must be a number.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return nullptr;
        }

        // Calculate the absolute stack index from the frame-relative index
        size_t stackIndex = (vm->currentFrame->slots - vm->stack.data()) + index;

        auto newValue = vm->addValues(vm->stack[stackIndex], INT_VAL(1));
        vm->releaseAndDelete(vm->stack[stackIndex]);
        vm->stack[stackIndex] = newValue;

        vm->push(vm->retainValue(newValue));
        vm->releaseAndDelete(slot);
        return &instr + 1;
    }

    const DecodedInstruction* op_post_decr_local_decoded(VM* vm, const DecodedInstruction& instr)
    {
#ifdef DEBUG_CHECK_STACK
        if (vm->stack.size() < 1)
        {
            vm->runtimeError("Not enough values on stack for local post-decrement.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return nullptr;
        }
#endif
        auto slot = vm->pop();

        if (not isValueNumber(slot))
        {
            vm->releaseAndDelete(slot);
            vm->runtimeError("Local variable slot must be a number.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return nullptr;
        }

        int index = vm->getValueAsInt(slot);
        if (index < 0)
        {
            vm->releaseAndDelete(slot);
            vm->runtimeError("Local variable index cannot be negative.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return nullptr;
        }

        // Calculate the absolute stack index from the frame-relative index
        size_t stackIndex = (vm->currentFrame->slots - vm->stack.data()) + index;

        if (not isValueNumber(vm->stack[stackIndex]))
        {
            vm->releaseAndDelete(slot);
            vm->runtimeError("Operand after an unary (--) must be a number.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return nullptr;
        }

        // Old value is already on the stack (from OP_Get_Local before this opcode)
        // We just need to decrement the variable in its slot
        auto newValue = vm->subtractValues(vm->stack[stackIndex], INT_VAL(1));
        vm->releaseAndDelete(vm->stack[stackIndex]);
        vm->stack[stackIndex] = newValue;

        vm->releaseAndDelete(slot);
        return &instr + 1;
    }

    const DecodedInstruction* op_decr_local_decoded(VM* vm, const DecodedInstruction& instr)
    {
#ifdef DEBUG_CHECK_STACK
        if (vm->stack.size() < 1)
        {
            vm->runtimeError("Not enough values on stack for local decrement.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return nullptr;
        }
#endif
        auto slot = vm->pop();

        if (not isValueNumber(slot))
        {
            vm->releaseAndDelete(slot);
            vm->runtimeError("Local variable slot must be a number.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return nullptr;
        }

        int index = vm->getValueAsInt(slot);
        if (index < 0)
        {
            vm->releaseAndDelete(slot);
            vm->runtimeError("Local variable index cannot be negative.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return nullptr;
        }

        if (not isValueNumber(vm->currentFrame->slots[index]))
        {
            vm->releaseAndDelete(slot);
            vm->runtimeError("Operand after an unary (--) must be a number.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return nullptr;
        }

        // Calculate the absolute stack index from the frame-relative index
        size_t stackIndex = (vm->currentFrame->slots - vm->stack.data()) + index;

        auto newValue = vm->subtractValues(vm->stack[stackIndex], INT_VAL(1));
        vm->releaseAndDelete(vm->stack[stackIndex]);
        vm->stack[stackIndex] = newValue;

        vm->push(vm->retainValue(newValue));
        vm->releaseAndDelete(slot);
        return &instr + 1;
    }

    const DecodedInstruction* op_add_ll_decoded(VM* vm, const DecodedInstruction& instr)
    {
        // Both local slot indices were pre-extracted at decode time.
        auto value1 = vm->currentFrame->slots[instr.operands.indexed.byte1];
        auto value2 = vm->currentFrame->slots[instr.operands.indexed.byte2];
        vm->push(vm->addValues(value1, value2));
        return &instr + 1;
    }

    // OP_LessEqualLL: peephole fusion of OP_Get_Local A + OP_Get_Local B + OP_LessEqual.
    // Reads two locals directly and pushes the comparison result. No popping the
    // pushed local copies, no addValues-style boxing — straight slot read + compare.
    const DecodedInstruction* op_less_equal_ll_decoded(VM* vm, const DecodedInstruction& instr)
    {
        auto v1 = vm->currentFrame->slots[instr.operands.indexed.byte1];
        auto v2 = vm->currentFrame->slots[instr.operands.indexed.byte2];
        vm->push(vm->lessEqualValues(v1, v2));
        return &instr + 1;
    }

    // OP_LessLL: same shape as OP_LessEqualLL but using lessValues. Used by
    // loop conditions of the form `while (x < y)` where both are locals.
    const DecodedInstruction* op_less_ll_decoded(VM* vm, const DecodedInstruction& instr)
    {
        auto v1 = vm->currentFrame->slots[instr.operands.indexed.byte1];
        auto v2 = vm->currentFrame->slots[instr.operands.indexed.byte2];
        vm->push(vm->lessValues(v1, v2));
        return &instr + 1;
    }

    // Two-byte-operand subtract peephole fusions.

    const DecodedInstruction* op_subtract_ll_decoded(VM* vm, const DecodedInstruction& instr)
    {
        uint8_t local1 = instr.operands.indexed.byte1;
        uint8_t local2 = instr.operands.indexed.byte2;

        auto value1 = vm->currentFrame->slots[local1];
        auto value2 = vm->currentFrame->slots[local2];

        vm->push(vm->subtractValues(value1, value2));
        return &instr + 1;
    }

    const DecodedInstruction* op_subtract_lc_decoded(VM* vm, const DecodedInstruction& instr)
    {
        uint8_t local1        = instr.operands.indexed.byte1;
        uint8_t constantIndex = instr.operands.indexed.byte2;

        auto value1 = vm->currentFrame->slots[local1];
        auto value2 = vm->currentFrame->closure->function->chunk.constants[constantIndex];

        vm->push(vm->subtractValues(value1, value2));
        return &instr + 1;
    }

    const DecodedInstruction* op_subtract_cl_decoded(VM* vm, const DecodedInstruction& instr)
    {
        uint8_t constantIndex = instr.operands.indexed.byte1;
        uint8_t local2        = instr.operands.indexed.byte2;

        auto value1 = vm->currentFrame->closure->function->chunk.constants[constantIndex];
        auto value2 = vm->currentFrame->slots[local2];

        vm->push(vm->subtractValues(value1, value2));
        return &instr + 1;
    }

    // Register-based ops: operands are 1–3 slot indices stored in
    // instr.operands.indexed (or .byte for the single-operand op_incr_r).

    const DecodedInstruction* op_load_constant_r_decoded(VM* vm, const DecodedInstruction& instr)
    {
        uint8_t destSlot   = instr.operands.indexed.byte1;
        uint8_t constIndex = instr.operands.indexed.byte2;

        vm->currentFrame->slots[destSlot] =
            vm->currentFrame->closure->function->chunk.constants[constIndex];
        return &instr + 1;
    }

    const DecodedInstruction* op_move_r_decoded(VM* vm, const DecodedInstruction& instr)
    {
        uint8_t destSlot = instr.operands.indexed.byte1;
        uint8_t srcSlot  = instr.operands.indexed.byte2;

        vm->currentFrame->slots[destSlot] = vm->currentFrame->slots[srcSlot];
        return &instr + 1;
    }

    const DecodedInstruction* op_add_rrr_decoded(VM* vm, const DecodedInstruction& instr)
    {
        uint8_t destSlot = instr.operands.indexed.byte1;
        uint8_t src1Slot = instr.operands.indexed.byte2;
        uint8_t src2Slot = instr.operands.indexed.byte3;

        Value a = vm->currentFrame->slots[src1Slot];
        Value b = vm->currentFrame->slots[src2Slot];

        vm->currentFrame->slots[destSlot] = vm->addValues(a, b);
        return &instr + 1;
    }

    const DecodedInstruction* op_less_rr_decoded(VM* vm, const DecodedInstruction& instr)
    {
        uint8_t src1Slot = instr.operands.indexed.byte1;
        uint8_t src2Slot = instr.operands.indexed.byte2;

        Value a = vm->currentFrame->slots[src1Slot];
        Value b = vm->currentFrame->slots[src2Slot];

        vm->push(vm->lessValues(a, b));
        return &instr + 1;
    }

    const DecodedInstruction* op_incr_r_decoded(VM* vm, const DecodedInstruction& instr)
    {
        uint8_t slot = instr.operands.byte;
        Value   val  = vm->currentFrame->slots[slot];

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
            vm->currentFrame->slots[slot] = vm->addValues(val, INT_VAL(1));
        }
        return &instr + 1;
    }

    const DecodedInstruction* op_less_rrr_decoded(VM* vm, const DecodedInstruction& instr)
    {
        uint8_t destSlot = instr.operands.indexed.byte1;
        uint8_t src1Slot = instr.operands.indexed.byte2;
        uint8_t src2Slot = instr.operands.indexed.byte3;

        Value a = vm->currentFrame->slots[src1Slot];
        Value b = vm->currentFrame->slots[src2Slot];

        vm->currentFrame->slots[destSlot] = vm->lessValues(a, b);
        return &instr + 1;
    }

    // OP_Jump_If_False_R: register-based conditional jump (slot + 16-bit
    // offset). The precomputed false-branch target lives in
    // instr.jumpTargetPtr (resolved by resolveJumpTargets).
    // Note: the compiler does not currently emit this op; the handler is
    // provided for completeness.
    const DecodedInstruction* op_jump_if_false_r_decoded(VM* vm, const DecodedInstruction& instr)
    {
        uint8_t slot = instr.operands.indexed.byte1;
        Value condition = vm->currentFrame->slots[slot];

        if (not isValueTrue(condition))
            return instr.jumpTargetPtr;

        return &instr + 1;
    }
}