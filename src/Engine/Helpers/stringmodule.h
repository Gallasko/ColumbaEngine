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
            addNativeFunction("strlen", nativeStrlen);
            addNativeFunction("splitLines", nativeSplitLines);
        }

    private:
        static Value nativeToString(VM* vm, int argCount, Value* args)
        {
            if (argCount != 1)
            {
                throw std::runtime_error("toString expects exactly 1 arguments (value)");
            }

            auto element = vm->valueToElement(args[0]);
            std::string strRepr = element.toString();
            return vm->createString(strRepr);
        }

        static Value nativeStrlen(VM* vm, int argCount, Value* args)
        {
            if (argCount != 1)
            {
                throw std::runtime_error("strlen expects exactly 1 arguments (string)");
            }

            if (not IS_STRING(args[0]))
            {
                throw std::runtime_error("strlen expects a string as the argument");
            }

            auto str = vm->asString(args[0])->toString();

            return makeIntValue(str.size());
        }

        static Value nativeSplitLines(VM* vm, int argCount, Value* args)
        {
            if (argCount != 1)
            {
                throw std::runtime_error("splitLines expects exactly 1 argument (string)");
            }

            if (!IS_STRING(args[0]))
            {
                throw std::runtime_error("splitLines expects a string as the argument");
            }

            auto str = vm->asString(args[0])->toString();

            // Get the Table class from VM globals
            auto it = vm->globals.find("__Table");
            if (it == vm->globals.end())
            {
                throw std::runtime_error("Table class not found in VM globals");
            }

            Klass* tableClass = vm->asClass(it->second);

            // Create a table to hold the lines
            Value tableValue = vm->createInstance(tableClass);
            ObjInstance* table = vm->asInstance(tableValue);

            // Split the string by newlines
            std::string line;
            int lineIndex = 0;

            for (size_t i = 0; i < str.length(); ++i)
            {
                char c = str[i];

                // Handle different line ending types: \n, \r\n, \r
                if (c == '\n')
                {
                    // Store the current line
                    table->fields[std::to_string(lineIndex)] = vm->createString(line);
                    lineIndex++;
                    line.clear();
                }
                else if (c == '\r')
                {
                    // Check if next character is \n (Windows line ending)
                    if (i + 1 < str.length() && str[i + 1] == '\n')
                    {
                        // Skip the \n, we'll handle it as one line ending
                        i++;
                    }
                    // Store the current line
                    table->fields[std::to_string(lineIndex)] = vm->createString(line);
                    lineIndex++;
                    line.clear();
                }
                else
                {
                    line += c;
                }
            }

            // Don't forget the last line if the string doesn't end with a newline
            if (!line.empty() || (str.length() > 0 && (str.back() == '\n' || str.back() == '\r')))
            {
                table->fields[std::to_string(lineIndex)] = vm->createString(line);
            }

            return tableValue;
        }

    };
}