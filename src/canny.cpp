#include "canny.hpp"
#include <cmath>

CannyEdgeDetector::CannyEdgeDetector(int w, int h) : width(w), height(h) {}

// --- Gaussian Blur Implementation ---
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
            for (int ky = -1; ky <= 1; ky++) {
                for (int kx = -1; kx <= 1; kx++) {
                    sum += input[(y + ky) * width + (x + kx)] * kernel[ky + 1][kx + 1];
                }
            }
            output[y * width + x] = (unsigned char)(sum / weight);
        }
    }
}

// --- Sobel Gradient Implementation ---
void CannyEdgeDetector::applySobel(const unsigned char* input, unsigned char* output) {
    int Gx[3][3] = {{-1, 0, 1}, {-2, 0, 2}, {-1, 0, 1}};
    int Gy[3][3] = {{-1, -2, -1}, { 0,  0,  0}, { 1,  2,  1}};

    for (int y = 1; y < height - 1; y++) {
        for (int x = 1; x < width - 1; x++) {
            int sumX = 0, sumY = 0;
            for (int ky = -1; ky <= 1; ky++) {
                for (int kx = -1; kx <= 1; kx++) {
                    int pixel = input[(y + ky) * width + (x + kx)];
                    sumX += pixel * Gx[ky + 1][kx + 1];
                    sumY += pixel * Gy[ky + 1][kx + 1];
                }
            }
            int magnitude = std::sqrt(sumX * sumX + sumY * sumY);
            output[y * width + x] = (unsigned char)(magnitude > 255 ? 255 : magnitude);
        }
    }
}