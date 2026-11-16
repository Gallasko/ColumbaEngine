#include "gtest/gtest.h"

#include "Compiler/vm.h"
#include "Compiler/chunk_serializer.h"
#include "Compiler/module_cache.h"
#include "ECS/entitysystem.h"

#include <filesystem>
#include <fstream>
#include <string>

namespace pg
{
    namespace test
    {
        // `import "module" as name` binds a native module's functions to a table `name`; a plain
        // import stays flat. The binding is recorded in the chunk, so every way a chunk comes back
        // (module cache, a .pgc file) binds it again
        class ImportAs : public ::testing::Test
        {
        protected:
            std::filesystem::path dir;
            std::string output;

            void SetUp() override
            {
                dir = std::filesystem::temp_directory_path() / ("pg_import_as_" + std::to_string(::testing::UnitTest::GetInstance()->random_seed()) + "_" + ::testing::UnitTest::GetInstance()->current_test_info()->name());

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

            std::string path(const std::string& name) const { return (dir / name).string(); }

            // The script run in a VM of its own; what it printed lands in `output`
            InterpretResult run(const std::string& name, ScriptFrontEnd frontEnd = ScriptFrontEnd::Ast)
            {
                EntitySystem ecs;
                ecs.setVMOptimizationLevel(VmOptimizationLevel::O3);
                ecs.setVMFrontEnd(frontEnd);

                VM vm;
                ecs.setupVm(vm);

                const InterpretResult result = vm.interpretFromFile(path(name));
                output = vm.testOutput;
                return result;
            }
        };

        TEST_F(ImportAs, BindsATableOfTheModule)
        {
            write("a.pg", "import \"string\" as s\n\n__dprint(s.strlen(\"abc\"))\n");

            EXPECT_EQ(run("a.pg"), InterpretResult::OK);
            EXPECT_EQ(output, "3\n");
        }

        TEST_F(ImportAs, DefinesNoFlatNames)
        {
            write("a.pg", "import \"string\" as s\n\n__dprint(strlen(\"a\"))\n");

            EXPECT_EQ(run("a.pg"), InterpretResult::RUNTIME_ERROR);
        }

        TEST_F(ImportAs, BothFormsTogether)
        {
            write("a.pg", "import \"string\"\nimport \"string\" as s\n\n__dprint(strlen(\"ab\") + s.strlen(\"abc\"))\n");

            EXPECT_EQ(run("a.pg"), InterpretResult::OK);
            EXPECT_EQ(output, "5\n");
        }

        TEST_F(ImportAs, MissingModuleIsACompileError)
        {
            write("a.pg", "import \"nosuch\" as n\n");

            EXPECT_EQ(run("a.pg"), InterpretResult::COMPILE_ERROR);
        }

        TEST_F(ImportAs, FileImportIsACompileError)
        {
            write("lib.pg", "var x = 1\n");
            write("a.pg", "import \"lib\" as l\n");

            EXPECT_EQ(run("a.pg"), InterpretResult::COMPILE_ERROR);
        }

        TEST_F(ImportAs, NameMustBeAnIdentifier)
        {
            write("a.pg", "import \"string\" as \"s\"\n");

            EXPECT_EQ(run("a.pg"), InterpretResult::COMPILE_ERROR);
        }

        TEST_F(ImportAs, PrattFrontEndRefusesIt)
        {
            write("a.pg", "import \"string\" as s\n\n__dprint(s.strlen(\"abc\"))\n");

            EXPECT_EQ(run("a.pg", ScriptFrontEnd::Pratt), InterpretResult::COMPILE_ERROR);
        }

        TEST_F(ImportAs, ModuleCacheBindsItAgain)
        {
            write("lib.pg", "import \"string\" as s\n\nfun f()\n{\n    return s.strlen(\"ab\")\n}\n");
            write("main.pg", "import \"lib\"\n\n__dprint(f())\n");

            EXPECT_EQ(run("main.pg"), InterpretResult::OK);
            EXPECT_EQ(output, "2\n");

            EXPECT_EQ(run("main.pg"), InterpretResult::OK);
            EXPECT_EQ(output, "2\n");

            EXPECT_EQ(modulecache::counts().taken, 1u);
        }

        TEST_F(ImportAs, SerializedChunkBindsItAgain)
        {
            write("a.pg", "import \"string\" as s\n\n__dprint(s.strlen(\"abcd\"))\n");

            {
                EntitySystem ecs;
                ecs.setVMFrontEnd(ScriptFrontEnd::Ast);
                VM vm;
                ecs.setupVm(vm);

                ASSERT_EQ(vm.interpretFromFile(path("a.pg"), true, path("a.pgc")), InterpretResult::OK);
            }

            EntitySystem ecs;
            VM vm;
            ecs.setupVm(vm);

            EXPECT_EQ(vm.interpretFromBytecodeFile(path("a.pgc")), InterpretResult::OK);
            EXPECT_EQ(vm.testOutput, "4\n");
        }

        TEST_F(ImportAs, PgcImportBindsItAgain)
        {
            write("lib.pg", "import \"string\" as s\n\nfun f()\n{\n    return s.strlen(\"xyz\")\n}\n");

            {
                EntitySystem ecs;
                ecs.setVMFrontEnd(ScriptFrontEnd::Ast);
                VM vm;
                ecs.setupVm(vm);

                ASSERT_EQ(vm.interpretFromFile(path("lib.pg"), true, path("lib.pgc")), InterpretResult::OK);
            }

            std::filesystem::remove(dir / "lib.pg");
            write("main.pg", "import \"lib\"\n\n__dprint(f())\n");

            EXPECT_EQ(run("main.pg"), InterpretResult::OK);
            EXPECT_EQ(output, "3\n");
        }

        TEST_F(ImportAs, OldPlainRecordsStillLoadFlat)
        {
            EntitySystem ecs;
            VM vm;
            ecs.setupVm(vm);

            EXPECT_TRUE(vm.replayImport("string"));
            EXPECT_FALSE(IS_UNDEFINED(vm.findGlobal("strlen")));
            EXPECT_TRUE(IS_UNDEFINED(vm.findGlobal("string")));
        }

        TEST_F(ImportAs, ReplayedRecordBindsATable)
        {
            EntitySystem ecs;
            VM vm;
            ecs.setupVm(vm);

            EXPECT_TRUE(vm.replayImport("string as str"));
            EXPECT_TRUE(IS_UNDEFINED(vm.findGlobal("strlen")));
            EXPECT_FALSE(IS_UNDEFINED(vm.findGlobal("str")));
            EXPECT_FALSE(vm.replayImport("nosuch as n"));
        }

        TEST_F(ImportAs, ResetVmBindsItOnTheNextImport)
        {
            EntitySystem ecs;
            ecs.setVMFrontEnd(ScriptFrontEnd::Ast);
            VM vm;
            ecs.setupVm(vm);

            ASSERT_EQ(vm.interpretFromText("import \"string\" as s\n__dprint(s.strlen(\"a\"))\n"), InterpretResult::OK);
            ASSERT_EQ(vm.interpretFromText("import \"string\" as s\n__dprint(s.strlen(\"ab\"))\n"), InterpretResult::OK);
            EXPECT_EQ(vm.testOutput, "1\n2\n");
        }

        TEST(ImportRecord, RoundTrips)
        {
            EXPECT_EQ(VM::importRecord("os", ""), "os");
            EXPECT_EQ(VM::importRecord("os", "o"), "os as o");
        }
    }
}
