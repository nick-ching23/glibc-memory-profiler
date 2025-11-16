/* malloc/malloc-prof.c – MVP: detect every allocation */

#define _GNU_SOURCE
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>     // write
#include <stdlib.h>     // getenv
#include "malloc-prof.h"

static int mp_enabled = -1;          // -1 = uninitialized, 0=off, 1=on
static uint64_t mp_alloc_count = 0;  // not atomic for MVP; races OK for now

static void
mp_init_if_needed(void)
{
    if (mp_enabled != -1)
        return;

    const char *env = getenv("GLIBC_MALLOC_PROFILE");
    if (env && env[0] == '1') {
        mp_enabled = 1;
        const char msg[] = "malloc-prof: enabled (MVP)\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
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

    // MVP: just count allocations
    mp_alloc_count++;

    // Optional: print occasionally to prove it's firing
    if ((mp_alloc_count & ((1u << 20) - 1)) == 0) { // every ~1M allocs
        const char msg[] = "malloc-prof: hit 1M allocations\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
    }
}