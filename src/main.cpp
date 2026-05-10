#include "image_types.hpp"
#include "canny.hpp"
#include <cstdio>
#include <cstdint>
#include <vector>
#include "nms.h"
#include <algorithm>

using namespace std;

int main() {
    const int W = 512;
    const int H = 512;
    const int N = W * H;

    Image tiger(W, H);
    Image blurred(W, H);

    // Gx and Gy SEPARATE (Structure of Arrays) - better for RVV vectorization
    vector<int16_t> gx(N), gy(N);

    // Two magnitude methods for comparison
    vector<unsigned char> mag_l1(N), mag_l2(N);

    // Separate uint16_t mag for NMS (needs full precision, not clamped to 255)
    vector<uint16_t> mag_l2_u16(N);
    
    // Direction: 0=0deg, 1=45deg, 2=90deg, 3=135deg
    vector<unsigned char> direction(N);


    // NMS output
    vector<uint16_t> nms_out(N);


    // Step 1: Read raw grayscale from stdin
    (void)fread(tiger.data.data(), 1, N, stdin);

    // Step 2: Initialize detector
    CannyEdgeDetector detector(W, H);

    // Step 3: Gaussian Blur (5x5, weight=273, zero-padding)
    detector.applyGaussianBlur(tiger.data.data(), blurred.data.data());

    // Step 4: Sobel -> separate Gx and Gy
    detector.applySobel(blurred.data.data(), gx.data(), gy.data());

    // Step 5a: L1 magnitude = |Gx| + |Gy|
    detector.computeMagnitudeL1(gx.data(), gy.data(), mag_l1.data());

    // Step 5b: L2 magnitude = sqrt(Gx^2 + Gy^2)
    detector.computeMagnitudeL2(gx.data(), gy.data(), mag_l2.data());

     // Convert to uint16_t for NMS (avoids type mismatch)
    for (int i = 0; i < N; ++i)
    mag_l2_u16[i] = static_cast<uint16_t>(mag_l2[i]);

    // Step 6: Gradient direction
    detector.computeDirection(gx.data(), gy.data(), direction.data());


    // Step 7: Non-Maximum Suppression (uses L2 magnitude)
    non_maximum_suppression(mag_l2_u16.data(), direction.data(), nms_out.data(), W, H);

/*
    // Step 8: Output L2 then L1 to stdout
    fwrite(mag_l2.data(), 1, N, stdout);
    fwrite(mag_l1.data(), 1, N, stdout);
*/

// Step 8: Output L2 then L1 then NMS to stdout
    fwrite(mag_l2.data(),  2, N, stdout);   // 2 bytes per pixel (uint16_t)
    fwrite(mag_l1.data(),  2, N, stdout);
    fwrite(nms_out.data(), 2, N, stdout);
/*
    // Step 9: Save L1 to file for visual comparison with L2
    // L1 is faster (no sqrt) but overestimates diagonal edges
    FILE* f = fopen("/tmp/mag_l1.raw", "wb");
    if (f) {
        fwrite(mag_l1.data(), 1, N, f);
        fclose(f);
    }
*/

 // Step 9: Save NMS output to file for visual inspection
/*    FILE* f = fopen("/tmp/nms_out.raw", "wb");
    if (f) {
        fwrite(nms_out.data(), 2, N, f);
        fclose(f);
    }
*/

// Step 9: Normalize NMS to uint8_t and save for visual inspection
vector<unsigned char> nms_u8(N);
uint16_t max_val = *max_element(nms_out.begin(), nms_out.end());
if (max_val == 0) max_val = 1;
for (int i = 0; i < N; ++i)
    nms_u8[i] = static_cast<unsigned char>((uint32_t)nms_out[i] * 255 / max_val);

FILE* f = fopen("/tmp/nms_out.raw", "wb");
if (f) {
    fwrite(nms_u8.data(), 1, N, f);   // 1 byte per pixel now
    fclose(f);
}

    return 0;
}
