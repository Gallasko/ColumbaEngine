#pragma once

#include "Compiler/native_module.h"

#include <cmath>
#include <stdexcept>

namespace pg
{
    /**
     * Algorithm Module
     * Provides utility functions for common algorithms and data structure operations
     */
    class AlgorithmModule : public NativeModule
    {
    public:
        AlgorithmModule()
        {
            // Add utility functions
            addNativeFunction("contain", nativeContain);
            addNativeFunction("lookup", nativeLookup);
            addNativeFunction("pick", nativePick);
            addNativeFunction("findBy", nativeFindBy);
            addNativeFunction("indexBy", nativeIndexBy);
            addNativeFunction("copy", nativeCopy);
            addNativeFunction("merge", nativeMerge);
            addNativeFunction("toInt", nativeToInt);
            addNativeFunction("toFloat", nativeToFloat);
            addNativeFunction("typeOf", nativeTypeOf);

            // Array functions
            addNativeFunction("len", nativeLen);
            addNativeFunction("push", nativePush);
            addNativeFunction("pop", nativePop);
            addNativeFunction("insert", nativeInsert);
            addNativeFunction("removeAt", nativeRemoveAt);
        }

    private:
        /**
         * Check if a table contains a specific key
         * Usage: contain(table, "keyName")
         * Returns: true if the key exists as a field in the table, false otherwise
         *
         * This is useful for checking if a key exists before accessing it:
         *   if (not contain(sysData, "i")) {
         *       sysData["i"] = 0
         *   }
         */
        static Value nativeContain(VM* vm, int argCount, Value* args)
        {
            if (argCount != 2)
            {
                throw std::runtime_error("contain expects exactly 2 arguments (table, key)");
            }

            // Second argument must be a string (key name)
            if (not IS_STRING(args[1]))
            {
                throw std::runtime_error("contain expects second argument to be a string key");
            }

            // First argument must be an instance (table)
            if (IS_INSTANCE(args[0]))
            {
                ObjInstance* instance = vm->asInstance(args[0]);
                std::string key = vm->asString(args[1]);

                // Check if the key exists in the instance's fields
                bool exists = instance->hasField(key);

                return makeBoolValue(exists);
            }
            else if (IS_STRING(args[0]))
            {
                std::string value = vm->asString(args[0]);
                std::string key = vm->asString(args[1]);

                // Check if the key exists in the instance's fields
                bool exists = value.find(key) != value.npos;

                return makeBoolValue(exists);

            }
            else
            {
                throw std::runtime_error("contain expects first argument to be a table instance or a string");
            }
        }

        /**
         * The field of a table, or a fallback when it has none
         * Usage: lookup(table, "keyName", fallback)
         *
         * What a script writes `if (contain(t, k)) return t[k]; return fallback` for, in one call:
         * the key is looked up once, and no script function is called around the native one.
         */
        static Value nativeLookup(VM* vm, int argCount, Value* args)
        {
            if (argCount != 3)
            {
                throw std::runtime_error("lookup expects exactly 3 arguments (table, key, fallback)");
            }

            if (not IS_STRING(args[1]))
            {
                throw std::runtime_error("lookup expects second argument to be a string key");
            }

            if (not IS_INSTANCE(args[0]))
            {
                throw std::runtime_error("lookup expects first argument to be a table instance");
            }

            ObjInstance* instance = vm->asInstance(args[0]);
            const size_t slot = instance->findField(vm->asString(args[1]));

            // The arguments are released once the call is over: what is returned is retained
            if (slot != ObjInstance::npos)
                return vm->retainValue(instance->fieldValue(slot));

            return vm->retainValue(args[2]);
        }

        // The text a table keys a field by: a string as it is, an integer written out (as t[k] does)
        static bool keyText(VM* vm, const Value& key, std::string& text)
        {
            if (IS_STRING(key))
                text = vm->asString(key);
            else if (IS_INT(key))
                text = std::to_string(AS_INT(key));
            else
                return false;

            return true;
        }

