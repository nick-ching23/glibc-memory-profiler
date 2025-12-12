// mprof-tools/alloc_hist.c
#include <argp.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <bpf/libbpf.h>
#include <bpf/bpf.h>
#include "alloc_hist.skel.h"

// libbpf skeleton types
typedef __u32 u32;
typedef __u64 u64;

static volatile sig_atomic_t exiting = 0;

static void sig_handler(int sig)
{
    (void)sig;
    exiting = 1;
}

/* ------------------------------------------------------
 * Log-scaled histogram rendering (no libm)
 * ----------------------------------------------------*/

/* integer log2; returns -1 for x == 0 */
static inline int ilog2_u64(u64 x)
{
    int r = -1;
    while (x) {
        x >>= 1;
        r++;
    }
    return r;
}

static void print_bar(u32 bucket, u64 count, u64 max_count, int width)
{
    unsigned long long lo =
        (bucket >= 63) ? (1ULL << 63) : (1ULL << bucket);
    unsigned long long hi =
        (bucket >= 63) ? ~0ULL : ((1ULL << (bucket + 1)) - 1ULL);

    /* log-scale the bar length */
    u64 c = count + 1;
    u64 m = max_count + 1;

    int lc = ilog2_u64(c);
    int lm = ilog2_u64(m);

    int bar = 0;
    if (lm > 0) {
        bar = (lc * width + lm / 2) / lm;   // rounded
        if (bar > width) bar = width;
        if (count > 0 && bar == 0) bar = 1; // ensure visibility
    }

    printf("%8llu -> %-8llu : %10llu |",
           lo, hi, (unsigned long long)count);

    for (int i = 0; i < bar; i++) putchar('#');
    for (int i = bar; i < width; i++) putchar(' ');
    printf("|\n");
}

static void usage(const char *prog)
{
    fprintf(stderr,
            "Usage: %s <PID> <PATH_TO_LIBC>\n"
            "Example:\n"
            "  sudo %s 12345 /path/to/glibc-install/lib/libc.so.6\n",
            prog, prog);
}

/* ------------------------------------------------------
 * Main
 * ----------------------------------------------------*/

int main(int argc, char **argv)
{
    struct alloc_hist_bpf *skel = NULL;
    int err;

    if (argc < 3) {
        usage(argv[0]);
        return 1;
    }

    int pid = atoi(argv[1]);
    const char *libc_path = argv[2];

    /* 1) Open + load BPF skeleton */
    skel = alloc_hist_bpf__open();
    if (!skel) {
        fprintf(stderr, "Failed to open BPF skeleton\n");
        return 1;
    }

    err = alloc_hist_bpf__load(skel);
    if (err) {
        fprintf(stderr, "Failed to load BPF skeleton: %d\n", err);
        alloc_hist_bpf__destroy(skel);
        return 1;
    }

    /* 2) Attach USDT probe */
    printf("Attaching to USDT 'memory_prof_sample' in %s (PID %d)...\n",
           libc_path, pid);

    skel->links.trace_alloc =
        bpf_program__attach_usdt(
            skel->progs.trace_alloc,
            pid,
            libc_path,
            "libc",
            "memory_prof_sample",
            NULL);

    if (!skel->links.trace_alloc) {
        perror("Failed to attach USDT");
        alloc_hist_bpf__destroy(skel);
        return 1;
    }

    /* 3) Live polling loop */
    signal(SIGINT, sig_handler);
    printf("Live histogram (per-second, log-scaled). Ctrl+C to stop.\n");

    int map_fd = bpf_map__fd(skel->maps.hist_map);
    if (map_fd < 0) {
        fprintf(stderr, "Failed to get hist_map fd\n");
        alloc_hist_bpf__destroy(skel);
        return 1;
    }

    u64 prev[64] = {0};
    const int BAR_WIDTH = 50;

    while (!exiting) {
        sleep(1);

        u64 delta[64] = {0};
        u64 maxc = 0;

        /* snapshot + delta */
        for (u32 key = 0; key < 64; key++) {
            u64 value = 0;
            if (bpf_map_lookup_elem(map_fd, &key, &value) == 0) {
                delta[key] = value - prev[key];
                prev[key] = value;
                if (delta[key] > maxc)
                    maxc = delta[key];
            }
        }

        /* redraw */
        printf("\033[2J\033[H");
        printf("=== Allocation Size Histogram (Log2) [per-second] ===\n");
        printf("Log-scaled to max bucket this interval (width=%d)\n\n",
               BAR_WIDTH);

        for (u32 key = 0; key < 64; key++) {
            if (delta[key] > 0) {
                print_bar(key, delta[key], maxc, BAR_WIDTH);
            }
        }

        fflush(stdout);
    }

    /* 4) Final cumulative histogram */
    printf("\n\n=== Allocation Size Histogram (Log2) [cumulative] ===\n");

    u64 totals[64] = {0};
    u64 maxc = 0;

    for (u32 key = 0; key < 64; key++) {
        bpf_map_lookup_elem(map_fd, &key, &totals[key]);
        if (totals[key] > maxc)
            maxc = totals[key];
    }

    for (u32 key = 0; key < 64; key++) {
        if (totals[key] > 0) {
            print_bar(key, totals[key], maxc, BAR_WIDTH);
        }
    }

    alloc_hist_bpf__destroy(skel);
    return 0;
}