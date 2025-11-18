/*
- have a look at using compiler flags for profiler off 
*/

#define _GNU_SOURCE
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>

#include "malloc_prof.h"

/* ------------------------------------------------------
 * Global profiler configuration
 * ----------------------------------------------------*/

static int mp_global_enabled = -1;                   /* -1 = uninitialized, 0=off, 1=on */
static uint64_t mp_sample_stride_bytes = 512 * 1024; /* default 512KB */
static int mp_stats_enabled = 0;                     /* dump human stats */
static const char *mp_out_base = NULL;               /* binary dump path */

/* Per-thread TLS state */
__thread struct __mp_tls __mp_tls_state;


/* ------------------------------------------------------
 * Initialization
 * ----------------------------------------------------*/

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
            if (end && *end == '\0' && v >= 1024)
                mp_sample_stride_bytes = v;
        }

        const char *stats_env = getenv("GLIBC_MALLOC_PROFILE_STATS");
        if (stats_env && stats_env[0] == '1')
            mp_stats_enabled = 1;

        const char *out_env = getenv("GLIBC_MALLOC_PROFILE_OUT");
        if (out_env && out_env[0] != '\0')
            mp_out_base = out_env;

    } else {
        mp_global_enabled = 0;
    }
}

static inline void
mp_thread_init_if_needed(struct __mp_tls *st)
{
    if (st->bytes_until_sample == 0)
        st->bytes_until_sample = mp_sample_stride_bytes;
}


/* ------------------------------------------------------
 * Hashing & aggregation
 * ----------------------------------------------------*/

static inline size_t
mp_hash_pc(uintptr_t pc)
{
    uint64_t x = (uint64_t)pc;
    x ^= x >> 33;
    x *= 0xff51afd7ed558ccdULL;
    x ^= x >> 33;
    x *= 0xc4ceb9fe1a85ec53ULL;
    x ^= x >> 33;
    return (size_t)x;
}

static inline void
mp_record_site(struct __mp_tls *st, uintptr_t pc, size_t size)
{
    if (pc == 0)
        return;

    size_t cap = MP_SITE_CAP;
    size_t idx = mp_hash_pc(pc) % cap;

    for (size_t probe = 0; probe < cap; ++probe) {
        struct mp_site *s = &st->sites[idx];

        if (s->pc == 0) {
            /* install new site */
            s->pc = pc;
            s->sample_count = 1;
            s->total_bytes = size;
            return;
        }
        if (s->pc == pc) {
            /* update existing */
            s->sample_count++;
            s->total_bytes += size;
            return;
        }
        idx = (idx + 1) % cap;
    }

    /* table is full */
    st->site_overflow++;
}


/* ------------------------------------------------------
 * Allocation hook called from malloc.c
 * ----------------------------------------------------*/

void
__mp_on_alloc(size_t size, void *ptr)
{
    (void)ptr; /* not needed for aggregation */

    mp_global_init_if_needed();
    if (!mp_global_enabled)
        return;

    struct __mp_tls *st = &__mp_tls_state;
    mp_thread_init_if_needed(st);

    st->alloc_count++;

    uint64_t stride = mp_sample_stride_bytes;
    uint64_t remaining = st->bytes_until_sample;

    /* Fast path */
    if (__glibc_likely(size < remaining)) {
        st->bytes_until_sample = remaining - size;
        return;
    }

    /* Slow path: sample event */
    size_t consumed = size - remaining;
    uint64_t samples = 1 + consumed / stride;

    st->sample_count += samples;

    /* capture caller PC one frame above */
    uintptr_t pc = (uintptr_t)__builtin_return_address(0);

    /* record sample */
    mp_record_site(st, pc, size);

    /* reset bytes_until_sample */
    uint64_t overshoot = consumed % stride;
    st->bytes_until_sample = stride - overshoot;
}


/* ------------------------------------------------------
 * Binary dump format
 * ----------------------------------------------------*/

#define MP_MAGIC   0x4D50524F46494C45ULL   /* "MPROFILE" */
#define MP_VERSION 1

struct mp_file_header {
    uint64_t magic;
    uint32_t version;
    uint32_t reserved;
    uint64_t stride_bytes;
    uint64_t alloc_count;
    uint64_t sample_count;
    uint64_t site_overflow;
    uint64_t n_sites;
};

struct mp_file_site_disk {
    uint64_t pc;
    uint64_t sample_count;
    uint64_t total_bytes;
};

static size_t
mp_count_sites(const struct __mp_tls *st)
{
    size_t n = 0;
    for (size_t i = 0; i < MP_SITE_CAP; ++i)
        if (st->sites[i].pc != 0)
            n++;
    return n;
}

static int
mp_build_filename(char *buf, size_t buf_sz, const struct __mp_tls *st)
{
    if (!mp_out_base)
        return -1;
    pid_t pid = getpid();
    int len = snprintf(buf, buf_sz, "%s.%d.%p.bin",
                       mp_out_base, (int)pid, (void *)st);
    return (len < 0 || (size_t)len >= buf_sz) ? -1 : 0;
}

static void
mp_dump_thread_to_file(const struct __mp_tls *st)
{
    if (!mp_out_base)
        return;

    if (st->alloc_count == 0 && st->sample_count == 0)
        return;

    char path[256];
    if (mp_build_filename(path, sizeof path, st) != 0)
        return;

    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC,
                  S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fd < 0)
        return;

    struct mp_file_header hdr;
    memset(&hdr, 0, sizeof hdr);
    hdr.magic         = MP_MAGIC;
    hdr.version       = MP_VERSION;
    hdr.stride_bytes  = mp_sample_stride_bytes;
    hdr.alloc_count   = st->alloc_count;
    hdr.sample_count  = st->sample_count;
    hdr.site_overflow = st->site_overflow;
    hdr.n_sites       = mp_count_sites(st);

    (void)write(fd, &hdr, sizeof hdr);

    for (size_t i = 0; i < MP_SITE_CAP; ++i) {
        const struct mp_site *s = &st->sites[i];
        if (s->pc == 0)
            continue;

        struct mp_file_site_disk fs;
        fs.pc           = (uint64_t)s->pc;
        fs.sample_count = s->sample_count;
        fs.total_bytes  = s->total_bytes;

        (void)write(fd, &fs, sizeof fs);
    }

    (void)close(fd);
}


/* ------------------------------------------------------
 * Destructor: dump stats + binary snapshot
 * ----------------------------------------------------*/

static void __attribute__((destructor))
__mp_dump_stats_destructor(void)
{
    if (!mp_global_enabled)
        return;

    struct __mp_tls *st = &__mp_tls_state;

    if (st->alloc_count == 0 && st->sample_count == 0)
        return;

    /* Optional human-readable stats. */
    if (mp_stats_enabled) {
        char buf[256];
        int len = snprintf(buf, sizeof buf,
                           "malloc-prof stats: thread=%p alloc_count=%llu "
                           "sample_count=%llu stride=%llu site_overflow=%llu\n",
                           (void *)st,
                           (unsigned long long)st->alloc_count,
                           (unsigned long long)st->sample_count,
                           (unsigned long long)mp_sample_stride_bytes,
                           (unsigned long long)st->site_overflow);
        if (len > 0)
            (void)write(STDERR_FILENO, buf, (size_t)len);
    }

    /* Binary profile dump. */
    if (mp_out_base)
        mp_dump_thread_to_file(st);
}