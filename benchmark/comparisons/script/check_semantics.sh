#!/usr/bin/env bash
# Differential semantics check: every script must produce byte-identical
# stdout (and the same exit code) with the full O3 pipeline and with --no-opt.
# The unoptimized interpreter is the semantic ground truth; any divergence is
# an optimizer or fast-path bug.
#
#   scripts/semantics/*.pg  — deterministic output, diffed byte-for-byte
#   scripts/*.pg            — bench scenarios; they print timings, so they are
#                             only smoke-run (must exit 0 in both modes)
#
# Usage: ./check_semantics.sh   (exit 0 = all pass)
set -uo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(git -C "$HERE" rev-parse --show-toplevel)"
BIN="$REPO/release/benchmark/comparisons/script/pgscript_run"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

if [[ ! -x "$BIN" ]]; then
    echo "pgscript_run not found at $BIN — build it first:" >&2
    echo "    cmake --build $REPO/release --target pgscript_run -j" >&2
    exit 1
fi

fail=0

# The VM's runtime-error handler dumps the value stack to stdout; its contents
# legitimately differ between opt modes (fused instructions absorb operands
# that --no-opt leaves on the stack), so those dump lines are filtered out.
strip_stack_dump() { grep -v '^ *\[<closure' "$1" > "$1.flt" || true; }

for script in "$HERE"/scripts/semantics/*.pg; do
    name="$(basename "$script")"
    "$BIN" --script="$script" > "$TMP/opt.out" 2>/dev/null
    opt_rc=$?
    "$BIN" --script="$script" --no-opt > "$TMP/noopt.out" 2>/dev/null
    noopt_rc=$?
    strip_stack_dump "$TMP/opt.out";   mv "$TMP/opt.out.flt" "$TMP/opt.out"
    strip_stack_dump "$TMP/noopt.out"; mv "$TMP/noopt.out.flt" "$TMP/noopt.out"

    if [[ "$opt_rc" -ne "$noopt_rc" ]]; then
        echo "FAIL  $name  exit codes differ: opt=$opt_rc no-opt=$noopt_rc"
        fail=1
    elif ! diff -q "$TMP/opt.out" "$TMP/noopt.out" >/dev/null; then
        echo "FAIL  $name  stdout differs (opt vs --no-opt):"
        diff "$TMP/noopt.out" "$TMP/opt.out" | head -20
        fail=1
    else
        echo "ok    $name  (exit $opt_rc, $(wc -l < "$TMP/opt.out") lines)"
    fi
done

for script in "$HERE"/scripts/*.pg; do
    name="$(basename "$script")"
    if ! "$BIN" --script="$script" --count=10 >/dev/null 2>&1; then
        echo "FAIL  $name  (bench smoke, optimized)"
        fail=1
    elif ! "$BIN" --script="$script" --count=10 --no-opt >/dev/null 2>&1; then
        echo "FAIL  $name  (bench smoke, --no-opt)"
        fail=1
    else
        echo "ok    $name  (bench smoke)"
    fi
done

if [[ "$fail" -ne 0 ]]; then
    echo "SEMANTICS CHECK FAILED"
    exit 1
fi
echo "all semantics checks passed"
