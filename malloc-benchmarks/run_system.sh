#!/usr/bin/env bash
set -euo pipefail

echo "== System glibc =="
for b in bench_fixed bench_var bench_mt; do
    echo
    echo "-- $b (system) --"
    ./$b
done

for b in bench_churn bench_churn_mt; do
    echo
    echo "-- $b (system) --"
    ./$b
done