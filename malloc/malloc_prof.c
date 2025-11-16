#define _GNU_SOURCE
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include "malloc_prof.h"

static int mp_enabled = -1;
static uint64_t mp_alloc_count = 0;

static void
mp_init_if_needed(void)
{
    if (mp_enabled != -1)
        return;

    const char *env = getenv("GLIBC_MALLOC_PROFILE");
    if (env && env[0] == '1') {
        mp_enabled = 1;
        const char msg[] = "malloc-prof: enabled (MVP)\n";
        (void)write(STDERR_FILENO, msg, sizeof(msg) - 1);
    } else {
        mp_enabled = 0;
    }
}

void
__mp_on_alloc(size_t size, void *ptr)
{
    (void)size;
    (void)ptr;

    mp_init_if_needed();
    if (!mp_enabled)
        return;

    uint64_t c = ++mp_alloc_count;

    // if (c <= 20) {
    //     char buf[128];
    //     int len = snprintf(buf, sizeof buf,
    //                        "malloc-prof: alloc #%llu\n",
    //                        (unsigned long long)c);
    //     if (len > 0)
    //         (void)write(STDERR_FILENO, buf, (size_t)len);
    //     return;
    // }

    if ((c % 1000000ull) == 0) {
        const char msg[] = "malloc-prof: hit 1M allocations\n";
        (void)write(STDERR_FILENO, msg, sizeof(msg) - 1);
    }
}