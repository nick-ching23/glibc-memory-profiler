// mprof-tools/alloc_hist.bpf.c
// 1. Use standard Linux headers instead of vmlinux.h to avoid type conflicts
#include "vmlinux.h"

#include <stdbool.h>

#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/usdt.bpf.h>

// 2. Define the histogram map
struct {
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, 64);
    __type(key, __u32);
    __type(value, __u64);
} hist_map SEC(".maps");

// 3. Manual Log2 function (Safe for Clang-14/ARM64)
// We replace __builtin_clzll with a manual bit check to stop the compiler crash.
static __always_inline __u32 log2_func(__u64 v) {
    __u32 shift = 0;
    
    // Binary search for the highest set bit
    if (v & 0xFFFFFFFF00000000) { shift += 32; v >>= 32; }
    if (v & 0xFFFF0000)         { shift += 16; v >>= 16; }
    if (v & 0xFF00)             { shift += 8;  v >>= 8;  }
    if (v & 0xF0)               { shift += 4;  v >>= 4;  }
    if (v & 0xC)                { shift += 2;  v >>= 2;  }
    if (v & 0x2)                { shift += 1;  v >>= 1;  }
    
    return shift;
}

// 4. The USDT Probe
SEC("usdt")
int BPF_USDT(trace_alloc, __u64 size) {
    __u32 bucket_key = log2_func(size);
    __u64 *count;

    count = bpf_map_lookup_elem(&hist_map, &bucket_key);
    if (!count)
        return 0;

    __sync_fetch_and_add(count, 1);
    
    return 0;
}

char LICENSE[] SEC("license") = "Dual BSD/GPL";