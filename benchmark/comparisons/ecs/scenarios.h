/**
 * @file scenarios.h
 * @brief Engine-agnostic ECS benchmark scenarios.
 *
 * Defines the canonical scenarios that every competitor (PgEngine, EnTT, flecs,
 * Bevy, ...) implements. Each engine has its own *_bench.cpp that wires the
 * scenario list to its native APIs and emits CSV rows in the shared format.
 *
 * Scenarios are modeled after github.com/abeimler/ecs_benchmark so results are
 * directly comparable to the public ECS leaderboards.
 *
 * CSV output format (one row per (engine, scenario, count, run)):
 *
 *     engine,scenario,entity_count,run_id,total_ns,per_entity_ns,peak_rss_kb
 *
 * Raw CSVs are aggregated downstream (min / median / p95) by plot.py.
 */

#pragma once

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#if defined(__linux__) || defined(__APPLE__)
    #include <sys/resource.h>
#endif

namespace pg::bench
{
    // ============================================================
    // POD components — shared across C++ engine benches.
    // Each engine registers/uses them in its own idiomatic way.
    // ============================================================

    struct Position { float x = 0.0f, y = 0.0f; };
    struct Velocity { float vx = 1.0f, vy = 1.0f; };

    // ============================================================
    // Scenario catalog
    // ============================================================

    enum class Scenario
    {
        CreateEmpty,         // createEntity() x N, no components
        CreateBulk,          // createEntities(N) where supported, else fallback
        CreateWith1Comp,     // createEntity() + attach<Position>
        CreateWith2Comp,     // createEntity() + attach<Position, Velocity>
        IterateRead1Comp,    // for each Position: sink += p.x
        IterateWrite1Comp,   // for each Position: p.x += 1; p.y += 1
        IterateApplyVel,     // for each (Position, Velocity): p += v * dt
    };

    inline const char* scenarioName(Scenario s)
    {
        switch (s)
        {
            case Scenario::CreateEmpty:       return "create_empty";
            case Scenario::CreateBulk:        return "create_bulk";
            case Scenario::CreateWith1Comp:   return "create_1comp";
            case Scenario::CreateWith2Comp:   return "create_2comp";
            case Scenario::IterateRead1Comp:  return "iterate_read_1comp";
            case Scenario::IterateWrite1Comp: return "iterate_write_1comp";
            case Scenario::IterateApplyVel:   return "iterate_apply_velocity";
        }
        return "unknown";
    }

    inline const std::vector<Scenario>& allScenarios()
    {
        static const std::vector<Scenario> v = {
            Scenario::CreateEmpty,
            Scenario::CreateBulk,
            Scenario::CreateWith1Comp,
            Scenario::CreateWith2Comp,
            Scenario::IterateRead1Comp,
            Scenario::IterateWrite1Comp,
            Scenario::IterateApplyVel,
        };
        return v;
    }

    // Default entity-count sweep. Override on the command line.
    inline const std::vector<int>& defaultEntityCounts()
    {
        static const std::vector<int> v = { 1000, 10000, 100000, 1000000 };
        return v;
    }

    constexpr float kFixedDt = 0.016f;

    // ============================================================
    // Timing
    // ============================================================

    using Clock     = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    inline TimePoint now() { return Clock::now(); }

    inline int64_t nanos(TimePoint a, TimePoint b)
    {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(b - a).count();
    }

    // ============================================================
    // Peak RSS in kilobytes. Returns 0 on platforms without rusage.
    //
    // Linux reports ru_maxrss in KB; macOS in bytes. We normalize to KB.
    // ============================================================

    inline int64_t peakRssKb()
    {
#if defined(__linux__)
        struct rusage ru;
        if (getrusage(RUSAGE_SELF, &ru) == 0) return static_cast<int64_t>(ru.ru_maxrss);
        return 0;
#elif defined(__APPLE__)
        struct rusage ru;
        if (getrusage(RUSAGE_SELF, &ru) == 0) return static_cast<int64_t>(ru.ru_maxrss) / 1024;
        return 0;
#else
        return 0;
#endif
    }

    // ============================================================
    // CSV emission
    // ============================================================

    inline void emitCsvHeader(std::FILE* out)
    {
        std::fprintf(out, "engine,scenario,entity_count,run_id,total_ns,per_entity_ns,peak_rss_kb\n");
        std::fflush(out);
    }

