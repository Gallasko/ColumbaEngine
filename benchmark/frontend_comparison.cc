/**
 * @file frontend_comparison.cc
 * @brief Benchmark: Pratt front-end vs AST front-end
 *
 * Runs the same scripts through both compiler front-ends and reports
 * compile time and execution time per front-end, scaled over increasing
 * workload counts like the ScriptPerformance benchmarks. This is the data
 * that decides which front-end survives long-term.
 *
 * Reading the output: ratios are Ast/Pratt, so > 1.00 means the AST
 * front-end is slower on that metric, < 1.00 means it is faster.
 */

#include "gtest/gtest.h"

#include <chrono>
#include <filesystem>
#include <functional>
#include <iostream>
#include <iomanip>
#include <vector>

#include "Compiler/vm.h"
#include "ECS/entitysystem.h"

#include "../test/mocklogger.h"

namespace pg
{
    namespace benchmark
    {
        namespace
        {
            constexpr int REPETITIONS = 3; // best-of-N to reduce noise

            struct FrontendTiming
            {
                int64_t compileUs = -1;
                int64_t execUs = -1;
                bool ok = false;
            };

            const char* frontEndName(ScriptFrontEnd fe)
            {
                return fe == ScriptFrontEnd::Ast ? "Ast" : "Pratt";
            }

            /** Optional per-benchmark ECS preparation (e.g. component systems a script needs) */
            using EcsSetup = std::function<void(EntitySystem&)>;

            /**
             * Time one script with one front-end on a fresh O3-configured VM:
             * compile-only pass, then a full run; execution time is the
             * difference. Best of REPETITIONS runs per metric.
             */
            FrontendTiming timeFrontEnd(const std::string& scriptPath, ScriptFrontEnd frontEnd, int count,
                                        const EcsSetup& setupEcs = nullptr)
            {
                FrontendTiming timing;

                EntitySystem ecs;
                ecs.setVMOptimizationLevel(VmOptimizationLevel::O3);
                ecs.setVMFrontEnd(frontEnd);

                if (setupEcs)
                    setupEcs(ecs);

                int64_t bestCompile = -1;
                int64_t bestTotal = -1;

                for (int rep = 0; rep < REPETITIONS; rep++)
                {
                    // Compile-only timing
                    {
                        VM vm;
                        ecs.setupVm(vm);

                        if (count > 0)
                            vm.defineGlobal("count", makeIntValue(count));

                        auto start = std::chrono::high_resolution_clock::now();
                        auto result = vm.interpretFromFile(scriptPath, true);
                        auto end = std::chrono::high_resolution_clock::now();

                        if (result != InterpretResult::OK)
                            return timing;

                        auto us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

                        if (bestCompile < 0 or us < bestCompile)
                            bestCompile = us;
                    }

                    // Full compile + run timing
                    {
                        VM vm;
                        ecs.setupVm(vm);

                        if (count > 0)
                            vm.defineGlobal("count", makeIntValue(count));

                        auto start = std::chrono::high_resolution_clock::now();
                        auto result = vm.interpretFromFile(scriptPath);
                        auto end = std::chrono::high_resolution_clock::now();

                        if (result != InterpretResult::OK)
                            return timing;

                        auto us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

                        if (bestTotal < 0 or us < bestTotal)
                            bestTotal = us;
                    }
                }

                timing.compileUs = bestCompile;
                timing.execUs = bestTotal > bestCompile ? bestTotal - bestCompile : 0;
                timing.ok = true;

                return timing;
            }

            double ratio(int64_t ast, int64_t pratt)
            {
                return pratt > 0 ? static_cast<double>(ast) / static_cast<double>(pratt) : 0.0;
            }

            /**
             * Compare both front-ends on one script over increasing counts,
             * ScriptPerformance-style.
             */
            void runScaledComparison(const std::string& name, const std::string& scriptPath,
                                     const std::vector<int>& counts, const EcsSetup& setupEcs = nullptr)
            {
                std::cout << "\n=== " << name << " (Pratt vs Ast, O3) ===" << std::endl;

                if (not std::filesystem::exists(scriptPath))
                {
                    std::cout << "  script not found (" << scriptPath << "), skipped" << std::endl;
                    return;
                }

                // Compile time does not depend on count - measure once
                auto prattRef = timeFrontEnd(scriptPath, ScriptFrontEnd::Pratt, counts.empty() ? -1 : counts.front(), setupEcs);
                auto astRef = timeFrontEnd(scriptPath, ScriptFrontEnd::Ast, counts.empty() ? -1 : counts.front(), setupEcs);

                if (not prattRef.ok or not astRef.ok)
                {
                    std::cout << (prattRef.ok ? "" : "  [Pratt FAILED]") << (astRef.ok ? "" : "  [Ast FAILED]") << std::endl;
                    return;
                }

                std::cout << "  compile: Pratt " << prattRef.compileUs << " us, Ast " << astRef.compileUs
                          << " us (x" << std::fixed << std::setprecision(2)
                          << ratio(astRef.compileUs, prattRef.compileUs) << ")" << std::endl;

                for (int count : counts)
                {
                    auto pratt = timeFrontEnd(scriptPath, ScriptFrontEnd::Pratt, count, setupEcs);
                    auto ast = timeFrontEnd(scriptPath, ScriptFrontEnd::Ast, count, setupEcs);

                    if (not pratt.ok or not ast.ok)
                    {
                        std::cout << "  Count " << count
                                  << (pratt.ok ? "" : " [Pratt FAILED]") << (ast.ok ? "" : " [Ast FAILED]") << std::endl;
                        continue;
                    }

                    std::cout << "  Count " << std::left << std::setw(7) << count
                              << " exec: Pratt " << std::setw(8) << pratt.execUs << " us, Ast "
                              << std::setw(8) << ast.execUs << " us (x"
                              << std::fixed << std::setprecision(2) << ratio(ast.execUs, pratt.execUs) << ")";

                    if (count > 0 and pratt.execUs > 0)
                    {
                        std::cout << "  [" << std::setprecision(3)
                                  << pratt.execUs / static_cast<double>(count) << " vs "
                                  << ast.execUs / static_cast<double>(count) << " us/item]";
                    }

                    std::cout << std::endl;
                }
            }

