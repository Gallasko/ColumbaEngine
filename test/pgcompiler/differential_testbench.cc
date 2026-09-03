#include "gtest/gtest.h"

#include "Compiler/vm.h"
#include "ECS/entitysystem.h"
#include "math_module.h"
#include "Files/filemodule.h"
#include "Helpers/stringmodule.h"
#include "Helpers/algorithmmodule.h"

#include "../mocklogger.h"

#include <algorithm>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

namespace pg {
namespace test {

/**
 * Differential testbench: Pratt front-end vs AST front-end
 *
 * Every script of the corpus is compiled and run twice - once through the
 * single-pass Pratt compiler (CParser) and once through the AST front-end
 * (AstCompiler) - on otherwise identical fresh VMs. Both runs must agree on
 * the InterpretResult and on the __dprint output. This is the equivalence
 * oracle that lets the two front-ends stay interchangeable until benchmarks
 * decide a winner.
 */
class FrontendDifferential : public ::testing::Test
{
protected:
    static constexpr const char* scriptsDir = "test/pgcompiler/scripts";

    // Scripts the AST front-end is known to not handle yet, with the reason.
    // Must shrink to empty (tracked by ExpectedFailListIsMinimal below).
    const std::set<std::string>& expectedFailures() const
    {
        static const std::set<std::string> failures = {
        };

        return failures;
    }

    // Scripts that must NOT be executed at all - independent of the
    // front-end. Unlike expectedFailures these are never run, so a broken
    // script here cannot take the whole suite down.
    const std::set<std::string>& skippedScripts() const
    {
        static const std::set<std::string> skipped = {
            // Imports "helper_add", which only exists as a stale .pgc left in
            // the build dir by old runs; ChunkSerializer OOMs deserializing it
            // (both front-ends - the import path is shared code). The
            // matching ScriptTestBench test is disabled for the same reason.
            "compiled_import.pg",
        };

        return skipped;
    }

    void registerNativeFunctions(VM& vm)
    {
        // Same registration set as ScriptTestBench so scripts behave identically
        vm.addNativeModule("math", MathModule());
        vm.addNativeModule("mathNative", MathModule());
        vm.addNativeModule("file", FileModule());
        vm.addNativeModule("string", StringModule());
        vm.addNativeModule("algorithm", AlgorithmModule());

        vm.registerNative("__toString", [](VM* vmInstance, int argCount, Value* args) -> Value {
            if (argCount != 1) return makeBoolValue(false);

            std::string str;

            if (IS_STRING(args[0]))
                str = vmInstance->asString(args[0]);
            else if (IS_INT(args[0]))
                str = std::to_string(AS_INT(args[0]));
            else if (IS_DOUBLE(args[0]))
                str = std::to_string(AS_DOUBLE(args[0]));
            else if (IS_BOOL(args[0]))
                str = AS_BOOL(args[0]) ? "true" : "false";
            else
                str = "<unknown>";

            return vmInstance->createString(str);
        });
    }

    struct RunOutcome
    {
        InterpretResult result;
        std::string output;
    };

    RunOutcome runWithFrontEnd(const std::string& scriptPath, ScriptFrontEnd frontEnd)
    {
        // Full O3 configuration (bytecode passes, decode fusion, AST passes)
        // so the differential also validates every optimization layer
        EntitySystem ecs;
        ecs.setVMOptimizationLevel(VmOptimizationLevel::O3);
        ecs.setVMFrontEnd(frontEnd);

        VM vm;
        ecs.setupVm(vm);

        registerNativeFunctions(vm);

        RunOutcome outcome;
        outcome.result = vm.interpretFromFile(scriptPath);
        outcome.output = trim(vm.testOutput);

        return outcome;
    }

    std::vector<std::string> collectScripts() const
    {
        std::vector<std::string> scripts;

        for (const auto& entry : std::filesystem::directory_iterator(scriptsDir))
        {
            if (entry.path().extension() == ".pg")
                scripts.push_back(entry.path().string());
        }

        std::sort(scripts.begin(), scripts.end());

        return scripts;
    }

    static std::string trim(const std::string& str)
    {
        size_t first = str.find_first_not_of(" \t\n\r");
        if (first == std::string::npos) return "";
        size_t last = str.find_last_not_of(" \t\n\r");
        return str.substr(first, last - first + 1);
    }
};

TEST_F(FrontendDifferential, PrattAndAstFrontEndsAgree)
{
    MockLogger<TerminalSink> logger;

    auto scripts = collectScripts();

    ASSERT_FALSE(scripts.empty()) << "No scripts found in " << scriptsDir
        << " - check the test working directory";

    size_t compared = 0, skipped = 0;
    std::vector<std::string> mismatches;

    for (const auto& script : scripts)
    {
        auto name = std::filesystem::path(script).filename().string();

        if (expectedFailures().count(name) > 0 or skippedScripts().count(name) > 0)
        {
            skipped++;
            continue;
        }

        auto pratt = runWithFrontEnd(script, ScriptFrontEnd::Pratt);
        auto ast = runWithFrontEnd(script, ScriptFrontEnd::Ast);

        if (pratt.result != ast.result)
        {
            mismatches.push_back(name + ": result mismatch (Pratt="
                + std::to_string(static_cast<int>(pratt.result))
                + ", Ast=" + std::to_string(static_cast<int>(ast.result)) + ")");
        }
        else if (pratt.output != ast.output)
        {
            mismatches.push_back(name + ": output mismatch\n    Pratt: ["
                + pratt.output + "]\n    Ast:   [" + ast.output + "]");
        }

        compared++;
    }

    std::string report;
    for (const auto& mismatch : mismatches)
        report += "  " + mismatch + "\n";

    EXPECT_TRUE(mismatches.empty())
        << mismatches.size() << " script(s) diverge between front-ends:\n" << report;

    std::cout << "[FrontendDifferential] compared " << compared << " scripts, "
              << skipped << " on the expected-fail list, "
              << mismatches.size() << " mismatches" << std::endl;
}

// Scripts on the expected-fail list must actually diverge; once both
// front-ends agree the entry must be removed so coverage only grows.
TEST_F(FrontendDifferential, ExpectedFailListIsMinimal)
{
    MockLogger<TerminalSink> logger;

    std::vector<std::string> stale;

    for (const auto& name : expectedFailures())
    {
        auto path = std::string(scriptsDir) + "/" + name;

        auto pratt = runWithFrontEnd(path, ScriptFrontEnd::Pratt);
        auto ast = runWithFrontEnd(path, ScriptFrontEnd::Ast);

        if (pratt.result == ast.result and pratt.output == ast.output)
            stale.push_back(name);
    }

    std::string report;
    for (const auto& name : stale)
        report += "  " + name + "\n";

    EXPECT_TRUE(stale.empty())
        << "These scripts now agree between front-ends and must be removed from the expected-fail list:\n"
        << report;
}

} // namespace test
} // namespace pg
