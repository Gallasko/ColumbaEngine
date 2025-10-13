#pragma once

#include <string>
#include <vector>

#include "Memory/elementtype.h"

namespace pg
{
    struct Value;
    struct ObjFunction;
    struct ObjUpvalue;

    typedef Value (*NativeFn)(int argCount, Value* args);

    struct NativeFunction
    {
        NativeFn function;
    };

    struct Closure
    {
        Closure(ObjFunction* func);

        ObjFunction* function;
        std::vector<ObjUpvalue*> upvalues;
        // Upvalues would go here for a full implementation
    };

    enum class FunctionType
    {
        TYPE_FUNCTION,
        TYPE_SCRIPT
    };

    enum class InterpretResult
    {
        OK,
        COMPILE_ERROR,
        RUNTIME_ERROR
    };

    /**
     * @brief Tagged union value representation following Crafting Interpreters pattern
     *
     * Stores small values (int, bool) directly in the union for zero-allocation performance.
     * Complex values (strings, floats) use pointer overflow to ElementType on the heap.
     *
     * Memory layout: 16 bytes total (8-byte union + 4-byte type + 4-byte padding)
     */
    enum CompilerValueType
    {
        COMPILER_VAL_BOOL,
        COMPILER_VAL_INT,
        COMPILER_VAL_OBJ,     // Pointer overflow for complex values (strings, floats, etc.)
        COMPILER_VAL_FUNC,
        COMPILER_VAL_NATIVE,
        COMPILER_VAL_CLOSURE,
        COMPILER_VAL_UPVALUE,
    };

    struct Value
    {
        CompilerValueType type;
        union {
            bool boolean;
            int64_t number;     // Use int64_t for wider range than int
            ElementType* obj;   // Heap-allocated for strings, floats, size_t, etc.
            ObjFunction* function;
            NativeFunction* nativeFunc;
            Closure* closure;
            ObjUpvalue* upvalue;
        } as;
    };

    // Fast type checking macros (compile-time constant)
    #define IS_BOOL(value)          ((value).type == COMPILER_VAL_BOOL)
    #define IS_INT(value)           ((value).type == COMPILER_VAL_INT)
    #define IS_OBJ(value)           ((value).type == COMPILER_VAL_OBJ)
    #define IS_FUNC(value)          ((value).type == COMPILER_VAL_FUNC)
    #define IS_NAT_FUNC(value)      ((value).type == COMPILER_VAL_NATIVE)
    #define IS_CLOSURE(value)       ((value).type == COMPILER_VAL_CLOSURE)
    #define IS_UPVALUE(value)       ((value).type == COMPILER_VAL_UPVALUE)

    // Fast value extraction macros (direct memory access)
    #define AS_BOOL(value)      ((value).as.boolean)
    #define AS_INT(value)       ((value).as.number)
    #define AS_OBJ(value)       ((value).as.obj)
    #define AS_FUNC(value)      ((value).as.function)
    #define AS_NAT_FUNC(value)  ((value).as.nativeFunc)
    #define AS_CLOSURE(value)   ((value).as.closure)
    #define AS_UPVALUE(value)   ((value).as.upvalue)

    struct CallFrame
    {
        Closure *closure;
        uint8_t *ip;
        Value *slots;
    };

    // Fast value creation functions (C++ compatible)
    inline Value makeBoolValue(bool value)
    {
        Value result;
        result.type = COMPILER_VAL_BOOL;
        result.as.boolean = value;
        return result;
    }

    inline Value makeIntValue(int64_t value)
    {
        Value result;
        result.type = COMPILER_VAL_INT;
        result.as.number = value;
        return result;
    }

    inline Value makeObjValue(ElementType* obj)
    {
        Value result;
        result.type = COMPILER_VAL_OBJ;
        result.as.obj = obj;
        return result;
    }

    inline Value makeFuncValue(ObjFunction* func)
    {
        Value result;
        result.type = COMPILER_VAL_FUNC;
        result.as.function = func;
        return result;
    }

    inline Value makeNativeFuncValue(NativeFunction *func)
    {
        Value result;
        result.type = COMPILER_VAL_NATIVE;
        result.as.nativeFunc = func;
        return result;
    }

    inline Value makeClosureValue(Closure* closure)
    {
        Value result;
        result.type = COMPILER_VAL_CLOSURE;
        result.as.closure = closure;
        return result;
    }

    inline Value makeUpvalueValue(ObjUpvalue* upvalue)
    {
        Value result;
        result.type = COMPILER_VAL_UPVALUE;
        result.as.upvalue = upvalue;
        return result;
    }

    // Convenience macros
    #define BOOL_VAL(value)      makeBoolValue(value)
    #define INT_VAL(value)       makeIntValue(value)
    #define OBJ_VAL(object)      makeObjValue(object)
    #define FUNC_VAL(func)       makeFuncValue(func)
    #define NATIVE_VAL(func)     makeNativeFuncValue(func)
    #define CLOSURE_VAL(closure) makeClosureValue(closure)
    #define UPVALUE_VAL(upvalue) makeUpvalueValue(upvalue)

    struct ObjUpvalue
    {
        ObjUpvalue(Value* slot) : location(slot) {}

        Value* location;
        Value closed = INT_VAL(0); // Closed-over value when moved from stack to heap
        ObjUpvalue* next = nullptr;
    };

    // Convert ElementType to optimized Value (minimize heap allocation)
    inline Value elementToValue(const ElementType& element)
    {
        if (element.isBool())
            return BOOL_VAL(element.get<bool>());
        else if (element.type == ElementType::UnionType::INT)
        {
            int intVal = element.get<int>();
            return INT_VAL(static_cast<int64_t>(intVal));
        }
        else
        {
            // Strings, floats, complex types always use heap
            return OBJ_VAL(new ElementType(element));
        }
    }

    // Create a copy of a Value (for when we need to store the same value in multiple places)
    inline Value copyValue(const Value& value)
    {
        // Integers and booleans can be copied directly (no heap allocation)
        if (IS_INT(value) || IS_BOOL(value))
            return value;
        // Functions are pointers, just copy the pointer
        else if (IS_FUNC(value))
            return value;
        else if (IS_NAT_FUNC(value))
            return value;
        else if (IS_CLOSURE(value))
            return value; // Todo: deep copy closure if needed
        else if (IS_UPVALUE(value))
            return value; // Upvalues are pointers, just copy
        // For heap objects, create a new copy
        else
            return OBJ_VAL(new ElementType(*AS_OBJ(value)));
    }

    inline int getValueAsInt(const Value& value)
    {
        if (IS_INT(value))
            return static_cast<int>(AS_INT(value));
        else if (IS_BOOL(value))
            return AS_BOOL(value) ? 1 : 0;
        else if (IS_OBJ(value) && AS_OBJ(value)->type == ElementType::UnionType::INT)
            return AS_OBJ(value)->get<int>();

        throw std::runtime_error("Value is not an integer");
    }

    // Convert Value back to ElementType when needed
    inline ElementType valueToElement(const Value& value)
    {
        switch (value.type)
        {
            case COMPILER_VAL_BOOL:
                return ElementType(AS_BOOL(value));
            case COMPILER_VAL_INT:
                return ElementType(static_cast<int>(AS_INT(value)));
            case COMPILER_VAL_OBJ:
                return *AS_OBJ(value);
            case COMPILER_VAL_FUNC:
            case COMPILER_VAL_NATIVE:
            case COMPILER_VAL_CLOSURE:
            case COMPILER_VAL_UPVALUE:
                throw std::runtime_error("Cannot convert function Value to ElementType");
        }

        return ElementType(); // Should never reach
    }
}