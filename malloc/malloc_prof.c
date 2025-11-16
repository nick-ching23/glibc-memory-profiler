#define _GNU_SOURCE
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include "malloc_prof.h"

static int mp_global_enabled = -1;  // -1 = uninitialized, 0=off, 1=on

__thread struct __mp_tls __mp_tls_state;

static void
mp_global_init_if_needed(void)
{
    if (mp_global_enabled != -1)
        return;

    const char *env = getenv("GLIBC_MALLOC_PROFILE");
    if (env && env[0] == '1') {
        mp_global_enabled = 1;
        const char msg[] = "malloc-prof: enabled (MVP)\n";
        (void)write(STDERR_FILENO, msg, sizeof(msg) - 1);
    } else {
        mp_global_enabled = 0;
    }
}

void
__mp_on_alloc(size_t size, void *ptr)
{
    (void)size;
    (void)ptr;

    mp_global_init_if_needed();
    if (!mp_global_enabled)
        return;

    struct __mp_tls *st = &__mp_tls_state;
    uint64_t c = ++st->alloc_count;

    if ((c % 1000000ull) == 0) {
        const char msg[] = "malloc-prof: hit 1M allocations\n";
        (void)write(STDERR_FILENO, msg, sizeof(msg) - 1);
    }
}