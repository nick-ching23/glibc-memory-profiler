// hooks/mp_unwind_demo.c
#define _GNU_SOURCE 1
#include <stdint.h>
#include <stddef.h>
#include <unistd.h>    // write()
#include <stdio.h>     // snprintf()
#include <inttypes.h>  // PRIu64
#include <execinfo.h>  // backtrace, backtrace_symbols_fd

/*
 * These mirror the ABI exposed by glibc's malloc_prof.h.
 * In a real setup, you'd share a header.
 */

struct __mp_tls;  /* opaque, we just keep the pointer */

struct mp_sample_ctx {
    void            *user_pc;   /* currently always NULL on AArch64 */
    void            *alloc_pc;  /* internal allocator PC (RA(0))     */
    void            *ptr;       /* allocation result                 */
    size_t           size;      /* allocation size                   */
    struct __mp_tls *tstate;    /* per-thread profiler state         */
};

typedef void (*mp_sample_handler_t)(struct mp_sample_ctx *ctx);

/* Registration API exported by your custom glibc. */
int __mp_register_handler(mp_sample_handler_t cb);

/*
 * IMPORTANT NOTES:
 *
 *  - This handler calls backtrace()/backtrace_symbols_fd(), which may
 *    allocate internally. For a production tool, you'd likely:
 *      * lower sampling rate,
 *      * disable profiling in the unwinder thread, or
 *      * move unwinding to a helper thread / external process.
 *
 *  - For this MVP, we accept that overhead to demonstrate the design.
 */

static void
mp_unwind_handler(struct mp_sample_ctx *ctx)
{
    if (!ctx)
        return;

    /* Limit how much we spam stderr. */
    static uint64_t seen = 0;
    const uint64_t limit = 128;  // only unwind the first 128 samples

    if (seen++ >= limit)
        return;

    /* Capture a stack trace starting from this handler frame. */
    void *frames[64];
    int n = backtrace(frames, (int)(sizeof frames / sizeof frames[0]));

    char header[256];
    int len = snprintf(header, sizeof header,
                       "\n[mp_unwind_demo] sample #%" PRIu64
                       " size=%zu alloc_pc=%p frames=%d\n",
                       seen, ctx->size, ctx->alloc_pc, n);
    if (len > 0)
        (void)write(STDERR_FILENO, header, (size_t)len);

    if (n > 0) {
        /* This prints symbolized stack frames directly to stderr. */
        backtrace_symbols_fd(frames, n, STDERR_FILENO);
    }
}

/* Constructor: runs when the .so is loaded, registers our handler. */
__attribute__((constructor))
static void
mp_unwind_demo_init(void)
{
    (void)__mp_register_handler(mp_unwind_handler);
}