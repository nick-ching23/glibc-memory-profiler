#!/usr/bin/env bash
set -euo pipefail

LDLOADER="$HOME/Desktop/glibc-install/lib/ld-linux-aarch64.so.1"
LDLIB="$HOME/Desktop/glibc-install/lib"

echo "== Custom glibc (profiler ON) =="

for b in bench_fixed bench_var bench_mt; do
    echo
    echo "-- $b (custom, GLIBC_MALLOC_PROFILE=1) --"
    GLIBC_MALLOC_PROFILE=1 \
    "$LDLOADER" --library-path "$LDLIB" "./$b"
done

for b in bench_churn bench_churn_mt; do
    echo
    echo "-- $b (custom ON) --"
    GLIBC_MALLOC_PROFILE=1 \
        "$LDLOADER" --library-path "$LDLIB" "./$b"
done