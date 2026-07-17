#!/usr/bin/env bash
# perf stat on a single PgScript scenario. Prints cycles, instructions, IPC,
# branch-miss rate. Requires kernel.perf_event_paranoid <= 1 (the session
# usually sets this via `sudo sysctl -w kernel.perf_event_paranoid=1`).
#
# Usage:
#   ./perf_pg.sh                       # hot_loop @ 1M, 1 run
#   ./perf_pg.sh hot_loop 1000000      # explicit
#   ./perf_pg.sh compute_pi 100000 5   # 5 runs
set -euo pipefail

SCENARIO="${1:-hot_loop}"
COUNT="${2:-1000000}"
RUNS="${3:-1}"

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(git -C "$HERE" rev-parse --show-toplevel)"
SCRIPTS="$HERE/scripts"
BIN="$REPO/release/benchmark/comparisons/script/pgscript_run"

if [[ ! -x "$BIN" ]]; then
    echo "pgscript_run not found at $BIN — build it first:" >&2
    echo "    cmake --build $REPO/release --target pgscript_run -j" >&2
    exit 1
fi

SCRIPT="$SCRIPTS/${SCENARIO}.pg"
if [[ ! -f "$SCRIPT" ]]; then
    echo "no such scenario: $SCRIPT" >&2
    echo "available:" >&2
    ls "$SCRIPTS"/*.pg | xargs -n1 basename | sed 's/\.pg$//' >&2
    exit 1
fi

echo "=== perf stat: $SCENARIO n=$COUNT ($RUNS run(s)) ==="
for r in $(seq 1 "$RUNS"); do
    perf stat -e cycles,instructions,branches,branch-misses 2>&1 \
        "$BIN" --script="$SCRIPT" --count="$COUNT" \
        | awk '/cycles|instructions|branches|elapsed/'
    echo
done
