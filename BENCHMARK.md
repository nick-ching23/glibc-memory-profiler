# Benchmarks

Build both the custom glibc and original glibc.

```bash
./build-glibc.sh
./build-orig.sh
```

```bash
cd malloc-benchmarks
make
./benchmark.py
```
