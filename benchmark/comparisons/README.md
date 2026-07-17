# PgEngine Cross-Engine Benchmarks

A cross-engine performance comparison harness. The PgEngine baseline is here today; EnTT, flecs, and Bevy benches will be added in subsequent passes. All engines emit the same CSV schema so results aggregate cleanly.

The bigger plan (subsystems, competitors, phasing) lives in `/home/corse/.claude/plans/hello-claude-i-want-recursive-treehouse.md`. This README is just methodology + reproduction.

## Goals

1. **Marketing**: headline numbers for blog posts / READMEs.
2. **Optimization**: surface where PgEngine is behind, so we know what to fix.
3. **Validation**: confirm the sparse-set ECS / Taskflow / PgScript design choices pay off.

## Layout

```
benchmark/comparisons/
  README.md                       this file
  ecs/
    scenarios.h                   engine-agnostic scenario catalog + CSV utils
    pgengine_bench.cpp            PgEngine runner -> pgengine_ecs_bench
    CMakeLists.txt
    (entt_bench.cpp)              TODO
    (flecs_bench.cpp)             TODO
    (bevy_bench/)                 TODO (Cargo project)
  script/                         TODO (PgScript VM vs Lua/GDScript/Wren)
  results/
    raw/                          per-run CSVs (git-ignored)
    summary/                      aggregated tables + plots (committed)
    plot.py                       TODO
```

## CSV schema

Every bench writes one row per `(scenario × entity_count × run)`:

```
engine,scenario,entity_count,run_id,total_ns,per_entity_ns,peak_rss_kb
pgengine,iterate_apply_velocity,100000,0,418320,4.1832,42312
```

Raw rows in, aggregated stats out — the plotting layer computes min / median / p95 from the raw CSV. Never aggregate during the run.

## Scenarios (current)

| name | what it measures |
| --- | --- |
| `create_empty` | `createEntity()` × N |
| `create_bulk` | `createEntities(N)` where the engine supports it |
| `create_1comp` | createEntity + attach one component |
| `create_2comp` | createEntity + attach two components |
| `iterate_read_1comp` | sum-x over a single-component view |
| `iterate_write_1comp` | mutate every entity's single component |
| `iterate_apply_velocity` | the canonical `pos += vel * dt` two-component loop |

Modeled after [github.com/abeimler/ecs_benchmark](https://github.com/abeimler/ecs_benchmark) so results compare to the public ECS leaderboards.

## Building

The comparison benches are off by default — casual builds don't pull in EnTT/flecs.

```bash
cmake -B build -DPGE_BUILD_COMPARISON_BENCHMARKS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build --target pgengine_ecs_bench -j
```

## Running

```bash
./build/pgengine_ecs_bench --help

# Default sweep, write CSV:
./build/pgengine_ecs_bench --output=benchmark/comparisons/results/raw/pgengine.csv

# Focused: one scenario, one count, 20 runs:
./build/pgengine_ecs_bench \
    --scenarios=iterate_apply_velocity \
    --counts=100000 \
    --runs=20 \
    --output=/tmp/apply_vel.csv
```

Status messages go to stderr; CSV goes to stdout (or `--output`). Pipe-safe.

## Methodology — read before publishing any number

Methodology is the credibility, not the numbers. The list below is roughly in order of how badly each step matters.

1. **Release build, same flags.** All C++ engines: same compiler, `-O3 -DNDEBUG`, link-time optimization off (to keep the comparison hands-off) unless explicitly noted. Debug builds invalidate the whole comparison.
2. **Stable CPU clocks.** On Linux:
   ```bash
   sudo cpupower frequency-set --governor performance
   # disable turbo for noise reduction (optional, more reproducible but slower):
   echo 1 | sudo tee /sys/devices/system/cpu/intel_pstate/no_turbo
   ```
3. **Pin the process.** `taskset -c 2 ./pgengine_ecs_bench ...` keeps the bench off a noisy core (avoid 0 and 1; check `lscpu` for SMT siblings).
4. **Warmup + multiple runs.** Default is 1 warmup + 5 measured runs per scenario × count. Report **min** or **median** — never the arithmetic mean. The minimum is the closest you get to the unperturbed cost; the median is a fine alternative when you want central tendency.
5. **Idiomatic code per engine.** The EnTT bench MUST use `registry.view<Pos, Vel>().each(...)` — not a hand-rolled iterator. flecs MUST use `world.system<...>` or `query.each(...)`. Otherwise you're benchmarking your bad translation, not the engine.
6. **Apples to apples.** Same components, same scenario semantics, same N. If an engine has a faster path that PgEngine lacks (e.g. flecs cached queries) note it explicitly — don't disable it; that's part of what we're measuring.
7. **Noise check.** Same scenario × 10 runs should have < 5 % variance for ECS micros. If it's worse, fix the methodology before fixing the engine — likely missing pinning or governor.
8. **Publish raw CSVs.** Always commit `results/raw/` for any number you publish. Skeptics need to be able to recompute.

## Reproducing a published number

Anyone — including future-you — should be able to recreate any chart from this directory:

```bash
# 1. Build
cmake -B build -DPGE_BUILD_COMPARISON_BENCHMARKS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build --target pgengine_ecs_bench -j

# 2. Stable run environment
sudo cpupower frequency-set --governor performance

# 3. Sweep
taskset -c 2 ./build/pgengine_ecs_bench \
    --runs=10 \
    --output=benchmark/comparisons/results/raw/pgengine_$(date +%Y%m%d).csv

# 4. Plot (once plot.py exists)
python3 benchmark/comparisons/results/plot.py
```

If a colleague can't reproduce within ~30 minutes on a fresh checkout, this README is broken — fix it.

## Sanity check vs existing /benchmark/ecs_performance.cc

The standalone PgEngine bench is a new code path; it must agree with the existing Google Test bench within ~5 % for the same workload, otherwise the harness is wrong.

```bash
# Old harness (gtest)
./build/bench --gtest_filter=ECS_Benchmark.MultiComponentApplyVelocity

# New harness (CSV)
./build/pgengine_ecs_bench --scenarios=iterate_apply_velocity --counts=1000000 --runs=10
```

The per-entity numbers should match. If they don't, suspect the new harness first.

## Next steps

- Add EnTT bench (vcpkg or vendor) — easiest first competitor; sparse-set, like PgEngine.
- Add flecs bench (archetype design — different school of ECS).
- Add Bevy bench as a sibling Cargo project; emit the same CSV format.
- Add `results/plot.py` (matplotlib): per-scenario charts of ns/entity vs N, with min/median/p95 bars.
- Phase 2: PgScript VM benchmarks (`script/`) vs Lua, LuaJIT, GDScript, Wren, Python.
