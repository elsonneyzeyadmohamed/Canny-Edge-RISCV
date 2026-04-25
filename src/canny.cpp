#include "canny.hpp"
#include <cmath>

CannyEdgeDetector::CannyEdgeDetector(int w, int h) : width(w), height(h) {}

/**
 * Step 1: Smooth the image to remove noise.
 * Uses a standard 3x3 Gaussian kernel.
 */
void CannyEdgeDetector::applyGaussianBlur(const unsigned char* input, unsigned char* output) {
    int kernel[3][3] = {
        {1, 2, 1},
        {2, 4, 2},
        {1, 2, 1}
    };
    int weight = 16;

    for (int y = 1; y < height - 1; y++) {
        for (int x = 1; x < width - 1; x++) {
            int sum = 0;
            // Neighborhood convolution
            for (int ky = -1; ky <= 1; ky++) {
                for (int kx = -1; kx <= 1; kx++) {
                    sum += input[(y + ky) * width + (x + kx)] * kernel[ky + 1][kx + 1];
                }
            }
            output[y * width + x] = (unsigned char)(sum / weight);
        }
    }
}

/**
 * Step 2: Find image gradients (edges).
 * Uses Sobel horizontal (Gx) and vertical (Gy) operators.
 */
void CannyEdgeDetector::applySobel(const unsigned char* input, unsigned char* output) {
    int Gx[3][3] = {{-1, 0, 1}, {-2, 0, 2}, {-1, 0, 1}};
    int Gy[3][3] = {{-1, -2, -1}, { 0,  0,  0}, { 1,  2,  1}};

    for (int y = 1; y < height - 1; y++) {
        for (int x = 1; x < width - 1; x++) {
            int sumX = 0, sumY = 0;
            // Convolve with Gx and Gy kernels
            for (int ky = -1; ky <= 1; ky++) {
                for (int kx = -1; kx <= 1; kx++) {
                    int pixel = input[(y + ky) * width + (x + kx)];
                    sumX += pixel * Gx[ky + 1][kx + 1];
                    sumY += pixel * Gy[ky + 1][kx + 1];
                }
            }
            // Calculate gradient magnitude
            int magnitude = std::sqrt(sumX * sumX + sumY * sumY);
            output[y * width + x] = (unsigned char)(magnitude > 255 ? 255 : magnitude);
        }
    }
}