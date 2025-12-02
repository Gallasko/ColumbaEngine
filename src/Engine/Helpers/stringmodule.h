#pragma once

#include "Compiler/vm.h"
#include "Compiler/native_module.h"

namespace pg
{
    /**
     * String Module
     * Provides utility functions for string operations
     */
    class StringModule : public NativeModule
    {
    public:
        StringModule()
        {
            // Add utility functions
            addNativeFunction("toString", nativeToString);
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
        static Value nativeToString(VM* vm, int argCount, Value* args)
        {
            if (argCount != 1)
            {
                throw std::runtime_error("contain expects exactly 1 arguments (table, key)");
            }

            auto element = vm->valueToElement(args[0]);
            std::string strRepr = element.toString();
            return vm->createString(strRepr);
        }
    };
}