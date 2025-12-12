#include "common.h"

/* Simple xorshift64 PRNG so we don't pull in libc rand(), and it's cheap. */
static inline uint64_t
xs64(uint64_t *state)
{
    uint64_t x = *state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    *state = x;
    return x;
}

/* Simulated "work" on a buffer: touch it and do a tiny checksum. */
static inline uint64_t
work_on_buffer(unsigned char *p, size_t sz, uint64_t seed)
{
    uint64_t acc = seed;
    for (size_t i = 0; i < sz; i += 32) {
        p[i] = (unsigned char)(acc & 0xFF);
        acc = (acc * 1315423911u) ^ p[i];
    }
    return acc;
}

/* Simulate a heap with random churn:
   - Keep an array of pointers (like a pool of "live" objects).
   - Each iteration, randomly decide to allocate or free in that pool.
*/
int main(void)
{
    const size_t POOL_SIZE = 10000;            // number of live slots
    const size_t STEPS     = 2000000;         // iterations (2M)
    const size_t MIN_SIZE  = 32;
    const size_t MAX_SIZE  = 1024;

    void *pool[POOL_SIZE];
    for (size_t i = 0; i < POOL_SIZE; ++i)
        pool[i] = NULL;

    uint64_t rng = 0x123456789abcdefULL;
    uint64_t checksum = 0;

    uint64_t start = ns_now();

    for (size_t step = 0; step < STEPS; ++step) {
        uint64_t r = xs64(&rng);
        size_t idx = (size_t)(r % POOL_SIZE);

        if (pool[idx] == NULL) {
            // allocate a new object with "random" size
            size_t sz = MIN_SIZE + (size_t)(xs64(&rng) % (MAX_SIZE - MIN_SIZE + 1));
            unsigned char *p = malloc(sz);
            if (!p) die("malloc");

            checksum ^= work_on_buffer(p, sz, r);
            pool[idx] = p;
        } else {
            // occasionally free
            if ((r & 0x3) == 0) {  // ~25% free probability
                free(pool[idx]);
                pool[idx] = NULL;
            } else {
                // otherwise, just touch it
                unsigned char *p = pool[idx];
                checksum ^= work_on_buffer(p, MIN_SIZE, r);
            }
        }
    }

    // free remaining objects
    for (size_t i = 0; i < POOL_SIZE; ++i)
        free(pool[i]);

    uint64_t end = ns_now();

    double seconds = (end - start) / 1e9;
    double ops = (double)STEPS;
    double ns_per_step = (double)(end - start) / ops;

    printf("bench_churn: steps=%zu, pool=%zu, checksum=%llu\n",
           STEPS, POOL_SIZE, (unsigned long long)checksum);
    printf("  time: %.3f s\n", seconds);
    printf("  ns per step (alloc/free/work): %.1f\n", ns_per_step);

    return 0;
}
