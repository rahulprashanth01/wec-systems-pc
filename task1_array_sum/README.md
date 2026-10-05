# Task I - Pthread Array Sum Benchmark

Compares three ways of summing a big array of random `uint64_t`s:
1. Single threaded (baseline)
2. 4 threads, strided — thread `i` handles indices where `idx % 4 == i`
3. 4 threads, contiguous — thread `i` handles `[i*N/4, (i+1)*N/4)`

## How to build and run

```sh
cc -O3 -std=c11 -pthread array_sum_benchmark.c -o array_sum
./array_sum           # defaults to N = 64M
./array_sum 67108864  # or pass N explicitly
```

Needs `pthreads` and `clock_gettime` (any normal linux system has these).

## Results

Ran on my machine with N = 67,108,864:

| Method | Avg time (ms) |
|--------|------------:|
| Sequential | 19.230 |
| 4-thread strided | 24.934 |
| 4-thread contiguous | 7.979 |

## Analysis

The contiguous chunking approach is clearly the fastest — roughly 2.6x faster than doing it single-threaded. This makes sense because each thread is reading a contiguous block of memory, which plays nicely with CPU cache prefetching. The processor can predict the sequential access pattern and load cache lines ahead of time.

The strided approach is actually *slower* than single-threaded, which surprised me at first but makes sense when you think about it. When all 4 threads interleave their accesses (thread 0 reads index 0, thread 1 reads index 1, etc.), they're all hitting the same cache lines simultaneously. This causes:
- **False sharing** — multiple cores fighting over the same 64-byte cache lines
- Bad spatial locality — each thread's accesses are spread across the entire array instead of being localized

So the takeaway is that naive parallelism can actually hurt performance if you don't think about memory access patterns. Contiguous partitioning is the way to go for this kind of embarrassingly parallel reduction.
