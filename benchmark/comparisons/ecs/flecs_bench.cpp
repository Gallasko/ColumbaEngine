/**
 * @file flecs_bench.cpp
 * @brief Standalone flecs ECS bench — emits CSV in the shared format.
 *
 * flecs is an archetype-based ECS — fundamentally different from PgEngine
 * (sparse-set) and EnTT (sparse-set). Entities sharing the same component set
 * live in the same table, so multi-component iteration is essentially a linear
 * walk over contiguous arrays.
 *
 * Idiomatic APIs used:
 *   - world.component<T>()                    — pre-register (un-timed)
 *   - world.entity()                          — create one entity
 *   - entity.set<T>({...})                    — attach component
 *   - world.each([](T& a, U& b){...})         — iterate
 *
 * flecs has no high-level C++ bulk-create that maps cleanly to "N empty
 * entities" — the bulk APIs are designed around archetypes. We fall back to a
 * loop and document this in the README; the difference vs PgEngine's
 * createEntities() is itself an interesting finding.
 *
 * Pinned to v4.0.5 in CMakeLists.txt.
 */

#include "scenarios.h"

#include <flecs.h>

#include <cstdio>

using namespace pg::bench;

// Pre-register components on a fresh world so the timed runs measure only the
// hot path (creation/attach/iteration), not first-touch registration.
static void registerComponents(flecs::world& world)
{
    world.component<Position>();
    world.component<Velocity>();
}

static int64_t runCreateEmpty(int n)
{
    flecs::world world;
    registerComponents(world);

    auto t0 = now();
    for (int i = 0; i < n; ++i) world.entity();
    return nanos(t0, now());
}

static int64_t runCreateBulk(int n)
{
    // flecs has no clean C++ "create N empty entities" API; this falls back
    // to a loop. The C bulk API operates per-archetype and isn't equivalent.
    flecs::world world;
    registerComponents(world);

    auto t0 = now();
    for (int i = 0; i < n; ++i) world.entity();
    return nanos(t0, now());
}

static int64_t runCreateWith1Comp(int n)
{
    flecs::world world;
    registerComponents(world);

    auto t0 = now();
    for (int i = 0; i < n; ++i) world.entity().set<Position>({});
    return nanos(t0, now());
}

static int64_t runCreateWith2Comp(int n)
{
    flecs::world world;
    registerComponents(world);

    auto t0 = now();
    for (int i = 0; i < n; ++i)
        world.entity().set<Position>({}).set<Velocity>({});
    return nanos(t0, now());
}

static int64_t runIterateRead1Comp(int n)
{
    flecs::world world;
    registerComponents(world);
    for (int i = 0; i < n; ++i) world.entity().set<Position>({});

    volatile float sink = 0.0f;
    auto t0 = now();
    world.each([&](Position& p) { sink += p.x; });
    auto ns = nanos(t0, now());
    (void)sink;
    return ns;
}

static int64_t runIterateWrite1Comp(int n)
{
    flecs::world world;
    registerComponents(world);
    for (int i = 0; i < n; ++i) world.entity().set<Position>({});

    auto t0 = now();
    world.each([](Position& p) { p.x += 1.0f; p.y += 1.0f; });
    return nanos(t0, now());
}

static int64_t runIterateApplyVel(int n)
{
    flecs::world world;
    registerComponents(world);
    for (int i = 0; i < n; ++i)
        world.entity().set<Position>({}).set<Velocity>({});

    auto t0 = now();
    world.each([](Position& p, Velocity& v) {
        p.x += v.vx * kFixedDt;
        p.y += v.vy * kFixedDt;
    });
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

    const char* engine = opts.engineOverride.empty() ? "flecs" : opts.engineOverride.c_str();

    std::fprintf(stderr, "flecs ecs bench — runs=%d warmup=%d scenarios=%zu counts=%zu\n",
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
