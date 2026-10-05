# Task II - Lock-Free SPSC Ring Buffer

A single-producer single-consumer ring buffer implemented from scratch using C11 atomics (`stdatomic.h`) and `threads.h`. No external queue libraries used.

## Design

The buffer is backed by a fixed-size array with `CAPACITY = 8` (power of 2 so I can use bitmask instead of modulo for wrapping). There are two counters:
- `head` — the producer increments this after writing
- `tail` — the consumer increments this after reading

Each counter is padded to its own cache line (`_Alignas(64)`) to avoid false sharing between the two threads.

The key insight for making this lock-free is the memory ordering:
- Producer does a **release store** on `head` after writing data, so the consumer will always see the data before seeing the updated head pointer
- Consumer does an **acquire load** on `head` to read the producer's writes, and then a **release store** on `tail` so the producer knows when a slot is safe to reuse

Since there's only ever one thread writing each counter, we don't need any CAS (compare-and-swap) operations. The buffer is full when `head - tail == CAPACITY` and empty when `head == tail`. Letting the counters grow monotonically (instead of wrapping them) avoids the ambiguity where both full and empty would map to the same index.

## Building & running

```sh
cc -O2 -std=c11 -pthread spsc_ring_buffer.c -o spsc
./spsc
```

The output will show interleaved "pushed" and "popped" lines (order depends on scheduling), but the popped values should always be in strict FIFO order 1 through 32. At the end it prints whether the ordering check passed.
