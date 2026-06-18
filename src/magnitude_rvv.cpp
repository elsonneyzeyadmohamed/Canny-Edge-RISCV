#include "rvv_kernels.h"
#include <riscv_vector.h>
#include <cstdint>
#include <vector>

// ============================================================================
// Stage 3a (RVV): Gradient Magnitude - L1 Norm, fully vectorized
//
// L1 = |Gx| + |Gy|  -> normalize to [0,255] by dividing by the global max
//
// Only L1 gets an RVV version. L2 (sqrt(Gx^2+Gy^2)) stays scalar-only:
// RVV has no integer/fixed-point sqrt instruction in the base 'V' extension,
// so vectorizing it isn't a "free" win like L1 is. That's also why the
// header only declares computeMagnitudeL1_rvv and not an L2 variant.
//
// LMUL choice: we work on int16_t the whole way through (m2, i.e. SEW=16,
// LMUL=2). Gx/Gy come out of Sobel as int16_t, so loading them at e16m2
// keeps the data in its natural width and gives us 2 vector registers'
// worth of elements per strip-mine step instead of just 1 (m1). We don't
// go higher (m4) because we don't have many live vector variables at once
// here, so the extra elements-per-step wouldn't be worth the register
// pressure / spilling risk mentioned in section 6.2 of the hints guide.
//
// This whole kernel is VLEN-agnostic: every loop calls vsetvl itself to
// ask "how many elements can the hardware actually do right now" and
// then advances by exactly that many (strip-mining). Nothing here assumes
// VLEN=128. Running at VLEN=128/256/512 will all produce the same output. 
// ============================================================================
void computeMagnitudeL1_rvv(
    const int16_t* gx,
    const int16_t* gy,
    uint8_t* mag,
    int total_pixels
)
{
    // We need two passes over the data (same as the scalar version):
    //   Pass 1: compute |Gx|+|Gy| for every pixel AND find the global max
    //   Pass 2: divide every pixel by that max to normalize into [0,255]
    // We can't normalize until we know the max of the WHOLE image, so the
    // two passes can't be merged into one.
    std::vector<uint16_t> raw(total_pixels);
    uint16_t maxVal = 1; // matches the scalar version's "avoid divide-by-zero" floor

    // ---------------- Pass 1: |Gx| + |Gy|, plus running max ----------------
    int i = 0;
    while (i < total_pixels) {
        // Ask the hardware how many elements we can process this round.
        // This is the strip-mining step described in section 6.1 - it's
        // what makes the code work on any VLEN without recompiling.
        size_t vl = __riscv_vsetvl_e16m2(total_pixels - i);

        vint16m2_t vx = __riscv_vle16_v_i16m2(&gx[i], vl);
        vint16m2_t vy = __riscv_vle16_v_i16m2(&gy[i], vl);

        // Absolute value trick (per the hints guide: "use vmax of the value
        // and its negation if a dedicated abs intrinsic isn't available").
        // vrsub_vx(v, 0) computes 0 - v, i.e. negates every lane. Then
        // vmax(v, -v) keeps whichever of the two is non-negative, which is
        // exactly |v|. No dedicated "vabs" instruction exists in RVV, so
        // this two-instruction combo is the standard way to get it.
        vint16m2_t neg_x = __riscv_vrsub_vx_i16m2(vx, 0, vl);
        vint16m2_t neg_y = __riscv_vrsub_vx_i16m2(vy, 0, vl);
        vint16m2_t ax = __riscv_vmax_vv_i16m2(vx, neg_x, vl);
        vint16m2_t ay = __riscv_vmax_vv_i16m2(vy, neg_y, vl);

        // |Gx| + |Gy|. Max possible Sobel value is ~1020 (4*255), so
        // |Gx|+|Gy| tops out around 2040 - nowhere near overflowing
        // int16_t, so a plain (non-widening) add is safe here.
        vint16m2_t sum = __riscv_vadd_vv_i16m2(ax, ay, vl);
        // The sum is always >= 0 by construction, so reinterpreting the
        // same bits as unsigned is free and lets us store/compare as
        // uint16_t for the rest of the pipeline (matches mag[] being
        // unsigned, and lines up with the unsigned max-reduction below).
        vuint16m2_t usum = __riscv_vreinterpret_v_i16m2_u16m2(sum);

        __riscv_vse16_v_u16m2(&raw[i], usum, vl);

        // Vector reduction: collapse this chunk's elements down to a
        // single max value. vredmaxu writes its answer into element 0 of
        // an m1 result register - this is the "new RVV concept" the guide
        // warns about in 6.5: reductions behave differently from normal
        // elementwise ops. We seed the reduction with our running max so
        // far (as a 1-element m1 vector), so each chunk's result already
        // folds in everything we've seen in previous chunks - that's how
        // we get one global max out of multiple strip-mined iterations
        // without a separate scalar loop at the end.
        vuint16m1_t seed = __riscv_vmv_v_x_u16m1(maxVal, vl);
        vuint16m1_t reduced = __riscv_vredmaxu_vs_u16m2_u16m1(usum, seed, vl);
        maxVal = __riscv_vmv_x_s_u16m1_u16(reduced); // pull the scalar result out

        i += vl;
    }

    // ---------------- Pass 2: normalize raw[] into mag[] using maxVal ----------------
    i = 0;
    while (i < total_pixels) {
        size_t vl = __riscv_vsetvl_e16m2(total_pixels - i);

        vuint16m2_t vraw = __riscv_vle16_v_u16m2(&raw[i], vl);

        // raw[i] * 255 can reach ~2040*255 = ~520,200, which blows way past
        // what a 16-bit lane can hold. This is exactly the widening
        // situation from section 6.3: multiplying 16-bit values and
        // needing a 32-bit result. vwmulu does the multiply AND the
        // widening in one instruction. Note the LMUL chain: our 16-bit
        // input is m2, so per the guide's rule ("widening doubles the
        // LMUL"), the 32-bit output here is m4.
        vuint32m4_t product = __riscv_vwmulu_vx_u32m4(vraw, 255, vl);

        // Now divide by the global max to bring everything back into
        // [0,255]. RVV does have real integer divide instructions
        // (vdivu for unsigned), so we don't need a fixed-point trick here
        // the way the Gaussian normalization does in section 6.4.
        vuint32m4_t normalized = __riscv_vdivu_vx_u32m4(product, (uint32_t)maxVal, vl);

        // The result is guaranteed to be <= 255 (since raw[i] <= maxVal),
        // so we just need to narrow the 32-bit value back down to 8 bits
        // to match mag[]'s type. We do it in two narrowing steps
        // (32->16->8, halving the LMUL each time: m4->m2->m1) using a
        // shift of 0, i.e. "keep all the bits, just narrow the container."
        // Since the value already fits in 8 bits, no precision is lost.
        vuint16m2_t narrow16 = __riscv_vnsrl_wx_u16m2(normalized, 0, vl);
        vuint8m1_t  narrow8  = __riscv_vnsrl_wx_u8m1(narrow16, 0, vl);

        __riscv_vse8_v_u8m1(&mag[i], narrow8, vl);

        i += vl;
    }
}
