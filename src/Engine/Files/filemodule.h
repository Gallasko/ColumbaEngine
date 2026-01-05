#pragma once

#include "Compiler/vm.h"
#include "Compiler/native_module.h"

#include "filemanager.h"

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
            addNativeFunction("readFile", readFile);
        }

    private:
        static Value readFile(VM* vm, int argCount, Value* args)
        {
            if (argCount != 1)
            {
                throw std::runtime_error("contain expects exactly 1 arguments (table, key)");
            }

            if (not IS_STRING(args[0]))
            {
                throw std::runtime_error("filename should be a string");
            }

            auto filename = vm->asString(args[0]);

            auto file = UniversalFileAccessor::openTextFile(filename);

            return vm->createString(file.data);
        }
    };
}