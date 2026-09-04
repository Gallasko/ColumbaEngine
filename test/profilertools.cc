#include "stdafx.h"

#include "gtest/gtest.h"

// These tests exercise the PROFILE-gated capture layer; they only exist in
// builds configured with -DPG_PROFILE=ON.
#ifdef PROFILE

#include <thread>

#include "Profiler/profiler.h"
#include "Profiler/profilerstats.h"

#include "ECS/entitysystem.h"
#include "ECS/system.h"

namespace pg
{
    namespace test
    {
        namespace
        {
            struct CountedComp
            {
                CountedComp(int v) : value(v) {}
                int value;
            };

            struct CountedCompSystem : public System<Own<CountedComp>>
            {
                virtual void execute() override {}
            };
        }

        // ------------------------------------------------------------------
        // ProfilerStats
        // ------------------------------------------------------------------

        TEST(profilerstats_test, per_pass_counts_are_not_cumulative)
        {
            auto& stats = ProfilerStats::instance();
            stats.clear();

            stats.countEvent(1, typeid(int).name());
            stats.countEvent(1, typeid(int).name());
            stats.countEvent(1, typeid(int).name());
            stats.countStandardEvent("MyStandardEvent");

            stats.finalizePass(1, 10.0, 42);

            auto passes = stats.lastPasses(10);
            ASSERT_EQ(passes.size(), 1u);
            EXPECT_EQ(passes[0].pass, 1u);
            EXPECT_EQ(passes[0].entityCount, 42u);

            bool foundTyped = false, foundStandard = false;
            for (const auto& [name, count] : passes[0].eventCounts)
            {
                if (name == "int" and count == 3) foundTyped = true;         // demangled typeid(int)
                if (name == "MyStandardEvent" and count == 1) foundStandard = true;
            }
            EXPECT_TRUE(foundTyped);
            EXPECT_TRUE(foundStandard);

            // A pass with no events must come out empty (counters were swapped
            // out, not accumulated)
            stats.finalizePass(2, 20.0, 42);

            passes = stats.lastPasses(1);
            ASSERT_EQ(passes.size(), 1u);
            EXPECT_EQ(passes[0].pass, 2u);
            EXPECT_TRUE(passes[0].eventCounts.empty());
        }

        TEST(profilerstats_test, component_counts_attach_to_next_pass)
        {
            auto& stats = ProfilerStats::instance();
            stats.clear();

            stats.setComponentCounts({{"Position", 5}, {"Velocity", 3}});
            stats.finalizePass(1, 0.0, 8);

            auto passes = stats.lastPasses(1);
            ASSERT_EQ(passes.size(), 1u);
            ASSERT_EQ(passes[0].componentCounts.size(), 2u);

            // Consumed: the next pass gets none
            stats.finalizePass(2, 0.0, 8);
            passes = stats.lastPasses(1);
            EXPECT_TRUE(passes[0].componentCounts.empty());
        }

        TEST(profilerstats_test, ring_is_capped)
        {
            auto& stats = ProfilerStats::instance();
            stats.clear();

            for (uint64_t i = 0; i < 1100; i++)
                stats.finalizePass(i, 0.0, 0);

            auto passes = stats.lastPasses(5000);
            EXPECT_EQ(passes.size(), 1024u);
            EXPECT_EQ(passes.back().pass, 1099u); // newest kept, oldest dropped
        }

        TEST(profilerstats_test, concurrent_counting_loses_nothing)
        {
            auto& stats = ProfilerStats::instance();
            stats.clear();

            constexpr int threads = 4;
            constexpr int perThread = 1000;

            std::vector<std::thread> pool;
            for (int t = 0; t < threads; t++)
            {
                pool.emplace_back([&stats]() {
                    for (int i = 0; i < perThread; i++)
                        stats.countEvent(7, typeid(float).name());
                });
            }

            for (auto& t : pool)
                t.join();

            stats.finalizePass(1, 0.0, 0);

            auto passes = stats.lastPasses(1);
            ASSERT_EQ(passes.size(), 1u);
            ASSERT_EQ(passes[0].eventCounts.size(), 1u);
            EXPECT_EQ(passes[0].eventCounts[0].second, static_cast<uint32_t>(threads * perThread));
        }

        // ------------------------------------------------------------------
        // Profiler: ECS pass stamping / instants / snapshots
        // ------------------------------------------------------------------

        TEST(profiler_pass_test, instant_events_bypass_duration_filter)
        {
            auto& profiler = Profiler::instance();
            profiler.clear();

            profiler.recordInstant("MyMarker", "Marker");

            auto snapshot = profiler.snapshotSince(0.0);
            ASSERT_EQ(snapshot.size(), 1u);
            EXPECT_EQ(snapshot[0].name, "MyMarker");
            EXPECT_EQ(snapshot[0].durationMs, 0.0);
        }

        TEST(profiler_pass_test, events_are_stamped_with_current_ecs_pass)
        {
            auto& profiler = Profiler::instance();
            profiler.clear();

            const auto base = profiler.getCurrentEcsPass();

            auto next = profiler.beginEcsPass();
            EXPECT_EQ(next, base + 1);

            profiler.recordInstant("InPass", "Marker");

            auto snapshot = profiler.snapshotSince(0.0);
            ASSERT_EQ(snapshot.size(), 1u);
            EXPECT_EQ(snapshot[0].ecsPass, base + 1);
        }

        TEST(profiler_pass_test, snapshot_since_filters_by_start_time)
        {
            auto& profiler = Profiler::instance();
            profiler.clear();

            profiler.recordInstant("Old", "Marker");

            const double cutoff = profiler.nowMs() + 10000.0;
            EXPECT_TRUE(profiler.snapshotSince(cutoff).empty());
            EXPECT_EQ(profiler.snapshotSince(0.0).size(), 1u);
        }

        // ------------------------------------------------------------------
        // Component instance counters in the registry
        // ------------------------------------------------------------------

        TEST(profilerstats_test, registry_component_counts_track_attach_detach)
        {
            EntitySystem ecs;
            ecs.createSystem<CountedCompSystem>();

            auto findCount = [&ecs]() -> long {
                for (const auto& [name, count] : ecs.getComponentRegistry()->getComponentCounts())
                {
                    if (name.find("CountedComp") != std::string::npos)
                        return static_cast<long>(count);
                }
                return -1;
            };

            EXPECT_EQ(findCount(), 0);

            auto e0 = ecs.createEntity();
            auto e1 = ecs.createEntity();

            ecs.attachGeneric<CountedComp>(e0, 1);
            ecs.attachGeneric<CountedComp>(e1, 2);

            EXPECT_EQ(findCount(), 2);

            ecs.detach<CountedComp>(e0);

            EXPECT_EQ(findCount(), 1);
        }
    }
}

#endif // PROFILE
