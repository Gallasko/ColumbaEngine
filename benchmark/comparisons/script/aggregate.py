#!/usr/bin/env python3
"""
Aggregate the script bench CSV into a human-readable comparison table.

Reads benchmark/comparisons/results/raw/scripts.csv and prints min-of-runs
per (language, scenario, count). Pass `--lang=pgscript` (etc.) to filter,
or `--csv=PATH` to point at a different file.

Default output is "script_ns" (in-script self-time, pure VM cost). Pass
`--wall` to use the subprocess wall-clock column instead.
"""

import argparse
import csv
import os
import sys
from collections import defaultdict


SCRIPT_DIR  = os.path.dirname(os.path.abspath(__file__))
DEFAULT_CSV = os.path.join(SCRIPT_DIR, "..", "results", "raw", "scripts.csv")

SCENARIOS = [
    "compute_pi", "fib_recursive", "hot_loop",
    "binary_tree", "matmul", "table_lookup",
]

COUNTS = {
    "compute_pi":    [10_000, 100_000, 1_000_000],
    "fib_recursive": [20, 25, 28],
    "hot_loop":      [10_000, 100_000, 1_000_000],
    "binary_tree":   [10, 14, 16],
    "matmul":        [16, 32, 64],
    "table_lookup":  [10_000, 100_000, 1_000_000],
}

# Order matters: PgScript first so it's the leftmost column when present.
LANG_ORDER = ["pgscript", "pgscript-ast", "pgscript-noopt", "lua", "python"]
LANG_LABEL = {
    "pgscript":        "PgScript O3",
    "pgscript-ast":    "PgScript AST",
    "pgscript-noopt":  "PgScript O0",
    "lua":             "Lua 5.4",
    "python":          "CPython",
}


def fmt_ms(ns):
    if not ns:
        return "       -"
    return f"{ns / 1e6:>7.2f} ms"


def load(csv_path, column):
    """Returns dict[(lang, scenario, count)] -> list[int ns]."""
    out = defaultdict(list)
    with open(csv_path) as f:
        for r in csv.DictReader(f):
            v = r.get(column)
            if v:
                out[(r["language"], r["scenario"], int(r["count"]))].append(int(v))
    return out


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--csv",   default=DEFAULT_CSV)
    p.add_argument("--lang",  default="",
                   help="comma-separated language filter (default: all present)")
    p.add_argument("--wall",  action="store_true",
                   help="use wall_ns column instead of script_ns (default)")
    args = p.parse_args()

    if not os.path.exists(args.csv):
        sys.exit(f"CSV not found: {args.csv}")

    column = "wall_ns" if args.wall else "script_ns"
    rows = load(args.csv, column)

    if not rows:
        sys.exit(f"no rows with column '{column}' in {args.csv}")

    present = {k[0] for k in rows.keys()}
    langs = [l for l in LANG_ORDER if l in present]
    # Languages in the CSV but missing from LANG_ORDER must still show up
    # (new variants would otherwise silently vanish from the table)
    langs += sorted(l for l in present if l not in LANG_ORDER)
    if args.lang:
        wanted = {l.strip() for l in args.lang.split(",") if l.strip()}
        langs = [l for l in langs if l in wanted]
    if not langs:
        sys.exit("no matching languages in CSV")

    label = "WALL TIME (incl. startup)" if args.wall else "SCRIPT TIME (pure VM, in-script self-timed)"
    print(f"\n=== {label} — min of runs ===\n")
    header = f"  {'scenario':<14} {'count':>10}   " + "  ".join(f"{LANG_LABEL.get(l, l):>12}" for l in langs)
    print(header)
    print("  " + "-" * (len(header) - 2))
    for s in SCENARIOS:
        for n in COUNTS[s]:
            row = {l: (min(rows[(l, s, n)]) if rows.get((l, s, n)) else 0) for l in langs}
            cells = "  ".join(f"{fmt_ms(row[l]):>12}" for l in langs)
            print(f"  {s:<14} {n:>10}   {cells}")
        print()


if __name__ == "__main__":
    main()
