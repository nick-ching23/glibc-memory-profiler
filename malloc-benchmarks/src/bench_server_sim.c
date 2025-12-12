// malloc-benchmarks/src/bench_server_sim.c
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <stdbool.h>
#include <sys/time.h>

// Defaults
#define DEFAULT_THREADS 4
#define DEFAULT_SECONDS 60
#define MAX_ACTIVE_PTRS 1024

// Workload Configuration (Server Simulation)
// 1. Small (70%): 16B - 256B (Request headers, metadata, small JSON)
// 2. Medium (25%): 1KB - 16KB (String buffers, serialized objects)
// 3. Large (5%): 256KB - 2MB (Images, file uploads, large payloads)
#define CHANCE_SMALL 70
#define CHANCE_MEDIUM 25
// Remaining 5% is Large

typedef struct {
    int id;
    int duration_sec;
    size_t total_allocs;
} thread_arg_t;

// Helper: Thread-safe random range
size_t rand_range(unsigned int *seed, size_t min, size_t max) {
    return min + (rand_r(seed) % (max - min + 1));
}

void *worker(void *arg) {
    thread_arg_t *args = (thread_arg_t *)arg;
    unsigned int seed = time(NULL) + args->id; 
    
    // Circular buffer to hold pointers (simulates overlapping request lifetimes)
    void *ptrs[MAX_ACTIVE_PTRS] = {0};
    int ptr_idx = 0;

    struct timeval start, curr;
    gettimeofday(&start, NULL);
    double elapsed = 0;

    printf("[Thread %d] Server simulation started (%ds)...\n", args->id, args->duration_sec);

    while (elapsed < args->duration_sec) {
        // 1. Determine Request Size (Traffic Distribution)
        int roll = rand_r(&seed) % 100;
        size_t size;

        if (roll < CHANCE_SMALL) {
            size = rand_range(&seed, 16, 256);
        } else if (roll < (CHANCE_SMALL + CHANCE_MEDIUM)) {
            size = rand_range(&seed, 1024, 16384);
        } else {
            size = rand_range(&seed, 262144, 2 * 1024 * 1024);
        }

        // 2. Handle Request (Allocate)
        void *p = malloc(size);
        if (!p) {
            perror("malloc failed");
            break;
        }

        // 3. Process Request (Dirty Memory)
        // Writes to the memory to ensure OS pages are actually committed
        ((char*)p)[0] = 1;
        ((char*)p)[size-1] = 1;

        // 4. Request Completion (Free)
        // Simulates varying request latency by freeing an old pointer 
        // from the circular buffer before adding the new one.
        if (ptrs[ptr_idx]) {
            free(ptrs[ptr_idx]);
        }
        ptrs[ptr_idx] = p;
        
        // Move to next slot
        ptr_idx = (ptr_idx + 1) % MAX_ACTIVE_PTRS;
        args->total_allocs++;

        // Update timer every 1000 requests to minimize overhead
        if (args->total_allocs % 1000 == 0) {
            gettimeofday(&curr, NULL);
            elapsed = (curr.tv_sec - start.tv_sec) + 
                      (curr.tv_usec - start.tv_usec) / 1000000.0;
        }
    }

    // Cleanup lingering active requests
    for (int i = 0; i < MAX_ACTIVE_PTRS; i++) {
        if (ptrs[i]) free(ptrs[i]);
    }

    return NULL;
}

int main(int argc, char *argv[]) {
    int num_threads = DEFAULT_THREADS;
    int duration = DEFAULT_SECONDS;

    if (argc > 1) num_threads = atoi(argv[1]);
    if (argc > 2) duration = atoi(argv[2]);

    printf("=== Bench Server Sim (Synthetic Backend Load) ===\n");
    printf("Threads:  %d\n", num_threads);
    printf("Duration: %d seconds\n", duration);
    printf("PID:      %d\n\n", getpid()); 

    pthread_t threads[num_threads];
    thread_arg_t args[num_threads];

    for (int i = 0; i < num_threads; i++) {
        args[i].id = i;
        args[i].duration_sec = duration;
        args[i].total_allocs = 0;
        pthread_create(&threads[i], NULL, worker, &args[i]);
    }

    size_t total_ops = 0;
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
        total_ops += args[i].total_allocs;
    }

    printf("\nDone! Total Requests Handled: %zu\n", total_ops);
    printf("Throughput: %.2f req/sec\n", total_ops / (double)duration);

    return 0;
}