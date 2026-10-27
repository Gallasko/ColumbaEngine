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
    }
}
