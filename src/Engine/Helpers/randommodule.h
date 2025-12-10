#pragma once

#include "Compiler/native_module.h"
#include "Maths/randomnumbergenerator.h"
#include <cmath>
#include <random>

namespace pg
{
    /**
     * Random Number Generation Module
     * Provides random number generation functions for scripts
     */
    class RandomModule : public NativeModule
    {
    public:
        RandomModule()
        {
            // Add random number generation functions
            addNativeFunction("random", nativeRandom);
            addNativeFunction("randomRange", nativeRandomRange);
            addNativeFunction("randomInt", nativeRandomInt);
            addNativeFunction("randomSeed", nativeRandomSeed);
        }

    private:
        // Generate random float between 0.0 and 1.0
        static Value nativeRandom(VM*, int argCount, Value*)
        {
            if (argCount != 0)
            {
                throw std::runtime_error("random expects no arguments");
            }

            auto& rng = RandomNumberGenerator::generator();
            int randomInt = rng->generateNumber();

            // Normalize to 0.0 - 1.0
            double normalized = static_cast<double>(randomInt) / static_cast<double>(RAND_MAX);
            return makeDoubleValue(normalized);
        }

        // Generate random float between min and max
        static Value nativeRandomRange(VM*, int argCount, Value* args)
        {
            if (argCount != 2)
            {
                throw std::runtime_error("randomRange expects exactly 2 arguments (min, max)");
            }

            double min = 0.0;
            double max = 0.0;

            if (IS_INT(args[0]))
                min = static_cast<double>(AS_INT(args[0]));
            else if (IS_DOUBLE(args[0]))
                min = AS_DOUBLE(args[0]);
            else
                throw std::runtime_error("randomRange expects numbers");

            if (IS_INT(args[1]))
                max = static_cast<double>(AS_INT(args[1]));
            else if (IS_DOUBLE(args[1]))
                max = AS_DOUBLE(args[1]);
            else
                throw std::runtime_error("randomRange expects numbers");

            auto& rng = RandomNumberGenerator::generator();
            int randomInt = rng->generateNumber();

            // Normalize and scale to range
            double normalized = static_cast<double>(randomInt) / static_cast<double>(RAND_MAX);
            double result = min + (normalized * (max - min));

            return makeDoubleValue(result);
        }

        // Generate random integer between min and max (inclusive)
        static Value nativeRandomInt(VM*, int argCount, Value* args)
        {
            if (argCount != 2)
            {
                throw std::runtime_error("randomInt expects exactly 2 arguments (min, max)");
            }

            int64_t min = 0;
            int64_t max = 0;

            if (IS_INT(args[0]))
                min = AS_INT(args[0]);
            else if (IS_DOUBLE(args[0]))
                min = static_cast<int64_t>(AS_DOUBLE(args[0]));
            else
                throw std::runtime_error("randomInt expects numbers");

            if (IS_INT(args[1]))
                max = AS_INT(args[1]);
            else if (IS_DOUBLE(args[1]))
                max = static_cast<int64_t>(AS_DOUBLE(args[1]));
            else
                throw std::runtime_error("randomInt expects numbers");

            if (min > max)
            {
                throw std::runtime_error("randomInt: min must be less than or equal to max");
            }

            auto& rng = RandomNumberGenerator::generator();
            int randomInt = rng->generateNumber();

            // Scale to range [min, max] inclusive
            int64_t range = max - min + 1;
            int64_t result = min + (randomInt % range);

            return makeIntValue(result);
        }

        // Set random seed for reproducible results
        static Value nativeRandomSeed(VM*, int argCount, Value* args)
        {
            if (argCount != 1)
            {
                throw std::runtime_error("randomSeed expects exactly 1 argument");
            }

            unsigned int seed = 0;

            if (IS_INT(args[0]))
                seed = static_cast<unsigned int>(AS_INT(args[0]));
            else if (IS_DOUBLE(args[0]))
                seed = static_cast<unsigned int>(AS_DOUBLE(args[0]));
            else
                throw std::runtime_error("randomSeed expects a number");

            auto& rng = RandomNumberGenerator::generator();
            rng->setSeed(seed);

            return makeIntValue(0);
        }
    };
}
