/* test_multilistener.c */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h> /* For memset */
#include "malloc_prof.h"

static int counter_A = 0;
static int counter_B = 0;

void listener_A(struct mp_sample_ctx *ctx) {
    __atomic_fetch_add(&counter_A, 1, __ATOMIC_RELAXED);
}

void listener_B(struct mp_sample_ctx *ctx) {
    __atomic_fetch_add(&counter_B, 1, __ATOMIC_RELAXED);
}

int main() {
    printf("Registering Listener A...\n");
    if (__mp_register_handler(listener_A) != 0) printf("Failed to register A\n");

    printf("Registering Listener B...\n");
    if (__mp_register_handler(listener_B) != 0) printf("Failed to register B\n");

    printf("Generating Load...\n");
    
    /* Make the pointer volatile so the compiler can't ignore it */
    volatile char *p;
    
    for (int i = 0; i < 50000; i++) {
        p = malloc(4096);
        if (p) {
            /* FORCE usage: Write to the memory so malloc cannot be optimized out */
            p[0] = 1; 
            free((void*)p);
        }
    }

    printf("Results:\n");
    printf("Listener A saw: %d samples\n", counter_A);
    printf("Listener B saw: %d samples\n", counter_B);

    if (counter_A > 0 && counter_A == counter_B) {
        printf("SUCCESS: Both listeners received identical streams.\n");
        return 0;
    } else {
        printf("FAILURE: Mismatch or no samples.\n");
        return 1;
    }
}