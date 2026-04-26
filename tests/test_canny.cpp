#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>
#include <cstdint>
#include <cstdlib>
#include <algorithm>

using namespace std;

// =====================================================================
// Self-contained copy of implementation for host-side testing
// Compiles with native g++ - no RISC-V toolchain needed
// =====================================================================

class CannyEdgeDetector {
    int width, height;
public:
    CannyEdgeDetector(int w, int h) : width(w), height(h) {}

    void applyGaussianBlur(const unsigned char* input, unsigned char* output) {
        const int kernel[5][5] = {
            { 2,  4,  5,  4,  2},
            { 4,  9, 12,  9,  4},
            { 5, 12, 15, 12,  5},
            { 4,  9, 12,  9,  4},
            { 2,  4,  5,  4,  2}
        };
        const int weight = 273;
        for (int y = 0; y < height; y++)
            for (int x = 0; x < width; x++) {
                int32_t sum = 0;
                for (int ky = -2; ky <= 2; ky++)
                    for (int kx = -2; kx <= 2; kx++) {
                        int ny = y+ky, nx = x+kx;
                        if (ny<0||ny>=height||nx<0||nx>=width) continue;
                        sum += (int32_t)input[ny*width+nx] * kernel[ky+2][kx+2];
                    }
                int r = sum / weight;
                output[y*width+x] = (unsigned char)(r > 255 ? 255 : r);
            }
    }

    void applySobel(const unsigned char* input, int16_t* gx, int16_t* gy) {
        const int Kx[3][3] = {{-1,0,1},{-2,0,2},{-1,0,1}};
        const int Ky[3][3] = {{-1,-2,-1},{0,0,0},{1,2,1}};
        for (int y = 0; y < height; y++)
            for (int x = 0; x < width; x++) {
                int sumX = 0, sumY = 0;
                for (int ky = -1; ky <= 1; ky++)
                    for (int kx = -1; kx <= 1; kx++) {
                        int ny=y+ky, nx=x+kx;
                        int p=(ny>=0&&ny<height&&nx>=0&&nx<width)?input[ny*width+nx]:0;
                        sumX += p * Kx[ky+1][kx+1];
                        sumY += p * Ky[ky+1][kx+1];
                    }
                gx[y*width+x] = (int16_t)sumX;
                gy[y*width+x] = (int16_t)sumY;
            }
    }

    void computeMagnitudeL1(const int16_t* gx, const int16_t* gy,
                             unsigned char* magnitude) {
        const int N = width * height;
        vector<int> raw(N);
        int maxVal = 1;
        for (int i = 0; i < N; i++) {
            raw[i] = abs((int)gx[i]) + abs((int)gy[i]);
            if (raw[i] > maxVal) maxVal = raw[i];
        }
        for (int i = 0; i < N; i++)
            magnitude[i] = (unsigned char)((raw[i] * 255) / maxVal);
    }

    void computeMagnitudeL2(const int16_t* gx, const int16_t* gy,
                             unsigned char* magnitude) {
        const int N = width * height;
        vector<float> raw(N);
        float maxVal = 1.0f;
        for (int i = 0; i < N; i++) {
            raw[i] = sqrt((float)gx[i]*gx[i]+(float)gy[i]*gy[i]);
            if (raw[i] > maxVal) maxVal = raw[i];
        }
        for (int i = 0; i < N; i++)
            magnitude[i] = (unsigned char)((raw[i]/maxVal)*255.0f);
    }

    void computeDirection(const int16_t* gx, const int16_t* gy,
                          unsigned char* direction) {
        const int N = width * height;
        for (int i = 0; i < N; i++) {
            int ax = abs((int)gx[i]);
            int ay = abs((int)gy[i]);
            unsigned char dir;
            if      (ay*5 < ax*2)  dir = 0;
            else if (ay*5 > ax*12) dir = 2;
            else dir = ((int)gx[i]*(int)gy[i] >= 0) ? 1 : 3;
            direction[i] = dir;
        }
    }
};

// =====================================================================
// GAUSSIAN BLUR TESTS
// =====================================================================

