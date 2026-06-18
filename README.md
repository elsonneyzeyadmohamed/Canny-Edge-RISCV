# Phase 4: Compiler Optimization Sweep

## Objective

The objective of Phase 4 is to evaluate the effect of compiler optimization levels on the execution time and binary size of the Canny Edge Detection pipeline on a RISC-V target.

The tested optimization variants are:

* `-O0`
* `-O2`
* `-O3`
* Auto-vectorized build using `-O3 -ftree-vectorize -fopt-info-vec-all`

Each Canny stage is executed for 100 iterations, and the average execution time per stage is reported.

---

## Toolchain and Target

The project was compiled using the RISC-V bare-metal toolchain:

```bash
riscv64-unknown-elf-g++
```

The target architecture is:

```bash
-march=rv64gcv
```

The binaries were executed using:

```bash
qemu-riscv64 -cpu rv64,v=true,vlen=128
```

The same compiler, Makefile settings, and QEMU command should be used for all optimization variants to make the comparison fair.

---

## Timing Method

The profiling code uses wall-clock timing based on `CLOCK_MONOTONIC`.

Normally, Linux/POSIX programs use:

```cpp
clock_gettime(CLOCK_MONOTONIC, &ts);
```

However, this project uses the `riscv64-unknown-elf-g++` bare-metal toolchain, which does not expose the POSIX `clock_gettime()` function directly through the C library.

To keep the same project toolchain while still using monotonic wall-clock timing, the implementation invokes the RISC-V Linux `clock_gettime` syscall directly under `qemu-riscv64`.

The timing path is:

```text
now_ms() -> linux_clock_gettime(CLOCK_MONOTONIC, &ts) -> RISC-V Linux syscall -> monotonic wall-clock time
```

This means the project still uses `CLOCK_MONOTONIC` timing, but through the lower-level syscall interface instead of the normal C library wrapper.

Each stage is measured as:

cpp
start_time = now_ms();
/* run stage 100 times */
end_time = now_ms();
average_time = (end_time - start_time) / 100;


This satisfies the requirement of using monotonic wall-clock timing and averaging over 100 iterations.

---
## Execution Command

The sweep was run using:

make sweep_run IMG=tiger-animals-cat-predator-preview.jpg

The input image is converted to a 512×512 grayscale raw stream before being passed to the RISC-V executable.

---

## Important Note About Timing Differences

Even if two team members use the same repository commit, the same compiler, the same Makefile, and the same QEMU command, their absolute timing values may still differ.

This is because the program is executed through QEMU user-mode emulation, and the measured wall-clock time depends on the host machine. Factors such as CPU speed, WSL performance, system load, power mode, thermal throttling, RAM speed, and QEMU runtime behavior can affect the total measured time.

Therefore, Phase 4 comparisons should be made using one consistent environment. The most important result is the relative trend between `-O0`, `-O2`, `-O3`, and auto-vectorized builds within the same machine, not the direct comparison of absolute milliseconds between different laptops.

The results below were collected from one environment and should be interpreted as one consistent optimization sweep.

---

## Phase 4 Optimization Results

| Build           | Compiler Flags                            | Total Average Time per Iteration (ms) | Speedup vs `-O0` |
| --------------- | ----------------------------------------- | ------------------------------------: | ---------------: |
| `O0`            | `-O0`                                     |                               251     |            1.00× |
| `O2`            | `-O2`                                     |                                       |            4.86× |
| `O3`            | `-O3`                                     |                               49      |            5.03× |
| Auto-vectorized | `-O3 -ftree-vectorize -fopt-info-vec-all` |                                       |            5.01× |

---

## Stage Timing Breakdown


   here put timing for all build types for each function


## Binary Size Results

| Build           | Text Size (bytes) | Data Size (bytes) | BSS Size (bytes) | Total Size `dec` (bytes) |
| --------------- | ----------------: | ----------------: | ---------------: | -----------------------: |
| `O0`            |           608,907 |             4,150 |           12,088 |                  625,145 |
| `O2`            |           568,717 |             4,166 |           12,088 |                  584,971 |
| `O3`            |           571,135 |             4,166 |           12,088 |                  587,389 |
| Auto-vectorized |           571,135 |             4,166 |           12,088 |                  587,389 |

