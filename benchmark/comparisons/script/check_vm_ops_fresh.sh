#!/usr/bin/env bash
# Freshness guard for the generated VM op-kernel files: regenerates the .inc
# files from tools/vm_ops_def.pg into a temp dir and diffs them against the
# committed copies in src/Engine/Compiler/generated/. Fails if someone edited
# the spec (or the generator) without rerunning GenerateVmOps, or hand-edited
# a generated file.
#
# Usage: ./check_vm_ops_fresh.sh   (exit 0 = committed files are fresh)
set -uo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(git -C "$HERE" rev-parse --show-toplevel)"
GEN_DIR="$REPO/src/Engine/Compiler/generated"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

# Any built runner with the file module works; prefer the bootstrap compiler.
RUNNER=""
for cand in "$REPO/release/PgCompilerBootstrap" "$REPO/release/PgCompilerRel" \
            "$REPO/release/bootstrap/PgCompiler" "$REPO/build/PgCompilerBootstrap"; do
    if [[ -x "$cand" ]]; then RUNNER="$cand"; break; fi
done
if [[ -z "$RUNNER" ]]; then
    echo "no PgCompiler runner found — build PgCompilerBootstrap first" >&2
    exit 1
fi

# Run from tools/ so `import "vm_ops_def"` resolves.
(cd "$REPO/tools" && "$RUNNER" gen_vm_ops.pg "$TMP" >/dev/null 2>&1)

fail=0
for committed in "$GEN_DIR"/*.inc; do
    name="$(basename "$committed")"
    if [[ ! -f "$TMP/$name" ]]; then
        echo "STALE  $name: generator no longer produces this file"
        fail=1
    elif ! diff -q "$committed" "$TMP/$name" >/dev/null; then
        echo "STALE  $name: committed file differs from freshly generated output:"
        diff "$committed" "$TMP/$name" | head -20
        fail=1
    else
        echo "fresh  $name"
    fi
done
for generated in "$TMP"/*.inc; do
    name="$(basename "$generated")"
    if [[ ! -f "$GEN_DIR/$name" ]]; then
        echo "STALE  $name: generated but not committed — run GenerateVmOps and commit"
        fail=1
    fi
done

if [[ "$fail" -ne 0 ]]; then
    echo "VM OPS FRESHNESS CHECK FAILED — run: cmake --build <build-dir> --target GenerateVmOps"
    exit 1
fi
echo "generated VM op files are fresh"
