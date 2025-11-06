#pragma once

#include "native_module.h"
#include <cmath>

namespace pg
{
    /**
     * Example Math Module
     * Provides mathematical constants and functions
     */
    class MathModule : public NativeModule
    {
    public:
        MathModule()
        {
            // Add mathematical constants
            addNativeVariable("PI", 3.14159265358979323846);
            addNativeVariable("E", 2.71828182845904523536);

            // Add mathematical functions
            addNativeFunction("sqrt", nativeSqrt);
            addNativeFunction("abs", nativeAbs);
            addNativeFunction("pow", nativePow);
            addNativeFunction("floor", nativeFloor);
            addNativeFunction("ceil", nativeCeil);
        }

    private:
        // Square root
        static Value nativeSqrt(VM*, int argCount, Value* args)
        {
            if (argCount != 1)
            {
                throw std::runtime_error("sqrt expects exactly 1 argument");
            }

            if (IS_INT(args[0]))
            {
                return makeDoubleValue(std::sqrt(static_cast<double>(AS_INT(args[0]))));
            }
            else if (IS_DOUBLE(args[0]))
            {
                return makeDoubleValue(std::sqrt(AS_DOUBLE(args[0])));
            }

            throw std::runtime_error("sqrt expects a number");
        }

        // Absolute value
        static Value nativeAbs(VM*, int argCount, Value* args)
        {
            if (argCount != 1)
            {
                throw std::runtime_error("abs expects exactly 1 argument");
            }

            if (IS_INT(args[0]))
            {
                return makeIntValue(std::abs(AS_INT(args[0])));
            }
            else if (IS_DOUBLE(args[0]))
            {
                return makeDoubleValue(std::fabs(AS_DOUBLE(args[0])));
            }

            throw std::runtime_error("abs expects a number");
        }

        // Power
        static Value nativePow(VM*, int argCount, Value* args)
        {
            if (argCount != 2)
            {
                throw std::runtime_error("pow expects exactly 2 arguments");
            }

            double base = 0.0;
            double exponent = 0.0;

            if (IS_INT(args[0]))
                base = static_cast<double>(AS_INT(args[0]));
            else if (IS_DOUBLE(args[0]))
                base = AS_DOUBLE(args[0]);
            else
                throw std::runtime_error("pow expects numbers");

            if (IS_INT(args[1]))
                exponent = static_cast<double>(AS_INT(args[1]));
            else if (IS_DOUBLE(args[1]))
                exponent = AS_DOUBLE(args[1]);
            else
                throw std::runtime_error("pow expects numbers");

            return makeDoubleValue(std::pow(base, exponent));
        }

        // Floor
        static Value nativeFloor(VM*, int argCount, Value* args)
        {
            if (argCount != 1)
            {
                throw std::runtime_error("floor expects exactly 1 argument");
            }

            if (IS_INT(args[0]))
            {
                return args[0]; // Already an integer
            }
            else if (IS_DOUBLE(args[0]))
            {
                return makeIntValue(static_cast<int64_t>(std::floor(AS_DOUBLE(args[0]))));
            }

            throw std::runtime_error("floor expects a number");
        }

        // Ceiling
        static Value nativeCeil(VM*, int argCount, Value* args)
        {
            if (argCount != 1)
            {
                throw std::runtime_error("ceil expects exactly 1 argument");
            }

            if (IS_INT(args[0]))
            {
                return args[0]; // Already an integer
            }
            else if (IS_DOUBLE(args[0]))
            {
                return makeIntValue(static_cast<int64_t>(std::ceil(AS_DOUBLE(args[0]))));
            }

            throw std::runtime_error("ceil expects a number");
        }
    };
}