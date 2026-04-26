#include "canny.hpp"
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <vector>

using namespace std;

CannyEdgeDetector::CannyEdgeDetector(int w, int h) : width(w), height(h) {}

// =============================================================
// Stage 1: Gaussian Blur
// 5x5 kernel, sigma~1.0, integer coefficients sum = 273
// Boundary: zero-padding (out-of-bounds pixels treated as 0)
// Accumulator: int32_t to avoid overflow (max = 255 * 15 * 25)
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
                    // Zero-padding: skip out-of-bounds (treat as 0)
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
// Gx and Gy stored SEPARATELY (Structure of Arrays / SoA)
// SoA chosen over AoS for RVV: consecutive Gx loads = 1 vector load
// int16_t sufficient: max Sobel value = 4*255 = 1020 < 32767
// =============================================================
void CannyEdgeDetector::applySobel(const unsigned char* input,
                                    int16_t* gx, int16_t* gy) {
    // Sobel-X: detects vertical edges (horizontal gradient)
    const int Kx[3][3] = {{-1, 0, 1}, {-2, 0, 2}, {-1, 0, 1}};
    // Sobel-Y: detects horizontal edges (vertical gradient)
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
// Faster than L2 (no sqrt), slight overestimate on diagonals
// Two-pass: find max -> normalize to [0,255]
// =============================================================
void CannyEdgeDetector::computeMagnitudeL1(const int16_t* gx,
                                            const int16_t* gy,
                                            unsigned char* magnitude) {
    const int N = width * height;
    vector<int> raw(N);

    int maxVal = 1;
    for (int i = 0; i < N; i++) {
        raw[i] = abs((int)gx[i]) + abs((int)gy[i]);
        if (raw[i] > maxVal) maxVal = raw[i];
    }
    for (int i = 0; i < N; i++)
        magnitude[i] = (unsigned char)((raw[i] * 255) / maxVal);
}

// =============================================================
// Stage 3b: Gradient Magnitude - L2 Norm
// L2 = sqrt(Gx^2 + Gy^2) - mathematically correct
// Two-pass: find max -> normalize to [0,255]
// =============================================================
void CannyEdgeDetector::computeMagnitudeL2(const int16_t* gx,
                                            const int16_t* gy,
                                            unsigned char* magnitude) {
    const int N = width * height;
    vector<float> raw(N);

    float maxVal = 1.0f;
    for (int i = 0; i < N; i++) {
        raw[i] = sqrt((float)gx[i] * gx[i] + (float)gy[i] * gy[i]);
        if (raw[i] > maxVal) maxVal = raw[i];
    }
    for (int i = 0; i < N; i++)
        magnitude[i] = (unsigned char)((raw[i] / maxVal) * 255.0f);
}

// =============================================================
// Stage 4: Gradient Direction
// Quantizes to 4 directions: 0, 45, 90, 135 degrees
// Uses INTEGER cross-multiplication instead of atan2():
//   tan(22.5) ~ 2/5  -> ay*5 < ax*2  => 0 deg
//   tan(67.5) ~ 12/5 -> ay*5 > ax*12 => 90 deg
//   otherwise        => 45 or 135 based on sign(Gx*Gy)
// Output: 0=0deg, 1=45deg, 2=90deg, 3=135deg
// =============================================================
void CannyEdgeDetector::computeDirection(const int16_t* gx,
                                          const int16_t* gy,
                                          unsigned char* direction) {
    const int N = width * height;
    for (int i = 0; i < N; i++) {
        int ax = abs((int)gx[i]);
        int ay = abs((int)gy[i]);

        unsigned char dir;
        if (ay * 5 < ax * 2)
            dir = 0;
        else if (ay * 5 > ax * 12)
            dir = 2;
        else
            dir = ((int)gx[i] * (int)gy[i] >= 0) ? 1 : 3;

        direction[i] = dir;
    }
}