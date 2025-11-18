#include "common.h"

int main(int argc, char **argv)
{
    (void)argc; (void)argv;

    const size_t N = 5 * 1000 * 1000; // 5M ops

    uint64_t start = ns_now();

    for (size_t i = 0; i < N; ++i) {
        size_t size = 16 + (i % 256);    // 16..271 bytes
        unsigned char *p = malloc(size);
        if (!p) die("malloc");

        // Touch the memory to avoid the compiler removing the loop.
        p[0] = (unsigned char)(i & 0xFF);

        free(p);
    }

    uint64_t end = ns_now();
    double seconds = (end - start) / 1e9;
    double ns_per_op = (double)(end - start) / (double)N;

    printf("bench_var: N=%zu\n", N);
    printf("  time: %.3f s\n", seconds);
    printf("  ns per malloc+free: %.1f\n", ns_per_op);

    return 0;
}