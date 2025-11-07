#pragma once

#include "object.h"
#include <map>
#include <string>

namespace pg
{
    // Forward declaration
    struct VM;

    /**
     * Base class for native modules
     * Similar to SysModule in the interpreter
     *
     * Usage:
     *   class MyModule : public NativeModule {
     *   public:
     *       MyModule() {
     *           addNativeFunction("myFunc", myNativeFunc);
     *           addNativeVariable("myVar", 42);
     *       }
     *   };
     */
    class NativeModule
    {
    friend struct VM;
    public:
        virtual ~NativeModule() {}

    protected:
        NativeModule() {}

        /**
         * Register a native function to be exported by this module
         * @param name The name of the function as it will appear in the script
         * @param function The C++ function pointer
         */
        void addNativeFunction(const std::string& name, NativeFn function)
        {
            exportedFunctions[name] = function;
        }

        /**
         * Register a native variable (as a constant value)
         * @param name The name of the variable
         * @param value The value (int, double, bool, or string)
         */
        template<typename T>
        void addNativeVariable(const std::string& name, const T& value)
        {
            // Store as lambda that returns the value when called
            exportedVariables[name] = value;
        }

    private:
        std::map<std::string, NativeFn> exportedFunctions;
        std::map<std::string, ElementType> exportedVariables;
    };
}