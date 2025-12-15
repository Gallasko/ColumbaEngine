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
            addNativeFunction("split", nativeSplit);
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

            auto str = vm->asString(args[0]);

            return makeIntValue(str.size());
        }

        static Value nativeSplit(VM* vm, int argCount, Value* args)
        {
            if (argCount != 2)
            {
                throw std::runtime_error("split expects exactly 2 arguments (string, delimiter)");
            }

            if (not IS_STRING(args[0]))
            {
                throw std::runtime_error("split expects a string as the first argument");
            }

            if (not IS_STRING(args[1]))
            {
                throw std::runtime_error("split expects a string as the second argument");
            }

            auto str = vm->asString(args[0]);
            auto delim = vm->asString(args[1]);

            // Create a vector to hold the parts
            Value vectorValue = vm->createVector();
            ObjVector* vector = vm->asVector(vectorValue);

            // Handle empty delimiter case
            if (delim.empty())
            {
                throw std::runtime_error("split delimiter cannot be empty");
            }

            // Split the string by delimiter
            size_t start = 0;
            size_t end = str.find(delim);

            while (end != std::string::npos)
            {
                vector->fields.push_back(vm->retainValue(vm->createString(str.substr(start, end - start))));
                start = end + delim.length();
                end = str.find(delim, start);
            }

            // Add the last part (or the whole string if delimiter not found)
            vector->fields.push_back(vm->retainValue(vm->createString(str.substr(start))));

            return vectorValue;
        }

        static Value nativeSplitLines(VM* vm, int argCount, Value* args)
        {
            if (argCount != 1)
            {
                throw std::runtime_error("splitLines expects exactly 1 argument (string)");
            }

            if (not IS_STRING(args[0]))
            {
                throw std::runtime_error("splitLines expects a string as the argument");
            }

            auto str = vm->asString(args[0]);

            // Create a vector to hold the lines (preserves order)
            Value vectorValue = vm->createVector();
            ObjVector* vector = vm->asVector(vectorValue);

            // Split the string by newlines
            std::string line;

            for (size_t i = 0; i < str.length(); ++i)
            {
                char c = str[i];

                // Handle different line ending types: \n, \r\n, \r
                if (c == '\n')
                {
                    // Store the current line
                    vector->fields.push_back(vm->retainValue(vm->createString(line)));
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
                    vector->fields.push_back(vm->retainValue(vm->createString(line)));
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
                vector->fields.push_back(vm->retainValue(vm->createString(line)));
            }

            return vectorValue;
        }

    };
}