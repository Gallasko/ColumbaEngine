#include "gtest/gtest.h"

#include "Compiler/vm.h"
#include "Compiler/ast/pass/program_consts.h"
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
            namespace fs = std::filesystem;

            std::atomic<uint64_t> counter{0};

            using Files = std::map<std::string, std::string>;

            struct Dir
            {
                explicit Dir(const Files& files) : path(fs::temp_directory_path() / ("pg_constexpr_" + std::to_string(counter.fetch_add(1))))
                {
                    fs::remove_all(path);
                    fs::create_directories(path);

                    for (const auto& [name, text] : files)
                        std::ofstream((path / name).string()) << text;
                }

                ~Dir()
                {
                    std::error_code ec;
                    fs::remove_all(path, ec);
                }

                fs::path path;
            };

            // Why the program is refused, as ProgramConstsPass says it: nothing when it may compile
            std::vector<std::string> errorsOf(const Files& files)
            {
                Dir dir(files);

                Lexer lexer;
                lexer.readFromText(files.at("main.pg"));

                Parser parser(lexer.getTokens());
                auto statements = parser.parse();

                EXPECT_FALSE(parser.hasError()) << "test source failed to parse";

                VM vm;
                vm.currentFileName = (dir.path / "main.pg").string();
                vm.astCompileDepth = 1;

                ProgramConstsPass pass;
                pass.runPass(&vm, statements);
                vm.astCompileDepth = 0;

                return pass.takeErrors();
            }

            std::string errorOf(const std::string& source)
            {
                const auto errors = errorsOf({{"main.pg", source}});

                return errors.empty() ? "" : errors.front();
            }

            InterpretResult compileAt(const Files& files, VmOptimizationLevel level)
            {
                Dir dir(files);

                EntitySystem ecs;
                ecs.setVMOptimizationLevel(level);
                ecs.setVMFrontEnd(ScriptFrontEnd::Ast);

                VM vm;
                ecs.setupVm(vm);

                return vm.interpretFromFile((dir.path / "main.pg").string(), true, (dir.path / "out.pgc").string());
            }

            // Compiled at a level, then run three times on one VM: a global after each run
            std::vector<int64_t> threeRuns(const Files& files, VmOptimizationLevel level, const std::string& global)
            {
                Dir dir(files);

                EntitySystem ecs;
                ecs.setVMOptimizationLevel(level);
                ecs.setVMFrontEnd(ScriptFrontEnd::Ast);

                const std::string compiled = (dir.path / "out.pgc").string();

                {
                    VM compiler;
                    ecs.setupVm(compiler);
                    EXPECT_EQ(compiler.interpretFromFile((dir.path / "main.pg").string(), true, compiled), InterpretResult::OK);
                }

                std::ifstream in(compiled, std::ios::binary);
                const std::vector<char> bytes{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};

                VM vm;
                ecs.setupVm(vm);
                ObjFunction* fn = vm.prepareCachedFunction(bytes, "runs");
                EXPECT_NE(fn, nullptr);

                std::vector<int64_t> out;

                for (int run = 0; run < 3; ++run)
                {
                    EXPECT_EQ(vm.runPreparedFunction(fn, 0, "runs"), InterpretResult::OK);

                    const Value value = vm.findGlobal(global);
                    out.push_back(IS_INT(value) ? AS_INT(value) : -1);
                }

                return out;
            }

            const std::vector<VmOptimizationLevel> everyLevel = {VmOptimizationLevel::O0, VmOptimizationLevel::O1, VmOptimizationLevel::O2, VmOptimizationLevel::O3};

            bool says(const std::string& error, const std::string& text)
            {
                return error.find(text) != std::string::npos;
            }

            const std::string algorithm = "import \"algorithm\"\n";
        }

        TEST(Constexpr, ALoopBuildingItRunsOnceAtEveryLevel)
        {
            const std::string source = algorithm +
                "constexpr squares = []\n"
                "for (var i = 0; i < 4; i++) {\n    push(squares, i * i)\n}\n"
                "var out = len(squares)\n";

            for (const auto level : everyLevel)
                EXPECT_EQ(threeRuns({{"main.pg", source}}, level, "out"), (std::vector<int64_t>{4, 4, 4}));
        }

        TEST(Constexpr, AFunctionCanBuildIt)
        {
            const std::string source = algorithm +
                "fun squaresTo(n) {\n"
                "    var out = []\n"
                "    for (var i = 0; i < n; i++) {\n        push(out, i * i)\n    }\n"
                "    return out\n"
                "}\n"
                "constexpr squares = squaresTo(4)\n"
                "var out = squares[3]\n";

            EXPECT_EQ(errorOf(source), "");

            for (const auto level : everyLevel)
                EXPECT_EQ(threeRuns({{"main.pg", source}}, level, "out"), (std::vector<int64_t>{9, 9, 9}));
        }

        TEST(Constexpr, AFunctionCanBuildNestedTables)
        {
            const std::string source = algorithm +
                "fun groupsOf(rows) {\n"
                "    var groups = {}\n"
                "    for (var i = 0; i < len(rows); i++) {\n"
                "        if (not contain(groups, rows[i])) {\n"
                "            groups[rows[i]] = []\n"
                "        }\n"
                "        push(groups[rows[i]], i)\n"
                "    }\n"
                "    return groups\n"
                "}\n"
                "constexpr groups = groupsOf([\"a\", \"b\", \"a\"])\n"
                "var out = len(groups[\"a\"])\n";

            EXPECT_EQ(errorOf(source), "");

            for (const auto level : everyLevel)
                EXPECT_EQ(threeRuns({{"main.pg", source}}, level, "out"), (std::vector<int64_t>{2, 2, 2}));
        }

        TEST(Constexpr, AWriteBelowATableAFunctionMadeReachesWhatThatTableHolds)
        {
            const std::string direct = errorOf("constexpr rows = {n: 1}\nfun touch() {\n    var held = {row: rows}\n    held.row.n = 2\n}\nvar out = 1\n");
            const std::string deeper = errorOf("constexpr rows = {n: 1}\nfun touch() {\n    var held = {inner: {}}\n    held.inner.row = rows\n    held.inner.row.n = 2\n}\nvar out = 1\n");

            EXPECT_TRUE(says(direct, "function `touch`")) << direct;
            EXPECT_TRUE(says(deeper, "function `touch`")) << deeper;
        }

        TEST(Constexpr, ADeepCopyIsWritableAllTheWayDown)
        {
            const std::string source = algorithm +
                "constexpr rows = {inner: {n: 1}}\n"
                "var row = deepCopy(rows)\n"
                "row.inner.n = row.inner.n + character.age\n"
                "var out = rows.inner.n\n";

            EXPECT_EQ(errorOf(source), "");
        }

        TEST(Constexpr, ReadingAnotherConstexprIsFine)
        {
            EXPECT_EQ(errorOf("constexpr base = {n: 2}\nconstexpr twice = {n: base.n * 2}\nvar out = twice.n\n"), "");
        }

        TEST(Constexpr, ItMayHoldTheRowsOfAnEarlierOne)
        {
            const std::string source = algorithm +
                "fun firstOf(rows) {\n"
                "    var first = []\n"
                "    push(first, rows[0])\n"
                "    return first\n"
                "}\n"
                "constexpr rows = [{n: 3}, {n: 4}]\n"
                "var gap = 1\n"
                "constexpr first = firstOf(rows)\n"
                "var out = first[0].n\n";

            EXPECT_EQ(errorOf(source), "");

            for (const auto level : everyLevel)
                EXPECT_EQ(threeRuns({{"main.pg", source}}, level, "out"), (std::vector<int64_t>{3, 3, 3}));
        }

        TEST(Constexpr, AWriteThroughALaterOneIsStillAWrite)
        {
            const std::string error = errorOf(algorithm + "constexpr rows = [{n: 3}]\nconstexpr later = rows[0]\nvar age = character.age\nlater.n = age\n");

            EXPECT_TRUE(says(error, "`rows`")) << error;
            EXPECT_TRUE(says(error, ":5")) << error;
        }

        TEST(Constexpr, AnEarlierOneBuiltOnAfterALaterOneTookItIsRefused)
        {
            const std::string error = errorOf(algorithm + "constexpr rows = []\nconstexpr later = rows\npush(rows, 1)\n");

            EXPECT_TRUE(says(error, "`rows`")) << error;
        }

        TEST(Constexpr, ACopyOfItIsWritable)
        {
            const std::string source = algorithm +
                "constexpr rows = [{n: 1}, {n: 2}]\n"
                "var row = copy(rows[1])\n"
                "row.n = row.n + 1\n"
                "var out = row.n + rows[1].n\n";

            EXPECT_EQ(errorOf(source), "");

            for (const auto level : everyLevel)
                EXPECT_EQ(threeRuns({{"main.pg", source}}, level, "out"), (std::vector<int64_t>{5, 5, 5}));
        }

        TEST(Constexpr, ReadingAnInputIsRefused)
        {
            const std::string error = errorOf("constexpr level = {n: character.age}\nvar out = level.n\n");

            EXPECT_TRUE(says(error, "`level`")) << error;
            EXPECT_TRUE(says(error, "`character`")) << error;
        }

        TEST(Constexpr, ReadingAnInputThroughAFunctionIsRefused)
        {
            const std::string error = errorOf("fun ageOf() {\n    return character.age\n}\nconstexpr level = {n: ageOf()}\nvar out = level.n\n");

            EXPECT_TRUE(says(error, "`character`")) << error;
        }

        TEST(Constexpr, AConstIsNotTakenAtItsWord)
        {
            const std::string error = errorOf("const kept = character.age\nconstexpr level = {n: kept}\nvar out = level.n\n");

            EXPECT_TRUE(says(error, "`character`")) << error;
        }

        TEST(Constexpr, AVarThatChangesIsRefused)
        {
            const std::string error = errorOf("var n = 1\nn = n + character.age\nconstexpr level = {n: n}\nvar out = level.n\n");

            EXPECT_TRUE(says(error, "`n`")) << error;
        }

        TEST(Constexpr, AWriteAfterItIsMadeIsRefusedWithItsLine)
        {
            const std::string error = errorOf("constexpr rows = {n: 1}\nvar age = character.age\nrows.n = age\n");

            EXPECT_TRUE(says(error, "`rows`")) << error;
            EXPECT_TRUE(says(error, ":3")) << error;
        }

        TEST(Constexpr, AFillThatReadsAnInputIsRefused)
        {
            const std::string error = errorOf(algorithm + "constexpr rows = []\nfor (var i = 0; i < 3; i++) {\n    push(rows, character.age)\n}\n");

            EXPECT_TRUE(says(error, "`character`")) << error;
        }

        TEST(Constexpr, AWriteThroughAnotherNameIsRefused)
        {
            EXPECT_TRUE(says(errorOf("constexpr rows = [{n: 1}]\nvar alias = rows\nalias[0].n = character.age\n"), "`rows`"));
            EXPECT_TRUE(says(errorOf(algorithm + "constexpr rows = [{n: 1}]\nvar held = []\npush(held, rows[0])\nheld[0].n = character.age\n"), "`rows`"));
        }

        TEST(Constexpr, AWriteBelowAShallowCopyIsRefused)
        {
            EXPECT_TRUE(says(errorOf(algorithm + "constexpr rows = {inner: {n: 1}}\nvar row = copy(rows)\nrow.inner.n = character.age\n"), "`rows`"));
        }

        TEST(Constexpr, AFunctionWritingItIsRefused)
        {
            const std::string error = errorOf("constexpr rows = {n: 1}\nfun touch() {\n    rows.n = 2\n}\nvar out = 1\n");

            EXPECT_TRUE(says(error, "function `touch`")) << error;
        }

        TEST(Constexpr, AFunctionWritingWhatItIsGivenIsRefusedWhereItIsGivenIt)
        {
            const std::string error = errorOf("fun set(row) {\n    row.n = character.age\n}\nconstexpr rows = {n: 1}\nvar age = 3\nset(rows)\n");

            EXPECT_TRUE(says(error, "`rows`")) << error;
            EXPECT_TRUE(says(error, ":6")) << error;
        }

        TEST(Constexpr, AssigningItIsRefused)
        {
            EXPECT_TRUE(says(errorOf("constexpr rows = {n: 1}\nvar out = 1\nfun f() {\n    rows = 2\n}\n"), "assigned"));
        }

        TEST(Constexpr, DefiningItTwiceIsRefused)
        {
            EXPECT_TRUE(says(errorOf("constexpr rows = {n: 1}\nconstexpr rows = {n: 2}\n"), "more than once"));
        }

        TEST(Constexpr, ACallTheCompilerCannotFollowIsRefused)
        {
            EXPECT_TRUE(says(errorOf("constexpr rows = {n: mystery()}\n"), "`rows`"));
        }

        TEST(Constexpr, AProgramTheAnalysisCannotReadIsRefused)
        {
            const std::string error = errorOf("class Thing {\n    __get(key) {\n        return 1\n    }\n}\nconstexpr rows = {n: 1}\n");

            EXPECT_TRUE(says(error, "cannot be proved")) << error;
            EXPECT_TRUE(says(error, "class")) << error;
        }

        TEST(Constexpr, AClassMayMakeIt)
        {
            const char* source =
                "class Row\n{\n    init(n)\n    {\n        this.n = n\n    }\n    twice()\n    {\n        return this.n * 2\n    }\n}\n"
                "fun make()\n{\n    var r = Row(3)\n    return {n: r.twice()}\n}\n"
                "constexpr rows = make()\n";

            EXPECT_EQ(errorOf(source), "") << source;
        }

        TEST(Constexpr, AMethodWritingItIsRefused)
        {
            const std::string error = errorOf(
                "constexpr rows = {n: 1}\n"
                "class Box\n{\n    clear()\n    {\n        rows.n = 0\n    }\n}\n");

            EXPECT_TRUE(says(error, "`rows`")) << error;
            EXPECT_TRUE(says(error, "Box.clear")) << error;
        }

        TEST(Constexpr, AnObjectHoldingItIsNoWayToWriteIt)
        {
            const std::string error = errorOf(
                "constexpr rows = {n: 1}\n"
                "class Box\n{\n    init(t)\n    {\n        this.t = t\n    }\n    clear()\n    {\n        this.t.n = 0\n    }\n}\n"
                "var b = Box(rows)\n"
                "b.clear()\n");

            EXPECT_TRUE(says(error, "`rows`")) << error;
        }

        TEST(Constexpr, AnImporterWritingItIsRefused)
        {
            const auto errors = errorsOf({{"main.pg", "import \"table\"\nrows.n = character.age\n"}, {"table.pg", "constexpr rows = {n: 1}\n"}});

            ASSERT_EQ(errors.size(), 1u);
            EXPECT_TRUE(says(errors[0], "table.pg:1")) << errors[0];
            EXPECT_TRUE(says(errors[0], "main.pg:2")) << errors[0];
        }

        TEST(Constexpr, AModuleImportedTwiceBuildsItOnce)
        {
            const Files files = {
                {"main.pg", algorithm + "import \"a\"\nimport \"b\"\nvar out = len(rows)\n"},
                {"a.pg", "import \"table\"\n"},
                {"b.pg", "import \"table\"\n"},
                {"table.pg", algorithm + "constexpr rows = []\nfor (var i = 0; i < 3; i++) {\n    push(rows, i)\n}\n"},
            };

            EXPECT_EQ(errorsOf(files), std::vector<std::string>{});

            for (const auto level : everyLevel)
                EXPECT_EQ(threeRuns(files, level, "out"), (std::vector<int64_t>{3, 3, 3}));
        }

        TEST(Constexpr, ARefusedProgramDoesNotCompileAtAnyLevel)
        {
            for (const auto level : everyLevel)
                EXPECT_EQ(compileAt({{"main.pg", "constexpr level = {n: character.age}\n"}}, level), InterpretResult::COMPILE_ERROR);
        }

        TEST(Constexpr, ItIsNotForAScope)
        {
            EXPECT_EQ(compileAt({{"main.pg", "fun f() {\n    constexpr rows = {n: 1}\n    return rows\n}\n"}}, VmOptimizationLevel::O3), InterpretResult::COMPILE_ERROR);
        }

        TEST(Constexpr, AVmWithoutThePassRefusesIt)
        {
            Dir dir(Files{{"main.pg", "constexpr rows = {n: 1}\n"}});

            VM vm;
            vm.setFrontEnd(ScriptFrontEnd::Ast);

            EXPECT_EQ(vm.interpretFromFile((dir.path / "main.pg").string()), InterpretResult::COMPILE_ERROR);
        }

        TEST(Constexpr, TheOtherFrontEndRefusesIt)
        {
            Dir dir(Files{{"main.pg", "constexpr rows = {n: 1}\n"}});

            EntitySystem ecs;
            ecs.setVMFrontEnd(ScriptFrontEnd::Pratt);

            VM vm;
            ecs.setupVm(vm);

            EXPECT_EQ(vm.interpretFromFile((dir.path / "main.pg").string()), InterpretResult::COMPILE_ERROR);
        }
    }
}
