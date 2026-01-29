#pragma once

#include "Compiler/vm.h"
#include "Compiler/native_module.h"

#include "filemanager.h"
#include <fstream>

namespace pg
{
    /**
     * File Module
     * Provides utility functions for file operations
     */
    class FileModule : public NativeModule
    {
    public:
        FileModule()
        {
            // Add utility functions
            addNativeFunction("readFile", nativeReadFile);
            addNativeFunction("writeFile", nativeWriteFile);
            addNativeFunction("fileExists", nativeFileExists);
        }

    private:
        static Value nativeReadFile(VM* vm, int argCount, Value* args)
        {
            if (argCount != 1)
            {
                throw std::runtime_error("readFile expects exactly 1 argument (filename)");
            }

            if (not IS_STRING(args[0]))
            {
                throw std::runtime_error("filename should be a string");
            }

            auto filename = vm->asString(args[0]);

            auto file = UniversalFileAccessor::openTextFile(filename);

            return vm->createString(file.data);
        }

        static Value nativeWriteFile(VM* vm, int argCount, Value* args)
        {
            if (argCount != 2)
            {
                throw std::runtime_error("writeFile expects exactly 2 arguments (filename, content)");
            }

            if (not IS_STRING(args[0]))
            {
                throw std::runtime_error("writeFile expects first argument to be a string (filename)");
            }

            if (not IS_STRING(args[1]))
            {
                throw std::runtime_error("writeFile expects second argument to be a string (content)");
            }

            auto filename = vm->asString(args[0]);
            auto content = vm->asString(args[1]);

            std::ofstream file(filename);
            if (!file)
            {
                throw std::runtime_error("Failed to write file: " + filename);
            }

            file << content;
            file.close();

            return makeBoolValue(true);
        }

        static Value nativeFileExists(VM* vm, int argCount, Value* args)
        {
            if (argCount != 1)
            {
                throw std::runtime_error("fileExists expects exactly 1 argument (filename)");
            }

            if (not IS_STRING(args[0]))
            {
                throw std::runtime_error("fileExists expects a string argument (filename)");
            }

            auto filename = vm->asString(args[0]);

            std::ifstream file(filename);
            bool exists = file.good();
            file.close();

            return makeBoolValue(exists);
        }
    };
}