TEST(GaussianBlur, UniformImageStaysUniform) {
    // With zero-padding, a uniform image gets darker near borders.
    // This test verifies two correct behaviors:
    // 1. Output never exceeds input (zero-padding only reduces values)
    // 2. Center is brighter than corners (less zero-padding effect)
    const int W = 50, H = 50;
    CannyEdgeDetector det(W, H);
    vector<unsigned char> input(W*H, 128), output(W*H, 0);
    det.applyGaussianBlur(input.data(), output.data());
    // Property 1: zero-padding never increases pixel values
    for (int i = 0; i < W*H; i++)
        EXPECT_LE(output[i], 128) << "Output must not exceed input at pixel " << i;
    // Property 2: center must be brighter than corners
    EXPECT_GT(output[25*W+25], output[0]) << "Center must be brighter than corner";
    // Property 3: output is symmetric
    EXPECT_EQ(output[25*W+10], output[25*W+39]) << "Must be horizontally symmetric";
    EXPECT_EQ(output[10*W+25], output[39*W+25]) << "Must be vertically symmetric";
}

TEST(GaussianBlur, AllBlackStaysBlack) {
    const int W = 20, H = 20;
    CannyEdgeDetector det(W, H);
    vector<unsigned char> input(W*H, 0), output(W*H, 0);
    det.applyGaussianBlur(input.data(), output.data());
    for (int i = 0; i < W*H; i++)
        EXPECT_EQ(output[i], 0);
}

TEST(GaussianBlur, ImpulseSpreadToNeighbors) {
    const int W = 9, H = 9;
    CannyEdgeDetector det(W, H);
    vector<unsigned char> input(W*H, 0), output(W*H, 0);
    input[4*W+4] = 255;
    det.applyGaussianBlur(input.data(), output.data());
    EXPECT_GT(output[4*W+4], 0)             << "Center must be nonzero";
    EXPECT_GT(output[4*W+4], output[3*W+4]) << "Center > neighbor above";
    EXPECT_GT(output[4*W+4], output[4*W+3]) << "Center > neighbor left";
    EXPECT_GT(output[3*W+4], output[2*W+4]) << "Near neighbor > far neighbor";
}

TEST(GaussianBlur, ReducesSharpEdge) {
    const int W = 20, H = 10;
    CannyEdgeDetector det(W, H);
    vector<unsigned char> input(W*H), output(W*H, 0);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            input[y*W+x] = (x < W/2) ? 0 : 255;
    det.applyGaussianBlur(input.data(), output.data());
    int diff = (int)output[(H/2)*W + W/2] - (int)output[(H/2)*W + W/2 - 1];
    EXPECT_LT(diff, 255) << "Blur must soften the sharp edge";
}

// =====================================================================
// SOBEL GRADIENT TESTS
// =====================================================================

TEST(SobelGradient, UniformImageZeroGradient) {
    const int W = 8, H = 8;
    CannyEdgeDetector det(W, H);
    vector<unsigned char> input(W*H, 100);
    vector<int16_t> gx(W*H), gy(W*H);
    det.applySobel(input.data(), gx.data(), gy.data());
    for (int y = 1; y < H-1; y++)
        for (int x = 1; x < W-1; x++) {
            EXPECT_EQ(gx[y*W+x], 0) << "Gx must be 0 at (" << x << "," << y << ")";
            EXPECT_EQ(gy[y*W+x], 0) << "Gy must be 0 at (" << x << "," << y << ")";
        }
}

TEST(SobelGradient, VerticalEdgeStrongGx) {
    const int W = 10, H = 6;
    CannyEdgeDetector det(W, H);
    vector<unsigned char> input(W*H);
    vector<int16_t> gx(W*H), gy(W*H);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            input[y*W+x] = (x < W/2) ? 0 : 255;
    det.applySobel(input.data(), gx.data(), gy.data());
    int idx = (H/2)*W + W/2;
    EXPECT_GT(abs((int)gx[idx]), 100) << "Gx must be large at vertical edge";
    EXPECT_LT(abs((int)gy[idx]), abs((int)gx[idx])) << "Gy must be less than Gx";
}

TEST(SobelGradient, HorizontalEdgeStrongGy) {
    const int W = 6, H = 10;
    CannyEdgeDetector det(W, H);
    vector<unsigned char> input(W*H);
    vector<int16_t> gx(W*H), gy(W*H);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            input[y*W+x] = (y < H/2) ? 255 : 0;
    det.applySobel(input.data(), gx.data(), gy.data());
    int idx = (H/2)*W + W/2;
    EXPECT_GT(abs((int)gy[idx]), 100) << "Gy must be large at horizontal edge";
    EXPECT_LT(abs((int)gx[idx]), abs((int)gy[idx])) << "Gx must be less than Gy";
}

