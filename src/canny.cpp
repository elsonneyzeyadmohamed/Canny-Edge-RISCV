#include "canny.hpp"
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <vector>

using namespace std;

// Constructor: simply stores the image width and height
// so every stage function can use them without needing extra parameters
CannyEdgeDetector::CannyEdgeDetector(int w, int h) : width(w), height(h) {}

// =============================================================
// Stage 1: Gaussian Blur
// 5x5 kernel, sigma~1.0, integer coefficients sum = 273
// Boundary: zero-padding (out-of-bounds pixels treated as 0)
// Accumulator: int32_t to avoid overflow (max = 255 * 15 * 25)
// =============================================================
void CannyEdgeDetector::applyGaussianBlur(const unsigned char* input,
                                           unsigned char* output) {
    // 5x5 Gaussian kernel approximated with integer weights
    // Center value (41) is the largest, weights decrease outward
    // to mimic the bell-shaped Gaussian curve
    const int kernel[5][5] = {
    { 1,  4,  7,  4, 1},
    { 4, 16, 26, 16, 4},
    { 7, 26, 41, 26, 7},
    { 4, 16, 26, 16, 4},
    { 1,  4,  7,  4, 1}
};
    // Sum of all kernel weights, used to normalize the result back to 0-255 range
    const int weight = 273;

    // Loop over every row of the image
    for (int y = 0; y < height; y++) {
        // Loop over every column of the image
        for (int x = 0; x < width; x++) {
            // Accumulator for the weighted sum of the 5x5 neighborhood
            // int32_t is used because the sum can exceed the range of a normal int on some platforms
            int32_t sum = 0;

            // Loop over the kernel rows, offset from -2 to +2 around the current pixel
            for (int ky = -2; ky <= 2; ky++) {
                // Loop over the kernel columns, offset from -2 to +2 around the current pixel
                for (int kx = -2; kx <= 2; kx++) {
                    // Compute the actual neighbor row in the image
                    int ny = y + ky;
                    // Compute the actual neighbor column in the image
                    int nx = x + kx;

                    // Zero-padding: if the neighbor falls outside the image bounds,
                    // skip it entirely (equivalent to treating it as 0, since 0 contributes nothing to the sum)
                    if (ny < 0 || ny >= height || nx < 0 || nx >= width)
                        continue;

                    // Add the weighted pixel value to the accumulator
                    // input[ny * width + nx] converts the 2D (nx, ny) coordinate into a 1D array index
                    sum += (int32_t)input[ny * width + nx] * kernel[ky + 2][kx + 2];
                }
            }

            // Normalize the accumulated sum back down by dividing by the total kernel weight
            int result = sum / weight;

            // Clamp the result to 255 in case of any rounding overflow, then store it as unsigned char
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
    // Negative weights on the left column, positive on the right
    const int Kx[3][3] = {{-1, 0, 1}, {-2, 0, 2}, {-1, 0, 1}};
    // Sobel-Y: detects horizontal edges (vertical gradient)
    // Negative weights on the top row, positive on the bottom
    const int Ky[3][3] = {{-1,-2,-1}, { 0, 0, 0}, { 1, 2, 1}};

    // Loop over every row of the image
    for (int y = 0; y < height; y++) {
        // Loop over every column of the image
        for (int x = 0; x < width; x++) {
            // Accumulators for the horizontal and vertical gradient sums
            int sumX = 0, sumY = 0;

            // Loop over the 3x3 neighborhood rows, offset from -1 to +1
            for (int ky = -1; ky <= 1; ky++) {
                // Loop over the 3x3 neighborhood columns, offset from -1 to +1
                for (int kx = -1; kx <= 1; kx++) {
                    // Compute the actual neighbor row in the image
                    int ny = y + ky;
                    // Compute the actual neighbor column in the image
                    int nx = x + kx;

                    // Default the pixel value to 0 (zero-padding for out-of-bounds neighbors)
                    int pixel = 0;

                    // If the neighbor is inside the image, read its real value instead of 0
                    if (ny >= 0 && ny < height && nx >= 0 && nx < width)
                        pixel = input[ny * width + nx];

                    // Accumulate the weighted pixel for the horizontal gradient
                    sumX += pixel * Kx[ky + 1][kx + 1];
                    // Accumulate the weighted pixel for the vertical gradient
                    sumY += pixel * Ky[ky + 1][kx + 1];
                }
            }

            // Store the horizontal gradient result, cast down to int16_t
            gx[y * width + x] = (int16_t)sumX;
            // Store the vertical gradient result, cast down to int16_t
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
    // Total number of pixels in the image
    const int N = width * height;

    // Temporary buffer to hold the raw (non-normalized) L1 magnitude values
    // Needed because we must find the max value before we can normalize to 0-255
    vector<int> raw(N);

    // Tracks the largest raw magnitude found, starts at 1 to avoid division by zero later
    int maxVal = 1;

    // First pass: compute |Gx| + |Gy| for every pixel and track the maximum value
    for (int i = 0; i < N; i++) {
        raw[i] = abs((int)gx[i]) + abs((int)gy[i]);
        if (raw[i] > maxVal) maxVal = raw[i];
    }

    // Second pass: normalize every raw value to the 0-255 range using the max found above
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
    // Total number of pixels in the image
    const int N = width * height;

    // Temporary buffer to hold the raw (non-normalized) L2 magnitude values
    // Stored as float because sqrt() returns a floating-point result
    vector<float> raw(N);

    // Tracks the largest raw magnitude found, starts at 1.0 to avoid division by zero later
    float maxVal = 1.0f;

    // First pass: compute sqrt(Gx^2 + Gy^2) for every pixel and track the maximum value
    for (int i = 0; i < N; i++) {
        raw[i] = sqrt((float)gx[i] * gx[i] + (float)gy[i] * gy[i]);
        if (raw[i] > maxVal) maxVal = raw[i];
    }

    // Second pass: normalize every raw value to the 0-255 range using the max found above
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
    // Total number of pixels in the image
    const int N = width * height;

    // Loop over every pixel
    for (int i = 0; i < N; i++) {
        // Absolute value of the horizontal gradient at this pixel
        int ax = abs((int)gx[i]);
        // Absolute value of the vertical gradient at this pixel
        int ay = abs((int)gy[i]);

        // Will hold the quantized direction (0, 1, 2, or 3)
        unsigned char dir;

        // If ay/ax is smaller than tan(22.5°), the gradient is closer to horizontal -> 0 degrees
        // Written as ay*5 < ax*2 to avoid floating-point division
        if (ay * 5 < ax * 2)
            dir = 0;
        // If ay/ax is larger than tan(67.5°), the gradient is closer to vertical -> 90 degrees
        // Written as ay*5 > ax*12 to avoid floating-point division
        else if (ay * 5 > ax * 12)
            dir = 2;
        // Otherwise the gradient falls in the diagonal range (45 or 135 degrees)
        // The sign of Gx*Gy tells us which diagonal: same sign -> 45 deg, opposite sign -> 135 deg
        else
            dir = ((int)gx[i] * (int)gy[i] >= 0) ? 1 : 3;

        // Store the final quantized direction for this pixel
        direction[i] = dir;
    }
}