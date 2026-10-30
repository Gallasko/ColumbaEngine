#include "gtest/gtest.h"

#include "Compiler/vm.h"
#include "Compiler/ast/pass/run_once.h"
#include "ECS/entitysystem.h"

#include "Interpreter/lexer.h"
#include "Interpreter/parser.h"

#include <algorithm>
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
            std::vector<StatementPtr> runOnce(const std::string& source, bool wholeProgram = false)
            {
                Lexer lexer;
                lexer.readFromText(source);

                Parser parser(lexer.getTokens());
                auto statements = parser.parse();

                EXPECT_FALSE(parser.hasError()) << "test source failed to parse";

                RunOncePass pass(wholeProgram);
                pass.runPass(nullptr, statements);

                std::vector<StatementPtr> out;

                while (not statements.empty())
                {
                    out.push_back(statements.front());
                    statements.pop();
                }

                return out;
            }

            // A block at the end of a source leaves the parser's empty statement after it
            size_t statementCount(const std::vector<StatementPtr>& statements)
            {
                return std::count_if(statements.begin(), statements.end(), [](const StatementPtr& s) { return s != nullptr; });
            }

            std::map<std::string, std::shared_ptr<FunctionStatement>> functionsOf(const std::vector<StatementPtr>& statements)
            {
                std::map<std::string, std::shared_ptr<FunctionStatement>> out;

                for (const auto& s : statements)
                {
                    if (s and s->kind() == AstKind::FunctionStatement)
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
                    if (s and s->kind() == AstKind::VariableStatement and std::static_pointer_cast<VariableStatement>(s)->name.text == name)
                        return std::static_pointer_cast<VariableStatement>(s);
                }

                return nullptr;
            }

            std::atomic<uint64_t> counter{0};

            std::queue<StatementPtr> parsedQueue(const std::string& source)
            {
                Lexer lexer;
                lexer.readFromText(source);

                Parser parser(lexer.getTokens());
                auto statements = parser.parse();

                EXPECT_FALSE(parser.hasError()) << "test source failed to parse";

                return statements;
            }

            // A script importing one module, passed as a compile does: the script at depth 1, then the
            // module from a parse of its own at depth 2. What comes out is the module's statements
            std::vector<StatementPtr> moduleAfterThePass(const std::string& module)
            {
                namespace fs = std::filesystem;
                const fs::path dir = fs::temp_directory_path() / ("pg_run_once_module_" + std::to_string(counter.fetch_add(1)));
                fs::create_directories(dir);

                const std::string script = "import joined\nvar out = 1\n";

                std::ofstream((dir / "main.pg").string()) << script;
                std::ofstream((dir / "joined.pg").string()) << module;

                VM vm;
                RunOncePass pass(true);
                vm.currentFileName = (dir / "main.pg").string();

                auto statements = parsedQueue(script);
                vm.astCompileDepth = 1;
                pass.runPass(&vm, statements);

                auto own = parsedQueue(module);
                vm.astCompileDepth = 2;
                pass.runPass(&vm, own);
                vm.astCompileDepth = 0;

                std::error_code ec;
                fs::remove_all(dir, ec);

                std::vector<StatementPtr> out;

                while (not own.empty())
                {
                    out.push_back(own.front());
                    own.pop();
                }

                return out;
            }

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

        TEST(RunOncePass, InfersNoConstWhenTheAnalysisIsOff)
{
    auto statements = runOnce("var x = 1\nfun f() \n{\n    return x\n}\n");
    EXPECT_FALSE(variable(statements, "x")->isConst);
}

