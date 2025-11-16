#ifndef _MALLOC_PROF_H
#define _MALLOC_PROF_H

#include <stddef.h>
#include <stdint.h>

struct mp_sample {
    uintptr_t ptr;
    size_t    size;
    uint64_t  alloc_count_at_sample;
};

#define MP_RING_CAP 1024   /* number of samples to keep per thread */

struct __mp_tls {
    uint64_t alloc_count;        /* number of allocations in this thread */
    uint64_t bytes_until_sample; /* bytes remaining until next sample */
    uint64_t sample_count;       /* number of samples triggered */
    uint64_t rng;                /* reserved / future use */

    /* Ring buffer of recent samples. */
    struct mp_sample ring[MP_RING_CAP];
    uint64_t ring_head;          /* total samples recorded (monotonic) */
};

extern __thread struct __mp_tls __mp_tls_state;

void __mp_on_alloc(size_t size, void *ptr);

#endif