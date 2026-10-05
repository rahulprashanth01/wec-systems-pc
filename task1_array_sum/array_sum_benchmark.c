/*
 * Task I - Array sum benchmark with pthreads
 * 
 * Comparing three approaches:
 *   1) plain single-threaded loop
 *   2) 4 threads, each picks every 4th element (strided)
 *   3) 4 threads, each gets a contiguous chunk (N/4 elements)
 *
 * Build: cc -O3 -std=c11 -pthread array_sum_benchmark.c -o array_sum
 * Usage: ./array_sum [N]     (default N = 64M)
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <pthread.h>
#include <time.h>

#define NUM_THREADS 4
#define NUM_RUNS    7   // average over multiple runs for stability

// Structure to pass arguments to threads
struct thread_data {
    const uint64_t *array;
    size_t length;
    int thread_id;
    uint64_t result;
};

// simple xorshift64* prng
static uint64_t xorshift64(uint64_t *s) {
    *s ^= *s >> 12;
    *s ^= *s << 25;
    *s ^= *s >> 27;
    return *s * UINT64_C(2685821657736338717);
}

// Get current time in milliseconds
static double get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (ts.tv_sec * 1000.0) + (ts.tv_nsec / 1000000.0);
}

// 1. Standard sequential sum
static uint64_t sum_sequential(const uint64_t *array, size_t length) {
    uint64_t sum = 0;
    for (size_t i = 0; i < length; i++) {
        sum += array[i];
    }
    return sum;
}

// 2. Strided worker: Thread 0 does 0, 4, 8... Thread 1 does 1, 5, 9...
static void *worker_strided(void *arg) {
    struct thread_data *data = (struct thread_data *)arg;
    uint64_t sum = 0;
    
    for (size_t i = data->thread_id; i < data->length; i += NUM_THREADS) {
        sum += data->array[i];
    }
    
    data->result = sum;
    return NULL;
}

// 3. Contiguous worker: Thread 0 does first quarter, Thread 1 does second quarter...
static void *worker_contiguous(void *arg) {
    struct thread_data *data = (struct thread_data *)arg;
    uint64_t sum = 0;
    
    size_t chunk_size = data->length / NUM_THREADS;
    size_t start_index = data->thread_id * chunk_size;
    size_t end_index = start_index + chunk_size;
    
    // Make sure the last thread goes all the way to the end
    if (data->thread_id == NUM_THREADS - 1) {
        end_index = data->length;
    }

    for (size_t i = start_index; i < end_index; i++) {
        sum += data->array[i];
    }
    
    data->result = sum;
    return NULL;
}

// Helper to run threads with a specific worker function
static uint64_t run_with_threads(const uint64_t *array, size_t length, void *(*worker_function)(void *)) {
    pthread_t threads[NUM_THREADS];
    struct thread_data thread_args[NUM_THREADS];
    uint64_t total_sum = 0;

    // Create threads
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_args[i].array = array;
        thread_args[i].length = length;
        thread_args[i].thread_id = i;
        thread_args[i].result = 0;
        
        if (pthread_create(&threads[i], NULL, worker_function, &thread_args[i]) != 0) {
            printf("Error creating thread %d\n", i);
            exit(1);
        }
    }

    // Wait for threads to finish and combine results
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
        total_sum += thread_args[i].result;
    }

    return total_sum;
}

int main(int argc, char *argv[]) {
    // Determine the size of the array (default: 64 million elements)
    size_t n = (argc > 1) ? atoll(argv[1]) : (64 * 1024 * 1024);
    
    if (n < NUM_THREADS) {
        printf("Array size must be at least %d\n", NUM_THREADS);
        return 1;
    }
    
    printf("Initializing array with %zu elements...\n", n);
    uint64_t *array = (uint64_t *)malloc(n * sizeof(uint64_t));
    if (!array) {
        printf("Memory allocation failed!\n");
        return 1;
    }

    // Initialize array with pseudorandom values using xorshift64
    uint64_t seed = 0x5eed5eed12345678ULL;
    for (size_t i = 0; i < n; i++) {
        array[i] = xorshift64(&seed);
    }

    printf("Calculating expected result...\n");
    uint64_t expected_sum = sum_sequential(array, n);
    printf("Expected sum is: %llu\n\n", (unsigned long long)expected_sum);

    // Variables for timing
    double start_time, end_time, time_taken;
    uint64_t result;
    
    // --- Test 1: Sequential ---
    time_taken = 0;
    for (int r = 0; r < NUM_RUNS; r++) {
        start_time = get_time_ms();
        result = sum_sequential(array, n);
        end_time = get_time_ms();
        
        if (result != expected_sum) printf("Error in sequential sum!\n");
        time_taken += (end_time - start_time);
    }
    printf("1. Sequential approach:     %.3f ms\n", time_taken / NUM_RUNS);

    // --- Test 2: Threaded (Strided) ---
    time_taken = 0;
    for (int r = 0; r < NUM_RUNS; r++) {
        start_time = get_time_ms();
        result = run_with_threads(array, n, worker_strided);
        end_time = get_time_ms();
        
        if (result != expected_sum) printf("Error in strided sum!\n");
        time_taken += (end_time - start_time);
    }
    printf("2. 4-Threads (Strided):     %.3f ms\n", time_taken / NUM_RUNS);

    // --- Test 3: Threaded (Contiguous) ---
    time_taken = 0;
    for (int r = 0; r < NUM_RUNS; r++) {
        start_time = get_time_ms();
        result = run_with_threads(array, n, worker_contiguous);
        end_time = get_time_ms();
        
        if (result != expected_sum) printf("Error in contiguous sum!\n");
        time_taken += (end_time - start_time);
    }
    printf("3. 4-Threads (Contiguous):  %.3f ms\n", time_taken / NUM_RUNS);

    free(array);
    return 0;
}
