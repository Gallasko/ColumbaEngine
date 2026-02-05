/**
 * @file serialization.cc
 * @brief Benchmark for component serialization vs proxy system
 *
 * Tests the performance of entity/component serialization for scripts
 * with increasing numbers of Position components.
 */

#include "gtest/gtest.h"

#include <chrono>
#include <iostream>

#include "ECS/entitysystem.h"
#include "2D/position.h"
#include "Compiler/vm.h"
#include "Compiler/ecsserialization.h"
#include "ECS/standardsystem.h"

#include "../test/mocklogger.h"

namespace pg
{
    namespace benchmark
    {
        // Test with various entity counts
        auto entityCounts = {10, 100, 500, 1000, 2000, 5000, 10000, 50000};

        /**
         * @brief Benchmark full serialization of Position components
         *
         * This tests the current implementation where all component fields
         * are copied into script tables with native function setters.
         */
        void benchmarkFullSerialization(unsigned int entityCount)
        {
            EntitySystem ecs("benchmark_serialization");

            auto start = std::chrono::high_resolution_clock::now();

            VM vm;
            ecs.setupVm(vm);

            auto end = std::chrono::high_resolution_clock::now();
            auto vm_duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

            auto sys = ecs.createSystem<PositionComponentSystem>();

            // Create entities with Position components
            std::vector<Entity*> entities;
            entities.reserve(entityCount);

            for (unsigned int i = 0; i < entityCount; ++i)
            {
                auto entity = ecs.createEntity();
                auto pos = entity->attach<PositionComponent>();

                // Set component values
                pos->x = static_cast<float>(i);
                pos->y = static_cast<float>(i + 1);
                pos->z = 0.0f;
                pos->width = 10.0f;
                pos->height = 10.0f;
                pos->rotation = 0.0f;
                pos->visible = true;
                pos->observable = true;

                entities.push_back(entity);
            }

            // Benchmark serialization (simulating what getEntities() does)
            start = std::chrono::high_resolution_clock::now();

            std::vector<Value> serializedComponents;
            serializedComponents.reserve(entityCount);

            for (auto* entity : entities)
            {
                // auto pos = entity->get<PositionComponent>();

                // This is what the current system does - full serialization
                Value tableValue = serializeComponentToTable(&vm, &ecs, entity, ecs.getComponentRegistry()->getTypeId<PositionComponent>());
                serializedComponents.push_back(tableValue);
            }

            end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

            std::cout << "[Full Serialization] (vm setup: " << vm_duration << " μs) " << entityCount << " entities: "
                      << duration << " μs ("
                      << (duration / static_cast<double>(entityCount)) << " μs/entity)"
                      << std::endl;

            // Cleanup
            for (auto* entity : entities)
            {
                ecs.removeEntity(entity);
            }
        }

        /**
         * @brief Benchmark script execution with serialized components
         *
         * Tests a realistic scenario: script iterates over entities and updates positions.
         */
        void benchmarkScriptUpdateWithSerialization(unsigned int entityCount)
        {
            // MockLogger<TerminalSink> logger;

            EntitySystem ecs("benchmark_script");
            VM vm;
            ecs.setupVm(vm);

            auto sys = ecs.createSystem<PositionComponentSystem>();

            auto testSys = createStandardSystem("testSystem")
                .ownComponent("Test")
                .onExecute("test/bench/updateBench.pg")
                .build();

            ecs.registerSystem(testSys);

            // Create entities
            for (unsigned int i = 0; i < entityCount; ++i)
            {
                auto entity = ecs.createEntity("entity_" + std::to_string(i));
                auto pos = entity->attach<PositionComponent>();
                pos->x = static_cast<float>(i);
                pos->y = static_cast<float>(i + 1);
                pos->z = 0.0f;
                pos->width = 10.0f;
                pos->height = 10.0f;
                pos->rotation = 0.0f;
                pos->visible = true;
                pos->observable = true;

                entity->attach("Test");
            }

            // Benchmark script execution (including serialization in getEntities)
            auto start = std::chrono::high_resolution_clock::now();

            ecs.executeOnce();

            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

            std::cout << "[Script Update] " << entityCount << " entities: "
                      << duration << " μs ("
                      << (duration / static_cast<double>(entityCount)) << " μs/entity)"
                      << std::endl;

            // Verify updates worked
            auto entities = sys->view<PositionComponent>();
            if (entities.nbComponents() > 2)
            {
                auto* pos = entities[1];

                if (pos->x != 1.0f or pos->y != 2.0f)
                {
                    std::cout << "WARNING: Position update did not work correctly!" << std::endl;
                }
            }
        }

        // /**
        //  * @brief Benchmark memory usage of serialization
        //  *
        //  * Estimates how much memory is used by serialized components.
        //  */
        // void benchmarkSerializationMemory(unsigned int entityCount)
        // {
        //     EntitySystem ecs("benchmark_memory");
        //     VM vm;
        //     ecs.setupVm(vm);

        //     // Create entities
        //     for (unsigned int i = 0; i < entityCount; ++i)
        //     {
        //         auto entity = ecs.createEntity();
        //         entity->attach<PositionComponent>(
        //             static_cast<float>(i),
        //             static_cast<float>(i + 1),
        //             0.0f, 10.0f, 10.0f, 0.0f, true, true
        //         );
        //     }

        //     // Measure memory before serialization
        //     size_t vmObjectsBefore = vm.objects.size();

        //     // Serialize all components
        //     std::vector<Value> serialized;
        //     auto entities = ecs.getEntities("Position");
        //     for (auto* entity : entities)
        //     {
        //         auto* pos = entity->get<PositionComponent>();
        //         serialized.push_back(serializeComponentToTable(&vm, &ecs, entity, pos->componentId));
        //     }

        //     // Measure memory after serialization
        //     size_t vmObjectsAfter = vm.objects.size();
        //     size_t newObjects = vmObjectsAfter - vmObjectsBefore;

        //     std::cout << "[Memory Usage] " << entityCount << " entities: "
        //               << newObjects << " VM objects created ("
        //               << (newObjects / static_cast<double>(entityCount)) << " objects/entity)"
        //               << std::endl;
        // }

        // Google Test benchmarks
        TEST(SerializationBenchmark, FullSerialization)
        {
            std::cout << "\n=== Full Serialization Benchmark ===" << std::endl;
            for (auto count : entityCounts)
            {
                benchmarkFullSerialization(count);
            }
        }

        TEST(SerializationBenchmark, ScriptUpdate)
        {
            std::cout << "\n=== Script Update Benchmark ===" << std::endl;
            for (auto count : entityCounts)
            {
                benchmarkScriptUpdateWithSerialization(count);
            }
        }

        // TEST(SerializationBenchmark, MemoryUsage)
        // {
        //     std::cout << "\n=== Memory Usage Benchmark ===" << std::endl;
        //     for (auto count : entityCounts)
        //     {
        //         benchmarkSerializationMemory(count);
        //     }
        // }

    } // namespace benchmark
} // namespace pg