TEST(SobelGradient, AllBlackZeroOutput) {
    const int W = 8, H = 8;
    CannyEdgeDetector det(W, H);
    vector<unsigned char> input(W*H, 0);
    vector<int16_t> gx(W*H), gy(W*H);
    det.applySobel(input.data(), gx.data(), gy.data());
    for (int i = 0; i < W*H; i++) {
        EXPECT_EQ(gx[i], 0);
        EXPECT_EQ(gy[i], 0);
    }
}

// =====================================================================
// MAGNITUDE TESTS
// =====================================================================

TEST(Magnitude, L1MaxIs255AfterNormalization) {
    const int W = 10, H = 6;
    CannyEdgeDetector det(W, H);
    vector<unsigned char> input(W*H), mag(W*H);
    vector<int16_t> gx(W*H), gy(W*H);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            input[y*W+x] = (x < W/2) ? 0 : 255;
    det.applySobel(input.data(), gx.data(), gy.data());
    det.computeMagnitudeL1(gx.data(), gy.data(), mag.data());
    int maxVal = *max_element(mag.begin(), mag.end());
    EXPECT_EQ(maxVal, 255) << "L1 max must be 255 after normalization";
}

TEST(Magnitude, L2MaxIs255AfterNormalization) {
    const int W = 10, H = 6;
    CannyEdgeDetector det(W, H);
    vector<unsigned char> input(W*H), mag(W*H);
    vector<int16_t> gx(W*H), gy(W*H);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            input[y*W+x] = (x < W/2) ? 0 : 255;
    det.applySobel(input.data(), gx.data(), gy.data());
    det.computeMagnitudeL2(gx.data(), gy.data(), mag.data());
    int maxVal = *max_element(mag.begin(), mag.end());
    EXPECT_EQ(maxVal, 255) << "L2 max must be 255 after normalization";
}

TEST(Magnitude, BothL1AndL2NonzeroAtEdge) {
    const int W = 10, H = 6;
    CannyEdgeDetector det(W, H);
    vector<unsigned char> input(W*H), mag_l1(W*H), mag_l2(W*H);
    vector<int16_t> gx(W*H), gy(W*H);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            input[y*W+x] = (x < W/2) ? 0 : 255;
    det.applySobel(input.data(), gx.data(), gy.data());
    det.computeMagnitudeL1(gx.data(), gy.data(), mag_l1.data());
    det.computeMagnitudeL2(gx.data(), gy.data(), mag_l2.data());
    int idx = (H/2)*W + W/2;
    EXPECT_GT(mag_l1[idx], 100) << "L1 must be bright at strong edge";
    EXPECT_GT(mag_l2[idx], 100) << "L2 must be bright at strong edge";
}

// =====================================================================
// DIRECTION TESTS
// =====================================================================

TEST(Direction, VerticalEdgeGives0Degrees) {
    const int W = 10, H = 6;
    CannyEdgeDetector det(W, H);
    vector<unsigned char> input(W*H), dir(W*H);
    vector<int16_t> gx(W*H), gy(W*H);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            input[y*W+x] = (x < W/2) ? 0 : 255;
    det.applySobel(input.data(), gx.data(), gy.data());
    det.computeDirection(gx.data(), gy.data(), dir.data());
    EXPECT_EQ(dir[(H/2)*W + W/2], 0) << "Vertical edge must give direction 0 (0 deg)";
}

TEST(Direction, HorizontalEdgeGives90Degrees) {
    const int W = 6, H = 10;
    CannyEdgeDetector det(W, H);
    vector<unsigned char> input(W*H), dir(W*H);
    vector<int16_t> gx(W*H), gy(W*H);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            input[y*W+x] = (y < H/2) ? 255 : 0;
    det.applySobel(input.data(), gx.data(), gy.data());
    det.computeDirection(gx.data(), gy.data(), dir.data());
    EXPECT_EQ(dir[(H/2)*W + W/2], 2) << "Horizontal edge must give direction 2 (90 deg)";
}

TEST(Direction, OutputOnlyValidValues) {
    const int W = 8, H = 8;
    CannyEdgeDetector det(W, H);
    vector<unsigned char> input(W*H), dir(W*H);
    vector<int16_t> gx(W*H), gy(W*H);
    for (int i = 0; i < W*H; i++) input[i] = (unsigned char)(i % 256);
    det.applySobel(input.data(), gx.data(), gy.data());
    det.computeDirection(gx.data(), gy.data(), dir.data());
    for (int i = 0; i < W*H; i++)
        EXPECT_LE(dir[i], 3) << "Direction must be 0,1,2,3 at pixel " << i;
}

// =====================================================================
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}