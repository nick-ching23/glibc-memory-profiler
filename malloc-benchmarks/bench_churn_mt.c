#include "common.h"
#include <pthread.h>

struct thread_args {
    size_t pool_size;
    size_t steps;
    size_t thread_id;
};

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

static void *
worker(void *arg)
{
    struct thread_args *a = arg;
    const size_t POOL_SIZE = a->pool_size;
    const size_t STEPS     = a->steps;
    const size_t MIN_SIZE  = 32;
    const size_t MAX_SIZE  = 1024;

    void **pool = calloc(POOL_SIZE, sizeof(void *));
    if (!pool) die("calloc");

    uint64_t rng = 0x123456789abcdefULL ^ (uint64_t)a->thread_id;
    uint64_t checksum = 0;

    for (size_t step = 0; step < STEPS; ++step) {
        uint64_t r = xs64(&rng);
        size_t idx = (size_t)(r % POOL_SIZE);

        if (pool[idx] == NULL) {
            size_t sz = MIN_SIZE + (size_t)(xs64(&rng) % (MAX_SIZE - MIN_SIZE + 1));
            unsigned char *p = malloc(sz);
            if (!p) die("malloc");
            checksum ^= work_on_buffer(p, sz, r);
            pool[idx] = p;
        } else {
            if ((r & 0x7) == 0) {  // ~12.5% free probability
                free(pool[idx]);
                pool[idx] = NULL;
            } else {
                unsigned char *p = pool[idx];
                checksum ^= work_on_buffer(p, MIN_SIZE, r);
            }
        }
    }

    for (size_t i = 0; i < POOL_SIZE; ++i)
        free(pool[i]);
    free(pool);

    // store checksum in the args to prevent the compiler from dropping work
    a->thread_id = (size_t)checksum;
    return NULL;
}

int main(void)
{
    const int    THREADS    = 4;
    const size_t POOL_SIZE  = 5000;
    const size_t STEPS      = 500000;  // per thread

    pthread_t ts[THREADS];
    struct thread_args args[THREADS];

    uint64_t start = ns_now();

    for (int i = 0; i < THREADS; ++i) {
        args[i].pool_size = POOL_SIZE;
        args[i].steps = STEPS;
        args[i].thread_id = (size_t)i;
        if (pthread_create(&ts[i], NULL, worker, &args[i]) != 0)
            die("pthread_create");
    }

    uint64_t combined_checksum = 0;
    for (int i = 0; i < THREADS; ++i) {
        pthread_join(ts[i], NULL);
        combined_checksum ^= (uint64_t)args[i].thread_id;
    }

    uint64_t end = ns_now();

    double seconds = (end - start) / 1e9;
    double total_steps = (double)STEPS * THREADS;
    double ns_per_step = (double)(end - start) / total_steps;

    printf("bench_churn_mt: threads=%d, steps/thread=%zu, pool=%zu, checksum=%llu\n",
           THREADS, STEPS, POOL_SIZE, (unsigned long long)combined_checksum);
    printf("  time: %.3f s\n", seconds);
    printf("  ns per step (alloc/free/work): %.1f\n", ns_per_step);

    return 0;
}
