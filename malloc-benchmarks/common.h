#ifndef MP_BENCH_COMMON_H
#define MP_BENCH_COMMON_H

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

static inline uint64_t
ns_now(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

static inline void
die(const char *msg)
{
    perror(msg);
    exit(1);
}

#endif