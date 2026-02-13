/**
 * @file script_performance.cc
 * @brief Benchmark for script language primitives
 *
 * Tests the performance of basic script operations to identify
 * bottlenecks in iteration, property access, and metamethods.
 */

#include "gtest/gtest.h"

#include <chrono>
#include <iostream>
#include <fstream>
#include <sstream>

#include "Compiler/vm.h"
#include "ECS/entitysystem.h"

#include "../test/mocklogger.h"

namespace pg
{
    namespace benchmark
    {
        /**
         * @brief Helper to run a script file and measure execution time
         */
        int64_t runScriptBenchmark(const std::string& scriptPath, int count = -1)
        {
            // MockLogger<TerminalSink> logger;

            VM vm;

            // If count is provided, add it as a global variable
            if (count > 0)
            {
                vm.globals["count"] = makeIntValue(count);
            }

            // Measure execution time
            auto start = std::chrono::high_resolution_clock::now();

            InterpretResult result = vm.interpretFromFile(scriptPath);

            auto end = std::chrono::high_resolution_clock::now();

            if (result != InterpretResult::OK)
            {
                std::cerr << "Script interpretation failed with result: " << static_cast<int>(result) << std::endl;
                return -1;
            }

            return std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        }

        /**
         * @brief Run a script benchmark with increasing counts
         */
        void runScaledBenchmark(const std::string& name, const std::string& scriptPath,
                                const std::vector<int>& counts)
        {
            std::cout << "\n=== " << name << " ===" << std::endl;

            for (int count : counts)
            {
                auto duration = runScriptBenchmark(scriptPath, count);
                if (duration >= 0)
                {
                    std::cout << "  Count " << count << ": " << duration << " μs";

                    if (count > 0)
                    {
                        double perItem = duration / static_cast<double>(count);
                        std::cout << " (" << perItem << " μs/item)";
                    }

                    std::cout << std::endl;
                }
            }
        }

        // Test counts for scaled benchmarks
        const std::vector<int> smallCounts = {100, 500, 1000, 2000, 5000};
        const std::vector<int> largeCounts = {1000, 5000, 10000, 25000, 50000};

        /**
         * @brief Benchmark 1: Raw iteration over vectors
         *
         * Tests baseline for loop and vector access performance
         */
        TEST(ScriptPerformance, RawIteration)
        {
            runScaledBenchmark("Raw Iteration (Vector Access)",
                             "test/bench/bench_01_raw_iteration.pg",
                             largeCounts);
        }

        /**
         * @brief Benchmark 2: Table property access
         *
         * Tests overhead of map/table field lookups
         */
        TEST(ScriptPerformance, TablePropertyAccess)
        {
            runScaledBenchmark("Table Property Access",
                             "test/bench/bench_02_table_property_access.pg",
                             smallCounts);
        }

        /**
         * @brief Benchmark 3: Script metamethods
         *
         * Tests performance of __get/__set defined in script
         */
        TEST(ScriptPerformance, ScriptMetamethods)
        {
            runScaledBenchmark("Script Metamethods (__get/__set)",
                             "test/bench/bench_03_struct_with_metamethods.pg",
                             smallCounts);
        }

        /**
         * @brief Benchmark 4: Foreach over table
         *
         * Tests for-in loop performance with table iteration
         */
        TEST(ScriptPerformance, ForeachTable)
        {
            runScaledBenchmark("Foreach Iteration (Table)",
                             "test/bench/bench_04_foreach_iteration.pg",
                             smallCounts);
        }

        /**
         * @brief Benchmark 5: Foreach over vector
         *
         * Tests for-in loop performance with vector iteration
         */
        TEST(ScriptPerformance, ForeachVector)
        {
            runScaledBenchmark("Foreach Iteration (Vector)",
                             "test/bench/bench_05_vector_foreach.pg",
                             smallCounts);
        }

        /**
         * @brief Benchmark 6: Native metamethods (C++ proxies)
         *
         * Tests performance of native __get/__set with real component proxies
         * This requires ECS setup
         */
        TEST(ScriptPerformance, NativeMetamethods)
        {
            std::cout << "\n=== Native Metamethods (Component Proxies) ===" << std::endl;

            for (int count : smallCounts)
            {
                EntitySystem ecs("bench_native");
                VM vm;
                ecs.setupVm(vm);

                // Pass count to script
                vm.globals["count"] = makeIntValue(count);

                auto start = std::chrono::high_resolution_clock::now();

                InterpretResult result = vm.interpretFromFile("test/bench/bench_06_native_metamethod.pg");

                auto end = std::chrono::high_resolution_clock::now();

                if (result != InterpretResult::OK)
                {
                    std::cerr << "Script interpretation failed with result: " << static_cast<int>(result) << std::endl;
                    continue;
                }

                auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

                std::cout << "  Count " << count << ": " << duration << " μs";

                if (count > 0)
                {
                    double perItem = duration / static_cast<double>(count);
                    std::cout << " (" << perItem << " μs/item)";
                }

                std::cout << std::endl;
            }
        }

        /**
         * @brief Comparison test - runs all benchmarks with same count
         */
        TEST(ScriptPerformance, Comparison)
        {
            const int testCount = 2000;

            std::cout << "\n========================================" << std::endl;
            std::cout << "PERFORMANCE COMPARISON (" << testCount << " iterations)" << std::endl;
            std::cout << "========================================" << std::endl;

            struct BenchResult {
                std::string name;
                int64_t duration;
            };

            std::vector<BenchResult> results;

            // Run each benchmark
            auto duration1 = runScriptBenchmark("test/bench/bench_01_raw_iteration.pg", testCount);
            if (duration1 >= 0) results.push_back({"Raw Iteration", duration1});

            auto duration2 = runScriptBenchmark("test/bench/bench_02_table_property_access.pg", testCount);
            if (duration2 >= 0) results.push_back({"Table Access", duration2});

            auto duration3 = runScriptBenchmark("test/bench/bench_03_struct_with_metamethods.pg", testCount);
            if (duration3 >= 0) results.push_back({"Script Metamethods", duration3});

            auto duration5 = runScriptBenchmark("test/bench/bench_05_vector_foreach.pg", testCount);
            if (duration5 >= 0) results.push_back({"Foreach (Vector)", duration5});

            // Print results
            std::cout << "\nResults:" << std::endl;
            std::cout << "----------------------------------------" << std::endl;

            int64_t baseline = results.empty() ? 1 : results[0].duration;

            for (const auto& result : results)
            {
                double perItem = result.duration / static_cast<double>(testCount);
                double overhead = ((result.duration - baseline) * 100.0) / baseline;

                std::cout << result.name << ":" << std::endl;
                std::cout << "  Total: " << result.duration << " μs" << std::endl;
                std::cout << "  Per item: " << perItem << " μs" << std::endl;
                std::cout << "  vs Baseline: " << overhead << "%" << std::endl;
                std::cout << std::endl;
            }
        }

    } // namespace benchmark
} // namespace pg
