#include "common.h"

int main(int argc, char **argv)
{
    (void)argc; (void)argv;

    const size_t N = 10 * 1000 * 1000; // 10M operations
    const size_t SIZE = 64;

    uint64_t start = ns_now();

    for (size_t i = 0; i < N; ++i) {
        void *p = malloc(SIZE);
        if (!p) die("malloc");
        free(p);
    }

    uint64_t end = ns_now();
    double seconds = (end - start) / 1e9;
    double ns_per_op = (double)(end - start) / (double)N;

    printf("bench_fixed: N=%zu, size=%zu\n", N, SIZE);
    printf("  time: %.3f s\n", seconds);
    printf("  ns per malloc+free: %.1f\n", ns_per_op);

    return 0;
}
