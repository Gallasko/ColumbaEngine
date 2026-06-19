/**
 * @file pgscript_run.cpp
 * @brief Minimal CLI for invoking a PgScript file from the command line.
 *
 * Used by the script benchmark harness so PgScript can be measured the same
 * way as Python / Lua / Wren — as a subprocess invoked once per (scenario, N).
 *
 * Adds one bench-only native:
 *   now()  -> int64 ns from std::chrono::steady_clock
 *
 * The script sets `count` from the global passed via --count=N, runs its work,
 * and is expected to print its self-timed duration as the last line of stdout
 * (the harness parses the last integer on stdout).
 *
 * Usage:
 *   pgscript_run --script=path/to/scenario.pg --count=1000000
 */

#include "Compiler/vm.h"
#include "Compiler/compiler_debug.h"
#include "Compiler/object.h"

// Bytecode optimization passes — replicate the O3 pipeline that
// EntitySystem::setOptimizationPasses installs by default. Without these,
// PgScript runs unoptimized bytecode and the cross-language comparison is
// unfair to itself.
#include "Compiler/pass/basic_operator_local_indexing.h"
#include "Compiler/pass/comparison_local_indexing.h"
#include "Compiler/pass/constant_folding.h"
#include "Compiler/pass/constant_var_access.h"
#include "Compiler/pass/fuse_op_pop.h"
#include "Compiler/pass/set_local_pop_fusion.h"
#include "Compiler/pass/increment_optimization_pass.h"
#include "Compiler/pass/long_jump_optimization_pass.h"
#include "Compiler/pass/popping_jump_pass.h"
#include "Compiler/pass/remove_def_get_global_redunduncy.h"
#include "Compiler/pass/remove_useless_jump_pass.h"
#include "Compiler/pass/simplify_constant_pass.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>

using namespace pg;

static int64_t nowNs()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

// Print a single Value to stdout in a benchmark-friendly form. Numbers as
// plain digits (no trailing newline noise), one value per call.
static void printValueToStdout(VM* vm, Value v)
{
    if (IS_INT(v))       std::printf("%lld\n", static_cast<long long>(AS_INT(v)));
    else if (IS_DOUBLE(v)) std::printf("%g\n", AS_DOUBLE(v));
    else if (IS_BOOL(v)) std::printf("%s\n", AS_BOOL(v) ? "true" : "false");
    else if (IS_STRING(v)) std::printf("%s\n", vm->asString(v).c_str());
    else std::printf("<value>\n");
}

