/**
 * @file ecs_performance.cc
 * @brief ECS entity/component throughput benchmarks
 *
 * Measures the three operations that matter most for a game engine:
 *   1. Entity + component creation
 *   2. Single-component iteration (read)
 *   3. Multi-component iteration (read-write) — the classic "apply velocity" loop
 *
 * Results are printed as nanoseconds total and nanoseconds per entity so they
 * can be compared directly against Unity DOTS, Godot, and Unreal benchmarks.
 */

#include "gtest/gtest.h"

#include <chrono>
#include <iostream>
#include <vector>

#include "ECS/entitysystem.h"
#include "ECS/system.h"

namespace pg
{
    namespace benchmark
    {

        // ====================================================================
        // Components
        // ====================================================================

        struct BenchPosition
        {
            float x = 0.0f;
            float y = 0.0f;
        };

        struct BenchVelocity
        {
            float vx = 1.0f;
            float vy = 1.0f;
        };

        // ====================================================================
        // Systems (needed so the ECS knows who owns each component type)
        // ====================================================================

        struct PositionStorage : public System<Own<BenchPosition>, StoragePolicy>
        {
            virtual std::string getSystemName() const override { return "PositionStorage"; }
        };

        struct VelocityStorage : public System<Own<BenchVelocity>, StoragePolicy>
        {
            virtual std::string getSystemName() const override { return "VelocityStorage"; }
        };

        // ====================================================================
        // Helpers
        // ====================================================================

        using Clock     = std::chrono::high_resolution_clock;
        using TimePoint = std::chrono::time_point<Clock>;

        inline TimePoint now() { return Clock::now(); }

        inline int64_t nanoseconds(TimePoint start, TimePoint end)
        {
            return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        }

        void printResult(const std::string& label, int count, int64_t ns)
        {
            double perEntity = static_cast<double>(ns) / static_cast<double>(count);
            std::cout << "  [" << count << "] " << label
                      << " — total: " << ns << " ns"
                      << "  |  per entity: " << perEntity << " ns"
                      << std::endl;
        }

        // Entity counts to sweep across
        const std::vector<int> entityCounts = { 100, 1000, 10000, 100000, 1000000 };

        // ====================================================================
        // Benchmark 1 — Entity creation
        // ====================================================================

        TEST(ECS_Benchmark, EntityCreation)
        {
            std::cout << "\n=== Entity Creation ===" << std::endl;

            for (int count : entityCounts)
            {
                EntitySystem ecs("bench_create");

                auto start = now();

                for (int i = 0; i < count; ++i)
                    ecs.createEntity();

                auto end = now();

                printResult("createEntity()", count, nanoseconds(start, end));
            }
        }

        // ====================================================================
        // Benchmark 2 — Bulk entity creation (createEntities)
        // ====================================================================

        TEST(ECS_Benchmark, BulkEntityCreation)
        {
            std::cout << "\n=== Bulk Entity Creation (createEntities) ===" << std::endl;

            for (int count : entityCounts)
            {
                EntitySystem ecs("bench_create_bulk");

                auto start = now();

                ecs.createEntities(static_cast<size_t>(count));

                auto end = now();

                printResult("createEntities()", count, nanoseconds(start, end));
            }
        }

        // ====================================================================
        // Benchmark 3 — Entity creation + single component attachment
        // ====================================================================

        TEST(ECS_Benchmark, EntityCreationWithComponent)
        {
            std::cout << "\n=== Entity Creation + Component Attachment (1 component) ===" << std::endl;

            for (int count : entityCounts)
            {
                EntitySystem ecs("bench_attach");
                ecs.createSystem<PositionStorage>();

                auto start = now();

                for (int i = 0; i < count; ++i)
                {
                    auto entity = ecs.createEntity();
                    ecs._attach<BenchPosition>(entity);
                }

                auto end = now();

                printResult("createEntity() + attachGeneric<Position>", count, nanoseconds(start, end));
            }
        }

