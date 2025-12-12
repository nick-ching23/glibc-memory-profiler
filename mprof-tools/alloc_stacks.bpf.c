// mprof-tools/alloc_stacks.bpf.c
#include "vmlinux.h"
#include <stdbool.h>

#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/usdt.bpf.h>

char LICENSE[] SEC("license") = "Dual BSD/GPL";

/* Aggregate per stack */
struct agg {
    __u64 count;
    __u64 bytes;
};

/* --- stack trace storage --- */
#define MAX_STACK_DEPTH 127

struct {
    __uint(type, BPF_MAP_TYPE_STACK_TRACE);
    __uint(max_entries, 8192);
    __type(key, __u32);
    __type(value, __u64[MAX_STACK_DEPTH]);
} stackmap SEC(".maps");

/* --- per-cpu aggregation by stack id --- */
struct {
    __uint(type, BPF_MAP_TYPE_PERCPU_HASH);
    __uint(max_entries, 32768);
    __type(key, __u32);        /* stack_id */
    __type(value, struct agg);
} stacks SEC(".maps");

/* --- debug sanity counter: how many sane sizes we saw --- */
struct {
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, 1);
    __type(key, __u32);
    __type(value, __u64);
} seen_ok SEC(".maps");

SEC("usdt")
int trace_alloc(struct pt_regs *ctx)
{
    long size_l = 0;

    /* USDT arg #0 == size (confirmed via bpftrace) */
    if (bpf_usdt_arg(ctx, 0, &size_l) < 0)
        return 0;

    __u64 size = (__u64)size_l;

    /* sanity clamp: your benchmark maxes at 2 MiB */
    if (size == 0 || size > (4ULL << 20))
        return 0;

    /* record that we saw a sane size */
    {
        __u32 k = 0;
        __u64 *v = bpf_map_lookup_elem(&seen_ok, &k);
        if (v)
            (*v)++;
    }

    int sid = bpf_get_stackid(ctx, &stackmap, BPF_F_USER_STACK);
    if (sid < 0)
        return 0;

    __u32 key = (__u32)sid;

    struct agg *a = bpf_map_lookup_elem(&stacks, &key);
    if (!a) {
        struct agg init = {
            .count = 1,
            .bytes = size,
        };
        bpf_map_update_elem(&stacks, &key, &init, BPF_ANY);
        return 0;
    }

    a->count += 1;
    a->bytes += size;
    return 0;
}