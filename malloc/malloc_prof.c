#define _GNU_SOURCE
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include "malloc_prof.h"

/* Process-wide config. */
static int mp_global_enabled = -1;                      // -1 = uninitialized, 0=off, 1=on
static uint64_t mp_sample_stride_bytes = 64 * 1024;     // default: 64 KB
static int mp_stats_enabled = 0;                        // if true, dump stats at exit

/* One instance per thread (zero-initialized). */
__thread struct __mp_tls __mp_tls_state;

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
    (void)ptr;

    mp_global_init_if_needed();
    if (!mp_global_enabled)
        return;

    struct __mp_tls *st = &__mp_tls_state;
    mp_thread_init_if_needed(st);

    st->alloc_count++;

    uint64_t stride = mp_sample_stride_bytes;
    uint64_t remaining = st->bytes_until_sample;

    /* Fast path: we haven't hit the next sampling boundary yet. */
    if (__glibc_likely(size < remaining)) {
        st->bytes_until_sample = remaining - size;
        return;
    }

    /* Slow path: this allocation crosses one or more sample thresholds. */

    size_t consumed = size - remaining;   // bytes beyond this threshold
    uint64_t samples = 1 + consumed / stride;

    /* count how many samples this thread generated. */
    st->sample_count += samples;

    /* Set bytes_until_sample for the NEXT sample after this allocation. */
    uint64_t overshoot = consumed % stride;
    st->bytes_until_sample = stride - overshoot;
}

/* Debug-only: dump per-thread stats at exit if GLIBC_MALLOC_PROFILE_STATS=1.
   This runs once for the main thread when the process exits. */
static void __attribute__((destructor))
__mp_dump_stats_destructor(void)
{
    if (!mp_global_enabled || !mp_stats_enabled)
        return;

    struct __mp_tls *st = &__mp_tls_state;

    /* If this thread never allocated, nothing interesting to say. */
    if (st->alloc_count == 0 && st->sample_count == 0)
        return;

    char buf[256];
    int len = snprintf(buf, sizeof buf,
                       "malloc-prof stats: thread=%p "
                       "alloc_count=%llu sample_count=%llu stride=%llu\n",
                       (void*)st,
                       (unsigned long long)st->alloc_count,
                       (unsigned long long)st->sample_count,
                       (unsigned long long)mp_sample_stride_bytes);
    if (len > 0)
        (void)write(STDERR_FILENO, buf, (size_t)len);
}