int main(int argc, char** argv)
{
    std::string scriptPath;
    int64_t     count         = 0;
    bool        optimize      = true;  // O3 by default — match EntitySystem
    bool        dumpBytecode  = false; // print disassembly for every compiled function
    bool        debugPasses   = false; // print each pass's effect (verbose)
    bool        profile       = false; // enable instruction-level profiling

    for (int i = 1; i < argc; ++i)
    {
        std::string a = argv[i];
        if (a.rfind("--script=", 0) == 0)
            scriptPath = a.substr(std::strlen("--script="));
        else if (a.rfind("--count=", 0) == 0)
            count = std::stoll(a.substr(std::strlen("--count=")));
        else if (a == "--no-opt")
            optimize = false;
        else if (a == "--dump-bytecode")
            dumpBytecode = true;
        else if (a == "--debug-passes")
            debugPasses = true;
        else if (a == "--profile")
            profile = true;
        else if (a == "--help" || a == "-h")
        {
            std::fprintf(stderr,
                "Usage: %s --script=PATH [--count=N] [options]\n"
                "\n"
                "Runs a PgScript file with the bench `now()` and `print()` natives.\n"
                "If --count is given, the script can read it via the global `count`.\n"
                "\n"
                "Options:\n"
                "  --no-opt          disable PgScript bytecode optimization passes\n"
                "  --dump-bytecode   disassemble every compiled function after passes\n"
                "  --debug-passes    print disassembly after each optimization pass\n"
                "  --profile         enable instruction profiling; print report at exit\n",
                argv[0]);
            return 0;
        }
        else
        {
            std::fprintf(stderr, "unknown arg: %s\n", a.c_str());
            return 1;
        }
    }

    if (scriptPath.empty())
    {
        std::fprintf(stderr, "error: --script=PATH is required\n");
        return 1;
    }

    VM vm;

    // Install the same O3 bytecode-optimization pipeline EntitySystem uses.
    // Default ON — that's what real PgScript users get. --no-opt for ablation.
    // Operand-elision specialization happens at decode time (decoded_fusion.h);
    // the passes below are pure bytecode peepholes.
    if (optimize)
    {
        vm.enableBytecodeOptimization();
        vm.enableDecodeFusion = true;
        vm.addOptimizationPass(std::make_unique<LongJumpOptimizationPass>());
        vm.addOptimizationPass(std::make_unique<PoppingJumpPass>());
        vm.addOptimizationPass(std::make_unique<RemoveUselessJumpPass>());
        vm.addOptimizationPass(std::make_unique<RemoveDefGetGlobalRedunduncy>());
        // Increment must run before FuseOpPop coalesces its trailing Pop
        // into a PopN.
        vm.addOptimizationPass(std::make_unique<IncrementOptimizationPass>());
        vm.addOptimizationPass(std::make_unique<FuseOpPop>());
        vm.addOptimizationPass(std::make_unique<ConstantFoldingPass>());
        vm.addOptimizationPass(std::make_unique<ConstantVarAccess>());
        vm.addOptimizationPass(std::make_unique<SimplifyConstantToShort>());
    }
    else
    {
        vm.disableBytecodeOptimization();
        vm.enableDecodeFusion = false;
    }

    if (debugPasses) vm.enableOptimizationDebugging();

    // Enable instruction profiling for either --dump-bytecode (which uses
    // compiledFunctions, populated only when profiling is on) or --profile
    // (which prints the per-opcode report at exit).
    if (dumpBytecode || profile) vm.enableProfiling();

    // now() -> int64 ns since steady_clock epoch. Bench-only; not part of the
    // engine's core natives. Used by scenarios for in-script self-timing.
    vm.registerNative("now", [](VM*, int /*argCount*/, Value* /*args*/) -> Value {
        return makeIntValue(nowNs());
    });

    // print(x) -> prints x to stdout. PgScript's __dprint goes to a test
    // buffer, which is the wrong sink for a CLI; this gives bench scripts
    // a plain stdout channel.
    vm.registerNative("print", [](VM* vm, int argCount, Value* args) -> Value {
        for (int i = 0; i < argCount; ++i) printValueToStdout(vm, args[i]);
        return makeIntValue(0);
    });

    if (count > 0)
    {
        vm.defineGlobal("count", makeIntValue(count));
    }

    // Execution still happens with --dump-bytecode: the function pointers
    // stored in compiledFunctions are only stable after full setup.
    InterpretResult result = vm.interpretFromFile(scriptPath);
    if (result != InterpretResult::OK)
    {
        std::fprintf(stderr, "pgscript_run: interpretation failed (code %d)\n",
                     static_cast<int>(result));
        return 2;
    }

    if (profile)
    {
        // sortByTime=false → sort by execution count, matching how the Python
        // analyzer reports its top opcodes. We want the count-dominant ops.
        vm.printProfilingReport(/*sortByTime=*/false);
    }

    if (dumpBytecode)
    {
        std::fprintf(stderr, "compiledFunctions.size() = %zu\n",
                     vm.compiledFunctions.size());
        for (auto* func : vm.compiledFunctions)
        {
            if (func == nullptr) { std::fprintf(stderr, "  <null>\n"); continue; }
            std::string name = func->name.empty() ? "<script>" : func->name;
            std::fprintf(stderr, "  %s  chunk.code.size=%zu  decodedChunk=%p\n",
                         name.c_str(), func->chunk.code.size(),
                         (void*)func->decodedChunk);
            disassembleChunk(&vm, func->chunk, name);
            std::printf("\n");
        }
    }

    return 0;
}
