#include "gtest/gtest.h"

#include "Compiler/vm.h"
#include "Compiler/module_cache.h"
#include "ECS/entitysystem.h"

#include <filesystem>
#include <fstream>
#include <string>

namespace pg
{
    namespace test
    {
        // A module imported again is made from the bytecode its first compile kept, unless what it
        // read or what the program tells its passes is not the same: then it is compiled again
        class ModuleCache : public ::testing::Test
        {
        protected:
            std::filesystem::path dir;

            void SetUp() override
            {
                dir = std::filesystem::temp_directory_path() / ("pg_module_cache_" + std::to_string(::testing::UnitTest::GetInstance()->random_seed()) + "_" + ::testing::UnitTest::GetInstance()->current_test_info()->name());

                std::filesystem::remove_all(dir);
                std::filesystem::create_directories(dir);

                modulecache::clear();
            }

            void TearDown() override
            {
                std::filesystem::remove_all(dir);
                modulecache::clear();
            }

            void write(const std::string& name, const std::string& source)
            {
                std::ofstream out(dir / name, std::ios::trunc);
                out << source;
            }

            // The script run at O3 in a VM of its own: what it printed
            std::string run(const std::string& name)
            {
                EntitySystem ecs;
                ecs.setVMOptimizationLevel(VmOptimizationLevel::O3);
                ecs.setVMFrontEnd(ScriptFrontEnd::Ast);

                VM vm;
                ecs.setupVm(vm);

                EXPECT_EQ(vm.interpretFromFile((dir / name).string()), InterpretResult::OK) << name;

                return vm.testOutput;
            }

            // The global `out` a script left. Printing would be a statement the analysis knows nothing
            // of, which leaves it nothing to decide
            int64_t outOf(const std::string& name, bool wholeProgram)
            {
                EntitySystem ecs;
                ecs.setVMOptimizationLevel(wholeProgram ? VmOptimizationLevel::O3 : VmOptimizationLevel::O2);
                ecs.setVMFrontEnd(ScriptFrontEnd::Ast);

                VM vm;
                ecs.setupVm(vm);

                EXPECT_EQ(vm.interpretFromFile((dir / name).string()), InterpretResult::OK) << name;

                const Value out = vm.findGlobal("out");
                EXPECT_TRUE(IS_INT(out)) << name;
                return IS_INT(out) ? AS_INT(out) : -1;
            }

            int64_t analyzed(const std::string& name) { return outOf(name, true); }
            int64_t plain(const std::string& name) { return outOf(name, false); }
        };

        TEST_F(ModuleCache, ASecondImportIsTakenFromTheCache)
        {
            write("m.pg", "fun twice(x)\n{\n    return x * 2\n}\n\nvar base = 20\n");
            write("a.pg", "import \"m\"\n\n__dprint(twice(base))\n");
            write("b.pg", "import \"m\"\n\n__dprint(twice(base) + 1)\n");

            const std::string a = run("a.pg");
            const std::string b = run("b.pg");

            EXPECT_EQ(a, "40\n");
            EXPECT_EQ(b, "41\n");

            EXPECT_EQ(modulecache::counts().compiled, 1u);
            EXPECT_EQ(modulecache::counts().taken, 1u);
        }

        TEST_F(ModuleCache, AModuleImportedTwiceInOneScriptIsCompiledOnce)
        {
            write("m.pg", "var base = 20\n");
            write("n.pg", "import \"m\"\n\nvar more = base + 1\n");
            write("a.pg", "import \"m\"\nimport \"n\"\n\n__dprint(more)\n");

            const std::string a = run("a.pg");

            EXPECT_EQ(a, "21\n");

            EXPECT_EQ(modulecache::counts().compiled, 2u);
            EXPECT_EQ(modulecache::counts().taken, 1u);
        }

        TEST_F(ModuleCache, AChangedSourceIsCompiledAgain)
        {
            write("m.pg", "fun value()\n{\n    return 1\n}\n");
            write("a.pg", "import \"m\"\n\n__dprint(value())\n");

            EXPECT_EQ(run("a.pg"), "1\n");

            write("m.pg", "fun value()\n{\n    return 7\n}\n");

            const std::string again = run("a.pg");

            EXPECT_EQ(again, "7\n");
            EXPECT_EQ(modulecache::counts().compiled, 2u);
            EXPECT_EQ(modulecache::counts().taken, 0u);
        }

        TEST_F(ModuleCache, AChangedNestedImportIsCompiledAgain)
        {
            // m keeps its source; n, which m imports, changes
            write("n.pg", "var inner = 3\n");
            write("m.pg", "import \"n\"\n\nvar outer = inner * 10\n");
            write("a.pg", "import \"m\"\n\n__dprint(outer)\n");

            EXPECT_EQ(run("a.pg"), "30\n");

            write("n.pg", "var inner = 5\n");

            const std::string again = run("a.pg");

            EXPECT_EQ(again, "50\n");
            EXPECT_EQ(modulecache::counts().taken, 0u);
        }

