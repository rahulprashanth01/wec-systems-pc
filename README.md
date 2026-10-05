# Systems SIG Recruitment 2026 — Parallel Computing

Solutions for the three parallel computing tasks.

## Structure

- **[task1_array_sum](task1_array_sum/)** — pthreads benchmark comparing sequential vs strided vs contiguous 4-thread array summation
- **[task2_spsc_ring_buffer](task2_spsc_ring_buffer/)** — lock-free single-producer single-consumer FIFO ring buffer using C11 atomics
- **[task3_forest_fire_ndk](task3_forest_fire_ndk/)** — forest fire cellular automaton running on GPU via OpenGL ES 3.1 compute shaders (Android NDK)

## How to run

Tasks 1 and 2 build and run on any Linux system (or Termux). Check each folder's README for the exact build commands.

Task 3 needs to be cross-compiled with the Android NDK and run on a device that supports OpenGL ES 3.1. See its README for setup instructions.
