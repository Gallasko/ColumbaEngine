#!/usr/bin/env bash
# Full multi-language bench (~3 min): PgScript O3, PgScript O0, Lua, Python.
# Prints the aggregated table at the end.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
COMP_DIR="$(cd "$HERE/.." && pwd)"
CSV="$COMP_DIR/results/raw/scripts.csv"

mkdir -p "$(dirname "$CSV")"

cd "$(git -C "$HERE" rev-parse --show-toplevel)/release"

python3 "$HERE/run_scripts.py" \
    --runs=5 --warmup=1 \
    --output="$CSV"

python3 "$HERE/aggregate.py" --csv="$CSV"
