/**
 * @file frontend_comparison.cc
 * @brief Benchmark: Pratt front-end vs AST front-end
 *
 * Runs the same scripts through both compiler front-ends and reports
 * compile time and total run time per front-end. This is the data that
 * decides which front-end survives long-term.
 */

#include "gtest/gtest.h"

#include <chrono>
#include <filesystem>
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
            struct FrontendTiming
            {
                int64_t compileUs = -1;
                int64_t totalUs = -1;
                bool ok = false;
            };

            const char* frontEndName(ScriptFrontEnd fe)
            {
                return fe == ScriptFrontEnd::Ast ? "Ast" : "Pratt";
            }

            /**
             * Time one script with one front-end: compile-only pass first,
             * then a full run, both on fresh O3-configured VMs.
             */
            FrontendTiming timeFrontEnd(const std::string& scriptPath, ScriptFrontEnd frontEnd, int count)
            {
                FrontendTiming timing;

                EntitySystem ecs;
                ecs.setVMOptimizationLevel(VmOptimizationLevel::O3);
                ecs.setVMFrontEnd(frontEnd);

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

                    timing.compileUs = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
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

                    timing.totalUs = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
                }

                timing.ok = true;

                return timing;
            }

            void compareOnScript(const std::string& name, const std::string& scriptPath, int count = -1)
            {
                if (not std::filesystem::exists(scriptPath))
                {
                    std::cout << "  " << name << ": script not found (" << scriptPath << "), skipped" << std::endl;
                    return;
                }

                auto pratt = timeFrontEnd(scriptPath, ScriptFrontEnd::Pratt, count);
                auto ast = timeFrontEnd(scriptPath, ScriptFrontEnd::Ast, count);

                std::cout << "  " << std::left << std::setw(36) << name;

                if (not pratt.ok or not ast.ok)
                {
                    std::cout << (pratt.ok ? "" : " [Pratt FAILED]") << (ast.ok ? "" : " [Ast FAILED]") << std::endl;
                    return;
                }

                auto ratio = [](int64_t a, int64_t b) -> double
                {
                    return b > 0 ? static_cast<double>(a) / static_cast<double>(b) : 0.0;
                };

                std::cout << " compile P/A: " << std::setw(7) << pratt.compileUs << " / "
                          << std::setw(7) << ast.compileUs << " us"
                          << " (x" << std::fixed << std::setprecision(2) << ratio(ast.compileUs, pratt.compileUs) << ")"
                          << " | total P/A: " << std::setw(8) << pratt.totalUs << " / "
                          << std::setw(8) << ast.totalUs << " us"
                          << " (x" << ratio(ast.totalUs, pratt.totalUs) << ")"
                          << std::endl;
            }
        }

        TEST(FrontendComparison, BenchScripts)
        {
            std::cout << "\n=== Front-end comparison (Pratt vs Ast), O3, bench scripts ===" << std::endl;
            std::cout << "  ratios are Ast/Pratt: > 1.0 means the AST front-end is slower" << std::endl;

            const int count = 100000;

            compareOnScript("raw_iteration", "test/bench/bench_01_raw_iteration.pg", count);
            compareOnScript("table_property_access", "test/bench/bench_02_table_property_access.pg", count);
            compareOnScript("struct_with_metamethods", "test/bench/bench_03_struct_with_metamethods.pg", count);
            compareOnScript("foreach_iteration", "test/bench/bench_04_foreach_iteration.pg", count);
            compareOnScript("vector_foreach", "test/bench/bench_05_vector_foreach.pg", count);
            compareOnScript("native_metamethod", "test/bench/bench_06_native_metamethod.pg", count);
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
                int64_t totalUs = 0;
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
                totalUs = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

                std::cout << "  " << std::left << std::setw(6) << frontEndName(frontEnd)
                          << ": " << totalUs << " us for " << compiled << " scripts ("
                          << failed << " failed to compile)" << std::endl;
            }
        }
    }
}
