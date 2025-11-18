#include "common.h"
#include <pthread.h>

struct thread_args {
    size_t iters;
    size_t size;
};

static void *worker(void *arg)
{
    struct thread_args *a = arg;
    const size_t N = a->iters;
    const size_t SIZE = a->size;

    for (size_t i = 0; i < N; ++i) {
        void *p = malloc(SIZE);
        if (!p) die("malloc");
        free(p);
    }
    return NULL;
}

int main(void)
{
    const int THREADS = 4;
    const size_t N_PER_THREAD = 3 * 1000 * 1000; // 3M ops per thread
    const size_t SIZE = 64;

    pthread_t ts[THREADS];
    struct thread_args args = { N_PER_THREAD, SIZE };

    uint64_t start = ns_now();

    for (int i = 0; i < THREADS; ++i) {
        if (pthread_create(&ts[i], NULL, worker, &args) != 0)
            die("pthread_create");
    }

    for (int i = 0; i < THREADS; ++i)
        pthread_join(ts[i], NULL);

    uint64_t end = ns_now();
    double seconds = (end - start) / 1e9;
    double total_ops = (double)N_PER_THREAD * THREADS;
    double ns_per_op = (double)(end - start) / total_ops;

    printf("bench_mt: threads=%d, N/thread=%zu, size=%zu\n",
           THREADS, N_PER_THREAD, SIZE);
    printf("  time: %.3f s\n", seconds);
    printf("  ns per malloc+free: %.1f\n", ns_per_op);

    return 0;
}