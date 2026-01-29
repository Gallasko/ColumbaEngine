#pragma once

#include "Compiler/vm.h"
#include "Compiler/native_module.h"
#include <algorithm>
#include <cctype>

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

            // New string manipulation functions
            addNativeFunction("startsWith", nativeStartsWith);
            addNativeFunction("endsWith", nativeEndsWith);
            addNativeFunction("contains", nativeContains);
            addNativeFunction("charAt", nativeCharAt);
            addNativeFunction("substring", nativeSubstring);
            addNativeFunction("replace", nativeReplace);
            addNativeFunction("join", nativeJoin);
            addNativeFunction("capitalize", nativeCapitalize);
            addNativeFunction("toLowerCase", nativeToLowerCase);
            addNativeFunction("toUpperCase", nativeToUpperCase);
            addNativeFunction("trim", nativeTrim);
            addNativeFunction("indexOf", nativeIndexOf);
        }

    private:
        static Value nativeToString(VM* vm, int argCount, Value* args)
        {
            if (argCount != 1)
            {
                throw std::runtime_error("toString expects exactly 1 arguments (value)");
            }

            // Handle integers directly to avoid 32-bit truncation in ElementType
            if (IS_INT(args[0]))
            {
                int64_t val = AS_INT(args[0]);
                std::string strRepr = std::to_string(val);
                return vm->createString(strRepr);
            }

            // For other types, use ElementType conversion
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

        static Value nativeStartsWith(VM* vm, int argCount, Value* args)
        {
            if (argCount != 2)
            {
                throw std::runtime_error("startsWith expects exactly 2 arguments (string, prefix)");
            }

            if (not IS_STRING(args[0]) || not IS_STRING(args[1]))
            {
                throw std::runtime_error("startsWith expects both arguments to be strings");
            }

            auto str = vm->asString(args[0]);
            auto prefix = vm->asString(args[1]);

            bool result = str.size() >= prefix.size() &&
                          str.compare(0, prefix.size(), prefix) == 0;

            return makeBoolValue(result);
        }

        static Value nativeEndsWith(VM* vm, int argCount, Value* args)
        {
            if (argCount != 2)
            {
                throw std::runtime_error("endsWith expects exactly 2 arguments (string, suffix)");
            }

            if (not IS_STRING(args[0]) || not IS_STRING(args[1]))
            {
                throw std::runtime_error("endsWith expects both arguments to be strings");
            }

            auto str = vm->asString(args[0]);
            auto suffix = vm->asString(args[1]);

            bool result = str.size() >= suffix.size() &&
                          str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;

            return makeBoolValue(result);
        }

        static Value nativeContains(VM* vm, int argCount, Value* args)
        {
            if (argCount != 2)
            {
                throw std::runtime_error("contains expects exactly 2 arguments (string, substring)");
            }

            if (not IS_STRING(args[0]) || not IS_STRING(args[1]))
            {
                throw std::runtime_error("contains expects both arguments to be strings");
            }

            auto str = vm->asString(args[0]);
            auto substring = vm->asString(args[1]);

            bool result = str.find(substring) != std::string::npos;

            return makeBoolValue(result);
        }

        static Value nativeCharAt(VM* vm, int argCount, Value* args)
        {
            if (argCount != 2)
            {
                throw std::runtime_error("charAt expects exactly 2 arguments (string, index)");
            }

            if (not IS_STRING(args[0]))
            {
                throw std::runtime_error("charAt expects first argument to be a string");
            }

            if (not IS_INT(args[1]))
            {
                throw std::runtime_error("charAt expects second argument to be an integer");
            }

            auto str = vm->asString(args[0]);
            int64_t index = AS_INT(args[1]);

            if (index < 0 || index >= static_cast<int64_t>(str.size()))
            {
                throw std::runtime_error("charAt index out of bounds");
            }

            std::string result(1, str[index]);
            return vm->createString(result);
        }

        static Value nativeSubstring(VM* vm, int argCount, Value* args)
        {
            if (argCount < 2 || argCount > 3)
            {
                throw std::runtime_error("substring expects 2 or 3 arguments (string, start, [end])");
            }

            if (not IS_STRING(args[0]))
            {
                throw std::runtime_error("substring expects first argument to be a string");
            }

            auto str = vm->asString(args[0]);
            int64_t start = AS_INT(args[1]);
            int64_t end = argCount == 3 ? AS_INT(args[2]) : static_cast<int64_t>(str.size());

            if (start < 0 || start > static_cast<int64_t>(str.size()))
            {
                throw std::runtime_error("substring start index out of bounds");
            }

            if (end < start || end > static_cast<int64_t>(str.size()))
            {
                throw std::runtime_error("substring end index out of bounds");
            }

            std::string result = str.substr(start, end - start);
            return vm->createString(result);
        }

        static Value nativeReplace(VM* vm, int argCount, Value* args)
        {
            if (argCount != 3)
            {
                throw std::runtime_error("replace expects exactly 3 arguments (string, from, to)");
            }

            if (not IS_STRING(args[0]) || not IS_STRING(args[1]) || not IS_STRING(args[2]))
            {
                throw std::runtime_error("replace expects all arguments to be strings");
            }

            auto str = vm->asString(args[0]);
            auto from = vm->asString(args[1]);
            auto to = vm->asString(args[2]);

            if (from.empty())
            {
                return vm->createString(str);
            }

            std::string result = str;
            size_t pos = 0;

            while ((pos = result.find(from, pos)) != std::string::npos)
            {
                result.replace(pos, from.length(), to);
                pos += to.length();
            }

            return vm->createString(result);
        }

        static Value nativeJoin(VM* vm, int argCount, Value* args)
        {
            if (argCount != 2)
            {
                throw std::runtime_error("join expects exactly 2 arguments (array, separator)");
            }

            if (not IS_VECTOR(args[0]))
            {
                throw std::runtime_error("join expects first argument to be a vector");
            }

            if (not IS_STRING(args[1]))
            {
                throw std::runtime_error("join expects second argument to be a string");
            }

            ObjVector* vector = vm->asVector(args[0]);
            auto separator = vm->asString(args[1]);

            std::string result;
            for (size_t i = 0; i < vector->fields.size(); ++i)
            {
                if (i > 0)
                {
                    result += separator;
                }

                // Convert value to string
                if (IS_STRING(vector->fields[i]))
                {
                    result += vm->asString(vector->fields[i]);
                }
                else if (IS_INT(vector->fields[i]))
                {
                    result += std::to_string(AS_INT(vector->fields[i]));
                }
                else if (IS_DOUBLE(vector->fields[i]))
                {
                    result += std::to_string(AS_DOUBLE(vector->fields[i]));
                }
                else if (IS_BOOL(vector->fields[i]))
                {
                    result += AS_BOOL(vector->fields[i]) ? "true" : "false";
                }
                else
                {
                    auto element = vm->valueToElement(vector->fields[i]);
                    result += element.toString();
                }
            }

            return vm->createString(result);
        }

        static Value nativeCapitalize(VM* vm, int argCount, Value* args)
        {
            if (argCount != 1)
            {
                throw std::runtime_error("capitalize expects exactly 1 argument (string)");
            }

            if (not IS_STRING(args[0]))
            {
                throw std::runtime_error("capitalize expects a string argument");
            }

            auto str = vm->asString(args[0]);

            if (str.empty())
            {
                return vm->createString(str);
            }

            std::string result = str;
            result[0] = std::toupper(result[0]);

            return vm->createString(result);
        }

        static Value nativeToLowerCase(VM* vm, int argCount, Value* args)
        {
            if (argCount != 1)
            {
                throw std::runtime_error("toLowerCase expects exactly 1 argument (string)");
            }

            if (not IS_STRING(args[0]))
            {
                throw std::runtime_error("toLowerCase expects a string argument");
            }

            auto str = vm->asString(args[0]);
            std::string result = str;

            std::transform(result.begin(), result.end(), result.begin(),
                [](unsigned char c) { return std::tolower(c); });

            return vm->createString(result);
        }

        static Value nativeToUpperCase(VM* vm, int argCount, Value* args)
        {
            if (argCount != 1)
            {
                throw std::runtime_error("toUpperCase expects exactly 1 argument (string)");
            }

            if (not IS_STRING(args[0]))
            {
                throw std::runtime_error("toUpperCase expects a string argument");
            }

            auto str = vm->asString(args[0]);
            std::string result = str;

            std::transform(result.begin(), result.end(), result.begin(),
                [](unsigned char c) { return std::toupper(c); });

            return vm->createString(result);
        }

        static Value nativeTrim(VM* vm, int argCount, Value* args)
        {
            if (argCount != 1)
            {
                throw std::runtime_error("trim expects exactly 1 argument (string)");
            }

            if (not IS_STRING(args[0]))
            {
                throw std::runtime_error("trim expects a string argument");
            }

            auto str = vm->asString(args[0]);

            // Find first non-whitespace
            size_t start = 0;
            while (start < str.size() && std::isspace(static_cast<unsigned char>(str[start])))
            {
                ++start;
            }

            // Find last non-whitespace
            size_t end = str.size();
            while (end > start && std::isspace(static_cast<unsigned char>(str[end - 1])))
            {
                --end;
            }

            std::string result = str.substr(start, end - start);
            return vm->createString(result);
        }

        static Value nativeIndexOf(VM* vm, int argCount, Value* args)
        {
            if (argCount != 2)
            {
                throw std::runtime_error("indexOf expects exactly 2 arguments (string, substring)");
            }

            if (not IS_STRING(args[0]) || not IS_STRING(args[1]))
            {
                throw std::runtime_error("indexOf expects both arguments to be strings");
            }

            auto str = vm->asString(args[0]);
            auto substring = vm->asString(args[1]);

            size_t pos = str.find(substring);

            if (pos == std::string::npos)
            {
                return makeIntValue(-1);
            }

            return makeIntValue(static_cast<int64_t>(pos));
        }

    };
}