#include "rvv_kernels.h"
#include <riscv_vector.h>
#include <cstdint>
#include <vector>

void computeMagnitudeL1_rvv(
    const int16_t* gx,
    const int16_t* gy,
    uint8_t* mag,
    int total_pixels
)
{
    std::vector<uint16_t> raw(total_pixels);
    int maxVal = 1;

    int i = 0;

    while (i < total_pixels) {
        size_t vl = __riscv_vsetvl_e16m2(total_pixels - i);

        vint16m2_t vx = __riscv_vle16_v_i16m2(&gx[i], vl);
        vint16m2_t vy = __riscv_vle16_v_i16m2(&gy[i], vl);

        vint16m2_t zero = __riscv_vmv_v_x_i16m2(0, vl);

        vint16m2_t neg_x = __riscv_vsub_vv_i16m2(zero, vx, vl);
        vint16m2_t neg_y = __riscv_vsub_vv_i16m2(zero, vy, vl);

        vbool8_t mask_x = __riscv_vmslt_vx_i16m2_b8(vx, 0, vl);
        vbool8_t mask_y = __riscv_vmslt_vx_i16m2_b8(vy, 0, vl);

        vx = __riscv_vmerge_vvm_i16m2(vx, neg_x, mask_x, vl);
        vy = __riscv_vmerge_vvm_i16m2(vy, neg_y, mask_y, vl);

        vint16m2_t sum = __riscv_vadd_vv_i16m2(vx, vy, vl);
        vuint16m2_t usum = __riscv_vreinterpret_v_i16m2_u16m2(sum);

        __riscv_vse16_v_u16m2(&raw[i], usum, vl);

        i += vl;
    }

// Partially vectorized 

    for (int j = 0; j < total_pixels; ++j) {
        if (raw[j] > maxVal) {
            maxVal = raw[j];
        }
    }

    for (int j = 0; j < total_pixels; ++j) {
        mag[j] = static_cast<uint8_t>((raw[j] * 255) / maxVal);
    }
}
