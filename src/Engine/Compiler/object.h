#pragma once

#include <memory>
#include <string>
#include <string_view>
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
    struct ComponentProxyMetadata;
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

        // Whether it has a __set or a __get metamethod: looked for again only when its methods
        // have changed (a method is only ever added: the count says it)
        inline bool hasSetMeta() { checkMeta(); return setMeta; }
        inline bool hasGetMeta() { checkMeta(); return getMeta; }

    private:
        inline void checkMeta()
        {
            if (metaCheckedAt == methods.size())
                return;

            setMeta = methods.find("__set") != methods.end();
            getMeta = methods.find("__get") != methods.end();
            metaCheckedAt = methods.size();
        }

        size_t metaCheckedAt = static_cast<size_t>(-1);
        bool setMeta = false;
        bool getMeta = false;
    };

    // ---- Shapes ---------------------------------------------------------------------------------
    // A shape is a sequence of field names: the layout tables that were filled with the same names
    // in the same order share, instead of each keeping its own names and its own name index. A
    // table holds its shape and its values, slot for slot. Adding a field moves it to the shape
    // with that name added, found in its shape's `next` (each VM keeps one tree of them, from the
    // empty shape): tables filled alike end on the same shape. A field is never removed, so a
    // shape only ever grows.
    struct ShapeTree;

    struct Shape
    {
        static constexpr size_t npos = static_cast<size_t>(-1);

        Shape* parent = nullptr;
        ShapeTree* tree = nullptr;                      // The tree it is part of
        std::string lastName;                           // The name of its last slot
        uint32_t depth = 0;                             // How many slots it has

        std::unordered_map<std::string, Shape*> next;   // The shape with one name more

        // Its names in slot order and its name -> slot index: built the second time it is asked
        // (most shapes are only steps on the way to another, asked once or never: those are walked).
        // Both point at the `lastName` of the shapes on its chain, which live as long as the tree:
        // a name it hands out stays good when the index is let go (ShapeTree::unbuildAll)
        uint32_t queries = 0;
        bool built = false;
        std::vector<const std::string*> names;
        std::unordered_map<std::string_view, uint32_t> index;

        void build();
        void unbuild();

        inline size_t slotOf(const std::string& name)
        {
            if (not built)
            {
                if (++queries < 2)
                {
                    for (Shape* s = this; s->depth > 0; s = s->parent)
                    {
                        if (s->lastName == name)
                            return s->depth - 1;
                    }

                    return npos;
                }

                build();
            }

            auto it = index.find(name);

            return it == index.end() ? npos : it->second;
        }

        inline const std::string& nameAt(size_t slot)
        {
            if (not built)
                build();

            return *names[slot];
        }

        // Whether its tables hold a __set field (a metamethod of their own): looked for once
        inline bool hasSetField()
        {
            if (setField < 0)
                setField = slotOf(setFieldName) != npos ? 1 : 0;

            return setField == 1;
        }

        static const std::string setFieldName;
        int8_t setField = -1;
    };

    // The shapes of one VM, freed with it
    struct ShapeTree
    {
        // Past these a table keeps a dictionary of its own (what every table was before shapes):
        // a table of a hundred keys, or tables filled in every order (keyed by ids, by stats).
        // A shape is never freed before its VM, so their number is what bounds the tree: a VM that
        // runs for hours (a solver copying tables in every order of their keys) stops making them
        static constexpr uint32_t maxDepth = 96;
        static constexpr size_t maxNext = 64;
        static constexpr size_t maxNextAtRoot = 1024;
        static constexpr size_t maxShapes = 32768;

        // The names the built shapes index, all together: past it, every index is let go, and
        // built again as it is asked (an index costs its depth, the deepest a hundred names)
        static constexpr size_t maxBuiltNames = 1 << 18;

        ShapeTree() { root.tree = this; }

        ShapeTree(const ShapeTree&) = delete;
        ShapeTree& operator=(const ShapeTree&) = delete;

        Shape root;
        std::vector<std::unique_ptr<Shape>> owned;
        size_t builtNames = 0;

        // `from` with `name` added, nullptr when the tree will not grow that far
        Shape* next(Shape* from, const std::string& name);

        // Every built index let go (the shapes stay, and so does every name they hand out)
        void unbuildAll();
    };

    struct ObjInstance
    {
        ObjInstance(Klass* klass) : klass(klass) {}

        Klass* klass;

        // Only set on component proxies (see componentproxy.h): the C++ component the instance forwards to and its property table
        void* proxyTarget = nullptr;

        const ComponentProxyMetadata* proxyMeta = nullptr;

        // ---- The fields ----------------------------------------------------------------------
        // In the order they were added: a slot is a field's place in that order, and stays its
        // field's (a field is never removed). Everything outside goes through these, never the
        // storage, so that the storage can change in one place.

        static constexpr size_t npos = static_cast<size_t>(-1);

        // Its fields laid out by `tree`'s shapes from now on (VM::createInstance does it): an
        // instance without a tree keeps a dictionary of its own
        inline void useShapes(ShapeTree* tree)
        {
            shapes = tree;
            shape = tree ? &tree->root : nullptr;
        }

        inline size_t fieldCount() const { return fieldValueList.size(); }

        inline const std::string& fieldName(size_t slot) const
        {
            return shape ? shape->nameAt(slot) : dictionary->names[slot];
        }

        inline Value fieldValue(size_t slot) const { return fieldValueList[slot]; }

        // The value in place, to replace it (the caller releases and retains)
        inline Value& fieldValueRef(size_t slot) { return fieldValueList[slot]; }

        // Every value, in slot order (releasing them all, handing them to C++ as a list)
        inline std::vector<Value>& allFieldValues() { return fieldValueList; }

        // The slot of a field, npos when the instance has none
        inline size_t findField(const std::string& name) const
        {
            if (shape)
                return shape->slotOf(name);

            if (not dictionary)
                return npos;

            auto it = dictionary->index.find(name);

            return it == dictionary->index.end() ? npos : it->second;
        }

        // Helper to get a field value (none if not found)
        inline Value getField(const std::string& name) const
        {
            const size_t slot = findField(name);

            return slot == npos ? makeNoneValue() : fieldValueList[slot];
        }

        inline bool hasField(const std::string& name) const
        {
            return findField(name) != npos;
        }

        // Helper to set a field value (creates or updates)
        void setField(const std::string& name, Value value, VM *vm = nullptr, bool deleteOld = false);

        // A field added after the others: its slot. A name already there gets a second slot, and is
        // found at the new one from then on (what a table literal writing a key twice does)
        size_t appendField(const std::string& name, Value value);

        // The same, for a name the caller has just found missing
        size_t appendNewField(const std::string& name, Value value);

        inline void reserveFields(size_t count)
        {
            fieldValueList.reserve(count);
        }

        // Its layout: nullptr when it keeps a dictionary of its own
        inline Shape* layout() const { return shape; }

        // A field added by a step already known: `next` is its shape with the name added (a
        // write's cache learned it). Only for an instance of that shape's parent
        inline void appendAlong(Shape* next, Value value)
        {
            shape = next;
            fieldValueList.push_back(value);
        }

        // An empty instance given a shape and its values at once (a table literal whose keys made
        // that shape before): `count` values, `stride` apart
        inline void fillShaped(Shape* filled, const Value* values, size_t count, size_t stride)
        {
            shape = filled;
            fieldValueList.reserve(count);

            for (size_t i = 0; i < count; ++i)
                fieldValueList.push_back(values[i * stride]);
        }

    private:
        // What an instance off the shapes keeps: its names in slot order and its name -> slot index
        struct Dictionary
        {
            std::vector<std::string> names;
            std::unordered_map<std::string, size_t> index;
        };

        // Off the shapes from now on, with the names it has
        void toDictionary();

        std::vector<Value> fieldValueList;
        Shape* shape = nullptr;
        ShapeTree* shapes = nullptr;
        std::unique_ptr<Dictionary> dictionary;
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