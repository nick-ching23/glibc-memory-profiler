#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>

#define ALLOCS_PER_THREAD 1000000

void *worker(void *arg) {
    for (int i = 0; i < ALLOCS_PER_THREAD; i++) {
        // 1. Allocate
        char *p = malloc(128);
        if (p) {
            // 2. FORCE USAGE: Write to the memory
            // This prevents the compiler from deleting the malloc
            p[0] = (char)i; 
            
            // 3. Free
            free(p);
        }
    }
    return NULL;
}

int main(int argc, char **argv) {
    if (argc != 2) return 1;
    int num_threads = atoi(argv[1]);
    
    pthread_t *t = malloc(sizeof(pthread_t) * num_threads);
    
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    for (int i = 0; i < num_threads; i++) 
        pthread_create(&t[i], NULL, worker, NULL);
        
    for (int i = 0; i < num_threads; i++) 
        pthread_join(t[i], NULL);
        
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    double time_taken = (end.tv_sec - start.tv_sec) + 
                        (end.tv_nsec - start.tv_nsec) / 1e9;
    
    printf("%d,%.4f\n", num_threads, time_taken);
    return 0;
}