#!/usr/bin/env bash
# Fast PgScript-only bench (~30 sec). Runs scenarios at multiple counts then
# prints the aggregated table. Use this for tight iteration when measuring
# PgScript changes; switch to bench_all.sh when you want the multi-language
# comparison.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
COMP_DIR="$(cd "$HERE/.." && pwd)"
CSV="$COMP_DIR/results/raw/scripts.csv"

mkdir -p "$(dirname "$CSV")"

# Run from the build dir so pgscript_run is findable next to the binary.
# run_scripts.py already searches release/ and build/ for the binary, but the
# bench needs the working dir to be the build dir so relative module paths
# resolve correctly.
cd "$(git -C "$HERE" rev-parse --show-toplevel)/release"

python3 "$HERE/run_scripts.py" \
    --languages=pgscript,pgscript-ast \
    --runs=5 --warmup=1 \
    --output="$CSV"

python3 "$HERE/aggregate.py" --csv="$CSV" --lang=pgscript,pgscript-ast
