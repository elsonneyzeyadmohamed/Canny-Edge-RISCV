#include "rvv_kernels.h"

#include <riscv_vector.h>
#include <cstdint>

// ============================================================================
// Scalar fallback for borders
// ============================================================================
// This function MUST match CannyEdgeDetector::applyGaussianBlur exactly.
//
// Kernel:
// 1   4   7   4   1
// 4   16  26  16  4
// 7   26  41  26  7
// 4   16  26  16  4
// 1   4   7   4   1
//
// Sum = 273
// Boundary = zero padding
// ============================================================================

static inline unsigned char gaussian5x5_scalar_zero_273(
    const unsigned char* input,
    int width,
    int height,
    int x,
    int y
)
{
    static const int kernel[5][5] = {
        { 1,  4,  7,  4, 1},
        { 4, 16, 26, 16, 4},
        { 7, 26, 41, 26, 7},
        { 4, 16, 26, 16, 4},
        { 1,  4,  7,  4, 1}
    };

    int32_t sum = 0;

    for (int ky = -2; ky <= 2; ++ky) {
        int yy = y + ky;

        if (yy < 0 || yy >= height) {
            continue;
        }

        for (int kx = -2; kx <= 2; ++kx) {
            int xx = x + kx;

            if (xx < 0 || xx >= width) {
                continue;
            }

            sum += static_cast<int32_t>(input[yy * width + xx]) *
                   kernel[ky + 2][kx + 2];
        }
    }

    int result = sum / 273;

    if (result > 255) {
        result = 255;
    }

    return static_cast<unsigned char>(result);
}

// ============================================================================
// Helper: acc += load_u8(ptr) * coeff
// ============================================================================

static inline vuint32m4_t add_weighted_u8(
    vuint32m4_t acc,
    const unsigned char* ptr,
    unsigned long coeff,
    size_t vl
)
{
    vuint8m1_t pixels_u8 = __riscv_vle8_v_u8m1(
        reinterpret_cast<const uint8_t*>(ptr),
        vl
    );

    vuint16m2_t product_u16 = __riscv_vwmulu_vx_u16m2(
        pixels_u8,
        coeff,
        vl
    );

    acc = __riscv_vwaddu_wv_u32m4(
        acc,
        product_u16,
        vl
    );

    return acc;
}

// ============================================================================
// RVV Gaussian Blur
// ============================================================================
// Strategy:
// - Borders are computed using scalar fallback to exactly match zero padding.
// - Interior area is vectorized.
// - The inner vector loop processes a strip of output pixels at once.
// - Width does NOT need to be power of two.
// - Tail handling is done by vsetvl.
// ============================================================================

void applyGaussianBlur_rvv(
    const unsigned char* input,
    unsigned char* output,
    int width,
    int height
)
{
    if (!input || !output || width <= 0 || height <= 0) {
        return;
    }

    // Very small images have no safe 5x5 interior.
    // Use exact scalar fallback for all pixels.
    if (width < 5 || height < 5) {
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                output[y * width + x] =
                    gaussian5x5_scalar_zero_273(input, width, height, x, y);
            }
        }
        return;
    }

    // Top two rows.
    for (int y = 0; y < 2; ++y) {
        for (int x = 0; x < width; ++x) {
            output[y * width + x] =
                gaussian5x5_scalar_zero_273(input, width, height, x, y);
        }
    }

    // Bottom two rows.
    for (int y = height - 2; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            output[y * width + x] =
                gaussian5x5_scalar_zero_273(input, width, height, x, y);
        }
    }

    // Middle rows.
    for (int y = 2; y < height - 2; ++y) {
        // Left border columns.
        output[y * width + 0] =
            gaussian5x5_scalar_zero_273(input, width, height, 0, y);

        output[y * width + 1] =
            gaussian5x5_scalar_zero_273(input, width, height, 1, y);

        // Right border columns.
        output[y * width + width - 2] =
            gaussian5x5_scalar_zero_273(input, width, height, width - 2, y);

        output[y * width + width - 1] =
            gaussian5x5_scalar_zero_273(input, width, height, width - 1, y);

        // Vectorized interior columns: x = 2 ... width - 3.
        int x = 2;
        const int end = width - 2;

        while (x < end) {
            size_t vl = __riscv_vsetvl_e8m1(static_cast<size_t>(end - x));

            vuint32m4_t acc = __riscv_vmv_v_x_u32m4(0, vl);

            const unsigned char* r0 = input + (y - 2) * width + x;
            const unsigned char* r1 = input + (y - 1) * width + x;
            const unsigned char* r2 = input + (y    ) * width + x;
            const unsigned char* r3 = input + (y + 1) * width + x;
            const unsigned char* r4 = input + (y + 2) * width + x;

            // Row y - 2: 1 4 7 4 1
            acc = add_weighted_u8(acc, r0 - 2,  1, vl);
            acc = add_weighted_u8(acc, r0 - 1,  4, vl);
            acc = add_weighted_u8(acc, r0,      7, vl);
            acc = add_weighted_u8(acc, r0 + 1,  4, vl);
            acc = add_weighted_u8(acc, r0 + 2,  1, vl);

            // Row y - 1: 4 16 26 16 4
            acc = add_weighted_u8(acc, r1 - 2,  4, vl);
            acc = add_weighted_u8(acc, r1 - 1, 16, vl);
            acc = add_weighted_u8(acc, r1,     26, vl);
            acc = add_weighted_u8(acc, r1 + 1, 16, vl);
            acc = add_weighted_u8(acc, r1 + 2,  4, vl);

            // Row y: 7 26 41 26 7
            acc = add_weighted_u8(acc, r2 - 2,  7, vl);
            acc = add_weighted_u8(acc, r2 - 1, 26, vl);
            acc = add_weighted_u8(acc, r2,     41, vl);
            acc = add_weighted_u8(acc, r2 + 1, 26, vl);
            acc = add_weighted_u8(acc, r2 + 2,  7, vl);

            // Row y + 1: 4 16 26 16 4
            acc = add_weighted_u8(acc, r3 - 2,  4, vl);
            acc = add_weighted_u8(acc, r3 - 1, 16, vl);
            acc = add_weighted_u8(acc, r3,     26, vl);
            acc = add_weighted_u8(acc, r3 + 1, 16, vl);
            acc = add_weighted_u8(acc, r3 + 2,  4, vl);

            // Row y + 2: 1 4 7 4 1
            acc = add_weighted_u8(acc, r4 - 2,  1, vl);
            acc = add_weighted_u8(acc, r4 - 1,  4, vl);
            acc = add_weighted_u8(acc, r4,      7, vl);
            acc = add_weighted_u8(acc, r4 + 1,  4, vl);
            acc = add_weighted_u8(acc, r4 + 2,  1, vl);

            // Exact scalar normalization.
            // Keep /273 for byte-exact comparison.
            vuint32m4_t div_u32 = __riscv_vdivu_vx_u32m4(
                acc,
                273,
                vl
            );

            // Values are already in 0..255, so narrowing is safe.
            vuint16m2_t out_u16 = __riscv_vnsrl_wx_u16m2(
                div_u32,
                0,
                vl
            );

            vuint8m1_t out_u8 = __riscv_vnsrl_wx_u8m1(
                out_u16,
                0,
                vl
            );

            __riscv_vse8_v_u8m1(
                reinterpret_cast<uint8_t*>(output + y * width + x),
                out_u8,
                vl
            );

            x += static_cast<int>(vl);
        }
    }
}
