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
    int64_t     count = 0;

    for (int i = 1; i < argc; ++i)
    {
        std::string a = argv[i];
        if (a.rfind("--script=", 0) == 0)
            scriptPath = a.substr(std::strlen("--script="));
        else if (a.rfind("--count=", 0) == 0)
            count = std::stoll(a.substr(std::strlen("--count=")));
        else if (a == "--help" || a == "-h")
        {
            std::fprintf(stderr,
                "Usage: %s --script=PATH [--count=N]\n"
                "\n"
                "Runs a PgScript file with the bench `now()` native pre-registered.\n"
                "If --count is given, the script can read it via the global `count`.\n",
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
        vm.globals["count"] = makeIntValue(count);
    }

    InterpretResult result = vm.interpretFromFile(scriptPath);
    if (result != InterpretResult::OK)
    {
        std::fprintf(stderr, "pgscript_run: interpretation failed (code %d)\n",
                     static_cast<int>(result));
        return 2;
    }

    // __dprint accumulates into vm.testOutput; flush it to stdout so the
    // bench harness can read the script's printed value.
    std::fwrite(vm.testOutput.data(), 1, vm.testOutput.size(), stdout);
    return 0;
}
