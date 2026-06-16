#include "image_types.hpp"
#include "canny.hpp"
#include "nms.h"
#include "double_threshold.h"
#include "hysteresis.h"
#include <cstdio>
#include <cstdint>
#include <vector>
#include <algorithm>
#include <time.h>      // Standard C time library
#include <iostream>

using namespace std;

// clock_gettime(CLOCK_MONOTONIC) measures real elapsed wall-clock time. For profiling under Linux/QEMU
// CLOCK_MONOTONIC is preferred because it captures the actual elapsed runtime of each stage and is not affected by changes to the system clock.
// for example if the cpu sleeps during the process clock gettime gets the sleep time but clock() doesnot 

static double now_ms() {
struct timespec ts;
clock_gettime(CLOCK_MONOTONIC, &ts);
return (ts.tv_sec * 1000.0) + (ts.tv_nsec / 1000000.0);
}

int main() {
const int W = 512;
const int H = 512;
const int N = W * H;
const int ITERATIONS = 100; // Total runs per stage to stabilize QEMU measurements

```
Image tiger(W, H);
Image blurred(W, H);

// Gx and Gy SEPARATE (Structure of Arrays) - better for RVV vectorization
vector<int16_t>       gx(N), gy(N);

// uint8_t magnitudes for display/stdout output
vector<unsigned char> mag_l1(N), mag_l2(N);

// uint16_t L2 magnitude for NMS (must NOT clamp to 255 before NMS)
vector<uint16_t>      mag_l2_u16(N);

// Direction: 0=0deg, 1=45deg, 2=90deg, 3=135deg
vector<unsigned char> direction(N);

// NMS output (uint16_t - local maxima of mag_l2_u16)
vector<uint16_t>      nms_out(N);

// Double threshold output (uint8_t: 255=strong, 128=weak, 0=none)
vector<unsigned char> dt_out(N);

// Temporary copy buffer to safely profile Hysteresis loops
vector<unsigned char> dt_out_temp(N);

// Step 1: Read raw grayscale from stdin
(void)fread(tiger.data.data(), 1, N, stdin);

// Step 2: Initialize detector
CannyEdgeDetector detector(W, H);

double start_time, end_time;
double elapsed_ms;

cerr << "--- Starting Phase 4 Performance Sweeps (" << ITERATIONS << " iterations) ---\n";

// ==========================================
// Step 3: Gaussian Blur (5x5, weight=273, zero-padding)
// ==========================================
start_time = now_ms();
for (int i = 0; i < ITERATIONS; ++i) {
    detector.applyGaussianBlur(tiger.data.data(), blurred.data.data());
}
end_time = now_ms();

elapsed_ms = end_time - start_time;
cerr << "Gaussian 5x5 Average Time : " << (elapsed_ms / ITERATIONS) << " ms\n";

// ==========================================
// Step 4: Sobel -> separate Gx and Gy
// ==========================================
start_time = now_ms();
for (int i = 0; i < ITERATIONS; ++i) {
    detector.applySobel(blurred.data.data(), gx.data(), gy.data());
}
end_time = now_ms();

elapsed_ms = end_time - start_time;
cerr << "Sobel Gx/Gy Average Time   : " << (elapsed_ms / ITERATIONS) << " ms\n";

// ==========================================
// Step 5: Gradient Magnitude (L2 Norm Evaluated)
// ==========================================
detector.computeMagnitudeL1(gx.data(), gy.data(), mag_l1.data());

start_time = now_ms();
for (int i = 0; i < ITERATIONS; ++i) {
    detector.computeMagnitudeL2(gx.data(), gy.data(), mag_l2.data());
}
end_time = now_ms();

elapsed_ms = end_time - start_time;
cerr << "Magnitude Average Time     : " << (elapsed_ms / ITERATIONS) << " ms\n";

// Step 5c: Convert L2 to uint16_t for NMS (preserves full range ~0-1441)
for (int i = 0; i < N; ++i)
    mag_l2_u16[i] = static_cast<uint16_t>(mag_l2[i]);

// ==========================================
// Step 6: Gradient direction
// ==========================================
start_time = now_ms();
for (int i = 0; i < ITERATIONS; ++i) {
    detector.computeDirection(gx.data(), gy.data(), direction.data());
}
end_time = now_ms();

elapsed_ms = end_time - start_time;
cerr << "Direction Average Time     : " << (elapsed_ms / ITERATIONS) << " ms\n";

// ==========================================
// Step 7: Non-Maximum Suppression (thins edges to 1-pixel width)
// ==========================================
start_time = now_ms();
for (int i = 0; i < ITERATIONS; ++i) {
    non_maximum_suppression(mag_l2_u16.data(), direction.data(), nms_out.data(), W, H);
}
end_time = now_ms();

elapsed_ms = end_time - start_time;
cerr << "NMS Average Time           : " << (elapsed_ms / ITERATIONS) << " ms\n";

// ==========================================
// Step 8: Double Threshold
// ==========================================
start_time = now_ms();
for (int i = 0; i < ITERATIONS; ++i) {
    double_threshold(nms_out.data(), dt_out.data(), N, /*low=*/10, /*high=*/30);
}
end_time = now_ms();

elapsed_ms = end_time - start_time;
cerr << "Double Threshold Avg Time  : " << (elapsed_ms / ITERATIONS) << " ms\n";

// saves doublethresholding before used in hysteresis
FILE* g = fopen("/tmp/dt_out.raw", "wb");
if (g) { fwrite(dt_out.data(), 1, N, g); fclose(g); }

// ==========================================
// Step 9: Hysteresis (Profiles copies to prevent in-place data pollution)
// ==========================================
start_time = now_ms();
for (int i = 0; i < ITERATIONS; ++i) {
    dt_out_temp = dt_out; // Fresh copy configuration for accurate looping
    hysteresis(dt_out_temp.data(), W, H);
}
end_time = now_ms();

elapsed_ms = end_time - start_time;
cerr << "Hysteresis Average Time    : " << (elapsed_ms / ITERATIONS) << " ms\n";

// Final in-place run to finalize data for downstream output
hysteresis(dt_out.data(), W, H);

cerr << "---------------------------------------------------------\n";

// Step 10: Write outputs to stdout (L2 | L1 | NMS | double threshold)
fwrite(mag_l2.data(),  1, N, stdout);
fwrite(mag_l1.data(),  1, N, stdout);
fwrite(nms_out.data(), 2, N, stdout);   // uint16_t -> 2 bytes per pixel
fwrite(dt_out.data(),  1, N, stdout);

// Step 11: Save NMS (normalized) and double threshold to /tmp for viewing
vector<unsigned char> nms_u8(N);
uint16_t max_val = *max_element(nms_out.begin(), nms_out.end());
if (max_val == 0) max_val = 1;
for (int i = 0; i < N; ++i)
    nms_u8[i] = static_cast<unsigned char>((uint32_t)nms_out[i] * 255 / max_val);

FILE* f = fopen("/tmp/nms_out.raw", "wb");
if (f) { fwrite(nms_u8.data(), 1, N, f); fclose(f); }

FILE* h = fopen("/tmp/hysteresis_out.raw", "wb");
if (h) { fwrite(dt_out.data(), 1, N, h); fclose(h); }

return 0;
```

}
