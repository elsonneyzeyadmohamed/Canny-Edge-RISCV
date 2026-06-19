#include "rvv_kernels.h"
#include <riscv_vector.h>
#include <cstdint>
#include <vector>

// =============================================================
// Helper: create a zero-padded copy of the input image
// Padding amount = 2 on each side (required for 5x5 kernel)
//
// Performance note: this used to allocate a brand-new std::vector
// on every single call. Since applyGaussianBlur_rvv is called many
// times in a row (e.g. 100x in the timing sweep), that meant 100
// full heap allocations + 100 full zero-inits + 100 full image
// copies, none of which has anything to do with the actual RVV
// convolution we're trying to measure. We now reuse one static
// buffer and only resize it if the image dimensions change.
// =============================================================
static std::vector<uint8_t>& getPaddedBuffer(const uint8_t* input,
                                                int width, int height,
                                                int pad) {
    int padded_width = width + 2 * pad;
    int padded_height = height + 2 * pad;

    static std::vector<uint8_t> padded;
    static int cached_width = -1;
    static int cached_height = -1;

    // Only reallocate (and re-zero the border) if the size actually changed
    if (cached_width != width || cached_height != height) {
        padded.assign(padded_width * padded_height, 0);
        cached_width = width;
        cached_height = height;
    }

    // The interior must be refreshed every call since 'input' contents
    // can differ between calls even when width/height stay the same.
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            padded[(y + pad) * padded_width + (x + pad)] = input[y * width + x];
        }
    }

    return padded;
}

// =============================================================
// Stage 1: Gaussian Blur (RVV vectorized version)
// 5x5 kernel, sigma~1.0, integer coefficients sum = 273
// Boundary: zero-padding (handled via padded buffer, no branches)
// Accumulator: int32_t (vector) to avoid overflow
// Vectorized over the x-axis (one row at a time)
//
// LMUL choice: m1/m2/m4 instead of mf4/mf2/m1.
// Under QEMU, every RVV instruction has a fixed interpretation
// overhead regardless of how many elements it processes. By using
// full registers (m1 for u8, m2 for u16, m4 for i32) we process
// 4x more elements per instruction and pay that overhead 4x less
// often — directly reducing emulation cost without changing the
// algorithm at all.
// =============================================================
void applyGaussianBlur_rvv(const uint8_t* input, uint8_t* output,
                             int width, int height) {
const int kernel[5][5] = {
    { 1,  4,  7,  4, 1},
    { 4, 16, 26, 16, 4},
    { 7, 26, 41, 26, 7},
    { 4, 16, 26, 16, 4},
    { 1,  4,  7,  4, 1}
};
    // weight = 273 is the kernel's coefficient sum (used only in the
    // scalar version's direct division). The RVV version below replaces
    // the division by 273 with a fixed-point multiply+shift (see below),
    // so 'weight' itself is not used directly in this function.
    const int pad = 2;

    int padded_width = width + 2 * pad;

    // Step 1: get the padded buffer. Only the very first call (or a call
    // with new dimensions) pays the allocation/zero-init cost; later calls
    // reuse the same backing memory and just refresh the interior pixels.
    std::vector<uint8_t>& padded = getPaddedBuffer(input, width, height, pad);

    // Step 2: process the image row by row
    for (int y = 0; y < height; y++) {
        int x = 0;
        while (x < width) {
            // Strip-mining: vsetvl_e8m1 gives min(width-x, VLEN/8) elements.
            // This is 4x more elements per iteration than the old e32m1
            // (which gave VLEN/32). The widening chain u8m1->u16m2->i32m4
            // stays consistent: each widen step doubles LMUL, so all vector
            // lengths remain in sync.
            size_t vl = __riscv_vsetvl_e8m1(width - x);

            // Accumulator vector: one int32 per output pixel.
            // m4 = 4 registers grouped together, holds VLEN/8 elements.
            vint32m4_t sum = __riscv_vmv_v_x_i32m4(0, vl);

            // Walk the 5x5 kernel taps. ky/kx stay scalar (compile-time loop),
            // only the inner pixel access is vectorized.
            for (int ky = -2; ky <= 2; ky++) {
                for (int kx = -2; kx <= 2; kx++) {
                    int src_row = y + pad + ky;
                    int src_col_start = x + pad + kx;

                    // Load vl consecutive uint8 pixels from the padded buffer.
                    // m1 = one full register, holds VLEN/8 bytes — 4x more
                    // than the old mf4 which held only VLEN/32 bytes.
                    vuint8m1_t v_in_u8 = __riscv_vle8_v_u8m1(
                        &padded[src_row * padded_width + src_col_start], vl);

                    // Widen u8m1 -> u16m2 (zero-extend, LMUL doubles: m1->m2).
                    vuint16m2_t v_in_u16 = __riscv_vzext_vf2_u16m2(v_in_u8, vl);
                    vint16m2_t v_in_i16 = __riscv_vreinterpret_v_u16m2_i16m2(v_in_u16);

                    // Widening multiply (vwmul): i16m2 x scalar -> i32m4
                    // (LMUL doubles again: m2->m4). One instruction does
                    // widen+multiply, avoiding a separate vzext step.
                    vint32m4_t product = __riscv_vwmul_vx_i32m4(
                        v_in_i16, kernel[ky + 2][kx + 2], vl);

                    // Accumulate into the running sum
                    sum = __riscv_vadd_vv_i32m4(sum, product, vl);
                }
            }

            // Normalize: divide by kernel weight (273).
            // Direct integer division is expensive even in scalar code, so
            // we approximate it with a fixed-point multiply + shift:
            //   65536 / 273 ~= 240  =>  sum / 273 ~= (sum * 240) >> 16
            vint32m4_t scaled = __riscv_vmul_vx_i32m4(sum, 240, vl);
            vint32m4_t result = __riscv_vsra_vx_i32m4(scaled, 16, vl);

            // Clamp to 255 (replicates: if (result > 255) result = 255;)
            // Mask type is vbool8_t for m4: ratio = VLEN / (VLEN*4/32) = 8
            vbool8_t mask_over = __riscv_vmsgt_vx_i32m4_b8(result, 255, vl);
            vint32m4_t clamped = __riscv_vmerge_vxm_i32m4(result, 255, mask_over, vl);

            // Narrow i32m4 -> u16m2 -> u8m1 before storing
            vuint32m4_t u_clamped = __riscv_vreinterpret_v_i32m4_u32m4(clamped);
            vuint16m2_t narrow16 = __riscv_vncvt_x_x_w_u16m2(u_clamped, vl);
            vuint8m1_t  narrow8  = __riscv_vncvt_x_x_w_u8m1(narrow16, vl);

            // Store result for this batch of pixels
            __riscv_vse8_v_u8m1(&output[y * width + x], narrow8, vl);

            x += vl;
        }
    }
}
