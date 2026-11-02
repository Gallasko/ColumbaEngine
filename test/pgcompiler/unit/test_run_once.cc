#include "gtest/gtest.h"

#include "Compiler/vm.h"
#include "Compiler/ast/pass/program_consts.h"
#include "Compiler/ast/pass/program_view.h"
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
            // The top-level statements of a source after the passes, as an EntitySystem registers them:
            // with the whole-program analysis, ProgramConstsPass decides the fills and RunOncePass joins none
            std::vector<StatementPtr> runOnce(const std::string& source, bool wholeProgram = false)
            {
                Lexer lexer;
                lexer.readFromText(source);

                Parser parser(lexer.getTokens());
                auto statements = parser.parse();

                EXPECT_FALSE(parser.hasError()) << "test source failed to parse";

                ProgramConstsPass consts;

                if (wholeProgram)
                    consts.runPass(nullptr, statements);

                RunOncePass pass(not wholeProgram);
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
            std::vector<StatementPtr> moduleAfterThePass(const std::string& module, std::string compiled = "")
            {
                if (compiled.empty())
                    compiled = module;

                namespace fs = std::filesystem;
                const fs::path dir = fs::temp_directory_path() / ("pg_run_once_module_" + std::to_string(counter.fetch_add(1)));
                fs::remove_all(dir);
                fs::create_directories(dir);

                const std::string script = "import joined\nvar out = 1\n";

                std::ofstream((dir / "main.pg").string()) << script;
                std::ofstream((dir / "joined.pg").string()) << module;

                VM vm;
                ProgramConstsPass consts;
                RunOncePass pass(false);
                vm.currentFileName = (dir / "main.pg").string();

                auto statements = parsedQueue(script);
                vm.astCompileDepth = 1;
                consts.runPass(&vm, statements);
                pass.runPass(&vm, statements);

                auto own = parsedQueue(compiled);
                vm.astCompileDepth = 2;
                consts.runPass(&vm, own);
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

            // Files compiled through every pass, then run three times on one VM: the globals asked after
            // each run. `main.pg` is the script
            // `precompiled`: a file compiled to its .pgc beside it first, as a build leaves it, named
            // `compiledAs` (its name with a `c` when empty)
            std::vector<std::string> threeRuns(const std::map<std::string, std::string>& files, bool analysis, const std::vector<std::string>& globals,
                                               const std::string& precompiled = "", const std::string& compiledAs = "")
            {
                namespace fs = std::filesystem;
                const fs::path dir = fs::temp_directory_path() / ("pg_run_once_runs_" + std::to_string(counter.fetch_add(1)));
                fs::remove_all(dir);
                fs::create_directories(dir);

                for (const auto& [name, text] : files)
                    std::ofstream((dir / name).string()) << text;

                EntitySystem ecs;
                ecs.setVMOptimizationLevel(analysis ? VmOptimizationLevel::O3 : VmOptimizationLevel::O2);
                ecs.setVMFrontEnd(ScriptFrontEnd::Ast);

                const std::string compiled = (dir / "out.pgc").string();

                if (not precompiled.empty())
                {
                    EntitySystem plain;
                    plain.setVMOptimizationLevel(VmOptimizationLevel::O2);
                    plain.setVMFrontEnd(ScriptFrontEnd::Ast);

                    VM compiler;
                    plain.setupVm(compiler);
                    const std::string target = compiledAs.empty() ? precompiled + "c" : compiledAs;
                    EXPECT_EQ(compiler.interpretFromFile((dir / precompiled).string(), true, (dir / target).string()), InterpretResult::OK);
                }

                {
                    VM compiler;
                    ecs.setupVm(compiler);
                    EXPECT_EQ(compiler.interpretFromFile((dir / "main.pg").string(), true, compiled), InterpretResult::OK);
                }

                std::ifstream in(compiled, std::ios::binary);
                const std::vector<char> bytes{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
                in.close();

                VM vm;
                ecs.setupVm(vm);
                ObjFunction* fn = vm.prepareCachedFunction(bytes, "runs");
                EXPECT_NE(fn, nullptr);

                std::vector<std::string> out;

                for (int run = 0; run < 3; ++run)
                {
                    if (vm.runPreparedFunction(fn, 0, "runs") != InterpretResult::OK)
                    {
                        out.push_back("failed");
                        continue;
                    }

                    std::string line;

                    for (const auto& g : globals)
                    {
                        const Value value = vm.findGlobal(g);
                        line += g + "=" + (IS_INT(value) ? std::to_string(AS_INT(value)) : "?") + " ";
                    }

                    out.push_back(line);
                }

                std::error_code ec;
                fs::remove_all(dir, ec);

                return out;
            }

            std::vector<std::string> lines(std::initializer_list<const char*> l)
            {
                return std::vector<std::string>(l.begin(), l.end());
            }

            // The runs with the analysis, which must be those without it
            std::vector<std::string> sameBothWays(const std::map<std::string, std::string>& files, const std::vector<std::string>& globals, const std::string& precompiled = "",
                                                  const std::string& compiledAs = "")
            {
                const auto off = threeRuns(files, false, globals, precompiled, compiledAs);
                EXPECT_EQ(threeRuns(files, true, globals, precompiled, compiledAs), off);
                return off;
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

        TEST(ProgramConstsPass, AModuleThatDoesNotLineUpHasNothingApplied)
        {
            const std::string module = "var kept = 1\nconst t = {}\nt.a = kept\n";

            auto lined = moduleAfterThePass(module);
            EXPECT_TRUE(variable(lined, "kept")->isConst);
            EXPECT_EQ(variable(lined, "t")->keptFills.size(), 1u);

            // One statement more, then a kind that differs at an index
            auto longer = moduleAfterThePass(module, module + "var more = 2\n");
            EXPECT_FALSE(variable(longer, "kept")->isConst);
            EXPECT_EQ(variable(longer, "t")->keptFills.size(), 0u);

            auto other = moduleAfterThePass(module, "var kept = 1\nconst t = {}\nfor (var i = 0; i < 2; i++)\n{\n    t.a = kept\n}\n");
            EXPECT_FALSE(variable(other, "kept")->isConst);
            EXPECT_EQ(variable(other, "t")->keptFills.size(), 0u);
        }

        TEST(ProgramConstsPass, ALoopFoldedAwayInAModuleChangesNoDecision)
        {
            const std::string module =
                "const c = {n: 0}\n"
                "var gate = 0\n"
                "gate = 1\n"
                "c.n = c.n + 1\n"
                "var p = 0\n"
                "var q = 0\n"
                "const n = 2\n"
                "const t = {v: 0}\n"
                "for (var i = 0; i < n; i++)\n"
                "{\n"
                "    var k = i\n"
                "}\n"
                "t.v = c.n\n"
                "for (var i = 0; i < 2; i++)\n"
                "{\n"
                "    p = p + 1\n"
                "    q = q + 1\n"
                "}\n";

            const auto expected = lines({"out=1 ", "out=2 ", "out=3 "});

            EXPECT_EQ(threeRuns({{"main.pg", "import \"m\"\nvar out = 0\nout = t.v\n"}, {"m.pg", module}}, false, {"out"}), expected);
            EXPECT_EQ(threeRuns({{"main.pg", "import \"m\"\nvar out = 0\nout = t.v\n"}, {"m.pg", module}}, true, {"out"}), expected);
            EXPECT_EQ(threeRuns({{"main.pg", module + "var out = 0\nout = t.v\n"}}, true, {"out"}), expected);
        }

        TEST(ProgramConstsPass, AJoinIsTheSameForAllThatReadsTheConst)
        {
            const std::string module =
                "const count = {n: 0}\n"
                "count.n = count.n + 1\n"
                "var out = count.n\n"
                "for (var i = 0; i < 2; i++)\n"
                "{\n"
                "    var k = i\n"
                "}\n";

            const std::map<std::string, std::string> split = {{"main.pg", "import \"m\"\nvar live = 0\nlive = count.n\n"}, {"m.pg", module}};

            EXPECT_EQ(threeRuns(split, false, {"out", "live"}), lines({"out=1 live=1 ", "out=2 live=2 ", "out=3 live=3 "}));

            // Joined, it runs once: `out`, kept as a const, and `live`, read on every run, say the same
            EXPECT_EQ(threeRuns(split, true, {"out", "live"}), lines({"out=1 live=1 ", "out=1 live=1 ", "out=1 live=1 "}));
        }

        TEST(ProgramConstsPass, AJoinDoesNotReadAConstThatChanges)
        {
            const std::string source =
                "const a = {n: 0}\n"
                "const b = {m: 0}\n"
                "b.m = a.n\n"
                "var gate = 0\n"
                "gate = 1\n"
                "a.n = a.n + 1\n"
                "var out = 0\n"
                "out = b.m\n";

            const auto expected = lines({"out=0 ", "out=1 ", "out=2 "});

            EXPECT_EQ(threeRuns({{"main.pg", source}}, false, {"out"}), expected);
            EXPECT_EQ(threeRuns({{"main.pg", source}}, true, {"out"}), expected);
        }

        TEST(ProgramConstsPass, TheDefaultLevelHasTheAnalysisAndO2DoesNot)
        {
            // The fill joins the const at O3 and runs once; at O2 it runs on every run
            const auto twoRuns = [](bool setO2)
            {
                namespace fs = std::filesystem;
                const fs::path tmp = fs::temp_directory_path() / ("pg_run_once_" + std::to_string(counter.fetch_add(1)) + ".pgc");

                EntitySystem ecs;
                ecs.setVMFrontEnd(ScriptFrontEnd::Ast);

                if (setO2)
                    ecs.setVMOptimizationLevel(VmOptimizationLevel::O2);

                {
                    VM compiler;
                    ecs.setupVm(compiler);
                    EXPECT_EQ(compiler.interpretFromText("const count = {n: 0}\ncount.n = count.n + 1\nvar out = count.n\n", true, tmp.string()), InterpretResult::OK);
                }

                std::ifstream in(tmp, std::ios::binary);
                const std::vector<char> bytes{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
                in.close();
                fs::remove(tmp);

                VM vm;
                ecs.setupVm(vm);
                ObjFunction* fn = vm.prepareCachedFunction(bytes, "levels");
                EXPECT_NE(fn, nullptr);

                EXPECT_EQ(vm.runPreparedFunction(fn, 0, "levels"), InterpretResult::OK);
                EXPECT_EQ(vm.runPreparedFunction(fn, 0, "levels"), InterpretResult::OK);

                return AS_INT(vm.findGlobal("out"));
            };

            EXPECT_EQ(twoRuns(false), 1);
            EXPECT_EQ(twoRuns(true), 2);
        }

        TEST(ProgramConstsPass, NothingRunsBeforeAFunctionItCalls)
        {
            const std::string module =
                "const a = {n: 0}\n"
                "fun fill(t)\n"
                "{\n"
                "    t.n = t.n + 1\n"
                "}\n"
                "fill(a)\n"
                "var out = a.n\n";

            const auto expected = lines({"out=1 live=1 ", "out=2 live=2 ", "out=3 live=3 "});
            const std::map<std::string, std::string> split = {{"main.pg", "import \"m\"\nvar live = 0\nlive = a.n\n"}, {"m.pg", module}};
            const std::map<std::string, std::string> single = {{"main.pg", module + "var live = 0\nlive = a.n\n"}};

            EXPECT_EQ(threeRuns(split, false, {"out", "live"}), expected);
            EXPECT_EQ(threeRuns(split, true, {"out", "live"}), expected);
            EXPECT_EQ(threeRuns(single, true, {"out", "live"}), expected);
        }

        TEST(ProgramConstsPass, NothingJoinsBeforeWhatItCallsRunsInTheProgram)
        {
            const auto once = lines({"out=1 ", "out=1 ", "out=1 "});

            // In the file, after the const
            EXPECT_EQ(sameBothWays({{"main.pg", "const a = {n: 0}\na.n = later(a)\nfun later(t)\n{\n    return 1\n}\nvar out = a.n\n"}}, {"out"}), once);

            EXPECT_EQ(sameBothWays({{"main.pg",
                "fun get()\n{\n    return later\n}\n"
                "const a = {n: 0}\n"
                "a.n = 1\n"
                "const c = {v: 0}\n"
                "c.v = a.n + one()\n"
                "fun one()\n{\n    return 1\n}\n"
                "var out = c.v\n"}}, {"out"}), lines({"out=2 ", "out=2 ", "out=2 "}));

            // In the script, after the module that calls it
            EXPECT_EQ(sameBothWays({{"main.pg", "import \"m\"\nfun helper()\n{\n    return 1\n}\nvar out = t.n\n"},
                                    {"m.pg", "const t = {n: 0}\nt.n = helper()\n"}}, {"out"}), once);

            // In a module imported after the const
            EXPECT_EQ(sameBothWays({{"main.pg", "const t = {n: 0}\nt.n = helper()\nimport \"h\"\nvar out = t.n\n"},
                                    {"h.pg", "fun helper()\n{\n    return 1\n}\n"}}, {"out"}), once);

            // A value of the program after the const
            sameBothWays({{"main.pg", "const a = {n: 0}\na.n = base\nvar base = 5\nvar out = a.n\n"}}, {"out"});
        }

        TEST(ProgramConstsPass, JoinsDoNotReorderWrites)
        {
            EXPECT_EQ(sameBothWays({{"main.pg", "const a = {n: 0}\nconst b = {k: 7}\na.n = b.k\na.n = 5\nvar out = a.n\n"}}, {"out"}),
                      lines({"out=5 ", "out=5 ", "out=5 "}));

            sameBothWays({{"main.pg", "const a = {x: 1}\nconst b = {m: 0}\nb.m = a.x\na.x = 2\nvar out = b.m\n"}}, {"out"});
        }

        TEST(ProgramConstsPass, AVarFoundConstClosesARunAsAVarDoes)
        {
            const auto twice = lines({"out=2 ", "out=2 ", "out=2 "});

            EXPECT_EQ(sameBothWays({{"main.pg",
                "fun one()\n{\n    return 1\n}\n"
                "const t = {n: 0}\n"
                "var k = one()\n"
                "t.n = 1\n"
                "t.n = t.n + 1\n"
                "var out = 0\n"
                "out = t.n\n"}}, {"out"}), twice);

            EXPECT_EQ(sameBothWays({{"main.pg",
                "const t = {n: 0}\n"
                "var k = 5\n"
                "for (var i = 0; i < 3; i++)\n"
                "{\n"
                "    var z = i\n"
                "}\n"
                "t.n = 1\n"
                "t.n = t.n + 1\n"
                "var out = 0\n"
                "out = t.n\n"}}, {"out"}), twice);
        }

        TEST(ProgramConstsPass, AReturnAtTheTopLevelStopsTheAnalysis)
        {
            // It decides whether what follows runs: `out` is never made, either way
            sameBothWays({{"main.pg", "const a = {n: 1}\nif (a.n == 1)\n{\n    return 5\n}\nvar out = 2\n"}}, {"out"});
        }

        TEST(ProgramConstsPass, AModuleWithAPgcBesideItIsNotRead)
        {
            const auto expected = lines({"seen=1 live=1 ", "seen=2 live=2 ", "seen=3 live=3 "});

            EXPECT_EQ(sameBothWays({{"main.pg", "import \"m\"\nvar seen = count.n\nvar live = 0\nlive = count.n\n"},
                                    {"m.pg", "const count = {n: 0}\ncount.n = count.n + 1\n"}}, {"seen", "live"}, "m.pg"), expected);
        }

        TEST(ProgramConstsPass, AnImportBelowTheTopLevelStopsTheAnalysis)
        {
            sameBothWays({{"main.pg", "const t = {n: 0}\nvar seen = t.n\nif (1 == 1)\n{\n    import \"m\"\n}\nvar out = 0\nout = t.n\n"},
                          {"m.pg", "t.n = t.n + 1\n"}}, {"seen", "out"});

            for (const std::string source : {"if (1 == 1)\n{\n    import \"string\"\n}\n", "fun f()\n{\n    import \"string\"\n}\n",
                                             "while (false)\n{\n    import \"string\"\n}\n", "{\n    import \"string\"\n}\n"})
            {
                EXPECT_FALSE(ast::readProgramFiles(nullptr, parsedQueue(source)).complete) << source;
            }

            EXPECT_TRUE(ast::readProgramFiles(nullptr, parsedQueue("import \"string\"\nvar x = 1\n")).complete);
        }

        TEST(ProgramConstsPass, AJoinOfATableMadeFromAnotherStaysThrough)
        {
            const std::map<std::string, std::string> files = {
                {"main.pg", "import \"algorithm\"\nimport \"t\"\nvar out = byId[\"b\"].v\n"},
                {"t.pg",
                 "const places = [{id: \"a\", v: 1}, {id: \"b\", v: 2}]\n"
                 "const byId = {}\n"
                 "for (var i = 0; i < len(places); i++)\n"
                 "{\n"
                 "    byId[places[i].id] = places[i]\n"
                 "}\n"}};

            EXPECT_EQ(sameBothWays(files, {"out"}), lines({"out=2 ", "out=2 ", "out=2 "}));

            auto module = moduleAfterThePass(files.at("t.pg"));
            EXPECT_EQ(variable(module, "byId")->keptFills.size(), 1u);
        }

        TEST(RunOncePass, ConstantFillsJoinInAnyOrderAfterTheAnalysis)
        {
            // Too far back for the analysis's joins, no matter for constant entries of two tables
            auto statements = runOnce("const a = {}\nconst b = {}\na.x = 1\nb.x = 1\na.y = 2\nb.y = 2\nvar out = 1\n", true);
            EXPECT_EQ(variable(statements, "a")->keptFills.size(), 2u);
            EXPECT_EQ(variable(statements, "b")->keptFills.size(), 2u);
            EXPECT_EQ(statementCount(statements), 3u);

            // The stat names of the rules: two tables filled in alternation, every fill joins its own
            std::string names = "const statNames = {}\nconst statShort = {}\n";

            for (const std::string stat : {"vit", "str", "agi", "wis", "cha", "luck"})
                names += "statNames." + stat + " = \"" + stat + "\"\nstatShort." + stat + " = \"" + stat.substr(0, 1) + "\"\n";

            auto alternating = runOnce(names + "var out = 1\n", true);
            EXPECT_EQ(variable(alternating, "statNames")->keptFills.size(), 6u);
            EXPECT_EQ(variable(alternating, "statShort")->keptFills.size(), 6u);
            EXPECT_EQ(statementCount(alternating), 3u);

            // Not past a statement that may read the const it fills: a is then made on every run, and
            // what read it as made once may not join either
            auto past = runOnce("const a = {n: 0}\nconst b = {k: 7}\nb.k = a.n\na.n = 5\nvar out = 1\n", true);
            EXPECT_EQ(variable(past, "b")->keptFills.size(), 0u);
            EXPECT_EQ(variable(past, "a")->keptFills.size(), 0u);
            EXPECT_EQ(statementCount(past), 5u);
        }

        TEST(RunOncePass, JoinsNoFillWhenTheFillsAreNotIts)
        {
            auto statements = parsedQueue("const a = {}\na.x = 1\nfun f()\n{\n    return 1\n}\n");

            RunOncePass pass(false);
            pass.runPass(nullptr, statements);

            std::vector<StatementPtr> out;

            while (not statements.empty())
            {
                out.push_back(statements.front());
                statements.pop();
            }

            EXPECT_EQ(variable(out, "a")->keptFills.size(), 0u);
            EXPECT_EQ(statementCount(out), 3u);
            EXPECT_TRUE(functionsOf(out).at("f")->keptOnce);
        }

        TEST(ProgramConstsPass, AFillOfAConstWrittenOnEveryRunStaysWhereItIs)
        {
            // A const written by a statement that does not join is made on every run: a constant entry
            // set in its run is part of that, past a var found const or a joined statement alike
            EXPECT_EQ(sameBothWays({{"main.pg",
                "const t = {n: 0}\n"
                "var k = 5\n"
                "t.n = 1\n"
                "var out = 0\n"
                "t.n = t.n + 1\n"
                "out = t.n\n"}}, {"out"}), lines({"out=2 ", "out=2 ", "out=2 "}));

            EXPECT_EQ(sameBothWays({{"main.pg",
                "const row = {cost: 0, seen: 0}\n"
                "var limit = 10\n"
                "row.cost = 3\n"
                "var out = 0\n"
                "row.seen = row.cost\n"
                "row.cost = row.cost * 2\n"
                "out = row.cost\n"}}, {"out"}), lines({"out=6 ", "out=6 ", "out=6 "}));

            EXPECT_EQ(sameBothWays({{"main.pg",
                "const c0 = {n: 7}\n"
                "var p1 = 3\n"
                "c0.n = 3\n"
                "var v1 = {n: c0.n}\n"
                "c0.n = 2\n"
                "var out = 0\n"
                "out = c0.n * 3 + v1.n\n"}}, {"out"}), lines({"out=9 ", "out=9 ", "out=9 "}));

            EXPECT_EQ(sameBothWays({{"main.pg",
                "const c = {v: 1}\n"
                "const u = {m: 0}\n"
                "const t = {n: 0}\n"
                "u.m = c.v + 1\n"
                "t.n = 1\n"
                "var out = 0\n"
                "t.n = t.n + 1\n"
                "out = t.n\n"}}, {"out"}), lines({"out=2 ", "out=2 ", "out=2 "}));
        }

        TEST(ProgramConstsPass, AFillThePlainWalkJoinsStaysJoined)
        {
            // The const is written on every run, yet its constant entry right after it is part of
            // making it either way: a program counting runs in it keeps counting
            EXPECT_EQ(sameBothWays({{"main.pg", "const t = {n: 0}\nt.n = 1\nvar out = 0\nt.n = t.n + 1\nout = t.n\n"}}, {"out"}),
                      lines({"out=2 ", "out=3 ", "out=4 "}));

            // A module imported twice in a run sets a constant entry it does not keep at every import
            EXPECT_EQ(sameBothWays({{"main.pg", "import \"m\"\nc0.n = 1\nimport \"h\"\nvar out = 0\nout = c0.n\n"},
                                    {"m.pg", "import \"h\"\n"},
                                    {"h.pg", "const c0 = {n: 1}\nc0.n = 7\n"}}, {"out"}), lines({"out=1 ", "out=1 ", "out=1 "}));

            // Not past a var found to be a const: the plain walk closes there
            auto past = runOnce("const t = {n: 0}\nvar k = 5\nt.n = 1\nvar out = 0\nt.n = t.n + 1\nout = t.n\n", true);
            EXPECT_EQ(variable(past, "t")->keptFills.size(), 0u);

            auto plain = runOnce("const t = {n: 0}\nt.n = 1\nvar out = 0\nt.n = t.n + 1\nout = t.n\n", true);
            EXPECT_EQ(variable(plain, "t")->keptFills.size(), 1u);
        }

        TEST(ProgramConstsPass, AFileNothingWasDecidedForGetsThePlainFills)
        {
            // The analysis off (a top-level return), a twin module, a module compiled beside: the
            // constant fills are those of the pass without the analysis, no fewer
            const std::map<std::string, std::string> doubleImport = {
                {"main.pg", "import \"m\"\nc0.n = 1\nimport \"h\"\nvar out = 0\nout = c0.n\nif (out == 99)\n{\n    return 2\n}\n"},
                {"m.pg", "import \"h\"\n"},
                {"h.pg", "const c0 = {n: 1}\nc0.n = 7\n"}};
            EXPECT_EQ(sameBothWays(doubleImport, {"out"}), lines({"out=1 ", "out=1 ", "out=1 "}));

            EXPECT_EQ(sameBothWays({{"main.pg", "const t = {n: 0}\nt.n = 1\nvar out = 0\nt.n = t.n + 1\nout = t.n\nif (out == 99)\n{\n    return 2\n}\n"}}, {"out"}),
                      lines({"out=2 ", "out=3 ", "out=4 "}));

            const std::map<std::string, std::string> twins = {
                {"main.pg", "import \"m\"\nc0.n = 1\nimport \"h\"\nimport \"twin\"\nvar out = 0\nout = c0.n\n"},
                {"m.pg", "import \"h\"\n"},
                {"h.pg", "const c0 = {n: 1}\nc0.n = 7\n"},
                {"twin.pg", "const c0 = {n: 1}\nc0.n = 7\n"}};
            EXPECT_EQ(sameBothWays(twins, {"out"}), lines({"out=1 ", "out=1 ", "out=1 "}));

            EXPECT_EQ(sameBothWays({{"main.pg", "import \"m\"\nconst t = {n: 0}\nt.n = 1\nvar out = 0\nt.n = t.n + 1\nout = t.n\n"},
                                    {"m.pg", "const count = {n: 0}\ncount.n = count.n + 1\n"}}, {"out"}, "m.pg", "m.pg.pgc"),
                      lines({"out=2 ", "out=3 ", "out=4 "}));
        }

        TEST(ProgramConstsPass, AModuleImportedTwiceJoinsNoStatementReadingItsConst)
        {
            // It runs at each import: joined, it would run once
            EXPECT_EQ(sameBothWays({{"main.pg", "import \"m\"\nimport \"h\"\nvar out = 0\nout = c0.n\n"},
                                    {"m.pg", "import \"h\"\n"},
                                    {"h.pg", "const c0 = {n: 1}\nc0.n = c0.n + 1\n"}}, {"out"}),
                      lines({"out=3 ", "out=5 ", "out=7 "}));
        }

        TEST(ProgramConstsPass, WhatAModuleImportedTwiceImportsRunsTwiceAsWell)
        {
            // n imports m a second time, so m runs twice, and h with it though it is imported once
            EXPECT_EQ(sameBothWays({{"main.pg", "import \"m\"\nimport \"n\"\nvar out = 0\nout = c0.n\n"},
                                    {"m.pg", "import \"h\"\n"},
                                    {"n.pg", "import \"m\"\n"},
                                    {"h.pg", "const c0 = {n: 1}\nc0.n = c0.n + 1\n"}}, {"out"}),
                      lines({"out=3 ", "out=5 ", "out=7 "}));
        }

        TEST(ProgramConstsPass, ATableVarOfAModuleImportedTwiceIsNotAConst)
        {
            // Each import makes a new table: `a` holds the first of them, `t` the last
            EXPECT_EQ(sameBothWays({{"main.pg", "import \"m\"\nvar a = t\nimport \"h\"\nvar out = 0\nif (a == t)\n{\n    out = 1\n}\nelse\n{\n    out = 2\n}\n"},
                                    {"m.pg", "import \"h\"\n"},
                                    {"h.pg", "var t = {n: 1}\n"}}, {"out"}),
                      lines({"out=2 ", "out=2 ", "out=2 "}));
        }

        TEST(ProgramConstsPass, AModuleCompiledAsPgPgcIsNotReadEither)
        {
            // The compiler takes `m.pg.pgc` as it takes `m.pgc`
            const auto expected = lines({"seen=1 live=1 ", "seen=2 live=2 ", "seen=3 live=3 "});

            EXPECT_EQ(sameBothWays({{"main.pg", "import \"m\"\nvar seen = count.n\nvar live = 0\nlive = count.n\n"},
                                    {"m.pg", "const count = {n: 0}\ncount.n = count.n + 1\n"}}, {"seen", "live"}, "m.pg", "m.pg.pgc"), expected);
        }

        TEST(RunOncePass, AVarInferredDoesNotKeepARunOpen)
        {
            auto statements = runOnce("const t = {n: 0}\nvar k = 5\nvar q = 0\nfor (var i = 0; i < 3; i++)\n{\n    q = q + 1\n}\nt.n = 1\n", true);
            EXPECT_TRUE(variable(statements, "k")->isConst);
            EXPECT_TRUE(variable(statements, "k")->inferredConst);
            EXPECT_EQ(variable(statements, "t")->keptFills.size(), 0u);
        }

        TEST(RunOncePass, AProgramWithAMetamethodIsNotAnalyzed)
        {
            auto statements = runOnce("class Box\n{\n    __get(key)\n    {\n        return 1\n    }\n}\nvar x = 1\n", true);
            EXPECT_FALSE(variable(statements, "x")->isConst);
        }

        TEST(RunOncePass, AProgramWithAPlainClassIsAnalyzed)
        {
            auto statements = runOnce("class Box\n{\n    size()\n    {\n        return 1\n    }\n}\nvar x = 1\n", true);
            EXPECT_TRUE(variable(statements, "x")->isConst);
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

        TEST(ProgramConstsPass, AJoinedStatementThatFailsFailsEveryRun)
        {
            // t.b is made with t: a run that fails on it leaves no half-made t for the next to skip
            const std::string source =
                "import \"algorithm\"\n"
                "var rows = [{x: 1}, {x: 2}]\n"
                "const t = {a: 1}\n"
                "t.b = rows[2].x\n"
                "var out = 0\n"
                "out = lookup(t, \"b\", 9)\n";

            EXPECT_EQ(variable(runOnce(source, true), "t")->keptFills.size(), 1u);
            EXPECT_EQ(sameBothWays({{"main.pg", source}}, {"out"}), lines({"failed", "failed", "failed"}));
        }

        TEST(RunOncePass, AFailedRunKeepsNoConst)
        {
            // `index` and `made` are the host's: made counts how often t is made
            Prepared script(
                "fun make() \n{\n    made = made + 1\n    return {a: 1}\n}\n"
                "const t = make()\n"
                "var rows = [{x: 1}]\n"
                "var out = rows[index].x + t.a\n");

            script.vm.defineGlobal("made", makeIntValue(0));

            auto runWith = [&](int64_t index)
            {
                script.vm.defineGlobal("index", makeIntValue(index));
                return script.vm.runPreparedFunction(script.fn, 0, "runonce");
            };

            auto made = [&]() { return AS_INT(script.vm.findGlobal("made")); };

            EXPECT_EQ(runWith(0), InterpretResult::OK);
            EXPECT_EQ(runWith(0), InterpretResult::OK);
            EXPECT_EQ(made(), 1);

            EXPECT_EQ(runWith(5), InterpretResult::RUNTIME_ERROR);
            EXPECT_TRUE(IS_UNDEFINED(script.vm.findGlobal("t")));

            EXPECT_EQ(runWith(0), InterpretResult::OK);
            EXPECT_EQ(made(), 2);
            EXPECT_EQ(AS_INT(script.vm.findGlobal("out")), 2);
        }

        TEST(RunOncePass, RunsFailedInACallLeaveNoFrames)
        {
            // Each failed run stops 5 calls deep, for more runs than the VM has frames, inside a
            // function kept from run to run
            Prepared script(
                "var rows = [{x: 1}]\n"
                "fun deep(i, n) \n{\n    if (n == 0)\n    {\n        return rows[i].x\n    }\n\n    return deep(i, n - 1)\n}\n"
                "var out = deep(index, 4)\n");

            auto runWith = [&](int64_t index)
            {
                script.vm.defineGlobal("index", makeIntValue(index));
                return script.vm.runPreparedFunction(script.fn, 0, "runonce");
            };

            for (int i = 0; i < 40; ++i)
                EXPECT_EQ(runWith(5), InterpretResult::RUNTIME_ERROR) << "run " << i;

            EXPECT_EQ(runWith(0), InterpretResult::OK);
            EXPECT_EQ(AS_INT(script.vm.findGlobal("out")), 1);
        }

        TEST(RunOncePass, ChunksFailedInACallLeaveNoFrames)
        {
            Prepared script(
                "var rows = [{x: 1}]\n"
                "fun deep(i, n) \n{\n    if (n == 0)\n    {\n        return rows[i].x\n    }\n\n    return deep(i, n - 1)\n}\n"
                "var out = deep(index, 4)\n");

            VM vm;
            script.ecs.setupVm(vm);

            auto runWith = [&](int64_t index)
            {
                vm.defineGlobal("index", makeIntValue(index));
                return vm.interpretFromCachedBytecode(script.bytes, 0, "runonce");
            };

            for (int i = 0; i < 40; ++i)
                EXPECT_EQ(runWith(5), InterpretResult::RUNTIME_ERROR) << "run " << i;

            EXPECT_EQ(runWith(0), InterpretResult::OK);
            EXPECT_EQ(AS_INT(vm.findGlobal("out")), 1);
        }

        TEST(ProgramConstsPass, AnImportCyclingBackLeavesTheProgramNotComplete)
        {
            namespace fs = std::filesystem;
            const fs::path dir = fs::temp_directory_path() / ("pg_run_once_cycle_" + std::to_string(counter.fetch_add(1)));
            fs::remove_all(dir);
            fs::create_directories(dir);

            auto completeWith = [&](const std::map<std::string, std::string>& files)
            {
                for (const auto& [name, text] : files)
                    std::ofstream((dir / name).string()) << text;

                VM vm;
                vm.currentFileName = (dir / "main.pg").string();

                return ast::readProgramFiles(&vm, parsedQueue(files.at("main.pg"))).complete;
            };

            // One file imported by two is no cycle
            EXPECT_TRUE(completeWith({{"main.pg", "import \"a\"\nimport \"b\"\n"}, {"a.pg", "import \"c\"\n"}, {"b.pg", "import \"c\"\n"}, {"c.pg", "var x = 1\n"}}));

            // Back to a module, back to a module's own file, back to the script
            EXPECT_FALSE(completeWith({{"main.pg", "import \"a\"\n"}, {"a.pg", "import \"b\"\n"}, {"b.pg", "import \"a\"\n"}}));
            EXPECT_FALSE(completeWith({{"main.pg", "import \"a\"\n"}, {"a.pg", "import \"a\"\n"}}));
            EXPECT_FALSE(completeWith({{"main.pg", "import \"a\"\n"}, {"a.pg", "import \"main\"\n"}}));

            std::error_code ec;
            fs::remove_all(dir, ec);
        }
    }
}
