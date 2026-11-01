#include "gtest/gtest.h"

#include "Compiler/vm.h"
#include "Compiler/value_nanbox.h"
#include "Helpers/stringmodule.h"
#include "Helpers/algorithmmodule.h"

#include <string>

namespace pg
{
    namespace test
    {
        namespace
        {
            const std::string kHeader = "import \"algorithm\"\n"
                                        "var maxInt = toInt(\"70368744177663\")\n"
                                        "var minInt = toInt(\"-70368744177664\")\n";

            // Run a script on a fresh vm and return what it printed with __dprint
            std::string runIntScript(const std::string& source, InterpretResult& result, bool optimize)
            {
                VM vm;

                vm.addNativeModule("string", StringModule());
                vm.addNativeModule("algorithm", AlgorithmModule());

                if (not optimize)
                    vm.disableBytecodeOptimization();

                result = vm.interpretFromText(kHeader + source);

                return vm.testOutput;
            }

            // A value past the range stops the script: nothing after it may run, optimized or not
            void expectOverflow(const std::string& source)
            {
                for (bool optimize : {true, false})
                {
                    InterpretResult result = InterpretResult::OK;
                    std::string output = runIntScript(source + "\n__dprint(\"unreachable\")\n", result, optimize);

                    EXPECT_EQ(result, InterpretResult::RUNTIME_ERROR) << "optimize " << optimize << ":\n" << source;
                    EXPECT_EQ(output, "") << "optimize " << optimize << ":\n" << source;
                }
            }

            void expectOutput(const std::string& source, const std::string& expected)
            {
                for (bool optimize : {true, false})
                {
                    InterpretResult result = InterpretResult::COMPILE_ERROR;
                    std::string output = runIntScript(source, result, optimize);

                    EXPECT_EQ(result, InterpretResult::OK) << "optimize " << optimize << ":\n" << source;
                    EXPECT_EQ(output, expected) << "optimize " << optimize << ":\n" << source;
                }
            }

            void expectCompileError(const std::string& source)
            {
                for (bool optimize : {true, false})
                {
                    InterpretResult result = InterpretResult::OK;
                    runIntScript(source, result, optimize);

                    EXPECT_EQ(result, InterpretResult::COMPILE_ERROR) << "optimize " << optimize << ":\n" << source;
                }
            }
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(int_range_test, fits_int_value_stops_at_the_47_bit_bounds)
        {
            EXPECT_TRUE(fitsIntValue(0));
            EXPECT_TRUE(fitsIntValue(INT_VALUE_MAX));
            EXPECT_TRUE(fitsIntValue(INT_VALUE_MIN));
            EXPECT_FALSE(fitsIntValue(INT_VALUE_MAX + 1));
            EXPECT_FALSE(fitsIntValue(INT_VALUE_MIN - 1));
            EXPECT_FALSE(fitsIntValue(INT64_MAX));
            EXPECT_FALSE(fitsIntValue(INT64_MIN));
        }

        TEST(int_range_test, multiply_refuses_a_product_past_64_bits)
        {
            int64_t out = 0;

            EXPECT_TRUE(multiplyFitsInt(INT_VALUE_MAX, 1, out));
            EXPECT_EQ(out, INT_VALUE_MAX);

            EXPECT_FALSE(multiplyFitsInt(INT_VALUE_MAX, 2, out));
            EXPECT_FALSE(multiplyFitsInt(INT_VALUE_MAX, INT_VALUE_MAX, out));
            EXPECT_FALSE(multiplyFitsInt(INT_VALUE_MIN, INT_VALUE_MIN, out));
        }

        TEST(int_range_test, values_at_the_bounds_stay_ints)
        {
            expectOutput("__dprint(maxInt)\n__dprint(minInt)\n__dprint(minInt + maxInt)\n",
                         "70368744177663\n-70368744177664\n-1\n");

            expectOutput("__dprint(maxInt - 1 + 1)\n__dprint(toInt(\"8388607\") * 8388608)\n__dprint(minInt / 2)\n",
                         "70368744177663\n70368735789056\n-35184372088832\n");
        }

