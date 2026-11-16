#include "gtest/gtest.h"

#include "Compiler/vm.h"
#include "ECS/entitysystem.h"
#include "Helpers/osmodule.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace pg
{
    namespace test
    {
        // The os module, registered as the command-line tool does: os.call runs a command and gives
        // back {code, out, err}
        class OsCall : public ::testing::Test
        {
        protected:
            std::filesystem::path dir;
            std::string output;
            std::string outGlobal;

            void SetUp() override
            {
                dir = std::filesystem::temp_directory_path() / ("pg_os_" + std::to_string(::testing::UnitTest::GetInstance()->random_seed()) + "_" + ::testing::UnitTest::GetInstance()->current_test_info()->name());
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

            // The script after `import "os" as os`; what it printed in `output`, its global `out`
            // (when a string) in `outGlobal`
            InterpretResult run(const std::string& body, bool withOs = true)
            {
                EntitySystem ecs;
                ecs.setVMOptimizationLevel(VmOptimizationLevel::O3);
                ecs.setVMFrontEnd(ScriptFrontEnd::Ast);

                VM vm;
                ecs.setupVm(vm);
                if (withOs)
                    vm.addNativeModule("os", OsModule{});

                const InterpretResult result = vm.interpretFromText("import \"os\" as os\nimport \"file\"\nimport \"string\"\nimport \"algorithm\"\n" + body);
                output = vm.testOutput;

                const Value out = vm.findGlobal("out");
                outGlobal = IS_STRING(out) ? std::string(vm.asString(out)) : std::string();
                return result;
            }
        };

        TEST_F(OsCall, StringRunsThroughTheShell)
        {
            ASSERT_EQ(run("var r = os.call(\"echo hi | tr a-z A-Z\")\n__dprint(r.code)\nvar out = r.out\n__dprint(strlen(r.err))\n"), InterpretResult::OK);
            EXPECT_EQ(output, "0\n0\n");
            EXPECT_EQ(outGlobal, "HI\n");
        }

        TEST_F(OsCall, ErrAndCode)
        {
            ASSERT_EQ(run("var r = os.call(\"echo x >&2; exit 3\")\n__dprint(r.code)\nvar out = r.err\n"), InterpretResult::OK);
            EXPECT_EQ(output, "3\n");
            EXPECT_EQ(outGlobal, "x\n");
        }

        TEST_F(OsCall, ListRunsDirectly)
        {
            ASSERT_EQ(run("var r = os.call([\"printf\", \"%s\", \"a'b $HOME c\"])\nvar out = r.out\n"), InterpretResult::OK);
            EXPECT_EQ(outGlobal, "a'b $HOME c");
        }

        TEST_F(OsCall, InputReachesTheCommand)
        {
            ASSERT_EQ(run("var r = os.call([\"cat\"], {input: \"abc\"})\nvar out = r.out\n"), InterpretResult::OK);
            EXPECT_EQ(outGlobal, "abc");
        }

        TEST_F(OsCall, NoInputIsAnEmptyStdin)
        {
            ASSERT_EQ(run("var r = os.call([\"cat\"])\n__dprint(r.code)\n__dprint(strlen(r.out))\n"), InterpretResult::OK);
            EXPECT_EQ(output, "0\n0\n");
        }

        TEST_F(OsCall, BigInputAndOutputTogether)
        {
            writeBytes("big.txt", std::string(8 * 1024 * 1024, 'x'));

            ASSERT_EQ(run("var big = readFile(\"" + path("big.txt") + "\")\n"
                          "var r = os.call([\"cat\"], {input: big})\n__dprint(strlen(r.out))\n"
                          "var both = os.call(\"cat | tee /dev/stderr\", {input: big})\n__dprint(strlen(both.out))\n__dprint(strlen(both.err))\n"), InterpretResult::OK);
            EXPECT_EQ(output, "8388608\n8388608\n8388608\n");
        }

        TEST_F(OsCall, MissingProgramIs127)
        {
            ASSERT_EQ(run("var r = os.call([\"no-such-program-xyz\"])\n__dprint(r.code)\nvar out = r.err\n"), InterpretResult::OK);
            EXPECT_EQ(output, "127\n");
            EXPECT_NE(outGlobal.find("no-such-program-xyz"), std::string::npos) << outGlobal;
        }

        TEST_F(OsCall, Cwd)
        {
            ASSERT_EQ(run("var r = os.call([\"pwd\"], {cwd: \"" + dir.string() + "\"})\nvar out = r.out\n"
                          "var bad = os.call([\"pwd\"], {cwd: \"" + path("missing") + "\"})\n__dprint(bad.code)\n"), InterpretResult::OK);
            EXPECT_EQ(outGlobal, dir.string() + "\n");
            EXPECT_EQ(output, "127\n");
        }

        TEST_F(OsCall, KilledBySignal)
        {
            ASSERT_EQ(run("var r = os.call(\"kill -9 $$\")\n__dprint(r.code)\n"), InterpretResult::OK);
            EXPECT_EQ(output, "137\n");
        }

        TEST_F(OsCall, UnknownOptionIsAnError)
        {
            EXPECT_EQ(run("os.call([\"true\"], {inputs: \"x\"})\n"), InterpretResult::RUNTIME_ERROR);
        }

        TEST_F(OsCall, BadCommandIsAnError)
        {
            EXPECT_EQ(run("os.call([])\n"), InterpretResult::RUNTIME_ERROR);
            EXPECT_EQ(run("os.call(5)\n"), InterpretResult::RUNTIME_ERROR);
            EXPECT_EQ(run("os.call([1])\n"), InterpretResult::RUNTIME_ERROR);
            EXPECT_EQ(run("os.call([\"true\"], {input: 5})\n"), InterpretResult::RUNTIME_ERROR);
        }

        TEST_F(OsCall, InputNotReadNoSigpipe)
        {
            writeBytes("big.txt", std::string(5 * 1024 * 1024, 'y'));

            ASSERT_EQ(run("var r = os.call([\"true\"], {input: readFile(\"" + path("big.txt") + "\")})\n__dprint(r.code)\n"), InterpretResult::OK);
            EXPECT_EQ(output, "0\n");
        }

        TEST_F(OsCall, NoDescriptorLeak)
        {
            auto openFds = [] {
                size_t count = 0;
                for ([[maybe_unused]] const auto& entry : std::filesystem::directory_iterator("/proc/self/fd"))
                    ++count;
                return count;
            };

            const size_t before = openFds();

            ASSERT_EQ(run("var i = 0\nwhile (i < 300)\n{\n    var r = os.call([\"true\"])\n    i++\n}\nvar last = os.call([\"echo\", \"ok\"])\nvar out = last.out\n"), InterpretResult::OK);
            EXPECT_EQ(outGlobal, "ok\n");

            EXPECT_EQ(openFds(), before);
        }

        TEST_F(OsCall, BytesComeBackUnchanged)
        {
            ASSERT_EQ(run("var r = os.call([\"printf\", \"\\\\377\"])\nvar out = r.out\n"), InterpretResult::OK);
            EXPECT_EQ(outGlobal, std::string("\xff"));
        }

        TEST_F(OsCall, NotInSetupVm)
        {
            EXPECT_EQ(run("var r = 1\n", false), InterpretResult::COMPILE_ERROR);
        }

        // The rest of os: the environment, time, folders and exit
        class OsFunctions : public OsCall
        {
        };

        TEST_F(OsFunctions, Env)
        {
            setenv("PG_OS_TEST", "v", 1);
            unsetenv("PG_OS_UNSET_XYZ");

            ASSERT_EQ(run("var out = os.env(\"PG_OS_TEST\")\n__dprint(typeOf(os.env(\"PG_OS_UNSET_XYZ\")))\n"), InterpretResult::OK);
            EXPECT_EQ(outGlobal, "v");
            EXPECT_EQ(output, "none\n");
        }

        TEST_F(OsFunctions, Time)
        {
            const int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

            ASSERT_EQ(run("__dprint(typeOf(os.time()))\n__dprint(os.time())\n"), InterpretResult::OK);
            ASSERT_EQ(output.substr(0, 4), "int\n");

            const int64_t seen = std::stoll(output.substr(4));
            EXPECT_LE(std::llabs(seen - now), 5000);
        }

        TEST_F(OsFunctions, FormatTime)
        {
            // An int literal is 32 bits: the time is made from two
            ASSERT_EQ(run("var out = os.formatTime(1794830998 * 1000 + 386, \"%Y-%m-%d %H:%M:%S\")\n__dprint(os.formatTime(0, \"%Y-%m-%d\"))\n"), InterpretResult::OK);
            EXPECT_EQ(outGlobal, "2026-11-16 12:09:58");
            EXPECT_EQ(output, "1970-01-01\n");
        }

        TEST_F(OsFunctions, FormatTimeTooLongIsAnError)
        {
            EXPECT_EQ(run("os.formatTime(0, \"" + std::string(300, 'x') + "\")\n"), InterpretResult::RUNTIME_ERROR);
        }

        TEST_F(OsFunctions, ListDirSorted)
        {
            writeBytes("b", "");
            writeBytes("a", "");
            writeBytes(".hidden", "");
            std::filesystem::create_directories(dir / "sub");

            ASSERT_EQ(run("var names = os.listDir(\"" + dir.string() + "\")\nfor (var n : names)\n{\n    __dprint(n)\n}\n"), InterpretResult::OK);
            EXPECT_EQ(output, ".hidden\na\nb\nsub\n");

            EXPECT_EQ(run("os.listDir(\"" + path("missing") + "\")\n"), InterpretResult::RUNTIME_ERROR);
            EXPECT_EQ(run("os.listDir(\"" + path("a") + "\")\n"), InterpretResult::RUNTIME_ERROR);
        }

        TEST_F(OsFunctions, IsDir)
        {
            writeBytes("file", "x");

            ASSERT_EQ(run("__dprint(os.isDir(\"" + dir.string() + "\"))\n__dprint(os.isDir(\"" + path("file") + "\"))\n__dprint(os.isDir(\"" + path("missing") + "\"))\n"), InterpretResult::OK);
            EXPECT_EQ(output, "true\nfalse\nfalse\n");
        }

        TEST_F(OsFunctions, MakeDirWithParents)
        {
            const std::string deep = path("x/y/z");

            ASSERT_EQ(run("os.makeDir(\"" + deep + "\")\n__dprint(os.isDir(\"" + deep + "\"))\nos.makeDir(\"" + deep + "\")\n__dprint(1)\n"), InterpretResult::OK);
            EXPECT_EQ(output, "true\n1\n");

            writeBytes("file", "x");
            EXPECT_EQ(run("os.makeDir(\"" + path("file") + "\")\n"), InterpretResult::RUNTIME_ERROR);
        }

        TEST_F(OsFunctions, ExitsWithTheCode)
        {
            EXPECT_EXIT(
                {
                    run("os.exit(7)\n__dprint(1)\n");
                    std::exit(0);
                },
                ::testing::ExitedWithCode(7), "");
        }
    }
}
