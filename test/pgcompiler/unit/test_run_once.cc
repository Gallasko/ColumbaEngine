#include "gtest/gtest.h"

#include "Compiler/vm.h"
#include "Compiler/ast/pass/run_once.h"
#include "ECS/entitysystem.h"

#include "Interpreter/lexer.h"
#include "Interpreter/parser.h"

#include <atomic>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

namespace pg
{
    namespace test
    {
        namespace
        {
            // The top-level statements of a source after the pass, by what they are
            std::vector<StatementPtr> runOnce(const std::string& source)
            {
                Lexer lexer;
                lexer.readFromText(source);

                Parser parser(lexer.getTokens());
                auto statements = parser.parse();

                EXPECT_FALSE(parser.hasError()) << "test source failed to parse";

                RunOncePass pass;
                pass.runPass(nullptr, statements);

                std::vector<StatementPtr> out;

                while (not statements.empty())
                {
                    out.push_back(statements.front());
                    statements.pop();
                }

                return out;
            }

            std::map<std::string, std::shared_ptr<FunctionStatement>> functionsOf(const std::vector<StatementPtr>& statements)
            {
                std::map<std::string, std::shared_ptr<FunctionStatement>> out;

                for (const auto& s : statements)
                {
                    if (s->kind() == AstKind::FunctionStatement)
                    {
                        auto f = std::static_pointer_cast<FunctionStatement>(s);
                        out[f->name.text] = f;
                    }
                }

                return out;
            }

            std::shared_ptr<VariableStatement> variable(const std::vector<StatementPtr>& statements, const std::string& name)
            {
                for (const auto& s : statements)
                {
                    if (s->kind() == AstKind::VariableStatement and std::static_pointer_cast<VariableStatement>(s)->name.text == name)
                        return std::static_pointer_cast<VariableStatement>(s);
                }

                return nullptr;
            }

            std::atomic<uint64_t> counter{0};

            // The script compiled and prepared as the game does it (the passes of an EntitySystem's VM)
            struct Prepared
            {
                EntitySystem ecs;
                VM vm;
                ObjFunction* fn = nullptr;
                std::vector<char> bytes;

                explicit Prepared(const std::string& source)
                {
                    ecs.setVMOptimizationLevel(VmOptimizationLevel::O3);
                    ecs.setVMFrontEnd(ScriptFrontEnd::Ast);

                    namespace fs = std::filesystem;
                    const fs::path tmp = fs::temp_directory_path() / ("pg_run_once_" + std::to_string(counter.fetch_add(1)) + ".pgc");

                    {
                        VM compiler;
                        ecs.setupVm(compiler);
                        EXPECT_EQ(compiler.interpretFromText(source, true, tmp.string()), InterpretResult::OK);
                    }

                    std::ifstream in(tmp.string(), std::ios::binary);
                    bytes.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
                    in.close();

                    std::error_code ec;
                    fs::remove(tmp, ec);

                    ecs.setupVm(vm);
                    fn = vm.prepareCachedFunction(bytes, "runonce");
                    EXPECT_NE(fn, nullptr);
                }

                int64_t run(const std::string& global)
                {
                    EXPECT_EQ(vm.runPreparedFunction(fn, 0, "runonce"), InterpretResult::OK);

                    const Value value = vm.findGlobal(global);
                    EXPECT_TRUE(IS_INT(value)) << global;

                    return IS_INT(value) ? AS_INT(value) : INT64_MIN;
                }
            };
        }

        TEST(RunOncePass, KeepsAFunctionDefinedOnceAndNeverWritten)
        {
            auto statements = runOnce(
                "fun once(x) \n{\n    return x\n}\n"
                "fun twice(x) \n{\n    return x\n}\n"
                "fun twice(x) \n{\n    return x + 1\n}\n"
                "fun written(x) \n{\n    return x\n}\n"
                "written = 3\n"
                "fun shadowed(x) \n{\n    return x\n}\n"
                "var shadowed = 1\n");

            auto functions = functionsOf(statements);

            EXPECT_TRUE(functions["once"]->keptOnce);
            EXPECT_FALSE(functions["twice"]->keptOnce);
            EXPECT_FALSE(functions["written"]->keptOnce);
            EXPECT_FALSE(functions["shadowed"]->keptOnce);
        }

        TEST(RunOncePass, JoinsTheConstantFillsOfAConstToIt)
        {
            auto statements = runOnce(
                "const names = {a: \"A\"}\n"
                "names[\"b\"] = \"B\"\n"
                "names.c = 3\n"
                "fun f(x) \n{\n    return x\n}\n"
                "names[\"d\"] = none\n"
                "var x = 1\n"
                "names[\"e\"] = 5\n");

            auto names = variable(statements, "names");

            ASSERT_NE(names, nullptr);
            EXPECT_EQ(names->keptFills.size(), 3u); // b, c, d: e comes after a statement that may read it

            // const, f, var x, the fill of e
            EXPECT_EQ(statements.size(), 4u);
        }

        TEST(RunOncePass, JoinsNoFillPastAConstThatMayReadIt)
        {
            auto statements = runOnce(
                "const names = {a: \"A\"}\n"
                "const plain = {b: 2}\n"
                "names[\"c\"] = 3\n"
                "const derived = names\n"
                "names[\"d\"] = 4\n"
                "derived[\"e\"] = 5\n");

            auto names = variable(statements, "names");
            auto derived = variable(statements, "derived");

            ASSERT_NE(names, nullptr);
            ASSERT_NE(derived, nullptr);

            // c joins past a const of constants; d does not join past one that reads names, and as
            // a statement of its own it ends the fills of derived too
            EXPECT_EQ(names->keptFills.size(), 1u);
            EXPECT_EQ(derived->keptFills.size(), 0u);
            EXPECT_EQ(statements.size(), 5u);
        }

        TEST(RunOncePass, KeptCodeAnswersTheSameOnEveryRun)
        {
            Prepared script(
                "const names = {a: 1}\n"
                "names[\"b\"] = 20\n"
                "fun twice(x) \n{\n    return x * 2\n}\n"
                "var answer = twice(names.b) + names.a\n");

            for (int i = 0; i < 3; ++i)
                EXPECT_EQ(script.run("answer"), 41) << "run " << i;

            // A script prepared again on the VM makes them anew
            script.vm.cleanupFunction(script.fn);
            script.fn = script.vm.prepareCachedFunction(script.bytes, "runonce");

            EXPECT_EQ(script.run("answer"), 41);
        }

        TEST(RunOncePass, AFunctionWrittenWhileRunningIsMadeAgainEachRun)
        {
            // f is written after it is called: each run calls the f its definition makes
            Prepared script(
                "fun f() \n{\n    return 1\n}\n"
                "var r = f()\n"
                "f = fun() \n{\n    return 2\n}\n");

            for (int i = 0; i < 3; ++i)
                EXPECT_EQ(script.run("r"), 1) << "run " << i;
        }
    }
}