        // ====================================================================
        // Benchmark 3 — Entity creation + two component attachments
        // ====================================================================

        TEST(ECS_Benchmark, EntityCreationWithTwoComponents)
        {
            std::cout << "\n=== Entity Creation + Component Attachment (2 components) ===" << std::endl;

            for (int count : entityCounts)
            {
                EntitySystem ecs("bench_attach2");
                ecs.createSystem<PositionStorage>();
                ecs.createSystem<VelocityStorage>();

                auto start = now();

                for (int i = 0; i < count; ++i)
                {
                    auto entity = ecs.createEntity();
                    ecs._attach<BenchPosition>(entity);
                    ecs._attach<BenchVelocity>(entity);
                }

                auto end = now();

                printResult("createEntity() + 2x attachGeneric", count, nanoseconds(start, end));
            }
        }

        // ====================================================================
        // Benchmark 4 — Single-component iteration (read)
        // ====================================================================

        TEST(ECS_Benchmark, SingleComponentIterationRead)
        {
            std::cout << "\n=== Single-Component Iteration (read) ===" << std::endl;

            for (int count : entityCounts)
            {
                EntitySystem ecs("bench_iter_r");
                ecs.createSystem<PositionStorage>();

                for (int i = 0; i < count; ++i)
                {
                    auto entity = ecs.createEntity();
                    ecs.attachGeneric<BenchPosition>(entity);
                }

                volatile float sink = 0.0f;

                auto start = now();

                auto components = ecs.view<BenchPosition>();
                for (auto comp : components)
                    sink += comp->x;

                auto end = now();

                (void)sink;

                printResult("view<Position>() read", count, nanoseconds(start, end));
            }
        }

        // ====================================================================
        // Benchmark 5 — Single-component iteration (write)
        // ====================================================================

        TEST(ECS_Benchmark, SingleComponentIterationWrite)
        {
            std::cout << "\n=== Single-Component Iteration (write) ===" << std::endl;

            for (int count : entityCounts)
            {
                EntitySystem ecs("bench_iter_w");
                ecs.createSystem<PositionStorage>();

                for (int i = 0; i < count; ++i)
                {
                    auto entity = ecs.createEntity();
                    ecs.attachGeneric<BenchPosition>(entity);
                }

                auto start = now();

                auto components = ecs.view<BenchPosition>();
                for (auto comp : components)
                {
                    comp->x += 1.0f;
                    comp->y += 1.0f;
                }

                auto end = now();

                printResult("view<Position>() write", count, nanoseconds(start, end));
            }
        }

        // ====================================================================
        // Benchmark 6 — Multi-component iteration: apply velocity to position
        //
        // This is the canonical "physics step" benchmark every engine measures.
        // Two separate views are iterated in lockstep — this is the simplest
        // approach; a group-based approach (registerGroup) would be even faster
        // and should be added once this baseline is established.
        // ====================================================================

        TEST(ECS_Benchmark, MultiComponentApplyVelocity)
        {
            std::cout << "\n=== Multi-Component Iteration: apply velocity to position ===" << std::endl;

            for (int count : entityCounts)
            {
                EntitySystem ecs("bench_mv");
                ecs.createSystem<PositionStorage>();
                ecs.createSystem<VelocityStorage>();

                for (int i = 0; i < count; ++i)
                {
                    auto entity = ecs.createEntity();
                    ecs.attachGeneric<BenchPosition>(entity);
                    ecs.attachGeneric<BenchVelocity>(entity);
                }

                float dt = 0.016f;

                auto start = now();

                auto positions  = ecs.view<BenchPosition>();
                auto velocities = ecs.view<BenchVelocity>();

                auto posIt = positions.begin();
                auto velIt = velocities.begin();

                for (; posIt != positions.end() && velIt != velocities.end(); ++posIt, ++velIt)
                {
                    (*posIt)->x += (*velIt)->vx * dt;
                    (*posIt)->y += (*velIt)->vy * dt;
                }

                auto end = now();

                printResult("view<Position+Velocity>() update", count, nanoseconds(start, end));
            }
        }