        /**
         * A field read with a default
         * Usage: pick(table, key, fallback)
         * Returns: table[key], or fallback when the table has no such field or it holds none
         *
         * What a script wrote as `var found = table[key]; if (typeOf(found) == "none") return fallback`:
         * the commonest read of the rules. A list takes an integer index (out of range: the fallback).
         * A table's __get metamethod is not asked
         */
        static Value nativePick(VM* vm, int argCount, Value* args)
        {
            if (argCount != 3)
                throw std::runtime_error("pick expects exactly 3 arguments (table, key, fallback)");

            if (IS_INSTANCE(args[0]))
            {
                std::string key;

                if (not keyText(vm, args[1], key))
                    throw std::runtime_error("pick expects a string or integer key");

                const ObjInstance* instance = vm->asInstance(args[0]);
                const size_t slot = instance->findField(key);

                if (slot != ObjInstance::npos and not IS_NONE(instance->fieldValue(slot)))
                    return vm->retainValue(instance->fieldValue(slot));

                return vm->retainValue(args[2]);
            }

            if (IS_VECTOR(args[0]))
            {
                if (not IS_INT(args[1]))
                    throw std::runtime_error("pick expects an integer index for a list");

                const ObjVector* vec = vm->asVector(args[0]);
                int64_t index = AS_INT(args[1]);

                if (index < 0)
                    index += static_cast<int64_t>(vec->fields.size());

                if (index >= 0 and index < static_cast<int64_t>(vec->fields.size()) and not IS_NONE(vec->fields[index]))
                    return vm->retainValue(vec->fields[index]);

                return vm->retainValue(args[2]);
            }

            throw std::runtime_error("pick expects a table or a list");
        }

        /**
         * The first entry of a list whose field has a value
         * Usage: findBy(list, "id", id)
         * Returns: the first table of the list whose field equals the value (as == compares), none when none does
         *
         * The search a script wrote as a loop over the list. Entries that are not tables, or have
         * no such field, are passed over
         */
        static Value nativeFindBy(VM* vm, int argCount, Value* args)
        {
            if (argCount != 3)
                throw std::runtime_error("findBy expects exactly 3 arguments (list, field, value)");

            if (not IS_VECTOR(args[0]) or not IS_STRING(args[1]))
                throw std::runtime_error("findBy expects a list and a field name");

            const std::string field = vm->asString(args[1]);

            for (const Value& entry : vm->asVector(args[0])->fields)
            {
                if (not IS_INSTANCE(entry))
                    continue;

                const ObjInstance* instance = vm->asInstance(entry);
                const size_t slot = instance->findField(field);

                if (slot != ObjInstance::npos and AS_BOOL(vm->equalsValues(instance->fieldValue(slot), args[2])))
                    return vm->retainValue(entry);
            }

            return makeNoneValue();
        }

        /**
         * A table of a list's entries by one of their fields
         * Usage: var byId = indexBy(list, "id")
         * Returns: a table whose key is each entry's field (a string or an integer) and whose value is that entry
         *
         * The first entry of a key is the one kept, as a search would find it. Built once, a
         * findBy over the same list is a field read
         */
        static Value nativeIndexBy(VM* vm, int argCount, Value* args)
        {
            if (argCount != 2)
                throw std::runtime_error("indexBy expects exactly 2 arguments (list, field)");

            if (not IS_VECTOR(args[0]) or not IS_STRING(args[1]))
                throw std::runtime_error("indexBy expects a list and a field name");

            const std::string field = vm->asString(args[1]);

            Value indexValue = vm->createTable();
            ObjInstance* index = vm->asInstance(indexValue);

            std::string key;

            for (const Value& entry : vm->asVector(args[0])->fields)
            {
                if (not IS_INSTANCE(entry))
                    continue;

                const ObjInstance* instance = vm->asInstance(entry);
                const size_t slot = instance->findField(field);

                if (slot == ObjInstance::npos or not keyText(vm, instance->fieldValue(slot), key))
                    continue;

                if (index->findField(key) == ObjInstance::npos)
                    index->appendNewField(key, vm->retainValue(entry));
            }

            return indexValue;
        }

