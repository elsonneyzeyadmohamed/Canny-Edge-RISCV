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
#include <time.h>
#include <iostream>

using namespace std;

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

static double now_ms()
{
    struct timespec ts;
    long ret = linux_clock_gettime(CLOCK_MONOTONIC, &ts);

    if (ret != 0) {
        cerr << "clock_gettime syscall failed\n";
        return 0.0;
    }

    return (ts.tv_sec * 1000.0) + (ts.tv_nsec / 1000000.0);
}

#ifndef IMG_W
#define IMG_W 512
#endif

#ifndef IMG_H
#define IMG_H 512
#endif

int main()
{
    const int W = IMG_W;
    const int H = IMG_H;
    const int N = W * H;

    const int ITERATIONS = 100;

    Image tiger(W, H);
    Image blurred(W, H);

    vector<int16_t> gx(N), gy(N);

    vector<unsigned char> mag_l1(N);
    vector<unsigned char> mag_l2(N);

    vector<uint16_t> mag_l2_u16(N);

    vector<unsigned char> direction(N);

    vector<uint16_t> nms_out(N);

    vector<unsigned char> dt_out(N);
    vector<unsigned char> dt_before_hysteresis(N);
    vector<unsigned char> dt_out_temp(N);

    size_t bytes_read = fread(tiger.data.data(), 1, N, stdin);

    if (bytes_read != static_cast<size_t>(N)) {
        cerr << "Warning: Expected " << N << " bytes, but read "
             << bytes_read << " bytes from stdin.\n";
    }

    CannyEdgeDetector detector(W, H);

    double start_time, end_time;
    double elapsed_ms;

    double gaussian_ms = 0.0;
    double sobel_ms = 0.0;
    double magnitude_l1_ms = 0.0;
    double magnitude_l2_ms = 0.0;
    double direction_ms = 0.0;
    double nms_ms = 0.0;
    double double_threshold_ms = 0.0;
    double hysteresis_ms = 0.0;

    cerr << "--- Starting Phase 4 Performance Sweeps ("
         << ITERATIONS << " iterations) ---\n";

    // ========================================================================
    // Step 3: Gaussian Blur
    // ========================================================================

    start_time = now_ms();

    for (int i = 0; i < ITERATIONS; ++i) {
#ifdef USE_RVV_GAUSSIAN
        applyGaussianBlur_rvv(
            tiger.data.data(),
            blurred.data.data(),
            W,
            H
        );
#else
        detector.applyGaussianBlur(
            tiger.data.data(),
            blurred.data.data()
        );
#endif
    }

    end_time = now_ms();
    elapsed_ms = end_time - start_time;
    gaussian_ms = elapsed_ms / ITERATIONS;

    cerr << "Gaussian 5x5 Average Time : " << gaussian_ms << " ms\n";

    // ========================================================================
    // Step 4: Sobel Gx/Gy
    // ========================================================================

    start_time = now_ms();

    for (int i = 0; i < ITERATIONS; ++i) {
        detector.applySobel(
            blurred.data.data(),
            gx.data(),
            gy.data()
        );
    }

    end_time = now_ms();
    elapsed_ms = end_time - start_time;
    sobel_ms = elapsed_ms / ITERATIONS;

    cerr << "Sobel Gx/Gy Average Time   : " << sobel_ms << " ms\n";

    // ========================================================================
    // Step 5a: Magnitude L1
    // ========================================================================

    start_time = now_ms();

    for (int i = 0; i < ITERATIONS; ++i) {
#ifdef RVV_KERNELS_H
        computeMagnitudeL1_rvv(
            gx.data(),
            gy.data(),
            mag_l1.data(),
            N
        );
#else
        detector.computeMagnitudeL1(
            gx.data(),
            gy.data(),
            mag_l1.data()
        );
#endif
    }

    end_time = now_ms();
    elapsed_ms = end_time - start_time;
    magnitude_l1_ms = elapsed_ms / ITERATIONS;

    cerr << "Magnitude L1 Average Time  : " << magnitude_l1_ms << " ms\n";

    // ========================================================================
    // Step 5b: Magnitude L2
    // ========================================================================

    start_time = now_ms();

    for (int i = 0; i < ITERATIONS; ++i) {
#ifdef USE_RVV_L2
        computeMagnitudeL2_rvv(
            gx.data(),
            gy.data(),
            mag_l2.data(),
            N
        );
#else
        detector.computeMagnitudeL2(
            gx.data(),
            gy.data(),
            mag_l2.data()
        );
#endif
    }

    end_time = now_ms();
    elapsed_ms = end_time - start_time;
    magnitude_l2_ms = elapsed_ms / ITERATIONS;

    cerr << "Magnitude L2 Average Time  : " << magnitude_l2_ms << " ms\n";

    for (int i = 0; i < N; ++i) {
        mag_l2_u16[i] = static_cast<uint16_t>(mag_l2[i]);
    }

    // ========================================================================
    // Step 6: Direction
    // ========================================================================

    start_time = now_ms();

    for (int i = 0; i < ITERATIONS; ++i) {
#ifdef USE_MANUAL_RVV
        computeDirection_rvv(
            gx.data(),
            gy.data(),
            direction.data(),
            N
        );
#else
        detector.computeDirection(
            gx.data(),
            gy.data(),
            direction.data()
        );
#endif
    }

    end_time = now_ms();
    elapsed_ms = end_time - start_time;
    direction_ms = elapsed_ms / ITERATIONS;

    cerr << "Direction Average Time     : " << direction_ms << " ms\n";

    // ========================================================================
    // Step 7: Non-Maximum Suppression
    // ========================================================================

    start_time = now_ms();

    for (int i = 0; i < ITERATIONS; ++i) {
        non_maximum_suppression(
            mag_l2_u16.data(),
            direction.data(),
            nms_out.data(),
            W,
            H
        );
    }

    end_time = now_ms();
    elapsed_ms = end_time - start_time;
    nms_ms = elapsed_ms / ITERATIONS;

    cerr << "NMS Average Time           : " << nms_ms << " ms\n";

    // ========================================================================
    // Step 8: Double Threshold
    // ========================================================================

    start_time = now_ms();

    for (int i = 0; i < ITERATIONS; ++i) {
        double_threshold(
            nms_out.data(),
            dt_out.data(),
            N,
            10,
            30
        );
    }

    end_time = now_ms();
    elapsed_ms = end_time - start_time;
    double_threshold_ms = elapsed_ms / ITERATIONS;

    cerr << "Double Threshold Avg Time  : " << double_threshold_ms << " ms\n";

    std::copy(
        dt_out.begin(),
        dt_out.end(),
        dt_before_hysteresis.begin()
    );

    FILE* dt_file = fopen("/tmp/dt_out.raw", "wb");
    if (dt_file) {
        fwrite(dt_before_hysteresis.data(), 1, N, dt_file);
        fclose(dt_file);
    }

    // ========================================================================
    // Step 9: Hysteresis
    // ========================================================================

    start_time = now_ms();

    for (int i = 0; i < ITERATIONS; ++i) {
        std::copy(
            dt_before_hysteresis.begin(),
            dt_before_hysteresis.end(),
            dt_out_temp.begin()
        );

        hysteresis(
            dt_out_temp.data(),
            W,
            H
        );
    }

    end_time = now_ms();
    elapsed_ms = end_time - start_time;
    hysteresis_ms = elapsed_ms / ITERATIONS;

    cerr << "Hysteresis Average Time    : " << hysteresis_ms << " ms\n";

    std::copy(
        dt_before_hysteresis.begin(),
        dt_before_hysteresis.end(),
        dt_out.begin()
    );

    hysteresis(
        dt_out.data(),
        W,
        H
    );

    // ========================================================================
    // Phase 5 Hotspot Identification
    // ========================================================================

    cerr << "---------------------------------------------------------\n";

    double total_ms =
        gaussian_ms +
        sobel_ms +
        magnitude_l2_ms +
        direction_ms +
        nms_ms +
        double_threshold_ms +
        hysteresis_ms;

    cerr << "\n--- Phase 5: Hotspot Identification ---\n";

    if (total_ms > 0.0) {
        cerr << "Gaussian  : " << (gaussian_ms / total_ms) * 100.0 << "%\n";
        cerr << "Sobel     : " << (sobel_ms / total_ms) * 100.0 << "%\n";
        cerr << "Mag L2    : " << (magnitude_l2_ms / total_ms) * 100.0 << "%\n";
        cerr << "Direction : " << (direction_ms / total_ms) * 100.0 << "%\n";
        cerr << "NMS       : " << (nms_ms / total_ms) * 100.0 << "%\n";
        cerr << "D.Thresh  : " << (double_threshold_ms / total_ms) * 100.0 << "%\n";
        cerr << "Hysteresis: " << (hysteresis_ms / total_ms) * 100.0 << "%\n";
        cerr << "Total avg : " << total_ms << " ms per iteration\n";
    } else {
        cerr << "Total avg : 0 ms per iteration\n";
    }

    // ========================================================================
    // Save helper raw files to /tmp
    // ========================================================================

    vector<unsigned char> nms_u8(N);

    uint16_t max_val = *max_element(
        nms_out.begin(),
        nms_out.end()
    );

    if (max_val == 0) {
        max_val = 1;
    }

    for (int i = 0; i < N; ++i) {
        nms_u8[i] = static_cast<unsigned char>(
            (static_cast<uint32_t>(nms_out[i]) * 255) / max_val
        );
    }

    FILE* nms_file = fopen("/tmp/nms_out.raw", "wb");
    if (nms_file) {
        fwrite(nms_u8.data(), 1, N, nms_file);
        fclose(nms_file);
    }

    FILE* hyst_file = fopen("/tmp/hysteresis_out.raw", "wb");
    if (hyst_file) {
        fwrite(dt_out.data(), 1, N, hyst_file);
        fclose(hyst_file);
    }

    // ========================================================================
    // Final raw output to stdout
    // ========================================================================
    // Layout:
    // gaussian          : N bytes
    // mag_l2            : N bytes
    // mag_l1            : N bytes
    // direction         : N bytes
    // nms_out           : 2N bytes
    // double_threshold  : N bytes
    // final_hysteresis  : N bytes
    //
    // Total output = 8N bytes.

    fwrite(blurred.data.data(), 1, N, stdout);
    fwrite(mag_l2.data(), 1, N, stdout);
    fwrite(mag_l1.data(), 1, N, stdout);
    fwrite(direction.data(), 1, N, stdout);
    fwrite(nms_out.data(), sizeof(uint16_t), N, stdout);
    fwrite(dt_before_hysteresis.data(), 1, N, stdout);
    fwrite(dt_out.data(), 1, N, stdout);

    return 0;
}
