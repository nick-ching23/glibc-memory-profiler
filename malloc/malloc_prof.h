#ifndef _MALLOC_PROF_H
#define _MALLOC_PROF_H

#include <stddef.h>
#include <stdint.h>

/* Per-thread profiler state.  We'll extend this later with
   bytes_until_sample, rng, etc. */
struct __mp_tls {
    uint64_t alloc_count;      /* number of allocations in this thread */
    uint64_t bytes_until_sample;
    uint64_t rng;
};

/* Thread-local instance for each thread. */
extern __thread struct __mp_tls __mp_tls_state;

/* Hook called from malloc.c on every successful allocation. */
void __mp_on_alloc(size_t size, void *ptr);

#endif