#include <gtest/gtest.h>
#include <cstdint>
#include <vector>
#include "double_threshold.h"

// ── Helpers ──────────────────────────────────────────────────────────────────
// It takes a high-precision 16-bit input array, automatically allocates an 8-bit output array of matching size, passes raw pointers (.data()) to your underlying C-style implementation, and returns the result.
static std::vector<uint8_t> run(const std::vector<uint16_t>& input,
                                uint16_t low, uint16_t high)
{
    std::vector<uint8_t> out(input.size());
    double_threshold(input.data(), out.data(),
                     static_cast<int>(input.size()), low, high);
    return out;
}

// These tests ensure your code accurately splits pixel data into the three core categories required for Canny Edge Detection
// ── Test 1: pixels above high_thresh → STRONG (255) ─────────────────────────
TEST(DoubleThreshold, StrongPixel) {
    std::vector<uint16_t> input = {250};
    auto out = run(input, 80, 200);
    EXPECT_EQ(out[0], 255);
}

// ── Test 2: pixels between low and high → WEAK (128) ────────────────────────
TEST(DoubleThreshold, WeakPixel) {
    std::vector<uint16_t> input = {120};
    auto out = run(input, 80, 200);
    EXPECT_EQ(out[0], 128);
}

// ── Test 3: pixels below low_thresh → NO EDGE (0) ───────────────────────────
TEST(DoubleThreshold, NoEdge) {
    std::vector<uint16_t> input = {30};
    auto out = run(input, 80, 200);
    EXPECT_EQ(out[0], 0);
}

// ── Test 4: pixel exactly equal to high_thresh → STRONG ─────────────────────
TEST(DoubleThreshold, ExactlyHighThresh) {
    std::vector<uint16_t> input = {200};
    auto out = run(input, 80, 200);
    EXPECT_EQ(out[0], 255);
}

// ── Test 5: pixel exactly equal to low_thresh → WEAK ────────────────────────
TEST(DoubleThreshold, ExactlyLowThresh) {
    std::vector<uint16_t> input = {80};
    auto out = run(input, 80, 200);
    EXPECT_EQ(out[0], 128);
}

// ── Test 6: pixel one below low_thresh → NO EDGE ────────────────────────────
TEST(DoubleThreshold, OneBelowLowThresh) {
    std::vector<uint16_t> input = {79};
    auto out = run(input, 80, 200);
    EXPECT_EQ(out[0], 0);
}

//These ensure your loops don't cause buffer overflows, handle large image streams, and behave correctly across diverse data arrays
// ── Test 7: all-zero input → all NO EDGE ────────────────────────────────────
TEST(DoubleThreshold, AllZeroInput) {
    std::vector<uint16_t> input(100, 0);
    auto out = run(input, 80, 200);
    for (int i = 0; i < 100; ++i)
        EXPECT_EQ(out[i], 0) << "failed at index " << i;
}


// ── Test 8: mixed input → correct classification for each pixel ──────────────
TEST(DoubleThreshold, MixedInput) {
    std::vector<uint16_t> input  = { 30,  80, 150, 200, 250,   0, 201,  79};
    std::vector<uint8_t>  expect = {  0, 128, 128, 255, 255,   0, 255,   0};

    auto out = run(input, 80, 200);
    for (size_t i = 0; i < input.size(); ++i)
        EXPECT_EQ(out[i], expect[i]) << "failed at index " << i
                                     << " input=" << input[i];
}

//These tests enforce the boundaries of your arithmetic types and confirm that values match expectations for subsequent stages

// ── Test 9: max uint16_t value → STRONG ──────────────────────────────────────
TEST(DoubleThreshold, MaxUint16IsStrong) {
    std::vector<uint16_t> input = {65535};
    auto out = run(input, 80, 200);
    EXPECT_EQ(out[0], 255);
}

// ── Test 10: output values are only 0, 128, or 255 (no other values) ─────────
TEST(DoubleThreshold, OutputOnlyValidValues) {
    // Sweep all possible uint16_t values (sample every 7 to keep test fast)
    for (uint32_t v = 0; v <= 65535; v += 7) {
        std::vector<uint16_t> input = {static_cast<uint16_t>(v)};
        auto out = run(input, 80, 200);
        EXPECT_TRUE(out[0] == 0 || out[0] == 128 || out[0] == 255)
            << "unexpected output " << (int)out[0]
            << " for input " << v;
    }
}

// ── Test 11: large buffer (512x512) runs without crash ───────────────────────
TEST(DoubleThreshold, LargeBuffer) {
    const int N = 512 * 512;
    std::vector<uint16_t> input(N);
    for (int i = 0; i < N; ++i)
        input[i] = static_cast<uint16_t>(i % 300);   // values 0–299

    std::vector<uint8_t> out(N);
    EXPECT_NO_THROW(
        double_threshold(input.data(), out.data(), N, 80, 200)
    );
}

// ── Test 12: WEAK_PIXEL value is 128 (matches hysteresis expectation) ────────
TEST(DoubleThreshold, WeakValueIs128) {
    std::vector<uint16_t> input = {100};   // between low=80 and high=200
    auto out = run(input, 80, 200);
    EXPECT_EQ(out[0], 128)
        << "WEAK must be 128 so hysteresis can find it";
}
//This is the standard entry function for an executable built with GoogleTest.
//returns 0 if all test cases pass, or 1 if any fail.
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
