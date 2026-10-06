#include "gtest/gtest.h"

#include "Compiler/vm.h"
#include "Compiler/value_nanbox.h"
#include "Compiler/chunk.h"
#include "Compiler/chunk_serializer.h"
#include "Helpers/stringmodule.h"
#include "Helpers/algorithmmodule.h"

#include <limits>
#include <sstream>
#include <string>

namespace pg
{
    namespace test
    {
        namespace
        {
            // These scripts get none from a native, the literal has its own script test (none_literal.pg)
            void registerNoneNative(VM& vm)
            {
                vm.registerNative("getNone", [](VM*, int, Value*) -> Value {
                    return makeNoneValue();
                });
            }

            // Live refcount of a heap value, -1 if it is not refcounted
            int refCountOf(VM& vm, Value v)
            {
                if (not requiresRefCount(v))
                    return -1;

                const uint32_t index = GET_INDEX(v);

                auto& refCounts = vm.pools.getRefCountVector(v);

                if (index >= refCounts.size())
                    return -1;

                return static_cast<int>(refCounts[index]);
            }

            // Run a script on a fresh vm and return what it printed with __dprint
            std::string runNoneScript(const std::string& source, InterpretResult& result)
            {
                VM vm;

                vm.addNativeModule("string", StringModule());
                vm.addNativeModule("algorithm", AlgorithmModule());

                registerNoneNative(vm);

                result = vm.interpretFromText(source);

                return vm.testOutput;
            }
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(none_value_test, encoding)
        {
            Value none = makeNoneValue();

            EXPECT_EQ(none, NoneValue);
            EXPECT_EQ(none, 0xFFFF000000000000ULL);

            EXPECT_TRUE(IS_NONE(none));
            EXPECT_TRUE(IS_TAGGED(none));
            EXPECT_TRUE(IS_NEG_TAGGED(none));
            EXPECT_EQ(GET_TAG(none), TAG_NONE);

            EXPECT_STREQ(valueTypeName(none), "none");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(none_value_test, is_no_other_type)
        {
            Value none = makeNoneValue();

            EXPECT_FALSE(IS_DOUBLE(none));
            EXPECT_FALSE(IS_INT(none));
            EXPECT_FALSE(IS_BOOL(none));
            EXPECT_FALSE(IS_STRING(none));
            EXPECT_FALSE(IS_NAT_FUNC(none));
            EXPECT_FALSE(IS_CUSTOM_PTR(none));
            EXPECT_FALSE(IS_CLOSURE(none));
            EXPECT_FALSE(IS_FUNC(none));
            EXPECT_FALSE(IS_UPVALUE(none));
            EXPECT_FALSE(IS_CLASS(none));
            EXPECT_FALSE(IS_INSTANCE(none));
            EXPECT_FALSE(IS_BOUND_METHOD(none));
            EXPECT_FALSE(IS_VECTOR(none));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(none_value_test, other_values_are_not_none)
        {
            EXPECT_FALSE(IS_NONE(makeIntValue(0)));
            EXPECT_FALSE(IS_NONE(makeBoolValue(false)));
            EXPECT_FALSE(IS_NONE(makeDoubleValue(0.0)));
            EXPECT_FALSE(IS_NONE(makeSmallStringValue("", 0)));
            EXPECT_FALSE(IS_NONE(makeCustomPtrValue(0, 0)));
            EXPECT_FALSE(IS_NONE(makeNativeFuncValue(0)));
            EXPECT_FALSE(IS_NONE(makeStringValue(0)));

            // A default initialized Value is the double 0.0
            EXPECT_FALSE(IS_NONE(Value{0}));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(none_value_test, hardware_nan_is_not_none)
        {
            const double nan = std::numeric_limits<double>::quiet_NaN();

            Value positiveNan = makeDoubleValue(nan);
            Value negativeNan = makeDoubleValue(-nan);

            EXPECT_TRUE(IS_DOUBLE(positiveNan));
            EXPECT_TRUE(IS_DOUBLE(negativeNan));

            EXPECT_FALSE(IS_NONE(positiveNan));
            EXPECT_FALSE(IS_NONE(negativeNan));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(none_value_test, is_not_refcounted)
        {
            VM vm;

            Value none = makeNoneValue();

            EXPECT_FALSE(requiresRefCount(none));

            // Retain and release must leave an immediate untouched
            Value retained = vm.retainValue(none);

            vm.releaseAndDelete(retained);

            EXPECT_TRUE(IS_NONE(retained));
            EXPECT_TRUE(IS_NONE(vm.copyValue(none)));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(none_value_test, is_falsy)
        {
            EXPECT_FALSE(isValueTrue(makeNoneValue()));
            EXPECT_FALSE(isValueNumber(makeNoneValue()));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(undefined_value_test, encoding)
        {
            Value undefined = makeUndefinedValue();

            EXPECT_EQ(undefined, UndefinedValue);
            EXPECT_EQ(undefined, 0xFFFF800000000000ULL);

            EXPECT_TRUE(IS_UNDEFINED(undefined));
            EXPECT_TRUE(IS_TAGGED(undefined));
            EXPECT_TRUE(IS_NEG_TAGGED(undefined));
            EXPECT_EQ(GET_TAG(undefined), TAG_UNDEFINED);

            EXPECT_FALSE(requiresRefCount(undefined));

            EXPECT_STREQ(valueTypeName(undefined), "undefined");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(undefined_value_test, is_distinct_from_none)
        {
            EXPECT_NE(UndefinedValue, NoneValue);

            EXPECT_FALSE(IS_NONE(makeUndefinedValue()));
            EXPECT_FALSE(IS_UNDEFINED(makeNoneValue()));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(undefined_value_test, is_no_other_type)
        {
            Value undefined = makeUndefinedValue();

            EXPECT_FALSE(IS_DOUBLE(undefined));
            EXPECT_FALSE(IS_INT(undefined));
            EXPECT_FALSE(IS_BOOL(undefined));
            EXPECT_FALSE(IS_STRING(undefined));
            EXPECT_FALSE(IS_NAT_FUNC(undefined));
            EXPECT_FALSE(IS_CUSTOM_PTR(undefined));

            EXPECT_FALSE(IS_UNDEFINED(makeIntValue(0)));
            EXPECT_FALSE(IS_UNDEFINED(makeBoolValue(false)));
            EXPECT_FALSE(IS_UNDEFINED(makeDoubleValue(0.0)));
            EXPECT_FALSE(IS_UNDEFINED(makeDoubleValue(-std::numeric_limits<double>::quiet_NaN())));

            // A default initialized Value is the double 0.0
            EXPECT_FALSE(IS_UNDEFINED(Value{0}));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(undefined_value_test, declared_global_is_undefined)
        {
            VM vm;

            EXPECT_TRUE(IS_UNDEFINED(vm.findGlobal("neverDeclared")));

            // Decoding a script reserves the slot of a global before anything is assigned to it
            const uint32_t slot = vm.globalSlot("declared");

            EXPECT_TRUE(IS_UNDEFINED(vm.globalCells[slot]));
            EXPECT_TRUE(IS_UNDEFINED(vm.findGlobal("declared")));

            // Asking for the slot again must not create a second cell
            EXPECT_EQ(vm.globalSlot("declared"), slot);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(undefined_value_test, global_set_to_none_is_defined)
        {
            VM vm;

            vm.defineGlobal("empty", makeNoneValue());

            const Value value = vm.findGlobal("empty");

            EXPECT_TRUE(IS_NONE(value));
            EXPECT_FALSE(IS_UNDEFINED(value));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(undefined_value_test, redefined_global_releases_the_old_value)
        {
            VM vm;

            // Keep a reference of our own so the count stays readable after the global lets go
            Value text = vm.createString("a string long enough to live in the pool");

            vm.defineGlobal("text", vm.retainValue(text));

            EXPECT_EQ(refCountOf(vm, text), 2);

            vm.defineGlobal("text", makeIntValue(3));

            EXPECT_TRUE(IS_INT(vm.findGlobal("text")));
            EXPECT_EQ(refCountOf(vm, text), 1);

            vm.releaseAndDelete(text);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(none_value_test, equality)
        {
            VM vm;

            Value none = makeNoneValue();

            EXPECT_TRUE(AS_BOOL(vm.equalsValues(none, makeNoneValue())));
            EXPECT_FALSE(AS_BOOL(vm.notEqualsValues(none, makeNoneValue())));

            // None only equals itself, in particular it is neither 0 nor false
            EXPECT_FALSE(AS_BOOL(vm.equalsValues(none, makeIntValue(0))));
            EXPECT_FALSE(AS_BOOL(vm.equalsValues(makeIntValue(0), none)));
            EXPECT_FALSE(AS_BOOL(vm.equalsValues(none, makeDoubleValue(0.0))));
            EXPECT_FALSE(AS_BOOL(vm.equalsValues(none, makeBoolValue(false))));
            EXPECT_FALSE(AS_BOOL(vm.equalsValues(none, makeSmallStringValue("", 0))));

            EXPECT_TRUE(AS_BOOL(vm.notEqualsValues(none, makeIntValue(0))));
            EXPECT_TRUE(AS_BOOL(vm.notEqualsValues(none, makeBoolValue(false))));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(none_value_test, chunk_constant_round_trip)
        {
            VM vm;

            Chunk chunk;

            chunk.constants.push_back(makeIntValue(7));
            chunk.constants.push_back(makeNoneValue());
            chunk.constants.push_back(makeBoolValue(false));

            std::ostringstream out(std::ios::binary);

            ASSERT_TRUE(ChunkSerializer::serialize(chunk, out, &vm));

            VM otherVm;

            Chunk loaded;

            std::istringstream in(out.str(), std::ios::binary);

            ASSERT_TRUE(ChunkSerializer::deserialize(loaded, in, &otherVm));

            ASSERT_EQ(loaded.constants.size(), 3u);

            EXPECT_TRUE(IS_INT(loaded.constants[0]));
            EXPECT_TRUE(IS_NONE(loaded.constants[1]));
            EXPECT_TRUE(IS_BOOL(loaded.constants[2]));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(none_value_test, script_print_and_compare)
        {
            const std::string source =
                "var value = getNone()\n"
                "__dprint(value)\n"
                "__dprint(value == getNone())\n"
                "__dprint(value == 0)\n"
                "__dprint(value == false)\n"
                "__dprint(value != 0)\n";

            InterpretResult result;

            std::string output = runNoneScript(source, result);

            EXPECT_EQ(result, InterpretResult::OK);
            EXPECT_EQ(output, "none\ntrue\nfalse\nfalse\ntrue\n");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(none_value_test, script_condition_is_false)
        {
            const std::string source =
                "var value = getNone()\n"
                "if (value)\n"
                "{\n"
                "    __dprint(\"truthy\")\n"
                "}\n"
                "else\n"
                "{\n"
                "    __dprint(\"falsy\")\n"
                "}\n";

            InterpretResult result;

            std::string output = runNoneScript(source, result);

            EXPECT_EQ(result, InterpretResult::OK);
            EXPECT_EQ(output, "falsy\n");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(none_value_test, script_goes_through_locals_and_calls)
        {
            const std::string source =
                "fun identity(x)\n"
                "{\n"
                "    var local = x\n"
                "    return local\n"
                "}\n"
                "__dprint(identity(getNone()))\n"
                "__dprint(identity(getNone()) == getNone())\n";

            InterpretResult result;

            std::string output = runNoneScript(source, result);

            EXPECT_EQ(result, InterpretResult::OK);
            EXPECT_EQ(output, "none\ntrue\n");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(none_value_test, script_stored_in_containers)
        {
            const std::string source =
                "var table = {a: getNone(), b: 3}\n"
                "var list = [getNone(), 7]\n"
                "__dprint(table.a)\n"
                "__dprint(table.b)\n"
                "__dprint(list[0])\n"
                "__dprint(list[1])\n";

            InterpretResult result;

            std::string output = runNoneScript(source, result);

            EXPECT_EQ(result, InterpretResult::OK);
            EXPECT_EQ(output, "none\n3\nnone\n7\n");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(none_value_test, script_type_of_and_to_string)
        {
            const std::string source =
                "import \"string\"\n"
                "import \"algorithm\"\n"
                "__dprint(typeOf(getNone()))\n"
                "__dprint(toString(getNone()))\n"
                "__dprint(\"value: \" + toString(getNone()))\n";

            InterpretResult result;

            std::string output = runNoneScript(source, result);

            EXPECT_EQ(result, InterpretResult::OK);
            EXPECT_EQ(output, "none\nnone\nvalue: none\n");
        }
    }
}
