#!/usr/bin/env python3
"""
Python opcode-level analyzer for the script bench scenarios.

For each scenario it does two runs of the .py file:

  1. Untraced wall-clock timing  (min of N runs, no overhead)
  2. Traced run via sys.settrace + frame.f_trace_opcodes  (counts opcodes)

These can't share a run — tracing adds ~50-200x overhead. The trace also
uses a smaller --count so it finishes in reasonable time; timing uses the
same smaller count so the ns/opcode figure is internally consistent.

Output per scenario:
  * Static disassembly of the hot function (via dis.dis)
  * Top-N most-frequent opcodes with share of total
  * Total opcodes executed, wall time, and average ns/opcode
"""

import argparse
import contextlib
import dis
import io
import os
import statistics
import sys
import time
from collections import Counter


SCRIPTS_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "scripts")

# (script_filename, hot-function-name-to-disassemble, trace-count)
# trace-count is small enough that the traced run takes a few seconds at most.
SCENARIOS = [
    ("compute_pi",    "compute_pi.py",    "compute_pi",  10_000),
    ("fib_recursive", "fib_recursive.py", "fib",         20),
    ("hot_loop",      "hot_loop.py",      "hot_loop",    10_000),
    ("binary_tree",   "binary_tree.py",   "count_nodes", 10),
    ("matmul",        "matmul.py",        "multiply",    16),
    ("table_lookup",  "table_lookup.py",  "sum_lookup",  10_000),
]


def compile_script(path):
    with open(path) as f:
        return compile(f.read(), path, "exec")


def _exec_silenced(code):
    """exec the script with its stdout swallowed so our analysis output
    isn't interleaved with the script's own `print(elapsed)`."""
    with contextlib.redirect_stdout(io.StringIO()):
        exec(code, {"__name__": "__main__"})


def time_untraced(code, path, count, runs=3):
    sys.argv = [path, str(count)]
    times = []
    for _ in range(runs):
        t0 = time.perf_counter_ns()
        _exec_silenced(code)
        times.append(time.perf_counter_ns() - t0)
    return min(times), statistics.median(times)


def trace_opcodes(code, path, count):
    sys.argv = [path, str(count)]
    counts = Counter()

    def tracer(frame, event, arg):
        # f_trace_opcodes must be set on every entered frame.
        frame.f_trace_opcodes = True
        if event == "opcode":
            opcode = frame.f_code.co_code[frame.f_lasti]
            counts[opcode] += 1
        return tracer

    sys.settrace(tracer)
    try:
        _exec_silenced(code)
    finally:
        sys.settrace(None)

    return counts


def find_function(code, name):
    """Walk the module's code object recursively to find a function by name.
    Returns the inner code object or None."""
    for const in code.co_consts:
        if isinstance(const, type(code)):
            if const.co_name == name:
                return const
            found = find_function(const, name)
            if found is not None:
                return found
    return None


def opname(code_int):
    """Map a numeric opcode to its mnemonic."""
    try:
        return dis.opname[code_int]
    except IndexError:
        return f"<{code_int}>"


def analyse(label, filename, hot_func, count):
    path = os.path.join(SCRIPTS_DIR, filename)
    code = compile_script(path)

    print("=" * 72)
    print(f"  {label.upper()}  ({filename}, count={count})")
    print("=" * 72)

    # Static disassembly of the hot function.
    fn_code = find_function(code, hot_func)
    if fn_code is not None:
        print(f"\n--- Static disassembly: {hot_func}() ---")
        dis.dis(fn_code)
    else:
        print(f"  (hot function {hot_func!r} not found in {filename})")

    # Untraced wall time.
    print("\n--- Wall time (untraced) ---")
    best, med = time_untraced(code, path, count, runs=3)
    print(f"  best of 3:   {best:>12,} ns  ({best / 1e6:.3f} ms)")
    print(f"  median of 3: {med:>12,} ns  ({med  / 1e6:.3f} ms)")

    # Traced opcode counts.
    print("\n--- Opcode counts (traced run) ---")
    counts = trace_opcodes(code, path, count)
    total = sum(counts.values())
    print(f"  total opcodes executed: {total:>12,}")
    print(f"  unique opcodes:         {len(counts):>12,}")
    if total > 0:
        ns_per_op = best / total
        print(f"  ns / opcode (best wall / total ops): {ns_per_op:.3f}")

    print("\n  top opcodes by frequency:")
    for op, n in counts.most_common(12):
        share = 100.0 * n / total if total else 0
        print(f"    {opname(op):<24} {n:>10,}  ({share:5.1f}%)")

    print()
    return {"label": label, "best_ns": best, "total_ops": total, "counts": counts}


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--scenarios", default="",
                   help="comma-separated subset (default: all)")
    args = p.parse_args()

    wanted = set(args.scenarios.split(",")) if args.scenarios else None

    rows = []
    for label, filename, hot, count in SCENARIOS:
        if wanted and label not in wanted:
            continue
        rows.append(analyse(label, filename, hot, count))

    # Cross-scenario summary table.
    print("=" * 72)
    print("  SUMMARY  (per scenario, at its trace-count)")
    print("=" * 72)
    print(f"  {'scenario':<14} {'count':>8}  {'wall_ns':>14}  {'total_ops':>14}  {'ns/op':>8}")
    print("  " + "-" * 70)
    for r, (label, filename, hot, count) in zip(rows, SCENARIOS):
        if wanted and label not in wanted:
            continue
        ns_per = r["best_ns"] / r["total_ops"] if r["total_ops"] else 0
        print(f"  {label:<14} {count:>8}  {r['best_ns']:>14,}  {r['total_ops']:>14,}  {ns_per:>8.3f}")


if __name__ == "__main__":
    main()
