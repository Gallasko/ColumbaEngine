#include "gtest/gtest.h"

#include "Compiler/vm.h"
#include "ECS/entitysystem.h"
#include "Helpers/jsonmodule.h"

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace pg
{
    namespace test
    {
        // The json module: JSON text to PgScript values and back, keys in their order
        class JsonModuleTest : public ::testing::Test
        {
        protected:
            std::filesystem::path dir;
            std::string output;

            void SetUp() override
            {
                dir = std::filesystem::temp_directory_path() / ("pg_json_" + std::to_string(::testing::UnitTest::GetInstance()->random_seed()) + "_" + ::testing::UnitTest::GetInstance()->current_test_info()->name());
                std::filesystem::remove_all(dir);
                std::filesystem::create_directories(dir);
            }

            void TearDown() override { std::filesystem::remove_all(dir); }

            std::string path(const std::string& name) const { return (dir / name).string(); }

            void writeBytes(const std::string& name, const std::string& bytes)
            {
                std::ofstream out(dir / name, std::ios::binary | std::ios::trunc);
                out << bytes;
            }

            // The script after `import "json" as json`, run in a VM given only ecs.setupVm
            InterpretResult run(const std::string& body)
            {
                EntitySystem ecs;
                ecs.setVMOptimizationLevel(VmOptimizationLevel::O3);
                ecs.setVMFrontEnd(ScriptFrontEnd::Ast);

                VM vm;
                ecs.setupVm(vm);

                const InterpretResult result = vm.interpretFromText("import \"json\" as json\nimport \"file\"\nimport \"algorithm\"\nimport \"string\"\n" + body);
                output = vm.testOutput;
                return result;
            }
        };

        TEST_F(JsonModuleTest, ParsesEachType)
        {
            ASSERT_EQ(run("var v = json.parse(\"{\\\"a\\\":1,\\\"b\\\":2.5,\\\"c\\\":\\\"x\\\",\\\"d\\\":true,\\\"e\\\":null,\\\"f\\\":[1,2]}\")\n"
                          "__dprint(typeOf(v.a))\n__dprint(typeOf(v.b))\n__dprint(typeOf(v.c))\n__dprint(typeOf(v.d))\n__dprint(typeOf(v.e))\n__dprint(typeOf(v.f))\n"
                          "__dprint(v.a)\n__dprint(v.c)\n__dprint(v.f[1])\n"), InterpretResult::OK);
            EXPECT_EQ(output, "int\ndouble\nstring\nbool\nnone\narray\n1\nx\n2\n");
        }

        TEST_F(JsonModuleTest, WholeDoubleStaysDouble)
        {
            ASSERT_EQ(run("__dprint(typeOf(json.parse(\"2.0\")))\n__dprint(typeOf(json.parse(\"1e3\")))\n__dprint(typeOf(json.parse(\"7\")))\n"), InterpretResult::OK);
            EXPECT_EQ(output, "double\ndouble\nint\n");
        }

        TEST_F(JsonModuleTest, BigIntegerIsDouble)
        {
            // PgScript ints run from -2^46 to 2^46 - 1 (INT_VALUE_MIN, INT_VALUE_MAX)
            ASSERT_EQ(run("__dprint(typeOf(json.parse(\"70368744177664\")))\n__dprint(typeOf(json.parse(\"70368744177663\")))\n__dprint(typeOf(json.parse(\"-70368744177664\")))\n__dprint(typeOf(json.parse(\"-70368744177665\")))\n"), InterpretResult::OK);
            EXPECT_EQ(output, "double\nint\nint\ndouble\n");
        }

        TEST_F(JsonModuleTest, KeyOrderKept)
        {
            ASSERT_EQ(run("__dprint(json.stringify(json.parse(\"{\\\"z\\\":1,\\\"a\\\":2,\\\"m\\\":3}\")))\n"), InterpretResult::OK);
            EXPECT_EQ(output, "{\"z\":1,\"a\":2,\"m\":3}\n");
        }

        TEST_F(JsonModuleTest, StringifiesValues)
        {
            ASSERT_EQ(run("__dprint(json.stringify([1, 2.0, \"x\", true, none, 2.5, -3]))\n"), InterpretResult::OK);
            EXPECT_EQ(output, "[1,2.0,\"x\",true,null,2.5,-3]\n");
        }

        TEST_F(JsonModuleTest, IntKeysAsStringsFirst)
        {
            ASSERT_EQ(run("var t = {b: 1}\nt[5] = 2\n__dprint(json.stringify(t))\n"), InterpretResult::OK);
            EXPECT_EQ(output, "{\"5\":2,\"b\":1}\n");
        }

        TEST_F(JsonModuleTest, IntAndStringKeyOfTheSameTextIsAnError)
        {
            // t[5] and t["5"] are two keys; JSON has one "5": refused rather than one value dropped
            EXPECT_EQ(run("var t = {}\nt[5] = 1\nt[\"5\"] = 2\njson.stringify(t)\n"), InterpretResult::RUNTIME_ERROR);
        }

        TEST_F(JsonModuleTest, Indent)
        {
            ASSERT_EQ(run("__dprint(json.stringify({a: [1]}, 2))\n"), InterpretResult::OK);
            EXPECT_EQ(output, "{\n  \"a\": [\n    1\n  ]\n}\n");
        }

        TEST_F(JsonModuleTest, RoundTripNested)
        {
            ASSERT_EQ(run("var v = {name: \"a\", list: [1, [2, 3], {deep: none}], n: 1.5}\n"
                          "var text = json.stringify(v)\n__dprint(text)\n__dprint(json.stringify(json.parse(text)))\n"), InterpretResult::OK);
            EXPECT_EQ(output, "{\"name\":\"a\",\"list\":[1,[2,3],{\"deep\":null}],\"n\":1.5}\n{\"name\":\"a\",\"list\":[1,[2,3],{\"deep\":null}],\"n\":1.5}\n");
        }

        TEST_F(JsonModuleTest, EmptyContainers)
        {
            ASSERT_EQ(run("__dprint(json.stringify({}))\n__dprint(json.stringify([]))\nvar e = json.parse(\"{}\")\n__dprint(typeOf(e))\n"), InterpretResult::OK);
            EXPECT_EQ(output, "{}\n[]\ntable\n");
        }

        TEST_F(JsonModuleTest, BadTextIsAnError)
        {
            EXPECT_EQ(run("json.parse(\"{\\n  \\\"a\\\": }\")\n"), InterpretResult::RUNTIME_ERROR);
        }

        TEST_F(JsonModuleTest, BadTextGivesLineAndColumn)
        {
            VM vm;
            Value args[] = { vm.createString(std::string("{\n  \"a\": }")) };

            try
            {
                JsonModule::nativeParse(&vm, 1, args);
                FAIL() << "no error";
            }
            catch (const std::runtime_error& e)
            {
                const std::string message = e.what();
                EXPECT_NE(message.find("json.parse:"), std::string::npos) << message;
                EXPECT_NE(message.find("line 2, column 8"), std::string::npos) << message;
            }
        }

        TEST_F(JsonModuleTest, SelfContainingIsAnError)
        {
            EXPECT_EQ(run("var t = {}\nt.me = t\njson.stringify(t)\n"), InterpretResult::RUNTIME_ERROR);
        }

        TEST_F(JsonModuleTest, FunctionIsAnError)
        {
            EXPECT_EQ(run("fun f()\n{\n    return 1\n}\njson.stringify({f: f})\n"), InterpretResult::RUNTIME_ERROR);
        }

        TEST_F(JsonModuleTest, DeepNestingIsAnErrorNotACrash)
        {
            writeBytes("deep.json", std::string(100000, '[') + std::string(100000, ']'));

            EXPECT_EQ(run("json.parse(readFile(\"" + path("deep.json") + "\"))\n"), InterpretResult::RUNTIME_ERROR);
        }

        TEST_F(JsonModuleTest, NestingUpToTheLimitIsFine)
        {
            writeBytes("ok.json", std::string(500, '[') + std::string(500, ']'));

            EXPECT_EQ(run("var v = json.parse(readFile(\"" + path("ok.json") + "\"))\n__dprint(typeOf(v))\n"), InterpretResult::OK);
            EXPECT_EQ(output, "array\n");
        }

        TEST_F(JsonModuleTest, InvalidUtf8IsReplaced)
        {
            writeBytes("bad.txt", std::string("a\xff" "b"));

            ASSERT_EQ(run("__dprint(json.stringify(readFile(\"" + path("bad.txt") + "\")))\n"), InterpretResult::OK);
            EXPECT_EQ(output, "\"a\xef\xbf\xbd" "b\"\n");
        }

        TEST_F(JsonModuleTest, StringsAreEscaped)
        {
            ASSERT_EQ(run("var t = json.parse(\"\\\"q\\\\\\\"\\\\n\\\"\")\n__dprint(strlen(t))\n__dprint(json.stringify(t))\n"), InterpretResult::OK);
            EXPECT_EQ(output, "3\n\"q\\\"\\n\"\n");
        }
    }
}
