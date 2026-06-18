#define _POSIX_C_SOURCE 199309L
#include "rvv_kernels.h"
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
// as a result of using baremetal compiler we are trying to get the clock_gettime function in a lower level way 

#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 1
#endif

static long linux_clock_gettime(long clock_id, struct timespec* ts)
{
register long a0 asm("a0") = clock_id;
register long a1 asm("a1") = reinterpret_cast<long>(ts);
register long a7 asm("a7") = 113;

    
asm volatile (
    "ecall"
    : "+r"(a0)
    : "r"(a1), "r"(a7)
    : "memory"
);

return a0;

}

static double now_ms() {
struct timespec ts;
long ret = linux_clock_gettime(CLOCK_MONOTONIC, &ts);


if (ret != 0) {
    cerr << "clock_gettime syscall failed\n";
    return 0.0;
}

return (ts.tv_sec * 1000.0) + (ts.tv_nsec / 1000000.0);

}
// get time monotonic way is finished here 

int main() {
const int W = 512;
const int H = 512;
const int N = W * H;
const int ITERATIONS = 100; // Total runs per stage to stabilize QEMU measurements


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
size_t bytes_read = fread(tiger.data.data(), 1, N, stdin);
if (bytes_read != static_cast<size_t>(N)) {
    cerr << "Warning: Expected " << N << " bytes, but read "
         << bytes_read << " bytes from stdin.\n";
}

// Step 2: Initialize detector
CannyEdgeDetector detector(W, H);

double start_time, end_time;
double elapsed_ms;

double gaussian_ms = 0.0;
double sobel_ms = 0.0;
double magnitude_ms = 0.0;
double direction_ms = 0.0;
double nms_ms = 0.0;
double double_threshold_ms = 0.0;
double hysteresis_ms = 0.0;

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
gaussian_ms = elapsed_ms / ITERATIONS;
cerr << "Gaussian 5x5 Average Time : " << gaussian_ms << " ms\n";

// ==========================================
// Step 4: Sobel -> separate Gx and Gy
// ==========================================
start_time = now_ms();
for (int i = 0; i < ITERATIONS; ++i) {
    detector.applySobel(blurred.data.data(), gx.data(), gy.data());
}
end_time = now_ms();

elapsed_ms = end_time - start_time;
sobel_ms = elapsed_ms / ITERATIONS;
cerr << "Sobel Gx/Gy Average Time   : " << sobel_ms << " ms\n";


// magnitude L1 generates absolute gx and gy instead of root of squares 
// coded to compare only but not used in path 

detector.computeMagnitudeL1(gx.data(), gy.data(), mag_l1.data());



// ==========================================
// Step 5: Gradient Magnitude (L2 Norm Evaluated) root of squares is used 
// ==========================================


start_time = now_ms();
for (int i = 0; i < ITERATIONS; ++i) {

#ifdef USE_RVV_L2
    computeMagnitudeL2_rvv(gx.data(), gy.data(), mag_l2.data(), N);
#else
    detector.computeMagnitudeL2(gx.data(), gy.data(), mag_l2.data());
#endif
}
end_time = now_ms();

elapsed_ms = end_time - start_time;
magnitude_ms = elapsed_ms / ITERATIONS;
cerr << "Magnitude Average Time     : " << magnitude_ms << " ms\n";

// Step 5c: Convert L2 to uint16_t for NMS (preserves full range ~0-1441)
for (int i = 0; i < N; ++i)
    mag_l2_u16[i] = static_cast<uint16_t>(mag_l2[i]);

// ==========================================
// Step 6: Gradient direction
// ==========================================
start_time = now_ms();
for (int i = 0; i < ITERATIONS; ++i) {

#ifdef USE_MANUAL_RVV
    computeDirection_rvv(gx.data(), gy.data(), direction.data(), N);
#else

    detector.computeDirection(gx.data(), gy.data(), direction.data());
#endif
}
end_time = now_ms();

elapsed_ms = end_time - start_time;
direction_ms = elapsed_ms / ITERATIONS;
cerr << "Direction Average Time     : " << direction_ms << " ms\n";

// ==========================================
// Step 7: Non-Maximum Suppression (thins edges to 1-pixel width)
// ==========================================
start_time = now_ms();
for (int i = 0; i < ITERATIONS; ++i) {
    non_maximum_suppression(mag_l2_u16.data(), direction.data(), nms_out.data(), W, H);
}
end_time = now_ms();

elapsed_ms = end_time - start_time;
nms_ms = elapsed_ms / ITERATIONS;
cerr << "NMS Average Time           : " << nms_ms << " ms\n";

// ==========================================
// Step 8: Double Threshold
// ==========================================
start_time = now_ms();
for (int i = 0; i < ITERATIONS; ++i) {
    double_threshold(nms_out.data(), dt_out.data(), N, 10, 30);
}
end_time = now_ms();

elapsed_ms = end_time - start_time;
double_threshold_ms = elapsed_ms / ITERATIONS;
cerr << "Double Threshold Avg Time  : " << double_threshold_ms << " ms\n";

// saves doublethresholding before used in hysteresis
FILE* g = fopen("/tmp/dt_out.raw", "wb");
if (g) { fwrite(dt_out.data(), 1, N, g); fclose(g); }

// ==========================================
// Step 9: Hysteresis (Profiles copies to prevent in-place data pollution)
// ==========================================
start_time = now_ms();
for (int i = 0; i < ITERATIONS; ++i) {
    std::copy(dt_out.begin(), dt_out.end(), dt_out_temp.begin());
    hysteresis(dt_out_temp.data(), W, H);
}
end_time = now_ms();

elapsed_ms = end_time - start_time;
hysteresis_ms = elapsed_ms / ITERATIONS;
cerr << "Hysteresis Average Time    : " << hysteresis_ms << " ms\n";

// Final in-place run to finalize data for downstream output
hysteresis(dt_out.data(), W, H);

cerr << "---------------------------------------------------------\n";

double total_ms =
    gaussian_ms +
    sobel_ms +
    magnitude_ms +
    direction_ms +
    nms_ms +
    double_threshold_ms +
    hysteresis_ms;

cerr << "\n--- Phase 5: Hotspot Identification ---\n";

if (total_ms > 0.0) {
    cerr << "Gaussian  : " << (gaussian_ms / total_ms) * 100.0 << "%\n";
    cerr << "Sobel     : " << (sobel_ms / total_ms) * 100.0 << "%\n";
    cerr << "Magnitude : " << (magnitude_ms / total_ms) * 100.0 << "%\n";
    cerr << "Direction : " << (direction_ms / total_ms) * 100.0 << "%\n";
    cerr << "NMS       : " << (nms_ms / total_ms) * 100.0 << "%\n";
    cerr << "D.Thresh  : " << (double_threshold_ms / total_ms) * 100.0 << "%\n";
    cerr << "Hysteresis: " << (hysteresis_ms / total_ms) * 100.0 << "%\n";
    cerr << "Total avg : " << total_ms << " ms per iteration\n";
} else {
    cerr << "Total avg : 0 ms per iteration\n";
}

// Step 10: Write outputs to stdout (L2 | L1 | NMS | double threshold)
fwrite(mag_l2.data(),  1, N, stdout);
fwrite(mag_l1.data(),  1, N, stdout);
fwrite(nms_out.data(), sizeof(uint16_t), N, stdout);
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

}