TEST(RunOncePass, InfersAConstWhenTheAnalysisIsOn)
{
    auto statements = runOnce("var x = 1\nfun f() \n{\n    return x\n}\n", true);
    EXPECT_TRUE(variable(statements, "x")->isConst);
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

        TEST(RunOncePass, ALocalOfTheSameNameWritesNoFunction)
        {
            // A local (a variable of a function or a block, a parameter, a loop's variable) of a
            // function's name is not the function: only a global written makes it not kept
            auto statements = runOnce(
                "fun has(x) \n{\n    return x\n}\n"
                "fun floor(x) \n{\n    return x\n}\n"
                "fun count(x) \n{\n    return x\n}\n"
                "fun first(x) \n{\n    return x\n}\n"
                "fun user(floor) \n{\n    var has = 0\n    has = 1\n    for (var count = 0; count < 2; count++)\n    {\n        has = has + count\n    }\n    return has + floor\n}\n"
                "fun writer() \n{\n    first = 2\n    return 0\n}\n");

            auto functions = functionsOf(statements);

            EXPECT_TRUE(functions["has"]->keptOnce);
            EXPECT_TRUE(functions["floor"]->keptOnce);
            EXPECT_TRUE(functions["count"]->keptOnce);
            EXPECT_FALSE(functions["first"]->keptOnce); // written as a global inside writer
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

        TEST(RunOncePass, AFillJoinsTheLaterOfTwoConstsOfTheSameName)
        {
            auto statements = runOnce(
                "const t = {a: 1}\n"
                "const t = {b: 2}\n"
                "t[\"c\"] = 3\n");

            std::vector<std::shared_ptr<VariableStatement>> consts;

            for (const auto& s : statements)
            {
                if (auto v = std::dynamic_pointer_cast<VariableStatement>(s))
                    consts.push_back(v);
            }

            ASSERT_EQ(consts.size(), 2u);
            EXPECT_EQ(consts[0]->keptFills.size(), 0u);
            EXPECT_EQ(consts[1]->keptFills.size(), 1u);
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

        TEST(RunOncePass, TheLoopFillingAConstJoinsIt)
        {
            const std::string source =
                "const places = [{id: \"a\"}, {id: \"b\"}]\n"
                "const byId = {}\n"
                "for (var i = 0; i < len(places); i++)\n"
                "{\n"
                "    byId[places[i].id] = places[i]\n"
                "}\n";

            auto on = runOnce(source, true);
            ASSERT_NE(variable(on, "byId"), nullptr);
            EXPECT_EQ(variable(on, "byId")->keptFills.size(), 1u);
            EXPECT_EQ(statementCount(on), 2u);

            auto off = runOnce(source);
            ASSERT_NE(variable(off, "byId"), nullptr);
            EXPECT_EQ(variable(off, "byId")->keptFills.size(), 0u);
            EXPECT_EQ(statementCount(off), 3u);
        }

        TEST(RunOncePass, AModuleJoinsAsTheScriptDoes)
        {
            const std::string filling =
                "const places = [{id: \"a\"}, {id: \"b\"}]\n"
                "const byId = {}\n"
                "for (var i = 0; i < len(places); i++)\n"
                "{\n"
                "    byId[places[i].id] = places[i]\n"
                "}\n";

            auto module = moduleAfterThePass(filling);
            ASSERT_NE(variable(module, "byId"), nullptr);
            EXPECT_EQ(variable(module, "byId")->keptFills.size(), 1u);
            EXPECT_EQ(statementCount(module), 2u);

            const std::string throughACopy =
                "const rows = [{x: 0}]\n"
                "var age = character.age\n"
                "const seen = {n: 0}\n"
                "for (var i = 0; i < 1; i++)\n"
                "{\n"
                "    var c = copy(rows)\n"
                "    c[0].x = 7\n"
                "    seen.n = 1\n"
                "}\n";

            // The loop writes rows through the copy, in a module as in a script
            EXPECT_EQ(variable(runOnce(throughACopy, true), "seen")->keptFills.size(), 0u);

            auto wrong = moduleAfterThePass(throughACopy);
            ASSERT_NE(variable(wrong, "seen"), nullptr);
            EXPECT_EQ(variable(wrong, "seen")->keptFills.size(), 0u);
        }

        TEST(RunOncePass, AProgramWithAClassIsNotAnalyzed)
        {
            auto statements = runOnce("class Box\n{\n    size()\n    {\n        return 1\n    }\n}\nvar x = 1\n", true);
            EXPECT_FALSE(variable(statements, "x")->isConst);
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
