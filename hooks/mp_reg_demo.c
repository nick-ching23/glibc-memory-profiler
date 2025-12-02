// hooks/mp_reg_demo.c
#define _GNU_SOURCE 1
#include <stdint.h>
#include <stddef.h>
#include <unistd.h>   // write()
#include <stdio.h>    // snprintf()
#include <inttypes.h> // PRIu64

/*
 * Mirror the mp_sample_ctx definition from malloc_prof.h.
 * In a real setup, you'd have a public header, but for this demo,
 * we duplicate the struct layout.
 */

struct __mp_tls; /* opaque to us */

struct mp_sample_ctx {
    void            *user_pc;   /* caller of malloc / __libc_malloc (RA(1)) */
    void            *alloc_pc;  /* internal allocator PC (RA(0)), optional   */
    void            *ptr;       /* allocation result                         */
    size_t           size;      /* allocation size                           */
    struct __mp_tls *tstate;    /* pointer to this thread's profiler state   */
};

/* Registration API exported by your custom glibc. */
typedef void (*mp_sample_handler_t)(struct mp_sample_ctx *ctx);

/* Declaration must match malloc_prof.h exactly. */
int __mp_register_handler(mp_sample_handler_t cb);

/*
 * IMPORTANT:
 *  - Do NOT call malloc/calloc/realloc/free in the handler.
 *  - Avoid printf (may allocate); use write() on a fixed buffer.
 */

static void
mp_demo_handler(struct mp_sample_ctx *ctx)
{
    if (!ctx)
        return;

    /* Log only the first N samples to avoid spamming stderr. */
    static uint64_t seen = 0;
    const uint64_t limit = 1000;

    if (seen++ >= limit)
        return;

    char buf[256];
    int len = snprintf(buf, sizeof buf,
                       "[mp_reg_demo] sample #% " PRIu64
                       ": user_pc=%p alloc_pc=%p ptr=%p size=%zu\n",
                       seen,
                       ctx->user_pc,
                       ctx->alloc_pc,
                       ctx->ptr,
                       ctx->size);

    if (len > 0) {
        (void)write(STDERR_FILENO, buf, (size_t)len);
    }
}

/* Constructor: runs when the .so is loaded, registers our handler. */
__attribute__((constructor))
static void
mp_demo_init(void)
{
    (void)__mp_register_handler(mp_demo_handler);
}