---

## Timing Verification

The timing implementation was verified by checking that all measured stages call `now_ms()`, and that `now_ms()` uses `CLOCK_MONOTONIC` through the direct RISC-V Linux `clock_gettime` syscall.

The important code path is:

```cpp
linux_clock_gettime(CLOCK_MONOTONIC, &ts);
```

Each measured stage uses:

```cpp
start_time = now_ms();
/* kernel loop */
end_time = now_ms();
```

This confirms that the Phase 4 timing sweep uses monotonic wall-clock timing instead of the normal `clock()` function. The only occurrence of `clock()` in the source file is inside a comment for explanation.

---

## Analysis

The `-O2`, `-O3`, and auto-vectorized builds greatly reduce the execution time compared to the unoptimized `-O0` build.

The `-O0` build has a total average runtime of 251 ms per iteration. After enabling compiler optimization, the runtime drops to approximately 40 ms per iteration.

The best runtime in this sweep is obtained by the `-O3` build:

```text
O3 total average time = 49 ms
```

compared to the `-O0` build.

The auto-vectorized build gives a very similar runtime to the normal `-O3` build:

```text
O3 total average time = 47 ms
Auto-vectorized total average time = 47.1 ms
```

This means that automatic vectorization did not provide a noticeable speedup over `-O3` in this run. The binary size of the `-O3` and auto-vectorized builds is also identical, suggesting that the compiler did not significantly change the generated code size for the auto-vectorized configuration.

The binary size decreases significantly from `-O0` to `-O2`, then slightly increases at `-O3`. This is expected because higher optimization levels may introduce additional optimized code transformations that slightly increase code size while improving runtime.

---

## Conclusion

Phase 4 shows that compiler optimization has a major effect on the Canny Edge Detection runtime. The optimized builds reduce the total execution time by about 5× compared to the unoptimized build.

The `-O3` build provides the best measured runtime in this sweep, while the auto-vectorized build performs almost the same as `-O3`. Therefore, further performance improvement may require manual RVV intrinsic optimization instead of relying only on automatic compiler vectorization.

Because the measured values are wall-clock times under QEMU, absolute timing values may differ between team members even with the same compiler and repository version. For this reason, all final Phase 4 comparisons should be taken from one consistent machine and environment.



## Phase 5: Hotspot Identification

### Best Binary: -O3 (49.17 ms/iteration)

| Stage        | Time (ms) | % of Total | RVV Target? |
|--------------|-----------|------------|-------------|
| Magnitude    | 12.37     | 25.2%      | ✅ YES      |
| Direction    | 11.96     | 24.3%      | ✅ YES      |
| Gaussian 5x5 | 8.97      | 18.2%      | ✅ YES      |
| D.Threshold  | 5.82      | 11.8%      | ❌ NO       |
| Hysteresis   | 4.32      | 8.8%       | ❌ NO       |
| NMS          | 2.96      | 6.0%       | ❌ NO       |
| Sobel Gx/Gy  | 2.77      | 5.6%       | ❌ NO       |

![Phase 5 Hotspot Profile](results/phase5_profile.png)

### Key Observations:
1. -O3 gives 5x speedup over -O0 (251ms → 49ms)
2. Direction stage REGRESSED at -O3 (3.4ms → 11.9ms) 
   due to branch-heavy conditional logic interfering with 
   loop unrolling — Amdahl's Law in practice
3. Auto-vectorization adds no benefit here (-O3 ≈ autovec)
   because GCC's cost model rejects most loops
4. Top 3 hotspots (Magnitude+Direction+Gaussian) = 67% 
   of runtime → these are the RVV targets for Phase 6

## Phase 6: Hotspot Results Comparison


## Gaussian:










## Magnetiude:










## Direction:



