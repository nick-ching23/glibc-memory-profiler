#!/usr/bin/env bash
set -euo pipefail

# Directory of the script (malloc-benchmarks/)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# Repo root is one level up
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

GLIBC_INSTALL="$ROOT_DIR/glibc-install"
LDLOADER="$GLIBC_INSTALL/lib/ld-linux-aarch64.so.1"
LDLIB="$GLIBC_INSTALL/lib"

echo "== Custom glibc (profiler ON) =="

for b in bench_fixed bench_var bench_mt; do
    echo
    echo "-- $b (custom, GLIBC_MALLOC_PROFILE=1) --"
    GLIBC_MALLOC_PROFILE=1 \
        "$LDLOADER" --library-path "$LDLIB" "$SCRIPT_DIR/$b"
done

for b in bench_churn bench_churn_mt; do
    echo
    echo "-- $b (custom ON) --"
    GLIBC_MALLOC_PROFILE=1 \
        "$LDLOADER" --library-path "$LDLIB" "$SCRIPT_DIR/$b"
done