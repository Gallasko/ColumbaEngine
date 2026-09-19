#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <cstdint>
#include <functional>

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

    typedef std::function<Value(VM* vm, int argCount, Value* args)> NativeFn;

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

    struct DecodedInstruction;

    struct CallFrame
    {
        Closure *closure;
        uint8_t *ip;
        Value *slots;
        Value *stackBase;  // Where the caller's stack ends (position to truncate to on return)

        // Decoded instruction in the caller's decoded chunk to resume at
        // when this frame returns. Set at frame-push time by VM::call /
        // VM::callBound from vm->pendingCallResume (written by the frame-
        // pushing decoded handler). Returned by op_return_decoded straight
        // to the dispatch loop.
        const DecodedInstruction* callerResume = nullptr;
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

        // Interned field storage:
        // - fieldValues: Values in insertion order
        // - internedFields: Maps property name -> index in fieldValues
        // - fieldNames: slot -> property name, parallel to fieldValues, so
        //   positional access (OP_Table_At, the for-in protocol) is O(1)
        //   instead of scanning internedFields. Fields are append-only, so
        //   every fieldValues.push_back must push the name here too.
        std::vector<Value> fieldValues;
        std::unordered_map<std::string, size_t> internedFields;
        std::vector<std::string> fieldNames;

        // Helper to set a field value (creates or updates)
        void setField(const std::string& name, Value value, VM *vm = nullptr, bool deleteOld = false);

        // Helper to get a field value (returns nullptr-like value if not found)
        inline Value getField(const std::string& name) const
        {
            auto it = internedFields.find(name);

            if (it != internedFields.end())
            {
                return fieldValues[it->second];
            }

            return makeIntValue(0);  // Return a default value
        }

        inline bool hasField(const std::string& name) const
        {
            return internedFields.find(name) != internedFields.end();
        }
    };

    struct ObjBoundMethod
    {
        ObjBoundMethod(const Value& receiver, Closure* method) : receiver(receiver), method(method) {}

        Value receiver;
        Closure* method;
    };

    struct ObjVector
    {
        ObjVector() {}

        std::vector<Value> fields;
    };

    // Note: elementToValue, valueToElement, copyValue, getValueAsInt are now VM member functions
    // Access them via: vm->elementToValue(), vm->valueToElement(), etc.
}