        TEST_F(ModuleCache, AProgramThatRedefinesAFunctionGetsItsOwnCompile)
        {
            // In a's program `helper` is defined once: inlined into m's useHelper. In b's it is
            // defined twice, so it is called, and b's definition answers. Had b taken a's m, it
            // would print a's answer. useHelper is called through a variable, so m's own code runs
            write("h.pg", "fun helper(x)\n{\n    return x + 1\n}\n");
            write("m.pg", "fun useHelper(x)\n{\n    return helper(x)\n}\n");
            write("a.pg", "import \"h\"\nimport \"m\"\n\nvar call = useHelper\n__dprint(call(1))\n");
            write("b.pg", "import \"h\"\nimport \"m\"\n\nfun helper(x)\n{\n    return x + 100\n}\n\nvar call = useHelper\n__dprint(call(1))\n");

            const std::string a = run("a.pg");
            const std::string b = run("b.pg");

            EXPECT_EQ(a, "2\n");
            EXPECT_EQ(b, "101\n");
        }

        TEST_F(ModuleCache, TheAnalysisSharesAModuleItDecidedTheSameFor)
        {
            // The two programs are not alike, what is decided for m is
            write("m.pg", "const names = {a: 1}\nnames[\"b\"] = 2\nvar shown = 5\n");
            write("a.pg", "import \"m\"\nvar other = 1\nvar out = names.b + other\n");
            write("b.pg", "import \"m\"\nvar other = {n: 3}\nfun read()\n{\n    return other.n\n}\nvar out = names.b + read()\n");

            EXPECT_EQ(analyzed("a.pg"), 3);
            EXPECT_EQ(analyzed("b.pg"), 5);

            EXPECT_EQ(modulecache::counts().compiled, 1u);
            EXPECT_EQ(modulecache::counts().taken, 1u);
        }

        TEST_F(ModuleCache, TheAnalysisDecidingOtherwiseGivesAModuleItsOwnCompile)
        {
            // In a's program the entry reads what never changes: it joins the const, made once. In b's
            // it reads what is rebound: it stands on its own, each run makes it
            write("m.pg", "fun read()\n{\n    return level.n\n}\nconst t = {a: 1}\nt.b = read()\n");
            write("a.pg", "const level = {n: 1}\nimport \"m\"\nvar out = t.b\n");
            write("b.pg", "var level = {n: 1}\nlevel = {n: 5}\nimport \"m\"\nvar out = t.b\n");

            EXPECT_EQ(analyzed("a.pg"), 1);
            EXPECT_EQ(analyzed("b.pg"), 5);

            EXPECT_EQ(modulecache::counts().compiled, 2u);
            EXPECT_EQ(modulecache::counts().taken, 0u);
        }

        TEST_F(ModuleCache, AFileOfTheSameNamesDoesNotStandInForTheModule)
        {
            // In a's program f's entry joins t. In b's, s declares t as well: nothing joins in f, s's
            // entry does, and the two programs decide the same for "a file declaring t"
            write("f.pg", "const t = {a: 1}\nt.b = k.n\n");
            write("s.pg", "const t = {a: 2}\nt.b = 3\nt.c = k.n\n");
            write("a.pg", "const k = {n: 1}\nimport \"f\"\nvar out = t.b\n");
            write("b.pg", "const k = {n: 1}\nimport \"s\"\nimport \"f\"\nvar out = t.b\n");

            EXPECT_EQ(analyzed("a.pg"), 1);
            EXPECT_EQ(analyzed("b.pg"), 1);
        }

        TEST_F(ModuleCache, AModuleCompiledWithAndWithoutTheAnalysisIsNotShared)
        {
            // Off, the loop is folded away and the fill joins t. On, the loop stands and it does not: the
            // two compiles of m are not the same code, and each is taken only by its own side
            write("m.pg", "const t = {a: 1}\nfor (var i = 0; i < 0; i++)\n{\n}\nt.b = 2\nvar other = 5\n");
            write("a.pg", "import \"m\"\nvar out = t.b + other\n");

            EXPECT_EQ(plain("a.pg"), 7);
            EXPECT_EQ(analyzed("a.pg"), 7);
            EXPECT_EQ(plain("a.pg"), 7);

            EXPECT_EQ(modulecache::counts().compiled, 2u);
            EXPECT_EQ(modulecache::counts().taken, 1u);
        }

        TEST_F(ModuleCache, TheAnalysisCompilesAChangedFileAgain)
        {
            write("m.pg", "const t = {a: 1}\nt.b = 2\n");
            write("a.pg", "import \"m\"\nvar out = t.b\n");
            EXPECT_EQ(analyzed("a.pg"), 2);

            write("m.pg", "const t = {a: 1}\nt.b = 7\n");
            EXPECT_EQ(analyzed("a.pg"), 7);

            EXPECT_EQ(modulecache::counts().compiled, 2u);
            EXPECT_EQ(modulecache::counts().taken, 0u);
        }
    }
}
