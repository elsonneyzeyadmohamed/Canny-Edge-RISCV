## Phase 5: Hotspot Identification

### Best Binary: -O3 (49.17 ms/iteration)

| Stage        | Time (ms) | % of Total | RVV Target? |
|--------------|-----------|------------|-------------|
| Magnitude    | 12.37     | 25.2%      | ✅ YES      |
| Direction    | 11.96     | 24.3%      | ✅ YES      |
| Gaussian 5x5 | 8.97      | 18.2%      | ✅ YES      |
| D.Threshold  | 5.82      | 11.8%      | ⚠️ Maybe    |
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