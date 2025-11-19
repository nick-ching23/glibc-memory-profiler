# Glibc Malloc Sampling Profiler

> ⚠️ **PRE-ALPHA / DRAFT STATUS**  
> This is an early prototype. Significant performance verification, overhead analysis, and hot-path instruction tuning are ongoing. Use with caution.

A low-latency, byte-based sampling profiler integrated directly into **glibc’s `malloc`**.

Designed for high-performance production environments, this profiler avoids the cost of tracing every allocation and instead uses **statistical sampling**, **TLS fast paths**, and **lock-free counters** to achieve near-zero overhead in the hot path.

---

## 🔧 Design Overview

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

## 🛠️ Build & Install

This project requires an **out-of-tree build**.  
Your workspace should look like:

```
repo/
├── glibc-src/
├── glibc-build/
└── glibc-install/
```

1. Create Build & Install Directories
  ```
  mkdir glibc-build
  mkdir glibc-install
  ```
2. Configure the build
  ```
  cd glibc-build

  ../glibc-src/glibc/configure \
      --prefix=$(realpath ../glibc-install) \
      --disable-werror
  ```
