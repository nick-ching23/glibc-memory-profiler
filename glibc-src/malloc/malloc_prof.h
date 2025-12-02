#ifndef _MALLOC_PROF_H
#define _MALLOC_PROF_H 1

#include <stddef.h>
#include <stdint.h>

/* Per-call-site aggregation bucket. */
struct mp_site {
    uintptr_t pc;          /* call site (return address) */
    uint64_t  sample_count;
    uint64_t  total_bytes;
};

/* Per-thread aggregation capacity. */
#define MP_SITE_CAP 256    /* per-thread aggregation buckets */

/* Per-thread profiler state (TLS). */
struct __mp_tls {
    uint64_t alloc_count;        /* number of allocations in this thread */
    uint64_t bytes_until_sample; /* bytes remaining until next sample */
    uint64_t sample_count;       /* total samples in this thread */
    uint64_t rng;                /* reserved for future use */

    /* Aggregation by call site (PC). */
    struct mp_site sites[MP_SITE_CAP];
    uint64_t site_overflow;      /* samples that couldn't be placed in table */
};

extern __thread struct __mp_tls __mp_tls_state;

/* Per-sample context passed to external hook. */
struct mp_sample_ctx {
    void            *user_pc;   /* caller of malloc / __libc_malloc (RA(1)) */
    void            *alloc_pc;  /* internal allocator PC (RA(0)), optional    */
    void            *ptr;       /* allocation result                          */
    size_t           size;      /* allocation size                            */
    struct __mp_tls *tstate;    /* pointer to this thread's profiler state    */
};

/* Weak hook: downstream tools may override or probe this symbol. */
void __mp_on_sample(struct mp_sample_ctx *ctx)
    __attribute__ ((weak));

/* Called from malloc.c on each successful allocation. */
void __mp_on_alloc(size_t size, void *ptr);

#endif /* _MALLOC_PROF_H */