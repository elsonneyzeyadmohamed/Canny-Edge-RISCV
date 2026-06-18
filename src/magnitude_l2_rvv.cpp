#include "rvv_kernels.h"

#ifdef USE_RVV_L2

#include <riscv_vector.h>
#include <cstdint>
#include <vector>
#include <cmath>

// ============================================================================
// RVV L2 Gradient Magnitude
//
// Scalar equation:
//     L2 = sqrt(Gx^2 + Gy^2)
//     mag[i] = (L2 / max_L2) * 255
//
// Optimized idea:
//     max_L2 = sqrt(max(Gx^2 + Gy^2))
//
// So we do:
//     Pass 1:
//         compute sumSq = Gx^2 + Gy^2
//         store sumSq
//         find maxSq
//
//     Pass 2:
//         mag = sqrt(sumSq) * (255 / sqrt(maxSq))
//
// This avoids storing float raw magnitudes and avoids vector division.
// It uses one vector sqrt per pixel, which is necessary for true L2 magnitude.
// ============================================================================

void computeMagnitudeL2_rvv(
    const int16_t* gx,
    const int16_t* gy,
    uint8_t* mag,
    int total_pixels
)
{
    std::vector<uint32_t> sumsq(total_pixels);

    uint32_t maxSq = 1;  // avoid divide by zero, same idea as scalar maxVal = 1.0f

    // =========================================================
    // Pass 1: compute Gx^2 + Gy^2 and find global max square
    // =========================================================
    int i = 0;

    while (i < total_pixels) {
        size_t vl = __riscv_vsetvl_e16m2(total_pixels - i);

        // Load Gx and Gy.
        vint16m2_t vx = __riscv_vle16_v_i16m2(&gx[i], vl);
        vint16m2_t vy = __riscv_vle16_v_i16m2(&gy[i], vl);

        // Widening multiply:
        // int16 * int16 -> int32
        // This avoids overflow when squaring Sobel values.
        vint32m4_t gx2 = __riscv_vwmul_vv_i32m4(vx, vx, vl);
        vint32m4_t gy2 = __riscv_vwmul_vv_i32m4(vy, vy, vl);

        // sumSq = gx^2 + gy^2
        vint32m4_t vsum_i32 = __riscv_vadd_vv_i32m4(gx2, gy2, vl);

        // sumSq is always non-negative, so reinterpret as uint32.
        vuint32m4_t vsum_u32 = __riscv_vreinterpret_v_i32m4_u32m4(vsum_i32);

        // Store sumSq for pass 2.
        __riscv_vse32_v_u32m4(&sumsq[i], vsum_u32, vl);

        // Reduce this vector chunk to one max value.
        vuint32m1_t seed = __riscv_vmv_v_x_u32m1(maxSq, vl);
        vuint32m1_t reduced = __riscv_vredmaxu_vs_u32m4_u32m1(vsum_u32, seed, vl);
        maxSq = __riscv_vmv_x_s_u32m1_u32(reduced);

        i += vl;
    }

    // max_L2 = sqrt(maxSq)
    // invScale = 255 / max_L2
    float invScale = 255.0f / sqrtf(static_cast<float>(maxSq));

    // =========================================================
    // Pass 2: sqrt(sumSq) * invScale -> uint8 magnitude
    // =========================================================
    i = 0;

    while (i < total_pixels) {
        size_t vl = __riscv_vsetvl_e32m4(total_pixels - i);

        // Load stored sumSq.
        vuint32m4_t vsum_u32 = __riscv_vle32_v_u32m4(&sumsq[i], vl);

        // Convert uint32 sumSq to float32.
        vfloat32m4_t vf = __riscv_vfcvt_f_xu_v_f32m4(vsum_u32, vl);

        // L2 = sqrt(sumSq)
        vf = __riscv_vfsqrt_v_f32m4(vf, vl);

        // normalized = L2 * (255 / max_L2)
        vf = __riscv_vfmul_vf_f32m4(vf, invScale, vl);

        // Convert float to unsigned int using round-toward-zero.
        // This matches C++ static_cast<uint8_t>(float_value) behavior better
        // than rounding-to-nearest.
        vuint32m4_t vout32 = __riscv_vfcvt_rtz_xu_f_v_u32m4(vf, vl);

        // Safety clamp to 255.
        vuint32m4_t vmax255 = __riscv_vmv_v_x_u32m4(255, vl);
        vout32 = __riscv_vminu_vv_u32m4(vout32, vmax255, vl);

        // Narrow 32 -> 16 -> 8.
        vuint16m2_t vout16 = __riscv_vnsrl_wx_u16m2(vout32, 0, vl);
        vuint8m1_t  vout8  = __riscv_vnsrl_wx_u8m1(vout16, 0, vl);

        // Store uint8 magnitude.
        __riscv_vse8_v_u8m1(&mag[i], vout8, vl);

        i += vl;
    }
}

#endif  // USE_RVV_L2