        /**
         * A copy of a table or of a list, one level deep
         * Usage: var after = copy(character)
         * Returns: a new table with the same fields in the same order (or a new list with the same
         * entries); the values are the same values, not copies of them
         *
         * What a script wrote as `for (var k : t) { copy[k] = t[k] }`. A table copied keeps the
         * layout of its fields, so it is filled at once. The copy of an instance is a table
         */
        static Value nativeCopy(VM* vm, int argCount, Value* args)
        {
            if (argCount != 1)
                throw std::runtime_error("copy expects exactly 1 argument (a table or a list)");

            if (IS_VECTOR(args[0]))
            {
                Value copyValue = vm->createVector();
                ObjVector* to = vm->asVector(copyValue);
                const ObjVector* from = vm->asVector(args[0]);

                to->fields.reserve(from->fields.size());

                for (const Value& entry : from->fields)
                    to->fields.push_back(vm->retainValue(entry));

                return copyValue;
            }

            if (not IS_INSTANCE(args[0]))
                throw std::runtime_error("copy expects a table or a list");

            ObjInstance* from = vm->asInstance(args[0]);

            Value copyValue = vm->createTable();
            ObjInstance* to = vm->asInstance(copyValue);

            const size_t count = from->fieldCount();

            if (Shape* shape = from->layout())
            {
                to->fillShaped(shape, from->allFieldValues().data(), count, 1);
            }
            else
            {
                to->reserveFields(count);

                for (size_t slot = 0; slot < count; ++slot)
                    to->appendNewField(from->fieldName(slot), from->fieldValue(slot));
            }

            for (Value& value : to->allFieldValues())
                vm->retainValue(value);

            return copyValue;
        }

        /**
         * One table's fields written into another
         * Usage: merge(into, from)
         * Returns: into, with each field of from set on it in from's order (a field it had takes from's value)
         *
         * What a script wrote as `for (var k : from) { into[k] = from[k] }`. A __set metamethod is not asked
         */
        static Value nativeMerge(VM* vm, int argCount, Value* args)
        {
            if (argCount != 2)
                throw std::runtime_error("merge expects exactly 2 arguments (into, from)");

            if (not IS_INSTANCE(args[0]) or not IS_INSTANCE(args[1]))
                throw std::runtime_error("merge expects two tables");

            ObjInstance* into = vm->asInstance(args[0]);
            ObjInstance* from = vm->asInstance(args[1]);

            const size_t count = from->fieldCount();

            for (size_t slot = 0; slot < count; ++slot)
            {
                const std::string& name = from->fieldName(slot);
                const Value value = vm->retainValue(from->fieldValue(slot));
                const size_t at = into->findField(name);

                if (at != ObjInstance::npos)
                {
                    vm->releaseAndDelete(into->fieldValueRef(at));
                    into->fieldValueRef(at) = value;
                }
                else
                {
                    into->appendNewField(name, value);
                }
            }

            return vm->retainValue(args[0]);
        }

