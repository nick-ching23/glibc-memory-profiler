# Glibc Malloc Sampling Profiler

> [!WARNING]
> This is an early prototype. Significant performance verification, overhead analysis, and hot-path instruction tuning are ongoing. Use with caution.

A low-latency, byte-based sampling profiler integrated directly into **glibc's `malloc`**.

Designed for high-performance production environments, this profiler avoids the cost of tracing every allocation and instead uses **statistical sampling**, **TLS fast paths**, and **lock-free counters** to achieve near-zero overhead in the hot path.

---

## Key Source Files

The profiler is integrated directly into glibc's `malloc` subsystem. The main files involved in this prototype are:

- **`malloc/malloc.c`**\
  Core malloc implementation. Fast-path hooks and threshold checks are inserted here to ensure the profiler runs with minimal overhead.

- **`malloc/malloc_prof.c`**\
  Implementation of the sampling profiler:
  - TLS state & per-thread counters
  - Fast-path decrement logic
  - Slow-path sampling handler
  - Per-thread hash table aggregation
  - Output serialization routines

- **`malloc/malloc_prof.h`**\
  Header exposing profiler interfaces and TLS state structures.\
  Contains:
  - Profiler configuration flags
  - TLS layout
  - Function declarations for the slow path
  - Inline fast-path helpers

## Design Overview

The profiler is built around a **Fast Path / Slow Path** architecture:

### **Sampling Strategy**

- Samples by **allocated bytes** (default: every 512 KB), not call count
- Ensures heavy allocators are captured with high probability
- Filters out noise from small short-lived allocations

### **Fast Path (> 99%)**

- A lock-free, atomic-free TLS counter decrement
- ~4 CPU instructions
- No syscall, no lock, no branches on the common path

### **Slow Path (< 1%)**

- Triggered only when the TLS counter underflows
- Captures:
  - Return address (PC)
  - Allocation size
  - Thread ID
- Aggregates to a fixed-size per-thread hash table
- Performs all heavyweight logic off the hot path

---

## Build/Install

```bash
sudo apt update
sudo apt install bison gperf
```

```bash
mkdir -p glibc-build glibc-install
GLIBC_INSTALL=$(realpath ./glibc-install)

cd glibc-build
../glibc-src/configure --prefix="$GLIBC_INSTALL" --disable-werror

make -j"$(nproc)"
make install
```

## Running the Application

You must explicitly invoke the custom dynamic loader from the freshly built glibc and set the library path.

**Command Template:**

```bash
GLIBC_MALLOC_PROFILE=1 \
GLIBC_MALLOC_PROFILE_BYTES=1024 \
GLIBC_MALLOC_PROFILE_OUT=/tmp/mprof \
"$GLIBC_INSTALL"/lib/ld-linux-<ARCH>.so.<VER> \
    --library-path "$GLIBC_INSTALL"/lib \
    ./your_application
```

Example (x86_64)

```bash
GLIBC_MALLOC_PROFILE=1 \
GLIBC_MALLOC_PROFILE_BYTES=1024 \
GLIBC_MALLOC_PROFILE_OUT=/tmp/mprof \
"$GLIBC_INSTALL"/lib/ld-linux-x86-64.so.2 \
    --library-path "$GLIBC_INSTALL"/lib \
    ./bench_churn
```

## Loader Names by Architecture

| Architecture    | Loader                  |
| --------------- | ----------------------- |
| x86_64          | `ld-linux-x86-64.so.2`  |
| ARM64 (AArch64) | `ld-linux-aarch64.so.1` |
