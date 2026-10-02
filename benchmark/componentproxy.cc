/**
 * @file componentproxy.cc
 * @brief Benchmark of the component proxies, the bridge between scripts and C++ components
 *
 * Measures each step of the bridge on its own:
 *   1. Reading a component property from a script
 *   2. Writing a component property from a script
 *   3. Creating a proxy on a component
 *   4. Building the script table of an entity
 */

#include "gtest/gtest.h"

#include <chrono>
#include <iostream>
#include <memory>
#include <string>

#include "ECS/entitysystem.h"
#include "2D/position.h"
#include "Compiler/vm.h"
#include "Compiler/ecsserialization.h"

namespace pg
{
    namespace benchmark
    {
        namespace
        {
            constexpr int NbScriptAccesses = 1000000;
            constexpr int NbCreations = 100000;

            void printResult(const std::string& name, long long nanoseconds, int nbOperations)
            {
                std::cout << "[" << name << "] " << nbOperations << " operations: "
                          << (nanoseconds / 1000) << " us ("
                          << (nanoseconds / static_cast<double>(nbOperations)) << " ns/operation)"
                          << std::endl;
            }

            // Run a script against a proxy on a standalone PositionComponent, stored in the global "pos"
            void benchmarkScript(const std::string& name, const std::string& source)
            {
                EntitySystem ecs("benchmark_component_proxy");

                auto vm = std::make_unique<VM>();

                ecs.setupVm(*vm);

                auto metadata = ComponentProxyRegistry::instance().findMetadata("PositionComponent");

                ASSERT_NE(metadata, nullptr);

                PositionComponent component;

                component.x = 1.0f;

                vm->defineGlobal("pos", ComponentProxy::createProxy(vm.get(), metadata, &component));

                auto start = std::chrono::high_resolution_clock::now();

                auto result = vm->interpretFromText(source);

                auto end = std::chrono::high_resolution_clock::now();

                ASSERT_EQ(result, InterpretResult::OK);

                printResult(name, std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count(), NbScriptAccesses);
            }
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(component_proxy_benchmark, script_loop_baseline)
        {
            // Same loop without any proxy access, to subtract from the two next results
            benchmarkScript("Loop baseline",
                "var sum = 0.0\n"
                "var local = 1.0\n"
                "for (var i = 0; i < " + std::to_string(NbScriptAccesses) + "; i = i + 1)\n"
                "{\n"
                "    sum = sum + local\n"
                "}\n");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(component_proxy_benchmark, script_read)
        {
            benchmarkScript("Proxy read",
                "var sum = 0.0\n"
                "for (var i = 0; i < " + std::to_string(NbScriptAccesses) + "; i = i + 1)\n"
                "{\n"
                "    sum = sum + pos.x\n"
                "}\n");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(component_proxy_benchmark, script_write)
        {
            benchmarkScript("Proxy write",
                "for (var i = 0; i < " + std::to_string(NbScriptAccesses) + "; i = i + 1)\n"
                "{\n"
                "    pos.x = i\n"
                "}\n");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(component_proxy_benchmark, proxy_creation)
        {
            EntitySystem ecs("benchmark_component_proxy");

            auto vm = std::make_unique<VM>();

            ecs.setupVm(*vm);

            auto metadata = ComponentProxyRegistry::instance().findMetadata("PositionComponent");

            ASSERT_NE(metadata, nullptr);

            PositionComponent component;

            auto start = std::chrono::high_resolution_clock::now();

            for (int i = 0; i < NbCreations; ++i)
            {
                Value proxy = ComponentProxy::createProxy(vm.get(), metadata, &component);

                vm->releaseAndDelete(proxy);
            }

            auto end = std::chrono::high_resolution_clock::now();

            printResult("Proxy creation", std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count(), NbCreations);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(component_proxy_benchmark, entity_table)
        {
            EntitySystem ecs("benchmark_component_proxy");

            auto vm = std::make_unique<VM>();

            ecs.setupVm(*vm);

            ecs.createSystem<PositionComponentSystem>();

            auto entity = ecs.createEntity();

            entity->attach<PositionComponent>();

            auto start = std::chrono::high_resolution_clock::now();

            for (int i = 0; i < NbCreations; ++i)
            {
                Value table = serializeEntityToTable(vm.get(), &ecs, entity.entity);

                vm->releaseAndDelete(table);
            }

            auto end = std::chrono::high_resolution_clock::now();

            printResult("Entity table", std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count(), NbCreations);
        }
    }
}
