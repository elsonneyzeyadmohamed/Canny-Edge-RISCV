#include "image_types.hpp"
#include "canny.hpp"
#include <cstdio>
#include <cstdint>
#include <vector>

int main() {
    // Image dimensions
    const int W = 512;
    const int H = 512;
    const int N = W * H;

    // --- Input / Output buffers ---
    Image tiger(W, H);
    Image blurred(W, H);

    // Gx and Gy stored SEPARATELY (Structure of Arrays)
    // Better for RVV vectorization later
    std::vector<int16_t> gx(N), gy(N);

    // Magnitude buffers (L1 and L2)
    std::vector<unsigned char> mag_l1(N), mag_l2(N);

    // Direction buffer (values: 0, 1, 2, 3)
    std::vector<unsigned char> direction(N);

    // --- Step 1: Read raw grayscale image from stdin ---
    std::fread(tiger.data.data(), 1, N, stdin);

    // --- Step 2: Initialize detector ---
    CannyEdgeDetector detector(W, H);

    // --- Step 3: Gaussian Blur (5x5, zero-padding) ---
    detector.applyGaussianBlur(tiger.data.data(), blurred.data.data());

    // --- Step 4: Sobel -> separate Gx and Gy ---
    detector.applySobel(blurred.data.data(), gx.data(), gy.data());

    // --- Step 5a: Magnitude L1 = |Gx| + |Gy| ---
    detector.computeMagnitudeL1(gx.data(), gy.data(), mag_l1.data());

    // --- Step 5b: Magnitude L2 = sqrt(Gx^2 + Gy^2) ---
    detector.computeMagnitudeL2(gx.data(), gy.data(), mag_l2.data());

    // --- Step 6: Gradient Direction (0=0deg, 1=45, 2=90, 3=135) ---
    detector.computeDirection(gx.data(), gy.data(), direction.data());

    // --- Step 7: Output L2 magnitude (best quality) to stdout ---
    // Change mag_l2 to mag_l1 to compare outputs
    std::fwrite(mag_l2.data(), 1, N, stdout);

    return 0;
}