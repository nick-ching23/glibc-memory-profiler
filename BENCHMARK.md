# Benchmarks

Build both the custom glibc and original glibc.

```bash
./build-glibc.sh
./build-orig.sh
```

```bash
cd malloc-benchmarks
make
./scripts/benchmark.py
```

## Sample Output

Ubuntu 24.04 LTS, x86_64, AMD EPYC 9354P

> [!NOTE]
> `baseline2` is identical to `baseline`. It's used to ensure the benchmark is valid (ratio should be ~1.00).

```
======================= SUMMARY (average ns per operation) ================================

NOTE: Value in parentheses is the ratio compared to `baseline`.

Benchmark                     baseline           baseline2                 off                  on
--------------------------------------------------------------------------------------------------
bench_fixed                      5.419        5.403 (1.00)        5.314 (0.98)        5.572 (1.03)
bench_var                        5.612        5.602 (1.00)        6.230 (1.11)        6.741 (1.20)
bench_mt                         1.404        1.396 (0.99)        1.304 (0.93)        1.400 (1.00)
bench_churn                      9.763        9.753 (1.00)        9.966 (1.02)       10.076 (1.03)
bench_churn_mt                   3.941        3.966 (1.01)        3.978 (1.01)        3.991 (1.01)
```