        static Value nativeToInt(VM* vm, int argCount, Value* args)
        {
            if (argCount != 1)
            {
                throw std::runtime_error("toInt expects exactly 1 arguments (value)");
            }

            // Convert string to int (using 47-bit integer range)
            if (IS_STRING(args[0]))
            {
                auto str = vm->asString(args[0]);

                int64_t res = 0;
                try
                {
                    res = std::stoll(str);
                }
                catch (const std::out_of_range&)
                {
                    throw std::runtime_error("toInt: " + str + " is past the int range, use toFloat for larger values");
                }
                catch (const std::exception&)
                {
                    throw std::runtime_error("toInt could not convert the string " + str + " to an integer");
                }

                if (not fitsIntValue(res))
                    throw std::runtime_error("toInt: " + str + " is past the int range, use toFloat for larger values");

                // Use makeIntValue directly to create a 47-bit integer Value
                // This avoids ElementType conversion which would truncate to 32-bit int
                return makeIntValue(res);
            }

            // Default conversion for numeric types
            if (IS_INT(args[0]))
                return args[0];  // Already an integer

            if (IS_DOUBLE(args[0]))
            {
                const double whole = std::trunc(AS_DOUBLE(args[0]));

                if (not doubleFitsInt(whole))
                    throw std::runtime_error("toInt: the float is past the int range");

                return makeIntValue(static_cast<int64_t>(whole));
            }

            // Fallback to ElementType conversion
            auto value = vm->valueToElement(args[0]);
            auto intValue = value.get<int>();

            return makeIntValue(intValue);
        }

        /**
         * Convert a value to a double
         * Usage: toFloat("12.5") / toFloat(3)
         * Returns: the value as a double; a string that is not a number throws
         */
        static Value nativeToFloat(VM* vm, int argCount, Value* args)
        {
            if (argCount != 1)
            {
                throw std::runtime_error("toFloat expects exactly 1 arguments (value)");
            }

            if (IS_STRING(args[0]))
            {
                auto str = vm->asString(args[0]);

                double res = 0.0;
                size_t consumed = 0;
                try
                {
                    res = std::stod(str, &consumed);
                }
                catch (const std::exception&)
                {
                    throw std::runtime_error("toFloat could not convert the string " + str + " to a number");
                }

                if (consumed != str.size())
                    throw std::runtime_error("toFloat could not convert the string " + str + " to a number");

                return makeDoubleValue(res);
            }

            if (IS_DOUBLE(args[0]))
                return args[0];

            if (IS_INT(args[0]))
                return makeDoubleValue(static_cast<double>(AS_INT(args[0])));

            if (IS_BOOL(args[0]))
                return makeDoubleValue(AS_BOOL(args[0]) ? 1.0 : 0.0);

            throw std::runtime_error("toFloat expects a number or a string");
        }

        /**
         * Get the type of a value as a string
         * Usage: typeOf(value)
         * Returns: string describing the type ("int", "double", "string", "bool", "none", "array", "table", "function", "class", etc.)
         */
        static Value nativeTypeOf(VM* vm, int argCount, Value* args)
        {
            if (argCount != 1)
            {
                throw std::runtime_error("typeOf expects exactly 1 argument");
            }

            Value v = args[0];
            std::string typeName;

            if (IS_INT(v))
                typeName = "int";
            else if (IS_DOUBLE(v))
                typeName = "double";
            else if (IS_BOOL(v))
                typeName = "bool";
            else if (IS_STRING(v))
                typeName = "string";
            else if (IS_VECTOR(v))
                typeName = "array";
            else if (IS_INSTANCE(v))
                typeName = "table";
            else if (IS_CLASS(v))
                typeName = "class";
            else if (IS_FUNC(v))
                typeName = "function";
            else if (IS_CLOSURE(v))
                typeName = "closure";
            else if (IS_NAT_FUNC(v))
                typeName = "native_function";
            else if (IS_BOUND_METHOD(v))
                typeName = "bound_method";
            else if (IS_UPVALUE(v))
                typeName = "upvalue";
            else if (IS_NONE(v))
                typeName = "none";
            else
                typeName = "unknown";

            return vm->createString(typeName);
        }