            // Same counts as the ScriptPerformance scaled benchmarks
            const std::vector<int> smallCounts = {100, 500, 1000, 2000, 5000};
            const std::vector<int> largeCounts = {1000, 5000, 10000, 25000, 50000};
        }

        TEST(FrontendComparison, RawIteration)
        {
            runScaledComparison("Raw Iteration (Vector Access)",
                                "test/bench/bench_01_raw_iteration.pg", largeCounts);
        }

        TEST(FrontendComparison, TablePropertyAccess)
        {
            runScaledComparison("Table Property Access",
                                "test/bench/bench_02_table_property_access.pg", largeCounts);
        }

        TEST(FrontendComparison, ScriptMetamethods)
        {
            runScaledComparison("Script Metamethods (__get/__set)",
                                "test/bench/bench_03_struct_with_metamethods.pg", largeCounts);
        }

        TEST(FrontendComparison, ForeachTable)
        {
            // Linear since the O(1) OP_Table_At fix (fieldNames slot->name
            // vector); a flat us/item across this sweep proves it stays that way
            runScaledComparison("Foreach Iteration (Table)",
                                "test/bench/bench_04_foreach_iteration.pg", largeCounts);
        }

        TEST(FrontendComparison, ForeachVector)
        {
            runScaledComparison("Foreach Iteration (Vector)",
                                "test/bench/bench_05_vector_foreach.pg", largeCounts);
        }

        TEST(FrontendComparison, LoopInvariantHoisting)
        {
            // First AST-level optimization: the Ast column should beat Pratt
            // here (invariant arithmetic hoisted out of the loop)
            runScaledComparison("Loop Invariant Hoisting",
                                "test/bench/bench_07_loop_invariant.pg", largeCounts);
        }

        TEST(FrontendComparison, StaticLoopEvaluation)
        {
            // The loop bound lives in the script source, so no count sweep:
            // the Ast column trades compile time (compile-time interpretation
            // of 100k iterations) for near-zero execution
            runScaledComparison("Static Loop Evaluation",
                                "test/bench/bench_08_static_loop.pg", {0});
        }

        TEST(FrontendComparison, NativeMetamethods)
        {
            // PRE-EXISTING BREAKAGE, surfaced by this harness: bench_06
            // queries the native 'Position' component through
            // ecs.getEntities, which only resolves script-defined
            // StandardComponents - the script runtime-errors on BOTH
            // front-ends (ScriptPerformance.NativeMetamethods hides this by
            // printing the failure without asserting). Fixing it needs a
            // name->owner lookup for native components (only the attach
            // registry exists today). Skipped until then.
            GTEST_SKIP() << "bench_06 needs native-component name lookup in ecs.getEntities";
        }

        TEST(FrontendComparison, EntityLoopLowering)
        {
            // Script-defined StandardComponent workload (self-contained; see
            // bench_09 header): the AST front-end lowers the getEntities
            // loop to lazy id iteration, skipping the eager per-entity
            // full-table materialization the Pratt front-end pays for
            runScaledComparison("Entity Loop (getEntities lazy lowering)",
                                "test/bench/bench_09_entity_loop.pg", smallCounts);
        }

        TEST(FrontendComparison, CompileTimeOverCorpus)
        {
            std::cout << "\n=== Front-end compile-time totals over the script corpus ===" << std::endl;

            const char* scriptsDir = "test/pgcompiler/scripts";

            if (not std::filesystem::exists(scriptsDir))
            {
                std::cout << "  corpus not found (" << scriptsDir << "), skipped" << std::endl;
                return;
            }

            for (auto frontEnd : {ScriptFrontEnd::Pratt, ScriptFrontEnd::Ast})
            {
                size_t compiled = 0, failed = 0;

                auto start = std::chrono::high_resolution_clock::now();

                for (const auto& entry : std::filesystem::directory_iterator(scriptsDir))
                {
                    if (entry.path().extension() != ".pg")
                        continue;

                    // Stale-.pgc import OOMs the deserializer (see the
                    // differential testbench skip list)
                    if (entry.path().filename() == "compiled_import.pg")
                        continue;

                    VM vm;
                    vm.setFrontEnd(frontEnd);

                    if (vm.interpretFromFile(entry.path().string(), true) == InterpretResult::OK)
                        compiled++;
                    else
                        failed++;
                }

                auto end = std::chrono::high_resolution_clock::now();
                auto totalUs = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

                std::cout << "  " << std::left << std::setw(6) << frontEndName(frontEnd)
                          << ": " << totalUs << " us for " << compiled << " scripts ("
                          << failed << " failed to compile)" << std::endl;
            }
        }
    }
}
