#pragma once

#include "Compiler/native_module.h"

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
            addNativeFunction("toInt", nativeToInt);
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
                bool exists = instance->fields.find(key) != instance->fields.end();

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
                catch (const std::exception&)
                {
                    throw std::runtime_error("toInt could not convert the string " + str + " to an integer");
                }

                // Use makeIntValue directly to create a 47-bit integer Value
                // This avoids ElementType conversion which would truncate to 32-bit int
                return makeIntValue(res);
            }

            // Default conversion for numeric types
            if (IS_INT(args[0]))
                return args[0];  // Already an integer

            if (IS_DOUBLE(args[0]))
                return makeIntValue(static_cast<int64_t>(AS_DOUBLE(args[0])));

            // Fallback to ElementType conversion
            auto value = vm->valueToElement(args[0]);
            auto intValue = value.get<int>();

            return makeIntValue(intValue);
        }
    };
}