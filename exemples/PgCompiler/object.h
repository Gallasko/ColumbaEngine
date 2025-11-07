#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

#include "Memory/elementtype.h"
#include "value_nanbox.h"

#ifndef IN_PGCOMPILER_TESTS_BUILD
// Debugging features
// Flag used to trace execution step-by-step (With the stack contents)
// #define DEBUG_TRACE_EXECUTION

// Flag to debug runtime memory management (tracking allocations/frees)
// #define DEBUG_RUNTIME_MEMORY

// Flag to unforce stack checks that should never fail
// #define DEBUG_CHECK_STACK

// Flag to profile compile times
// #define DEBUG_PROFILE_COMPILE

// Flag to print tokens during lexing
// #define DEBUG_PRINT_TOKENS

// Flag to print compiled bytecode
// #define DEBUG_PRINT_CODE
#endif

namespace pg
{
    // Forward declarations
    struct ObjFunction;
    struct ObjUpvalue;
    struct Klass;
    struct ObjInstance;
    struct ObjBoundMethod;
    struct VM;

    // Value is now defined in value_nanbox.h as uint64_t
    typedef uint64_t Value;

    typedef Value (*NativeFn)(VM* vm, int argCount, Value* args);

    struct NativeFunction
    {
        NativeFn function;
    };

    struct Closure
    {
        Closure(ObjFunction* func);

        ObjFunction* function;
        std::vector<ObjUpvalue*> upvalues;
    };

    enum class FunctionType
    {
        TYPE_FUNCTION,
        TYPE_METHOD,
        TYPE_INITIALIZER,
        TYPE_SCRIPT,
    };

    enum class InterpretResult
    {
        OK,
        COMPILE_ERROR,
        RUNTIME_ERROR
    };

    // Value type checking and extraction macros are now in value_nanbox.h

    struct CallFrame
    {
        Closure *closure;
        uint8_t *ip;
        Value *slots;
    };

    // Value creation functions are now in value_nanbox.h
    // Convenience macros for backward compatibility
    #define BOOL_VAL(value)           makeBoolValue(value)
    #define INT_VAL(value)            makeIntValue(value)
    #define FLOAT_VAL(value)          makeFloatValue(value)
    #define DOUBLE_VAL(value)         makeDoubleValue(value)

    struct ObjUpvalue
    {
        ObjUpvalue(Value* slot) : location(slot) {}

        Value* location;
        Value closed = INT_VAL(0); // Closed-over value when moved from stack to heap
        ObjUpvalue* next = nullptr;
    };

    struct Klass
    {
        Klass(const std::string& className) : name(className) {}

        std::string name;
        std::unordered_map<std::string, Value> methods;
    };

    struct ObjInstance
    {
        ObjInstance(Klass* klass) : klass(klass) {}

        Klass* klass;
        std::unordered_map<std::string, Value> fields;
    };

    struct ObjBoundMethod
    {
        ObjBoundMethod(const Value& receiver, Closure* method) : receiver(receiver), method(method) {}

        Value receiver;
        Closure* method;
    };

    // Note: elementToValue, valueToElement, copyValue, getValueAsInt are now VM member functions
    // Access them via: vm->elementToValue(), vm->valueToElement(), etc.
}