        TEST(int_range_test, to_int_refuses_a_string_past_the_range)
        {
            expectOverflow("var x = toInt(\"70368744177664\")");
            expectOverflow("var x = toInt(\"-70368744177665\")");
            expectOverflow("var x = toInt(\"140737488355332\")");

            // Past 64 bits too, the parse itself fails there
            expectOverflow("var x = toInt(\"99999999999999999999\")");
        }

        TEST(int_range_test, to_int_refuses_a_float_past_the_range)
        {
            expectOverflow("var x = toInt(toFloat(\"70368744177664\"))");
            expectOverflow("var x = toInt(toFloat(\"1e20\"))");

            expectOutput("__dprint(toInt(toFloat(\"70368744177663\")))\n", "70368744177663\n");
        }

        TEST(int_range_test, arithmetic_refuses_a_result_past_the_range)
        {
            expectOverflow("var x = maxInt + 1");
            expectOverflow("var x = minInt - 1");
            expectOverflow("var x = maxInt * 2");
            expectOverflow("var x = toInt(\"8388608\") * 8388608");
            expectOverflow("var x = minInt / -1");
            expectOverflow("var x = -minInt");

            // Past 64 bits, a wrapped product could land back in the range
            expectOverflow("var x = maxInt * maxInt");
            expectOverflow("var x = minInt * minInt");
        }

        TEST(int_range_test, increments_refuse_a_result_past_the_range)
        {
            expectOverflow("var x = maxInt\nx++");
            expectOverflow("var x = minInt\nx--");
            expectOverflow("var x = maxInt\n++x");
            expectOverflow("var x = maxInt\nx += 1");

            expectOverflow("var t = {a: maxInt}\nt.a += 1");
            expectOverflow("var t = [maxInt]\nt[0] += 1");
        }

        TEST(int_range_test, locals_refuse_a_result_past_the_range)
        {
            // Locals run the fused handlers, not the ones the globals above run
            const std::string call = "\n}\nf()";

            expectOverflow("fun f()\n{\nvar x = maxInt\nx++" + call);
            expectOverflow("fun f()\n{\nvar x = minInt\nx--" + call);
            expectOverflow("fun f()\n{\nvar x = maxInt\nx += 1" + call);
            expectOverflow("fun f()\n{\nvar x = maxInt\nx = x + 1" + call);
            expectOverflow("fun f()\n{\nvar x = maxInt\nvar y = 1\nx = y + x" + call);
            expectOverflow("fun f()\n{\nvar x = maxInt\nx = x * 3" + call);
            expectOverflow("fun f()\n{\nvar x = minInt\nx = x - 1" + call);
        }

        TEST(int_range_test, a_float_operand_is_left_alone)
        {
            expectOutput("__dprint(maxInt + 1.5 > maxInt)\n__dprint(maxInt * 2.0 > maxInt)\n", "true\ntrue\n");
        }

        TEST(int_range_test, a_loop_evaluated_at_compile_time_still_refuses)
        {
            expectOverflow("var x = 1\nfor (var i = 0; i < 60; i++)\n{\nx = x * 2\n}");

            // The result is small again by the end, the steps on the way are not
            expectOverflow("var x = 2000000000\nfor (var i = 0; i < 1; i++)\n{\nx = x * x / x\n}");

            // Past 64 bits inside the evaluator
            expectOverflow("var x = 2000000000\nfor (var i = 0; i < 1; i++)\n{\nx = x * x * x\n}");
        }

        TEST(int_range_test, a_loop_inside_the_range_keeps_its_value)
        {
            expectOutput("var x = 1\nfor (var i = 0; i < 40; i++)\n{\nx = x * 2\n}\n__dprint(x)\n",
                         "1099511627776\n");
        }

        TEST(int_range_test, a_literal_past_32_bits_is_a_compile_error)
        {
            expectOutput("__dprint(2147483647)\n", "2147483647\n");

            expectCompileError("var x = 2147483648\n");
            expectCompileError("var x = 99999999999999999999\n");
        }
    }
}
