/**
 * @file entt_bench.cpp
 * @brief Standalone EnTT ECS bench — emits CSV in the shared format.
 *
 * Implements the same 7 scenarios as pgengine_bench.cpp using EnTT's idiomatic
 * APIs:
 *   - registry.create()                   — single entity
 *   - registry.create(first, last)        — bulk
 *   - registry.emplace<T>(entity, ...)    — attach
 *   - registry.view<T...>().each(...)     — iterate
 *
 * EnTT is a sparse-set ECS, like PgEngine, so this is the closest direct
 * design comparison. Pinned to v3.13.2 in CMakeLists.txt.
 */

#include "scenarios.h"

#include <entt/entt.hpp>

#include <cstdio>
#include <vector>

using namespace pg::bench;

static int64_t runCreateEmpty(int n)
{
    entt::registry reg;
    auto t0 = now();
    for (int i = 0; i < n; ++i) (void)reg.create();
    return nanos(t0, now());
}

static int64_t runCreateBulk(int n)
{
    entt::registry reg;
    std::vector<entt::entity> entities(static_cast<size_t>(n));
    auto t0 = now();
    reg.create(entities.begin(), entities.end());
    return nanos(t0, now());
}

static int64_t runCreateWith1Comp(int n)
{
    entt::registry reg;
    auto t0 = now();
    for (int i = 0; i < n; ++i)
    {
        auto e = reg.create();
        reg.emplace<Position>(e);
    }
    return nanos(t0, now());
}

static int64_t runCreateWith2Comp(int n)
{
    entt::registry reg;
    auto t0 = now();
    for (int i = 0; i < n; ++i)
    {
        auto e = reg.create();
        reg.emplace<Position>(e);
        reg.emplace<Velocity>(e);
    }
    return nanos(t0, now());
}

static int64_t runIterateRead1Comp(int n)
{
    entt::registry reg;
    for (int i = 0; i < n; ++i)
    {
        auto e = reg.create();
        reg.emplace<Position>(e);
    }

    volatile float sink = 0.0f;
    auto t0 = now();
    reg.view<Position>().each([&](const Position& p) { sink += p.x; });
    auto ns = nanos(t0, now());
    (void)sink;
    return ns;
}

static int64_t runIterateWrite1Comp(int n)
{
    entt::registry reg;
    for (int i = 0; i < n; ++i)
    {
        auto e = reg.create();
        reg.emplace<Position>(e);
    }

    auto t0 = now();
    reg.view<Position>().each([](Position& p) { p.x += 1.0f; p.y += 1.0f; });
    return nanos(t0, now());
}

static int64_t runIterateApplyVel(int n)
{
    entt::registry reg;
    for (int i = 0; i < n; ++i)
    {
        auto e = reg.create();
        reg.emplace<Position>(e);
        reg.emplace<Velocity>(e);
    }

    auto t0 = now();
    reg.view<Position, Velocity>().each([](Position& p, const Velocity& v) {
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

    const char* engine = opts.engineOverride.empty() ? "entt" : opts.engineOverride.c_str();

    std::fprintf(stderr, "entt ecs bench — runs=%d warmup=%d scenarios=%zu counts=%zu\n",
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
