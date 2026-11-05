/**
 * @file pgscript_run.cpp
 * @brief Minimal CLI for invoking a PgScript file from the command line.
 *
 * Used by the script benchmark harness so PgScript can be measured the same
 * way as Python / Lua / Wren — as a subprocess invoked once per (scenario, N).
 *
 * The VM is set up by EntitySystem::setupVm, as the game and the
 * PgCompiler CLI do: the same modules (math, algorithm, string, ...), the
 * same `print`, the same pass pipeline for the chosen -O level, and the AST
 * front-end unless --frontend=pratt. Adds one bench-only native:
 *   now()  -> int64 ns from std::chrono::steady_clock
 *
 * The script sets `count` from the global passed via --count=N, runs its work,
 * and is expected to print its self-timed duration as the last line of stdout
 * (the harness parses the last integer on stdout). Compile and runtime
 * errors go to stderr.
 *
 * Usage:
 *   pgscript_run --script=path/to/scenario.pg --count=1000000
 */

#include "logger.h"
#include "ECS/entitysystem.h"
#include "Compiler/vm.h"
#include "Compiler/compiler_debug.h"
#include "Compiler/object.h"

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

// Errors to stderr: stdout is the script's own output, which the harness
// reads, and the harness shows stderr when a run fails. Warnings are left out
// as in the CLI: the ECS's disabled SaveManager warns at exit that it cannot
// write the file ''.
class StderrSink : public Logger::LogSink
{
private:
    void processLog(const Logger::Info& log) override
    {
        if (log.level < Logger::InfoLevel::error)
            return;

        std::fprintf(stderr, "[Error]: %s\n", log.message.c_str());
    }
};

int main(int argc, char** argv)
{
    std::string scriptPath;
    int64_t     count         = 0;
    auto        optLevel      = VmOptimizationLevel::O3; // the engine's default
    bool        useAst        = true;  // the engine's default front-end
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
        else if (a == "--no-opt" || a == "-O0")
            optLevel = VmOptimizationLevel::O0;
        else if (a == "-O1")
            optLevel = VmOptimizationLevel::O1;
        else if (a == "-O2")
            optLevel = VmOptimizationLevel::O2;
        else if (a == "-O3")
            optLevel = VmOptimizationLevel::O3;
        else if (a == "--frontend=ast")
            useAst = true;
        else if (a == "--frontend=pratt")
            useAst = false;
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
                "Runs a PgScript file in a VM set up as the engine does, plus the bench\n"
                "`now()` native. If --count is given, the script can read it via the\n"
                "global `count`.\n"
                "\n"
                "Options:\n"
                "  -O0 .. -O3        optimization level (default: -O3, as the engine)\n"
                "  --no-opt          same as -O0\n"
                "  --frontend=ast    compile through the AST front-end (default)\n"
                "  --frontend=pratt  compile through the Pratt front-end\n"
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

    Logger::registerSink<StderrSink>();

    // Before the VM, so it outlives it, as in the CLI: the `ecs` module
    // points at it.
    EntitySystem ecs;
    VM vm;

    // Profiling first: setupVm times itself when it is on. --dump-bytecode
    // needs it too (compiledFunctions is filled only when profiling is on).
    if (dumpBytecode || profile) vm.enableProfiling();

    // Modules, natives and the pass pipeline for the level, as the engine
    // sets up every VM.
    ecs.setVMOptimizationLevel(optLevel);
    ecs.setVMFrontEnd(useAst ? ScriptFrontEnd::Ast : ScriptFrontEnd::Pratt);
    ecs.setupVm(vm);

    if (debugPasses) vm.enableOptimizationDebugging();

    // now() -> int64 ns since steady_clock epoch. Bench-only; not part of the
    // engine's core natives. Used by scenarios for in-script self-timing.
    vm.registerNative("now", [](VM*, int /*argCount*/, Value* /*args*/) -> Value {
        return makeIntValue(nowNs());
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
