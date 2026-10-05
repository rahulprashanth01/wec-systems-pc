/*
 * Task II - Lock-free SPSC ring buffer
 *
 * Fixed-size circular buffer (power of 2 capacity) using C11 atomics.
 * One producer thread, one consumer thread, no locks.
 *
 * Build: cc -O2 -std=c11 -pthread spsc_ring_buffer.c -o spsc
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdatomic.h>
#include <threads.h>

#define CAPACITY   8    // must be power of 2
#define NUM_ITEMS  32   // how many items to push through

typedef struct {
    _Alignas(64) atomic_size_t head;   // producer writes here
    _Alignas(64) atomic_size_t tail;   // consumer reads from here
    int data[CAPACITY];
} ringbuf;

// returns 1 on success, 0 if full
static int rb_push(ringbuf *rb, int val) {
    size_t h = atomic_load_explicit(&rb->head, memory_order_relaxed);
    size_t t = atomic_load_explicit(&rb->tail, memory_order_acquire);

    if (h - t == CAPACITY)
        return 0;  // full

    rb->data[h & (CAPACITY - 1)] = val;
    // release store so the consumer sees the data before the updated head
    atomic_store_explicit(&rb->head, h + 1, memory_order_release);
    return 1;
}

// returns 1 on success (and writes to *val), 0 if empty
static int rb_pop(ringbuf *rb, int *val) {
    size_t t = atomic_load_explicit(&rb->tail, memory_order_relaxed);
    size_t h = atomic_load_explicit(&rb->head, memory_order_acquire);

    if (t == h)
        return 0;  // empty

    *val = rb->data[t & (CAPACITY - 1)];
    // release store so the producer knows this slot is free
    atomic_store_explicit(&rb->tail, t + 1, memory_order_release);
    return 1;
}

static int producer_fn(void *arg) {
    ringbuf *rb = arg;
    for (int i = 1; i <= NUM_ITEMS; i++) {
        while (!rb_push(rb, i))
            thrd_yield();  // spin-wait if buffer is full
        printf("  pushed  %2d\n", i);
    }
    return 0;
}

static int consumer_fn(void *arg) {
    ringbuf *rb = arg;
    int val;
    for (int i = 1; i <= NUM_ITEMS; i++) {
        while (!rb_pop(rb, &val))
            thrd_yield();  // spin-wait if buffer is empty
        printf("  popped  %2d\n", val);
        if (val != i) {
            fprintf(stderr, "ERROR: expected %d but got %d (FIFO broken!)\n", i, val);
            return 1;
        }
    }
    return 0;
}

int main(void) {
    ringbuf rb = { .head = ATOMIC_VAR_INIT(0), .tail = ATOMIC_VAR_INIT(0) };

    thrd_t prod, cons;
    if (thrd_create(&prod, producer_fn, &rb) != thrd_success ||
        thrd_create(&cons, consumer_fn, &rb) != thrd_success) {
        fprintf(stderr, "failed to create threads\n");
        return 1;
    }

    int pret, cret;
    thrd_join(prod, &pret);
    thrd_join(cons, &cret);

    if (pret == 0 && cret == 0)
        printf("\nall %d items passed through in FIFO order, looks good!\n", NUM_ITEMS);
    else
        printf("\nsomething went wrong :(\n");

    return (pret || cret) ? 1 : 0;
}
