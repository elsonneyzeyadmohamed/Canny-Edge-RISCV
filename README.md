# Canny Edge Detection Project (RISC-V)

## Phase 5: Profiling and Hotspot Identification

Per-stage timing was measured over 100 iterations on QEMU (`-O3`, vlen=128) for the full Canny pipeline.

| Stage             | Time (ms) | % of Total |
|-------------------|-----------|------------|
| Gaussian 5x5      | 9.23      | 17.7%      |
| Sobel Gx/Gy       | 2.99      | 5.7%       |
| Magnitude L2      | 13.11     | 25.1%      |
| Direction         | 12.24     | 23.4%      |
| NMS               | 4.30      | 8.2%       |
| Double Threshold  | 6.00      | 11.5%      |
| Hysteresis        | 4.43      | 8.5%       |
| **Total**         | **52.31** | **100%**   |

![Phase 5 Profile](results/phase5_profile.png)

### Hotspots Identified

**Magnitude (25.1%)** and **Direction (23.4%)** are the largest contributors to execution time, followed by **Gaussian (17.7%)**.

### Phase 6 RVV Priority (Amdahl's Law)

1. **Magnitude** - pure arithmetic (multiply/add/sqrt), no branches, ideal for SIMD
2. **Gaussian Blur** - convolution loops, also pure arithmetic, highly vectorizable
3. **Direction** - branch-heavy logic; smaller RVV speedup expected but still worthwhile

Together, Magnitude + Gaussian = 42.8% of execution time and are the strongest RVV candidates.

To reproduce:
```bash
make riscv
make test_image
```
