#!/usr/bin/env python3
"""
Script-language benchmark harness.

For each (language, scenario, count, run) it spawns the language's interpreter
on the corresponding script and records two timings:

  wall_ns    end-to-end subprocess time (includes interpreter startup, parse,
             compile, execute) — what a user actually pays
  script_ns  in-script self-timed duration (excludes startup) — pure VM cost

Both go to the same CSV row so we can compare them later.

CSV schema:
    language,scenario,count,run_id,wall_ns,script_ns,peak_rss_kb

Pluggable per-language: each entry in LANGUAGES describes its file extension,
the command vector, and the count-passing convention.

Usage:
    python3 run_scripts.py [--languages=pgscript,python] [--scenarios=...]
                           [--counts=...] [--runs=5] [--warmup=1]
                           [--output=results.csv] [--append]
"""

import argparse
import csv
import os
import resource
import subprocess
import sys
import time
from typing import List, Optional

SCRIPT_DIR  = os.path.dirname(os.path.abspath(__file__))
SCRIPTS_DIR = os.path.join(SCRIPT_DIR, "scripts")

# ----------------------------------------------------------------------
# Language registry
#
# `cmd(script_path, count)` returns the argv list to subprocess.run().
# `ext` is the script file extension (used to find the matching scenario).
# `enabled_if` is a callable that returns True if the interpreter is reachable.
# ----------------------------------------------------------------------

# Resolve pgscript_run binary lazily — falls back to PATH or PGSCRIPT_RUN env.
def _pgscript_bin():
    env = os.environ.get("PGSCRIPT_RUN")
    if env and os.path.isfile(env) and os.access(env, os.X_OK):
        return env
    # Search common build output locations relative to repo root.
    repo = os.path.abspath(os.path.join(SCRIPT_DIR, "..", "..", ".."))
    for candidate in [
        os.path.join(repo, "release", "benchmark", "comparisons", "script", "pgscript_run"),
        os.path.join(repo, "build",   "benchmark", "comparisons", "script", "pgscript_run"),
    ]:
        if os.path.isfile(candidate) and os.access(candidate, os.X_OK):
            return candidate
    return "pgscript_run"  # last resort — must be on PATH


LANGUAGES = {
    "pgscript": {
        "ext": "pg",
        "cmd": lambda script, n: [_pgscript_bin(), f"--script={script}", f"--count={n}"],
    },
    "python": {
        "ext": "py",
        "cmd": lambda script, n: ["python3", script, str(n)],
    },
    # When lua / luajit / wren / godot get installed, just add entries here.
    # "lua":    {"ext": "lua",  "cmd": lambda s, n: ["lua",    s, str(n)]},
    # "luajit": {"ext": "lua",  "cmd": lambda s, n: ["luajit", s, str(n)]},
    # "wren":   {"ext": "wren", "cmd": lambda s, n: ["wren_cli", s, str(n)]},
    # "gdscript": {"ext": "gd", "cmd": lambda s, n: ["godot", "--headless", "--script", s, "--", str(n)]},
}

SCENARIOS = ["compute_pi", "fib_recursive", "hot_loop"]
DEFAULT_COUNTS = {
    "compute_pi":   [10_000, 100_000, 1_000_000],
    "fib_recursive": [20, 25, 28],          # exponential; fib(28) ~ 800k calls
    "hot_loop":     [10_000, 100_000, 1_000_000],
}

# ----------------------------------------------------------------------
# Measurement
# ----------------------------------------------------------------------

class RunResult:
    def __init__(self, wall_ns: int, script_ns: Optional[int],
                 peak_rss_kb: int, ok: bool, stderr: str):
        self.wall_ns     = wall_ns
        self.script_ns   = script_ns
        self.peak_rss_kb = peak_rss_kb
        self.ok          = ok
        self.stderr      = stderr


def _last_int_on_stdout(stdout: str) -> Optional[int]:
    """Pick the last line that parses as an int. That's the script's self-time."""
    for line in reversed(stdout.strip().splitlines()):
        s = line.strip()
        try:
            return int(s)
        except ValueError:
            continue
    return None


def run_once(cmd: List[str]) -> RunResult:
    """One measured invocation. Times wall-clock externally; reads script_ns
    from the last integer line on stdout. Captures child peak RSS via wait4."""

    # subprocess.run with shell=False — measure wall-clock via perf_counter_ns
    # and child RSS via getrusage(RUSAGE_CHILDREN). The latter is cumulative
    # across all children, so we diff before/after.
    rusage_before = resource.getrusage(resource.RUSAGE_CHILDREN)
    t0 = time.perf_counter_ns()

    try:
        proc = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=120,
        )
    except subprocess.TimeoutExpired as e:
        return RunResult(0, None, 0, False, f"timeout: {e}")

    wall_ns = time.perf_counter_ns() - t0
    rusage_after = resource.getrusage(resource.RUSAGE_CHILDREN)

    # ru_maxrss is the peak observed across all children so far. Differencing
    # gives an upper bound for this run; absolute value is fine if previous
    # children were smaller, which is the common case for our workloads.
    peak_rss_kb = max(0, rusage_after.ru_maxrss - rusage_before.ru_maxrss)
    if peak_rss_kb == 0:
        peak_rss_kb = rusage_after.ru_maxrss

    if proc.returncode != 0:
        return RunResult(wall_ns, None, peak_rss_kb, False,
                         (proc.stderr or "")[:500])

    return RunResult(
        wall_ns,
        _last_int_on_stdout(proc.stdout),
        peak_rss_kb,
        True,
        "",
    )

# ----------------------------------------------------------------------
# Main
# ----------------------------------------------------------------------

def parse_csv_list(s: str) -> List[str]:
    return [x for x in s.split(",") if x]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--languages", default=",".join(LANGUAGES.keys()),
                   help="comma-separated language IDs (default: all)")
    p.add_argument("--scenarios", default=",".join(SCENARIOS),
                   help="comma-separated scenario names (default: all)")
    p.add_argument("--counts", default="",
                   help="override count list (comma-separated). default: per-scenario defaults")
    p.add_argument("--runs", type=int, default=5)
    p.add_argument("--warmup", type=int, default=1)
    p.add_argument("--output", default="",
                   help="CSV output path (default: stdout)")
    p.add_argument("--append", action="store_true",
                   help="don't emit CSV header")
    args = p.parse_args()

    languages = parse_csv_list(args.languages)
    scenarios = parse_csv_list(args.scenarios)
    counts_override = [int(c) for c in parse_csv_list(args.counts)] if args.counts else None

    for lang in languages:
        if lang not in LANGUAGES:
            print(f"unknown language: {lang}", file=sys.stderr)
            return 1
    for s in scenarios:
        if s not in SCENARIOS:
            print(f"unknown scenario: {s}", file=sys.stderr)
            return 1

    out = open(args.output, "a" if args.append else "w") if args.output else sys.stdout
    try:
        writer = csv.writer(out)
        if not args.append:
            writer.writerow(["language", "scenario", "count", "run_id",
                             "wall_ns", "script_ns", "peak_rss_kb"])
            out.flush()

        for lang in languages:
            ext = LANGUAGES[lang]["ext"]
            cmd_fn = LANGUAGES[lang]["cmd"]

            for scen in scenarios:
                script = os.path.join(SCRIPTS_DIR, f"{scen}.{ext}")
                if not os.path.isfile(script):
                    print(f"skip {lang}/{scen}: missing {script}", file=sys.stderr)
                    continue

                count_list = counts_override or DEFAULT_COUNTS[scen]

                for n in count_list:
                    cmd = cmd_fn(script, n)

                    # Warmup runs are silently discarded; they exist so the
                    # OS page cache and any JIT/cache state are populated.
                    for _ in range(args.warmup):
                        run_once(cmd)

                    for r in range(args.runs):
                        res = run_once(cmd)
                        if not res.ok:
                            print(f"FAIL {lang}/{scen} n={n} run={r}: {res.stderr}",
                                  file=sys.stderr)
                            continue
                        writer.writerow([
                            lang, scen, n, r,
                            res.wall_ns,
                            res.script_ns if res.script_ns is not None else "",
                            res.peak_rss_kb,
                        ])
                        out.flush()

                    print(f"  {lang:<10} {scen:<14} n={n:<10} done", file=sys.stderr)
    finally:
        if out is not sys.stdout:
            out.close()

    return 0


if __name__ == "__main__":
    sys.exit(main())