        // ====================================================================
        // Benchmark 7 — Summary comparison at a fixed entity count
        // ====================================================================

        TEST(ECS_Benchmark, Summary)
        {
            const int count = 100000;
            const float dt  = 0.016f;

            std::cout << "\n========================================" << std::endl;
            std::cout << "ECS PERFORMANCE SUMMARY (" << count << " entities)" << std::endl;
            std::cout << "========================================" << std::endl;

            struct Result { std::string name; int64_t ns; };
            std::vector<Result> results;

            // --- Creation (one by one) ---
            {
                EntitySystem ecs("s_create");
                auto start = now();
                for (int i = 0; i < count; ++i) ecs.createEntity();
                auto end = now();
                results.push_back({"Entity creation (loop)", nanoseconds(start, end)});
            }

            // --- Bulk creation ---
            {
                EntitySystem ecs("s_create_bulk");
                auto start = now();
                ecs.createEntities(static_cast<size_t>(count));
                auto end = now();
                results.push_back({"Entity creation (bulk)", nanoseconds(start, end)});
            }

            // --- Creation + attach ---
            {
                EntitySystem ecs("s_attach");
                ecs.createSystem<PositionStorage>();
                auto start = now();
                for (int i = 0; i < count; ++i)
                {
                    auto e = ecs.createEntity();
                    ecs._attach<BenchPosition>(e);
                }
                auto end = now();
                results.push_back({"Create + attach (1 comp)", nanoseconds(start, end)});
            }

            // --- Iteration read ---
            {
                EntitySystem ecs("s_iter_r");
                ecs.createSystem<PositionStorage>();
                for (int i = 0; i < count; ++i)
                {
                    auto e = ecs.createEntity();
                    ecs.attachGeneric<BenchPosition>(e);
                }
                volatile float sink = 0.0f;
                auto start = now();
                auto comps = ecs.view<BenchPosition>();
                for (auto c : comps) sink += c->x;
                auto end = now();
                (void)sink;
                results.push_back({"Iteration read (1 comp)", nanoseconds(start, end)});
            }

            // --- Iteration write (apply velocity) ---
            {
                EntitySystem ecs("s_mv");
                ecs.createSystem<PositionStorage>();
                ecs.createSystem<VelocityStorage>();
                for (int i = 0; i < count; ++i)
                {
                    auto e = ecs.createEntity();
                    ecs.attachGeneric<BenchPosition>(e);
                    ecs.attachGeneric<BenchVelocity>(e);
                }
                auto start = now();
                auto positions  = ecs.view<BenchPosition>();
                auto velocities = ecs.view<BenchVelocity>();
                auto posIt = positions.begin();
                auto velIt = velocities.begin();
                for (; posIt != positions.end() && velIt != velocities.end(); ++posIt, ++velIt)
                {
                    (*posIt)->x += (*velIt)->vx * dt;
                    (*posIt)->y += (*velIt)->vy * dt;
                }
                auto end = now();
                results.push_back({"Apply velocity (2 comps)", nanoseconds(start, end)});
            }

            // --- Print ---
            std::cout << "\nResults:" << std::endl;
            std::cout << "----------------------------------------" << std::endl;

            for (const auto& r : results)
            {
                double perEntity = static_cast<double>(r.ns) / static_cast<double>(count);
                std::cout << r.name << ":" << std::endl;
                std::cout << "  Total:      " << r.ns << " ns  (" << r.ns / 1000 << " μs)" << std::endl;
                std::cout << "  Per entity: " << perEntity << " ns" << std::endl;
                std::cout << std::endl;
            }
        }

    } // namespace benchmark
} // namespace pg
