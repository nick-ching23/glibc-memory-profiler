#ifndef _MALLOC_PROF_H
#define _MALLOC_PROF_H

#include <stddef.h>
#include <stdint.h>

/* Per-thread profiler state.  */
struct __mp_tls {
    uint64_t alloc_count;        /* number of allocations in this thread */
    uint64_t bytes_until_sample; /* bytes remaining until next sample */
    uint64_t sample_count;       /* number of samples taken in this thread */
    uint64_t rng;                /* reserved for future (e.g., jitter) */
};

/* Thread-local state instance. */
extern __thread struct __mp_tls __mp_tls_state;

/* Hook called from malloc.c on each successful allocation. */
void __mp_on_alloc(size_t size, void *ptr);

#endif