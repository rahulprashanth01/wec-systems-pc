# Task III - Forest Fire Simulation (GPU Compute)

GPU-accelerated forest fire simulation using OpenGL ES 3.1 compute shaders on Android. Runs headless through EGL — no window or display needed.

## How it works

The grid is an M×M array of cells, each either Healthy (H), Burning (B), or Nothing (.). Every epoch:
- Burning cells turn to Nothing
- Healthy cells next to a burning cell catch fire with probability p=0.15
- Everything else stays the same

The simulation stops when nothing is burning anymore.

I'm using two shader storage buffers (SSBOs) that swap roles each epoch — one is the "read" buffer, the other is "write". This way every cell in the shader reads from the previous epoch's state, not a half-updated grid. A third small SSBO acts as an atomic counter to track how many cells are still on fire, which I read back on the CPU to check for termination.

For the randomness (the p=0.15 probability), I'm using a hash of the cell index XOR'd with the epoch number. It's deterministic but looks random enough across the grid. I tried using `gl_GlobalInvocationID` based seeds and this seemed to give the best distribution.

The workgroup size is 16×16 which seemed like a reasonable choice. I dispatch `ceil(M/16)` groups in each dimension, with bounds checking in the shader.

## Building

You need the Android NDK since this uses Android's EGL and GLES libraries.

```sh
# set up the NDK toolchain
export NDK=/path/to/android-ndk
export CC="$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android24-clang"

# compile
$CC -O3 -std=c11 -Wall forest_fire.c -o forest_fire -lEGL -lGLESv3 -llog

# push to device
adb push forest_fire /sdcard/Download/
```

Or use CMake (there's a CMakeLists.txt included):
```sh
cmake -B build \
  -DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-24
cmake --build build
```

## Running on the device

Android mounts `/sdcard` as noexec so you can't run the binary directly from there. Copy it to Termux's home first:

```sh
termux-setup-storage   # one-time setup
cp ~/storage/downloads/forest_fire ~/
chmod +x ~/forest_fire
~/forest_fire 10 20 128 512
```

Default sizes (if you run with no args) are 10, 20, and 128. For M ≤ 20 it prints the full grid each epoch. The program prints the GL version at startup — if your device doesn't support GLES 3.1 you'll get an error right away.

## Results

_TODO: fill in after running on device_

| Device | M | Epochs to extinguish |
|--------|--:|--------------------:|
|        | 10 | |
|        | 20 | |
|        | 128 | |
|        | 512 | |