    inline void emitCsvRow(std::FILE* out,
                           const char* engine,
                           Scenario scenario,
                           int entityCount,
                           int runId,
                           int64_t totalNs)
    {
        double perEntity = entityCount > 0
            ? static_cast<double>(totalNs) / static_cast<double>(entityCount)
            : 0.0;

        std::fprintf(out, "%s,%s,%d,%d,%lld,%.4f,%lld\n",
                     engine,
                     scenarioName(scenario),
                     entityCount,
                     runId,
                     static_cast<long long>(totalNs),
                     perEntity,
                     static_cast<long long>(peakRssKb()));
        std::fflush(out);
    }

    // ============================================================
    // Light CLI parser (no third-party deps).
    //
    // Supported flags:
    //   --scenarios=all | --scenarios=name1,name2,...
    //   --counts=1000,10000,100000,1000000
    //   --runs=5            (measured runs per scenario × count)
    //   --warmup=1          (untimed warmup runs, results discarded)
    //   --output=path.csv   (default: stdout)
    //   --append            (don't write CSV header — for run aggregation)
    //   --engine=name       (override the engine label; defaults to bench's own)
    // ============================================================

    struct Options
    {
        std::vector<Scenario> scenarios = allScenarios();
        std::vector<int>      counts    = defaultEntityCounts();
        int                   runs      = 5;
        int                   warmup    = 1;
        std::string           output;          // empty → stdout
        bool                  append    = false;
        std::string           engineOverride;
    };

    inline std::vector<std::string> splitComma(const std::string& s)
    {
        std::vector<std::string> out;
        std::string cur;
        for (char c : s)
        {
            if (c == ',') { if (!cur.empty()) out.push_back(cur); cur.clear(); }
            else cur.push_back(c);
        }
        if (!cur.empty()) out.push_back(cur);
        return out;
    }

    inline bool parseScenarioName(const std::string& name, Scenario& out)
    {
        for (Scenario s : allScenarios())
            if (name == scenarioName(s)) { out = s; return true; }
        return false;
    }

    // Returns true on success; on failure prints a hint to stderr and returns false.
    inline bool parseOptions(int argc, char** argv, Options& opts)
    {
        auto startsWith = [](const std::string& s, const char* prefix) {
            return s.rfind(prefix, 0) == 0;
        };

        for (int i = 1; i < argc; ++i)
        {
            std::string a = argv[i];

            if (startsWith(a, "--scenarios="))
            {
                std::string v = a.substr(std::string("--scenarios=").size());
                if (v == "all") { opts.scenarios = allScenarios(); continue; }
                opts.scenarios.clear();
                for (auto& name : splitComma(v))
                {
                    Scenario s;
                    if (!parseScenarioName(name, s))
                    {
                        std::fprintf(stderr, "unknown scenario: %s\n", name.c_str());
                        return false;
                    }
                    opts.scenarios.push_back(s);
                }
            }
            else if (startsWith(a, "--counts="))
            {
                std::string v = a.substr(std::string("--counts=").size());
                opts.counts.clear();
                for (auto& s : splitComma(v))
                    opts.counts.push_back(std::stoi(s));
            }
            else if (startsWith(a, "--runs="))
                opts.runs = std::stoi(a.substr(std::string("--runs=").size()));
            else if (startsWith(a, "--warmup="))
                opts.warmup = std::stoi(a.substr(std::string("--warmup=").size()));
            else if (startsWith(a, "--output="))
                opts.output = a.substr(std::string("--output=").size());
            else if (a == "--append")
                opts.append = true;
            else if (startsWith(a, "--engine="))
                opts.engineOverride = a.substr(std::string("--engine=").size());
            else if (a == "--help" || a == "-h")
            {
                std::fprintf(stderr,
                    "Usage: %s [options]\n"
                    "  --scenarios=all|<name,name,...>   default: all\n"
                    "  --counts=1000,10000,...           default: 1k,10k,100k,1M\n"
                    "  --runs=N                          measured runs (default 5)\n"
                    "  --warmup=N                        untimed warmups (default 1)\n"
                    "  --output=PATH                     CSV path (default: stdout)\n"
                    "  --append                          skip CSV header\n"
                    "  --engine=NAME                     override engine label\n",
                    argv[0]);
                return false;
            }
            else
            {
                std::fprintf(stderr, "unknown arg: %s (try --help)\n", a.c_str());
                return false;
            }
        }
        return true;
    }

} // namespace pg::bench
