#ifndef _MALLOC_PROF_H
#define _MALLOC_PROF_H

#include <stddef.h>
#include <stdint.h>

struct mp_site {
    uintptr_t pc;          /* call site (return address) */
    uint64_t  sample_count;
    uint64_t  total_bytes;
};

#define MP_SITE_CAP 256    /* per-thread aggregation buckets */

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

/* Called from malloc.c on each successful allocation. */
void __mp_on_alloc(size_t size, void *ptr);

#endif