// hooks/mp_log_hook.c
#define _GNU_SOURCE 1
#include <stdint.h>
#include <stddef.h>
#include <unistd.h>   // write()
#include <stdio.h>    // snprintf()
#include <inttypes.h> // PRIu64

/*
 * We need the same mp_sample_ctx layout that glibc was built with.
 * Easiest is to copy the definition from malloc_prof.h.
 *
 * NOTE: In a "real" downstream project you'd want a public header,
 * but for this prototype it's fine to duplicate it.
 */

struct mp_site;  /* unused here, forward-declare just in case */
struct __mp_tls; /* opaque to us */

/* Must exactly match malloc_prof.h */
struct mp_sample_ctx {
    void            *user_pc;   /* caller of malloc / __libc_malloc (RA(1)) */
    void            *alloc_pc;  /* internal allocator PC (RA(0)), optional    */
    void            *ptr;       /* allocation result                          */
    size_t           size;      /* allocation size                            */
    struct __mp_tls *tstate;    /* pointer to this thread's profiler state    */
};

/*
 * IMPORTANT: Do NOT call malloc/calloc/realloc/free here.
 * That would recurse back into the allocator and the profiler.
 *
 * We also avoid printf() (which may allocate) and instead use write().
 */

static int log_fd = -1;

/* Very simple, racy init: good enough for a demo. */
static void
mp_log_init_if_needed(void)
{
    if (log_fd != -1)
        return;

    /* Write to stderr (fd = 2). You could also open a custom file. */
    log_fd = STDERR_FILENO;
}

void
__mp_on_sample(struct mp_sample_ctx *ctx)
{
    mp_log_init_if_needed();

    if (log_fd < 0 || ctx == NULL)
        return;

    /* Log only the first N samples per process to avoid spam. */
    static uint64_t seen = 0;
    const uint64_t limit = 1000;

    if (seen++ >= limit)
        return;

    char buf[256];
    int len = snprintf(buf, sizeof buf,
                       "[mp_log_hook] sample: user_pc=%p alloc_pc=%p ptr=%p "
                       "size=%zu (sample #% " PRIu64 ")\n",
                       ctx->user_pc,
                       ctx->alloc_pc,
                       ctx->ptr,
                       ctx->size,
                       seen);

    if (len > 0) {
        (void)write(log_fd, buf, (size_t)len);
    }
}