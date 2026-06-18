#include "rvv_kernels.h"
#include <riscv_vector.h>
#include <cstdint>

// ============================================================================
// Stage 4 (RVV): Gradient Direction Quantization
//
// Scalar logic:
//
//   ax = abs(gx)
//   ay = abs(gy)
//
//   if      (ay * 5 < ax * 2)   dir = 0;
//   else if (ay * 5 > ax * 12)  dir = 2;
//   else                        dir = (gx * gy >= 0) ? 1 : 3;
//
// Direction encoding:
//   0 = 0 degrees
//   1 = 45 degrees
//   2 = 90 degrees
//   3 = 135 degrees
//
// This RVV implementation is VLEN-agnostic. It uses vsetvl each loop and
// advances by vl, so it should work for VLEN=128, 256, and 512.
// ============================================================================
void computeDirection_rvv(
    const int16_t* gx,
    const int16_t* gy,
    uint8_t* direction,
    int total_pixels
)
{
    int i = 0;

    while (i < total_pixels) {
        size_t vl = __riscv_vsetvl_e16m2(total_pixels - i);

        // Load Gx and Gy as signed 16-bit vectors.
        vint16m2_t vx = __riscv_vle16_v_i16m2(&gx[i], vl);
        vint16m2_t vy = __riscv_vle16_v_i16m2(&gy[i], vl);

        // Compute abs(Gx) and abs(Gy):
        // neg_x = 0 - gx
        // ax = max(gx, -gx)
        vint16m2_t neg_x = __riscv_vrsub_vx_i16m2(vx, 0, vl);
        vint16m2_t neg_y = __riscv_vrsub_vx_i16m2(vy, 0, vl);

        vint16m2_t ax = __riscv_vmax_vv_i16m2(vx, neg_x, vl);
        vint16m2_t ay = __riscv_vmax_vv_i16m2(vy, neg_y, vl);

        // Compute ay*5, ax*2, and ax*12.
        // Sobel values are small enough that these do not overflow int16_t.
        vint16m2_t ay5  = __riscv_vmul_vx_i16m2(ay, 5, vl);
        vint16m2_t ax2  = __riscv_vmul_vx_i16m2(ax, 2, vl);
        vint16m2_t ax12 = __riscv_vmul_vx_i16m2(ax, 12, vl);

        // Masks for scalar conditions:
        // condition 0 degrees: ay*5 < ax*2
        // condition 90 degrees: ay*5 > ax*12  <=> ax*12 < ay*5
        vbool8_t mask_dir0 = __riscv_vmslt_vv_i16m2_b8(ay5, ax2, vl);
        vbool8_t mask_dir2 = __riscv_vmslt_vv_i16m2_b8(ax12, ay5, vl);

        // Diagonal case:
        // dir = 1 if gx*gy >= 0
        // dir = 3 if gx*gy < 0
        //
        // Use widening multiply because gx*gy may exceed int16_t.
        vint32m4_t product = __riscv_vwmul_vv_i32m4(vx, vy, vl);
        vbool8_t mask_negative_product = __riscv_vmslt_vx_i32m4_b8(product, 0, vl);

        // Default diagonal direction = 1.
        vuint8m1_t vdir = __riscv_vmv_v_x_u8m1(1, vl);

        // If gx*gy < 0, diagonal direction becomes 3.
        vdir = __riscv_vmerge_vxm_u8m1(vdir, 3, mask_negative_product, vl);

        // Apply 90-degree condition.
        vdir = __riscv_vmerge_vxm_u8m1(vdir, 2, mask_dir2, vl);

        // Apply 0-degree condition last to match the scalar priority.
        vdir = __riscv_vmerge_vxm_u8m1(vdir, 0, mask_dir0, vl);

        // Store direction output.
        __riscv_vse8_v_u8m1(&direction[i], vdir, vl);

        i += vl;
    }
}
