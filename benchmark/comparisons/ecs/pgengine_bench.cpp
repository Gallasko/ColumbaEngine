/**
 * @file pgengine_bench.cpp
 * @brief Standalone PgEngine ECS bench — emits CSV for cross-engine comparison.
 *
 * Mirrors the scenarios defined in scenarios.h using PgEngine's native APIs:
 *   - createEntity() / createEntities()
 *   - _attach<T> / attachGeneric<T>
 *   - view<T>()
 *
 * Each scenario is run --runs times (default 5) with --warmup untimed runs first.
 * One CSV row is emitted per (scenario × count × run) so downstream tools can
 * compute min / median / p95 without re-running.
 *
 * Build: enable PGE_BUILD_COMPARISON_BENCHMARKS=ON in CMake.
 * Run:   ./pgengine_ecs_bench --output=results/raw/pgengine.csv
 */

#include "scenarios.h"

#include "ECS/entitysystem.h"
#include "ECS/system.h"

#include <cstdio>
#include <string>

using namespace pg;
using namespace pg::bench;

// PgEngine requires a System<Own<T>, StoragePolicy> per component type so the
// ECS knows who owns it. These are storage-only — no logic.
struct PositionStorage : public System<Own<Position>, StoragePolicy>
{
    std::string getSystemName() const override { return "BenchPositionStorage"; }
};

struct VelocityStorage : public System<Own<Velocity>, StoragePolicy>
{
    std::string getSystemName() const override { return "BenchVelocityStorage"; }
};

// ============================================================
// Scenario implementations.
//
// Each returns the measured ns for ONE timed run. The harness handles warmup
// and run loops. A fresh EntitySystem is built per run so we measure steady
// state, not amortized allocation.
// ============================================================

static int64_t runCreateEmpty(int n)
{
    EntitySystem ecs("bench_create_empty");
    auto t0 = now();
    for (int i = 0; i < n; ++i) ecs.createEntity();
    return nanos(t0, now());
}

static int64_t runCreateBulk(int n)
{
    EntitySystem ecs("bench_create_bulk");
    auto t0 = now();
    ecs.createEntities(static_cast<size_t>(n));
    return nanos(t0, now());
}

static int64_t runCreateWith1Comp(int n)
{
    EntitySystem ecs("bench_create_1comp");
    ecs.createSystem<PositionStorage>();
    auto t0 = now();
    for (int i = 0; i < n; ++i)
    {
        auto e = ecs.createEntity();
        ecs._attach<Position>(e);
    }
    return nanos(t0, now());
}

static int64_t runCreateWith2Comp(int n)
{
    EntitySystem ecs("bench_create_2comp");
    ecs.createSystem<PositionStorage>();
    ecs.createSystem<VelocityStorage>();
    auto t0 = now();
    for (int i = 0; i < n; ++i)
    {
        auto e = ecs.createEntity();
        ecs._attach<Position>(e);
        ecs._attach<Velocity>(e);
    }
    return nanos(t0, now());
}

static int64_t runIterateRead1Comp(int n)
{
    EntitySystem ecs("bench_iter_r");
    ecs.createSystem<PositionStorage>();
    for (int i = 0; i < n; ++i)
        ecs.attachGeneric<Position>(ecs.createEntity());

    volatile float sink = 0.0f;
    auto t0 = now();
    auto comps = ecs.view<Position>();
    for (auto c : comps) sink += c->x;
    auto ns = nanos(t0, now());
    (void)sink;
    return ns;
}

static int64_t runIterateWrite1Comp(int n)
{
    EntitySystem ecs("bench_iter_w");
    ecs.createSystem<PositionStorage>();
    for (int i = 0; i < n; ++i)
        ecs.attachGeneric<Position>(ecs.createEntity());

    auto t0 = now();
    auto comps = ecs.view<Position>();
    for (auto c : comps) { c->x += 1.0f; c->y += 1.0f; }
    return nanos(t0, now());
}

static int64_t runIterateApplyVel(int n)
{
    EntitySystem ecs("bench_apply_vel");
    ecs.createSystem<PositionStorage>();
    ecs.createSystem<VelocityStorage>();
    for (int i = 0; i < n; ++i)
    {
        auto e = ecs.createEntity();
        ecs.attachGeneric<Position>(e);
        ecs.attachGeneric<Velocity>(e);
    }

    auto t0 = now();
    auto positions  = ecs.view<Position>();
    auto velocities = ecs.view<Velocity>();
    auto pIt = positions.begin();
    auto vIt = velocities.begin();
    for (; pIt != positions.end() && vIt != velocities.end(); ++pIt, ++vIt)
    {
        (*pIt)->x += (*vIt)->vx * kFixedDt;
        (*pIt)->y += (*vIt)->vy * kFixedDt;
    }
    return nanos(t0, now());
}

static int64_t dispatch(Scenario s, int n)
{
    switch (s)
    {
        case Scenario::CreateEmpty:       return runCreateEmpty(n);
        case Scenario::CreateBulk:        return runCreateBulk(n);
        case Scenario::CreateWith1Comp:   return runCreateWith1Comp(n);
        case Scenario::CreateWith2Comp:   return runCreateWith2Comp(n);
        case Scenario::IterateRead1Comp:  return runIterateRead1Comp(n);
        case Scenario::IterateWrite1Comp: return runIterateWrite1Comp(n);
        case Scenario::IterateApplyVel:   return runIterateApplyVel(n);
    }
    return 0;
}

int main(int argc, char** argv)
{
    Options opts;
    if (!parseOptions(argc, argv, opts)) return 1;

    std::FILE* out = stdout;
    if (!opts.output.empty())
    {
        out = std::fopen(opts.output.c_str(), opts.append ? "a" : "w");
        if (!out)
        {
            std::fprintf(stderr, "cannot open %s for writing\n", opts.output.c_str());
            return 1;
        }
    }

    if (!opts.append) emitCsvHeader(out);

    const char* engine = opts.engineOverride.empty() ? "pgengine" : opts.engineOverride.c_str();

    // Status to stderr so it doesn't contaminate the CSV on stdout.
    std::fprintf(stderr, "pgengine ecs bench — runs=%d warmup=%d scenarios=%zu counts=%zu\n",
                 opts.runs, opts.warmup, opts.scenarios.size(), opts.counts.size());

    for (Scenario s : opts.scenarios)
    {
        for (int count : opts.counts)
        {
            for (int w = 0; w < opts.warmup; ++w) (void)dispatch(s, count);

            for (int r = 0; r < opts.runs; ++r)
            {
                int64_t ns = dispatch(s, count);
                emitCsvRow(out, engine, s, count, r, ns);
            }

            std::fprintf(stderr, "  %-24s n=%-8d done\n", scenarioName(s), count);
        }
    }

    if (out != stdout) std::fclose(out);
    return 0;
}