        /**
         * Get the length of an array or string
         * Usage: len(array) or len(string)
         * Returns: integer length
         */
        static Value nativeLen(VM* vm, int argCount, Value* args)
        {
            if (argCount != 1)
            {
                throw std::runtime_error("len expects exactly 1 argument (array or string)");
            }

            if (IS_VECTOR(args[0]))
            {
                ObjVector* vec = vm->asVector(args[0]);
                return makeIntValue(vec->fields.size());
            }
            else if (IS_STRING(args[0]))
            {
                // A long string is measured where it lives, a copy would be as long as it is
                if (IS_LONG_STRING(args[0]))
                    return makeIntValue(vm->asStringPtr(args[0])->size());

                auto str = vm->asString(args[0]);
                return makeIntValue(str.size());
            }
            else
            {
                throw std::runtime_error("len expects an array or string, got " + std::string(valueTypeName(args[0])));
            }

            return makeIntValue(0); // Unreachable, but silences compiler warning
        }

        /**
         * Push an element to the end of an array
         * Usage: push(array, value)
         * Returns: the array (for chaining)
         */
        static Value nativePush(VM* vm, int argCount, Value* args)
        {
            if (argCount != 2)
            {
                throw std::runtime_error("push expects exactly 2 arguments (array, value)");
            }

            if (not IS_VECTOR(args[0]))
            {
                throw std::runtime_error("push expects first argument to be an array");
            }

            ObjVector* vec = vm->asVector(args[0]);
            vec->fields.push_back(vm->retainValue(args[1]));

            // Retain before returning because VM will release all arguments
            return vm->retainValue(args[0]);
        }

        /**
         * Pop an element from the end of an array
         * Usage: pop(array)
         * Returns: the popped value
         */
        static Value nativePop(VM* vm, int argCount, Value* args)
        {
            if (argCount != 1)
            {
                throw std::runtime_error("pop expects exactly 1 argument (array)");
            }

            if (not IS_VECTOR(args[0]))
            {
                throw std::runtime_error("pop expects an array");
            }

            ObjVector* vec = vm->asVector(args[0]);
            if (vec->fields.empty())
            {
                throw std::runtime_error("pop called on empty array");
            }

            Value value = vec->fields.back();
            vec->fields.pop_back();

            return vm->retainValue(value); // Retain before returning
        }

        /**
         * Insert an element at a specific index in an array
         * Usage: insert(array, index, value)
         * Returns: the array (for chaining)
         */
        static Value nativeInsert(VM* vm, int argCount, Value* args)
        {
            if (argCount != 3)
            {
                throw std::runtime_error("insert expects exactly 3 arguments (array, index, value)");
            }

            if (not IS_VECTOR(args[0]))
            {
                throw std::runtime_error("insert expects first argument to be an array");
            }

            if (not IS_INT(args[1]))
            {
                throw std::runtime_error("insert expects second argument to be an integer index");
            }

            ObjVector* vec = vm->asVector(args[0]);
            int64_t index = AS_INT(args[1]);

            if (index < 0 or index > static_cast<int64_t>(vec->fields.size()))
            {
                throw std::runtime_error("insert index out of bounds");
            }

            vec->fields.insert(vec->fields.begin() + index, vm->retainValue(args[2]));

            // Retain before returning because VM will release all arguments
            return vm->retainValue(args[0]);
        }

        /**
         * Remove an element at a specific index from an array
         * Usage: removeAt(array, index)
         * Returns: the removed value
         */
        static Value nativeRemoveAt(VM* vm, int argCount, Value* args)
        {
            if (argCount != 2)
            {
                throw std::runtime_error("removeAt expects exactly 2 arguments (array, index)");
            }

            if (not IS_VECTOR(args[0]))
            {
                throw std::runtime_error("removeAt expects first argument to be an array");
            }

            if (not IS_INT(args[1]))
            {
                throw std::runtime_error("removeAt expects second argument to be an integer index");
            }

            ObjVector* vec = vm->asVector(args[0]);
            int64_t index = AS_INT(args[1]);

            if (index < 0 or index >= static_cast<int64_t>(vec->fields.size()))
            {
                throw std::runtime_error("removeAt index out of bounds");
            }

            Value value = vec->fields[index];
            vec->fields.erase(vec->fields.begin() + index);

            return value;
        }
    };
}