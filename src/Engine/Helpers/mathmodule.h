#pragma once

#include "Compiler/native_module.h"
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

            // Add trigonometric functions
            addNativeFunction("sin", nativeSin);
            addNativeFunction("cos", nativeCos);
            addNativeFunction("tan", nativeTan);
            addNativeFunction("atan2", nativeAtan2);
            addNativeFunction("asin", nativeAsin);
            addNativeFunction("acos", nativeAcos);
            addNativeFunction("atan", nativeAtan);
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

        // Sine (radians)
        static Value nativeSin(VM*, int argCount, Value* args)
        {
            if (argCount != 1)
            {
                throw std::runtime_error("sin expects exactly 1 argument");
            }

            if (IS_INT(args[0]))
            {
                return makeDoubleValue(std::sin(static_cast<double>(AS_INT(args[0]))));
            }
            else if (IS_DOUBLE(args[0]))
            {
                return makeDoubleValue(std::sin(AS_DOUBLE(args[0])));
            }

            throw std::runtime_error("sin expects a number");
        }

        // Cosine (radians)
        static Value nativeCos(VM*, int argCount, Value* args)
        {
            if (argCount != 1)
            {
                throw std::runtime_error("cos expects exactly 1 argument");
            }

            if (IS_INT(args[0]))
            {
                return makeDoubleValue(std::cos(static_cast<double>(AS_INT(args[0]))));
            }
            else if (IS_DOUBLE(args[0]))
            {
                return makeDoubleValue(std::cos(AS_DOUBLE(args[0])));
            }

            throw std::runtime_error("cos expects a number");
        }

        // Tangent (radians)
        static Value nativeTan(VM*, int argCount, Value* args)
        {
            if (argCount != 1)
            {
                throw std::runtime_error("tan expects exactly 1 argument");
            }

            if (IS_INT(args[0]))
            {
                return makeDoubleValue(std::tan(static_cast<double>(AS_INT(args[0]))));
            }
            else if (IS_DOUBLE(args[0]))
            {
                return makeDoubleValue(std::tan(AS_DOUBLE(args[0])));
            }

            throw std::runtime_error("tan expects a number");
        }

        // Arc tangent 2 (returns angle in radians from x,y coordinates)
        static Value nativeAtan2(VM*, int argCount, Value* args)
        {
            if (argCount != 2)
            {
                throw std::runtime_error("atan2 expects exactly 2 arguments (y, x)");
            }

            double y = 0.0;
            double x = 0.0;

            if (IS_INT(args[0]))
                y = static_cast<double>(AS_INT(args[0]));
            else if (IS_DOUBLE(args[0]))
                y = AS_DOUBLE(args[0]);
            else
                throw std::runtime_error("atan2 expects numbers");

            if (IS_INT(args[1]))
                x = static_cast<double>(AS_INT(args[1]));
            else if (IS_DOUBLE(args[1]))
                x = AS_DOUBLE(args[1]);
            else
                throw std::runtime_error("atan2 expects numbers");

            return makeDoubleValue(std::atan2(y, x));
        }

        // Arc sine (radians)
        static Value nativeAsin(VM*, int argCount, Value* args)
        {
            if (argCount != 1)
            {
                throw std::runtime_error("asin expects exactly 1 argument");
            }

            if (IS_INT(args[0]))
            {
                return makeDoubleValue(std::asin(static_cast<double>(AS_INT(args[0]))));
            }
            else if (IS_DOUBLE(args[0]))
            {
                return makeDoubleValue(std::asin(AS_DOUBLE(args[0])));
            }

            throw std::runtime_error("asin expects a number");
        }

        // Arc cosine (radians)
        static Value nativeAcos(VM*, int argCount, Value* args)
        {
            if (argCount != 1)
            {
                throw std::runtime_error("acos expects exactly 1 argument");
            }

            if (IS_INT(args[0]))
            {
                return makeDoubleValue(std::acos(static_cast<double>(AS_INT(args[0]))));
            }
            else if (IS_DOUBLE(args[0]))
            {
                return makeDoubleValue(std::acos(AS_DOUBLE(args[0])));
            }

            throw std::runtime_error("acos expects a number");
        }

        // Arc tangent (radians)
        static Value nativeAtan(VM*, int argCount, Value* args)
        {
            if (argCount != 1)
            {
                throw std::runtime_error("atan expects exactly 1 argument");
            }

            if (IS_INT(args[0]))
            {
                return makeDoubleValue(std::atan(static_cast<double>(AS_INT(args[0]))));
            }
            else if (IS_DOUBLE(args[0]))
            {
                return makeDoubleValue(std::atan(AS_DOUBLE(args[0])));
            }

            throw std::runtime_error("atan expects a number");
        }
    };
}