#include "canny.hpp"
#include <cmath>
#include <cstdlib>
#include <algorithm>

CannyEdgeDetector::CannyEdgeDetector(int w, int h) : width(w), height(h) {}

// =============================================================
// Stage 1: Gaussian Blur
// 5x5 kernel with sigma~1.0, integer coefficients sum = 273
// Boundary handling: zero-padding (out-of-bounds pixels = 0)
// Accumulator: int32_t to avoid overflow (max = 255 * 41 * 25)
// =============================================================
void CannyEdgeDetector::applyGaussianBlur(const unsigned char* input,
                                           unsigned char* output) {
    const int kernel[5][5] = {
        { 2,  4,  5,  4,  2},
        { 4,  9, 12,  9,  4},
        { 5, 12, 15, 12,  5},
        { 4,  9, 12,  9,  4},
        { 2,  4,  5,  4,  2}
    };
    const int weight = 273;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int32_t sum = 0;
            for (int ky = -2; ky <= 2; ky++) {
                for (int kx = -2; kx <= 2; kx++) {
                    int ny = y + ky;
                    int nx = x + kx;
                    // Zero-padding: treat out-of-bounds as 0
                    if (ny < 0 || ny >= height || nx < 0 || nx >= width)
                        continue;
                    sum += (int32_t)input[ny * width + nx] * kernel[ky + 2][kx + 2];
                }
            }
            int result = sum / weight;
            output[y * width + x] = (unsigned char)(result > 255 ? 255 : result);
        }
    }
}

// =============================================================
// Stage 2: Sobel Operator
// Returns Gx and Gy as SEPARATE int16_t arrays (Structure of Arrays)
// SoA layout chosen for RVV vectorization: consecutive Gx values
// load as a single vector instruction (no gather needed).
// int16_t is sufficient: max value = 4*255 = 1020 < 32767
// =============================================================
void CannyEdgeDetector::applySobel(const unsigned char* input,
                                    int16_t* gx, int16_t* gy) {
    const int Kx[3][3] = {{-1, 0, 1}, {-2, 0, 2}, {-1, 0, 1}};
    const int Ky[3][3] = {{-1,-2,-1}, { 0, 0, 0}, { 1, 2, 1}};

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int sumX = 0, sumY = 0;
            for (int ky = -1; ky <= 1; ky++) {
                for (int kx = -1; kx <= 1; kx++) {
                    int ny = y + ky;
                    int nx = x + kx;
                    int pixel = 0;
                    if (ny >= 0 && ny < height && nx >= 0 && nx < width)
                        pixel = input[ny * width + nx];
                    sumX += pixel * Kx[ky + 1][kx + 1];
                    sumY += pixel * Ky[ky + 1][kx + 1];
                }
            }
            gx[y * width + x] = (int16_t)sumX;
            gy[y * width + x] = (int16_t)sumY;
        }
    }
}

// =============================================================
// Stage 3a: Gradient Magnitude - L1 Norm
// L1 = |Gx| + |Gy|
// Faster than L2 (no sqrt), slight overestimate on diagonals.
// Two-pass: first find max, then normalize to [0,255].
// =============================================================
void CannyEdgeDetector::computeMagnitudeL1(const int16_t* gx,
                                            const int16_t* gy,
                                            unsigned char* magnitude) {
    const int N = width * height;
    std::vector<int> raw(N);

    int maxVal = 1;
    for (int i = 0; i < N; i++) {
        raw[i] = std::abs((int)gx[i]) + std::abs((int)gy[i]);
        if (raw[i] > maxVal) maxVal = raw[i];
    }
    for (int i = 0; i < N; i++)
        magnitude[i] = (unsigned char)((raw[i] * 255) / maxVal);
}

// =============================================================
// Stage 3b: Gradient Magnitude - L2 Norm
// L2 = sqrt(Gx^2 + Gy^2)  - mathematically correct
// Two-pass: first find max, then normalize to [0,255].
// =============================================================
void CannyEdgeDetector::computeMagnitudeL2(const int16_t* gx,
                                            const int16_t* gy,
                                            unsigned char* magnitude) {
    const int N = width * height;
    std::vector<float> raw(N);

    float maxVal = 1.0f;
    for (int i = 0; i < N; i++) {
        raw[i] = std::sqrt((float)gx[i] * gx[i] + (float)gy[i] * gy[i]);
        if (raw[i] > maxVal) maxVal = raw[i];
    }
    for (int i = 0; i < N; i++)
        magnitude[i] = (unsigned char)((raw[i] / maxVal) * 255.0f);
}

// =============================================================
// Stage 4: Gradient Direction
// Quantizes to 4 directions: 0, 45, 90, 135 degrees.
// Uses INTEGER cross-multiplication instead of atan2():
//   tan(22.5°) ~ 2/5  -> ay*5 < ax*2  => 0°
//   tan(67.5°) ~ 12/5 -> ay*5 > ax*12 => 90°
//   otherwise         => 45° or 135° based on sign(Gx*Gy)
// Output: 0=0°, 1=45°, 2=90°, 3=135°
// =============================================================
void CannyEdgeDetector::computeDirection(const int16_t* gx,
                                          const int16_t* gy,
                                          unsigned char* direction) {
    const int N = width * height;
    for (int i = 0; i < N; i++) {
        int ax = std::abs((int)gx[i]);
        int ay = std::abs((int)gy[i]);

        unsigned char dir;
        if (ay * 5 < ax * 2)
            dir = 0;                                    // ~0°
        else if (ay * 5 > ax * 12)
            dir = 2;                                    // ~90°
        else
            dir = ((int)gx[i] * (int)gy[i] >= 0) ? 1 : 3; // 45° or 135°

        direction[i] = dir;
    }
}