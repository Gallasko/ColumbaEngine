#!/usr/bin/env python3
"""
PgScript opcode-level analyzer. Mirrors analyze_python.py.

For each scenario it does two runs of the .pg file via pgscript_run:

  1. Untraced wall-clock timing (min of N runs) — the script self-reports its
     in-script elapsed ns via `now()`; we use that as the "wall_ns" so we are
     measuring the same thing Python's analyzer measures (workload only,
     not interpreter startup).

  2. Profiled run (--profile) once — pgscript_run prints the VM's
     per-instruction count report. We parse total instructions executed plus
     a count per opcode. The profiler adds ~10-15x overhead, so we cannot use
     its self-reported time for the ns/op figure — we cross-multiply against
     the untraced wall time.

Output mirrors analyze_python.py for direct side-by-side comparison.
"""

import argparse
import os
import re
import statistics
import subprocess
import sys
from collections import Counter


SCRIPT_DIR  = os.path.dirname(os.path.abspath(__file__))
SCRIPTS_DIR = os.path.join(SCRIPT_DIR, "scripts")


def find_pgscript_run():
    """Resolve pgscript_run from PGSCRIPT_RUN env or common build dirs."""
    env = os.environ.get("PGSCRIPT_RUN")
    if env and os.path.isfile(env) and os.access(env, os.X_OK):
        return env
    repo = os.path.abspath(os.path.join(SCRIPT_DIR, "..", "..", ".."))
    for candidate in [
        os.path.join(repo, "release", "benchmark", "comparisons", "script", "pgscript_run"),
        os.path.join(repo, "build",   "benchmark", "comparisons", "script", "pgscript_run"),
    ]:
        if os.path.isfile(candidate) and os.access(candidate, os.X_OK):
            return candidate
    sys.exit("pgscript_run not found (set PGSCRIPT_RUN or build the target)")


PGSCRIPT_RUN = find_pgscript_run()


# Match Python analyzer counts so the ns/op comparison is at the same workload.
SCENARIOS = [
    ("compute_pi",    "compute_pi.pg",    10_000),
    ("fib_recursive", "fib_recursive.pg", 20),
    ("hot_loop",      "hot_loop.pg",      10_000),
    ("binary_tree",   "binary_tree.pg",   10),
    ("matmul",        "matmul.pg",        16),
    ("table_lookup",  "table_lookup.pg",  10_000),
]


def run_untraced(script_path, count):
    """Returns the script's self-reported elapsed ns (last int line on stdout)."""
    res = subprocess.run(
        [PGSCRIPT_RUN, f"--script={script_path}", f"--count={count}"],
        capture_output=True, text=True, timeout=120,
    )
    if res.returncode != 0:
        sys.exit(f"pgscript_run failed: {res.stderr.strip()}")
    # Last integer line of stdout = the script's `print(elapsed)`.
    for line in reversed(res.stdout.strip().splitlines()):
        try:
            return int(line.strip())
        except ValueError:
            continue
    sys.exit(f"could not parse elapsed ns from: {res.stdout!r}")


def time_untraced(script_path, count, runs=3):
    times = [run_untraced(script_path, count) for _ in range(runs)]
    return min(times), statistics.median(times)


# Report header looks like:
#   Function            Offset  Opcode                   Count       Total (ms)     Avg (ns)       % Time
#   ---------------------------------------------------------------
#   hotLoop             6       OP_Get_Local             10001       0.191677       19.17          5.77      %
ROW_RE = re.compile(
    r"^\s*(\S+)\s+(\d+)\s+(OP_\S+)\s+(\d+)\s+([\d.]+)\s+([\d.]+)\s+([\d.]+)\s*%?\s*$"
)
TOTAL_RE = re.compile(r"^Total instructions executed:\s+(\d+)")


def run_profiled(script_path, count):
    """Returns (total_instructions, counter[opcode_name] -> count)."""
    res = subprocess.run(
        [PGSCRIPT_RUN, f"--script={script_path}", f"--count={count}", "--profile"],
        capture_output=True, text=True, timeout=300,
    )
    if res.returncode != 0:
        sys.exit(f"pgscript_run --profile failed: {res.stderr.strip()}")

    total = None
    counts = Counter()
    in_table = False
    for line in res.stdout.splitlines():
        if total is None:
            m = TOTAL_RE.match(line)
            if m:
                total = int(m.group(1))
                continue
        # The per-instruction table starts after a dashes-only separator.
        if set(line.strip()) == {"-"} and len(line.strip()) > 10:
            in_table = True
            continue
        if in_table:
            m = ROW_RE.match(line)
            if m:
                opname = m.group(3)
                cnt    = int(m.group(4))
                counts[opname] += cnt
    if total is None:
        sys.exit("could not find 'Total instructions executed' in profile output")
    return total, counts


def analyse(label, filename, count, top_n=12):
    script = os.path.join(SCRIPTS_DIR, filename)

    print("=" * 72)
    print(f"  {label.upper()}  ({filename}, count={count})")
    print("=" * 72)

    best, med = time_untraced(script, count, runs=3)
    print("\n--- Wall time (untraced; script self-reported via now()) ---")
    print(f"  best of 3:   {best:>12,} ns  ({best / 1e6:.3f} ms)")
    print(f"  median of 3: {med:>12,} ns  ({med  / 1e6:.3f} ms)")

    total, counts = run_profiled(script, count)
    print("\n--- Opcode counts (profiled run) ---")
    print(f"  total instructions executed: {total:>12,}")
    print(f"  unique opcodes:              {len(counts):>12,}")
    ns_per_op = best / total if total else 0
    print(f"  ns / opcode (best untraced wall / total ops): {ns_per_op:.3f}")

    print("\n  top opcodes by count:")
    for op, n in counts.most_common(top_n):
        share = 100.0 * n / total if total else 0
        print(f"    {op:<26} {n:>10,}  ({share:5.1f}%)")
    print()

    return {"label": label, "count": count, "best_ns": best,
            "total_ops": total, "counts": counts}


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--scenarios", default="",
                   help="comma-separated subset (default: all)")
    args = p.parse_args()

    wanted = set(args.scenarios.split(",")) if args.scenarios else None

    rows = []
    for label, filename, count in SCENARIOS:
        if wanted and label not in wanted:
            continue
        rows.append(analyse(label, filename, count))

    print("=" * 72)
    print("  SUMMARY  (per scenario, untraced wall / profiled opcode count)")
    print("=" * 72)
    print(f"  {'scenario':<14} {'count':>8}  {'wall_ns':>14}  {'total_ops':>14}  {'ns/op':>8}")
    print("  " + "-" * 70)
    for r in rows:
        ns_per = r["best_ns"] / r["total_ops"] if r["total_ops"] else 0
        print(f"  {r['label']:<14} {r['count']:>8}  {r['best_ns']:>14,}  {r['total_ops']:>14,}  {ns_per:>8.3f}")


if __name__ == "__main__":
    main()
