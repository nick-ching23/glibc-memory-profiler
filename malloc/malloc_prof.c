#define _GNU_SOURCE
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include "malloc_prof.h"

/* Process-wide config. */
static int mp_global_enabled = -1;                      // -1 = uninitialized, 0=off, 1=on
static uint64_t mp_sample_stride_bytes = 256 * 1024;     // default: 64 KB
static int mp_stats_enabled = 0;                        // if true, dump stats at exit

/* One instance per thread (zero-initialized). */
__thread struct __mp_tls __mp_tls_state;

static inline void
mp_record_sample(struct __mp_tls *st, size_t size, void *ptr)
{
    uint64_t h = st->ring_head++;
    size_t idx = (size_t)(h % MP_RING_CAP);

    st->ring[idx].ptr = (uintptr_t)ptr;
    st->ring[idx].size = size;
    st->ring[idx].alloc_count_at_sample = st->alloc_count;
}

/* Global init: read env vars once, print banner. */
static void
mp_global_init_if_needed(void)
{
    if (mp_global_enabled != -1)
        return;

    const char *env = getenv("GLIBC_MALLOC_PROFILE");
    if (env && env[0] == '1') {
        mp_global_enabled = 1;

        const char *stride_env = getenv("GLIBC_MALLOC_PROFILE_BYTES");
        if (stride_env) {
            char *end = NULL;
            unsigned long long v = strtoull(stride_env, &end, 10);
            if (end && *end == '\0' && v >= 1024) {  // require at least 1KB
                mp_sample_stride_bytes = (uint64_t)v;
            }
        }

        const char *stats_env = getenv("GLIBC_MALLOC_PROFILE_STATS");
        if (stats_env && stats_env[0] == '1')
            mp_stats_enabled = 1;

    } else {
        mp_global_enabled = 0;
    }
}

static inline void
mp_thread_init_if_needed(struct __mp_tls *st)
{
    if (st->bytes_until_sample == 0) {
        st->bytes_until_sample = mp_sample_stride_bytes;
    }
}

void
__mp_on_alloc(size_t size, void *ptr)
{
    mp_global_init_if_needed();
    if (!mp_global_enabled)
        return;

    struct __mp_tls *st = &__mp_tls_state;
    mp_thread_init_if_needed(st);

    st->alloc_count++;

    uint64_t stride = mp_sample_stride_bytes;
    uint64_t remaining = st->bytes_until_sample;

    /* Fast path: no sample. */
    if (__glibc_likely(size < remaining)) {
        st->bytes_until_sample = remaining - size;
        return;
    }

    /* Slow path: this allocation crosses one or more sample thresholds. */

    size_t consumed = size - remaining;
    uint64_t samples = 1 + consumed / stride;

    st->sample_count += samples;

    /* Record exactly one sample record for this allocation. */
    mp_record_sample(st, size, ptr);

    /* Compute next threshold distance. */
    uint64_t overshoot = consumed % stride;
    st->bytes_until_sample = stride - overshoot;
}

// DEBUG only...
static void __attribute__((destructor))
__mp_dump_stats_destructor(void)
{
    if (!mp_global_enabled || !mp_stats_enabled)
        return;

    struct __mp_tls *st = &__mp_tls_state;

    if (st->alloc_count == 0 && st->sample_count == 0)
        return;

    char buf[256];
    int len = snprintf(buf, sizeof buf,
                       "malloc-prof stats: thread=%p alloc_count=%llu "
                       "sample_count=%llu stride=%llu ring_head=%llu\n",
                       (void *)st,
                       (unsigned long long)st->alloc_count,
                       (unsigned long long)st->sample_count,
                       (unsigned long long)mp_sample_stride_bytes,
                       (unsigned long long)st->ring_head);
    if (len > 0)
        (void)write(STDERR_FILENO, buf, (size_t)len);

    /* Optionally, dump the last few samples. */
    size_t to_dump = st->ring_head < MP_RING_CAP ? (size_t)st->ring_head : MP_RING_CAP;
    for (size_t i = 0; i < to_dump; ++i) {
        size_t idx = (st->ring_head - to_dump + i) % MP_RING_CAP;
        struct mp_sample *s = &st->ring[idx];

        int len2 = snprintf(buf, sizeof buf,
                            "  sample[%zu]: ptr=%p size=%zu alloc_count=%llu\n",
                            i,
                            (void *)s->ptr,
                            s->size,
                            (unsigned long long)s->alloc_count_at_sample);
        if (len2 > 0)
            (void)write(STDERR_FILENO, buf, (size_t)len2);
    }
}