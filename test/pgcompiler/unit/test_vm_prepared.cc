// Tests for the persistent-VM "prepare once, run many" fast path:
//   VM::prepareCachedFunction() — deserialize + decode + freeze constants ONCE
//   VM::runPreparedFunction()   — execute the prepared function repeatedly
//
// This is a rework of the VM's per-run entry path (system hooks used to
// deserialize + decode the bytecode on EVERY frame/event). These tests pin
// down the behaviour that makes reuse correct:
//   - results match the one-shot interpretFromCachedBytecode() path
//   - repeated runs recompute correctly and reuse persistent globals/natives
//   - per-run temporaries are ref-counted and RECLAIMED between runs (no leak),
//     i.e. the constant line is frozen exactly once, not on every run
//   - the value stack and closure pool do not grow across runs

#include "gtest/gtest.h"

#include "Compiler/vm.h"
#include "Compiler/value_nanbox.h"
#include "Compiler/chunk.h"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <string>
#include <vector>

namespace pg {
namespace test {

namespace {

std::atomic<uint64_t> g_tmpCounter{0};

// Compile `source` to the same cached-bytecode blob the engine feeds to
// interpretFromCachedBytecode / prepareCachedFunction. `setupCompiler` lets a
// test register the native names the script references so compilation resolves
// them (the bodies never run here — compileOnly=true).
std::vector<char> compileToBytecode(const std::string& source,
                                    const std::function<void(VM&)>& setupCompiler = {})
{
    VM compiler;
    if (setupCompiler)
        setupCompiler(compiler);

    namespace fs = std::filesystem;
    const uint64_t id = g_tmpCounter.fetch_add(1);
    fs::path tmp = fs::temp_directory_path() / ("pge_prepared_test_" + std::to_string(id) + ".pgc");

    InterpretResult r = compiler.interpretFromText(source, /*compileOnly*/ true, tmp.string());
    EXPECT_EQ(r, InterpretResult::OK) << "compileToBytecode failed for source:\n" << source;

    std::ifstream in(tmp.string(), std::ios::binary);
    std::vector<char> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();

    std::error_code ec;
    fs::remove(tmp, ec);

    EXPECT_FALSE(bytes.empty()) << "compiled bytecode is empty for source:\n" << source;
    return bytes;
}

int64_t readIntGlobal(VM& vm, const std::string& name)
{
    VM::GlobalCell* cell = vm.findGlobalCell(name);
    EXPECT_NE(cell, nullptr) << "global '" << name << "' not found";
    if (cell == nullptr)
        return INT64_MIN;
    EXPECT_TRUE(cell->defined) << "global '" << name << "' not defined";
    EXPECT_TRUE(IS_INT(cell->value)) << "global '" << name << "' is not an int";
    return AS_INT(cell->value);
}

// Live refcount of a heap value (-1 if not ref-counted).
int refCountOf(VM& vm, Value v)
{
    if (not requiresRefCount(v))
        return -1;
    const uint32_t index = GET_INDEX(v);
    auto& refCounts = vm.pools.getRefCountVector(v);
    return index < refCounts.size() ? static_cast<int>(refCounts[index]) : -1;
}

} // namespace

// ---------------------------------------------------------------------------
// Parity: the prepared path yields the same result as the one-shot path.
// ---------------------------------------------------------------------------
TEST(VMPreparedTest, PreparedMatchesOneShot)
{
    auto bytes = compileToBytecode("var answer = 21 + 21");

    // One-shot path (deserialize + decode + run + cleanup, every call).
    VM oneShot;
    ASSERT_EQ(oneShot.interpretFromCachedBytecode(bytes, 0, "parity"), InterpretResult::OK);
    EXPECT_EQ(readIntGlobal(oneShot, "answer"), 42);

    // Prepared path (deserialize + decode + freeze ONCE, then run).
    VM prepared;
    ObjFunction* fn = prepared.prepareCachedFunction(bytes, "parity");
    ASSERT_NE(fn, nullptr);
    ASSERT_EQ(prepared.runPreparedFunction(fn, 0, "parity"), InterpretResult::OK);
    EXPECT_EQ(readIntGlobal(prepared, "answer"), 42);

    prepared.cleanupFunction(fn);
}

// ---------------------------------------------------------------------------
// Repeated runs recompute correctly and reuse the persistent global slot.
// ---------------------------------------------------------------------------
TEST(VMPreparedTest, RepeatedRunsRecomputeCorrectly)
{
    auto bytes = compileToBytecode("var answer = 6 * 7");

    VM vm;
    ObjFunction* fn = vm.prepareCachedFunction(bytes, "repeat");
    ASSERT_NE(fn, nullptr);

    for (int i = 0; i < 25; ++i)
    {
        ASSERT_EQ(vm.runPreparedFunction(fn, 0, "repeat"), InterpretResult::OK) << "run " << i;
        EXPECT_EQ(readIntGlobal(vm, "answer"), 42) << "run " << i;
    }

    vm.cleanupFunction(fn);
}

// ---------------------------------------------------------------------------
// A native registered on the persistent VM stays bound across every run.
// (decode() resolves the global slot for the native name once, at prepare.)
// ---------------------------------------------------------------------------
TEST(VMPreparedTest, NativeFunctionReuseAcrossRuns)
{
    int callCount = 0;

    auto bytes = compileToBytecode(
        "var c = bump()",
        [](VM& compiler) {
            // Name-only registration so compilation resolves `bump`.
            compiler.registerNative("bump", [](VM*, int, Value*) -> Value { return makeIntValue(0); });
        });

    VM vm;
    vm.registerNative("bump", [&callCount](VM*, int, Value*) -> Value {
        return makeIntValue(++callCount);
    });

    ObjFunction* fn = vm.prepareCachedFunction(bytes, "native");
    ASSERT_NE(fn, nullptr);

    constexpr int N = 5;
    for (int i = 1; i <= N; ++i)
    {
        ASSERT_EQ(vm.runPreparedFunction(fn, 0, "native"), InterpretResult::OK) << "run " << i;
        EXPECT_EQ(callCount, i) << "native not invoked exactly once per run";
        EXPECT_EQ(readIntGlobal(vm, "c"), i) << "run " << i;
    }

    vm.cleanupFunction(fn);
}

// ---------------------------------------------------------------------------
// The memory-footprint contract: per-run heap temporaries are reclaimed, so
// pool live-counts reach a steady state and STAY there. If the constant line
// were re-frozen on every run, each run's result string would be pinned as a
// permanent constant and the string pool would grow by one every run.
// ---------------------------------------------------------------------------
TEST(VMPreparedTest, PerRunHeapTemporariesAreReclaimed)
{
    // `makeBig` returns a fresh heap string (>5 chars, so not inlined) each run.
    auto makeBigCompiler = [](VM& compiler) {
        compiler.registerNative("makeBig", [](VM* v, int, Value*) -> Value {
            return v->createString("heap_payload_0123456789");
        });
    };

    auto bytes = compileToBytecode("var s = makeBig()", makeBigCompiler);

    VM vm;
    vm.registerNative("makeBig", [](VM* v, int, Value*) -> Value {
        return v->createString("heap_payload_0123456789");
    });

    ObjFunction* fn = vm.prepareCachedFunction(bytes, "mem");
    ASSERT_NE(fn, nullptr);

    constexpr int N = 200;
    std::vector<size_t> stringLive;
    stringLive.reserve(N);

    for (int i = 0; i < N; ++i)
    {
        ASSERT_EQ(vm.runPreparedFunction(fn, 0, "mem"), InterpretResult::OK) << "run " << i;
        stringLive.push_back(vm.pools.stringPool.getNbElements());
    }

    // Steady state from run 2 onward: identical live count every run — no leak.
    const size_t steady = stringLive[1];
    for (int i = 1; i < N; ++i)
    {
        EXPECT_EQ(stringLive[i], steady)
            << "string pool live count changed at run " << i
            << " (expected steady " << steady << ", got " << stringLive[i] << ")";
    }

    // And no monotonic growth between the first steady sample and the last.
    EXPECT_EQ(stringLive.back(), steady) << "string pool grew across runs (leak)";

    // The single live global string is held at refcount 1 (not over-retained).
    VM::GlobalCell* cell = vm.findGlobalCell("s");
    ASSERT_NE(cell, nullptr);
    ASSERT_TRUE(cell->defined);
    EXPECT_EQ(refCountOf(vm, cell->value), 1);

    vm.cleanupFunction(fn);
}

// ---------------------------------------------------------------------------
// The value stack returns to empty and no call frames leak after each run.
// ---------------------------------------------------------------------------
TEST(VMPreparedTest, StackAndFramesDoNotGrowAcrossRuns)
{
    auto bytes = compileToBytecode("var answer = 1 + 2 + 3 + 4");

    VM vm;
    ObjFunction* fn = vm.prepareCachedFunction(bytes, "stack");
    ASSERT_NE(fn, nullptr);

    for (int i = 0; i < 20; ++i)
    {
        ASSERT_EQ(vm.runPreparedFunction(fn, 0, "stack"), InterpretResult::OK) << "run " << i;
        EXPECT_EQ(vm.stack.size(), 0u) << "value stack not balanced after run " << i;
        EXPECT_EQ(vm.frameCount, 0) << "call frames leaked after run " << i;
    }

    vm.cleanupFunction(fn);
}

// ---------------------------------------------------------------------------
// runPreparedFunction() allocates one closure per run; it must be freed on
// return, so the closure pool live-count is flat across runs.
// ---------------------------------------------------------------------------
TEST(VMPreparedTest, ClosurePoolStableAcrossRuns)
{
    auto bytes = compileToBytecode("var answer = 9 * 9");

    VM vm;
    ObjFunction* fn = vm.prepareCachedFunction(bytes, "closure");
    ASSERT_NE(fn, nullptr);

    // Warm up once so the steady state is established.
    ASSERT_EQ(vm.runPreparedFunction(fn, 0, "closure"), InterpretResult::OK);
    const size_t steady = vm.pools.closurePool.getNbElements();

    for (int i = 0; i < 50; ++i)
    {
        ASSERT_EQ(vm.runPreparedFunction(fn, 0, "closure"), InterpretResult::OK) << "run " << i;
        EXPECT_EQ(vm.pools.closurePool.getNbElements(), steady)
            << "closure pool grew at run " << i << " (per-run closure not freed)";
    }

    vm.cleanupFunction(fn);
}

// ---------------------------------------------------------------------------
// A hot-reload swap: re-preparing new bytecode on the same VM after releasing
// the old function must run the new code and not leave the old one live.
// ---------------------------------------------------------------------------
TEST(VMPreparedTest, ReprepareAfterCleanupRunsNewBytecode)
{
    VM vm;

    auto bytesA = compileToBytecode("var answer = 10 + 10");
    ObjFunction* fnA = vm.prepareCachedFunction(bytesA, "reloadA");
    ASSERT_NE(fnA, nullptr);
    ASSERT_EQ(vm.runPreparedFunction(fnA, 0, "reloadA"), InterpretResult::OK);
    EXPECT_EQ(readIntGlobal(vm, "answer"), 20);
    vm.cleanupFunction(fnA);

    auto bytesB = compileToBytecode("var answer = 30 + 30");
    ObjFunction* fnB = vm.prepareCachedFunction(bytesB, "reloadB");
    ASSERT_NE(fnB, nullptr);
    ASSERT_EQ(vm.runPreparedFunction(fnB, 0, "reloadB"), InterpretResult::OK);
    EXPECT_EQ(readIntGlobal(vm, "answer"), 60);
    vm.cleanupFunction(fnB);
}

} // namespace test
} // namespace pg
