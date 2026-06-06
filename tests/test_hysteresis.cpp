#include <gtest/gtest.h>
#include <cstdint>
#include <vector>
#include "hysteresis.h"

static const uint8_t S = 255;   // STRONG
static const uint8_t W = 128;   // WEAK
static const uint8_t N =   0;   // NONE

// Helper: run hysteresis on a flat array, return result as vector
static std::vector<uint8_t> run(std::vector<uint8_t> input, int w, int h)
{
    hysteresis(input.data(), w, h);
    return input;
}

// ── Test 1: isolated WEAK pixel → removed ────────────────────────────────────
// A weak pixel with no strong neighbours must become 0
TEST(Hysteresis, IsolatedWeakRemoved) {
    // 3x3 grid, center is WEAK, all neighbours are NONE
    std::vector<uint8_t> grid = {
        N, N, N,
        N, W, N,
        N, N, N
    };
    auto out = run(grid, 3, 3);
    EXPECT_EQ(out[4], 0);   // center pixel must be killed
}

// ── Test 2: WEAK connected to STRONG → kept ──────────────────────────────────
TEST(Hysteresis, WeakConnectedToStrongKept) {
    // 3x3 grid: top-left is STRONG, center is WEAK (adjacent)
    std::vector<uint8_t> grid = {
        S, N, N,
        N, W, N,
        N, N, N
    };
    auto out = run(grid, 3, 3);
    EXPECT_EQ(out[4], 255);   // center promoted to STRONG
}

// ── Test 3: chain of WEAKs connected to one STRONG → all kept ────────────────
// STRONG → WEAK → WEAK → WEAK (all in a row)
TEST(Hysteresis, ChainOfWeaksAllPromoted) {
    std::vector<uint8_t> grid = {S, W, W, W, N};
    auto out = run(grid, 5, 1);
    EXPECT_EQ(out[0], 255);
    EXPECT_EQ(out[1], 255);
    EXPECT_EQ(out[2], 255);
    EXPECT_EQ(out[3], 255);
    EXPECT_EQ(out[4], 0);     // NONE stays NONE
}

// ── Test 4: STRONG pixels are never removed ───────────────────────────────────
TEST(Hysteresis, StrongPixelsPreserved) {
    std::vector<uint8_t> grid = {S, N, S, N, S};
    auto out = run(grid, 5, 1);
    EXPECT_EQ(out[0], 255);
    EXPECT_EQ(out[2], 255);
    EXPECT_EQ(out[4], 255);
}

// ── Test 5: all NONE input → all NONE output ─────────────────────────────────
TEST(Hysteresis, AllNoneStaysNone) {
    std::vector<uint8_t> grid(25, N);
    auto out = run(grid, 5, 5);
    for (auto v : out) EXPECT_EQ(v, 0);
}

// ── Test 6: all STRONG input → all STRONG output ────────────────────────────
TEST(Hysteresis, AllStrongStaysStrong) {
    std::vector<uint8_t> grid(25, S);
    auto out = run(grid, 5, 5);
    for (auto v : out) EXPECT_EQ(v, 255);
}

// ── Test 7: all WEAK with no STRONG → all removed ────────────────────────────
TEST(Hysteresis, AllWeakNoStrongAllRemoved) {
    std::vector<uint8_t> grid(25, W);
    auto out = run(grid, 5, 5);
    for (auto v : out) EXPECT_EQ(v, 0);
}

// ── Test 8: diagonal connection works (8-connectivity) ───────────────────────
TEST(Hysteresis, DiagonalConnectionWorks) {
    // STRONG at top-left, WEAK at center (diagonal neighbour)
    std::vector<uint8_t> grid = {
        S, N, N,
        N, W, N,
        N, N, N
    };
    auto out = run(grid, 3, 3);
    EXPECT_EQ(out[4], 255);   // diagonal weak must be promoted
}

// ── Test 9: output contains only 0 or 255 (no 128 remaining) ─────────────────
TEST(Hysteresis, NoWeakValuesInOutput) {
    std::vector<uint8_t> grid = {S, W, N, W, W, N, S, W, W};
    auto out = run(grid, 3, 3);
    for (auto v : out)
        EXPECT_TRUE(v == 0 || v == 255)
            << "found leftover WEAK value 128 in output";
}

// ── Test 10: large 512x512 buffer does not crash ──────────────────────────────
TEST(Hysteresis, LargeBufferNoCrash) {
    const int WW = 512, HH = 512;
    std::vector<uint8_t> grid(WW * HH, 0);
    // Put a STRONG pixel in the middle surrounded by WEAKs
    grid[HH/2 * WW + WW/2]     = S;
    grid[HH/2 * WW + WW/2 + 1] = W;
    grid[HH/2 * WW + WW/2 - 1] = W;
    EXPECT_NO_THROW(hysteresis(grid.data(), WW, HH));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
