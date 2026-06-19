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

// Local redefinition of CannyEdgeDetector for host-side testing (avoids RISC-V cross-compiler)
// NOTE: kernel center here (15) differs from canny.cpp (41) - known discrepancy, keep in sync manually
class CannyEdgeDetector {
    int width, height;
public:
    CannyEdgeDetector(int w, int h) : width(w), height(h) {}

    // Stage 1: 5x5 Gaussian blur, integer kernel (sum = 273)
    void applyGaussianBlur(const unsigned char* input, unsigned char* output) {
        const int kernel[5][5] = {
        { 1,  4,  7,  4, 1},
        { 4, 16, 26, 16, 4},
        { 7, 26, 41, 26, 7},
        { 4, 16, 26, 16, 4},
        { 1,  4,  7,  4, 1}
    };
        const int weight = 273;
        for (int y = 0; y < height; y++)
            for (int x = 0; x < width; x++) {
                // int32_t: max sum = 255*273 = 69615, fits safely
                int32_t sum = 0;
                for (int ky = -2; ky <= 2; ky++)
                    for (int kx = -2; kx <= 2; kx++) {
                        int ny = y+ky, nx = x+kx;
                        if (ny<0||ny>=height||nx<0||nx>=width) continue; // zero-padding
                        sum += (int32_t)input[ny*width+nx] * kernel[ky+2][kx+2];
                    }
                int r = sum / weight;
                output[y*width+x] = (unsigned char)(r > 255 ? 255 : r);
            }
    }

    // Stage 2: Sobel gradients, Gx/Gy stored separately (SoA)
    // int16_t sufficient: max response = 4*255 = 1020
    void applySobel(const unsigned char* input, int16_t* gx, int16_t* gy) {
        const int Kx[3][3] = {{-1,0,1},{-2,0,2},{-1,0,1}};
        const int Ky[3][3] = {{-1,-2,-1},{0,0,0},{1,2,1}};
        for (int y = 0; y < height; y++)
            for (int x = 0; x < width; x++) {
                int sumX = 0, sumY = 0;
                for (int ky = -1; ky <= 1; ky++)
                    for (int kx = -1; kx <= 1; kx++) {
                        int ny=y+ky, nx=x+kx;
                        int p=(ny>=0&&ny<height&&nx>=0&&nx<width)?input[ny*width+nx]:0; // zero-padding
                        sumX += p * Kx[ky+1][kx+1];
                        sumY += p * Ky[ky+1][kx+1];
                    }
                gx[y*width+x] = (int16_t)sumX;
                gy[y*width+x] = (int16_t)sumY;
            }
    }

    // Stage 3a: L1 magnitude = |Gx|+|Gy|, two-pass normalize to [0,255]
    void computeMagnitudeL1(const int16_t* gx, const int16_t* gy,
                             unsigned char* magnitude) {
        const int N = width * height;
        vector<int> raw(N);
        int maxVal = 1; // avoid div-by-zero
        for (int i = 0; i < N; i++) {
            raw[i] = abs((int)gx[i]) + abs((int)gy[i]);
            if (raw[i] > maxVal) maxVal = raw[i];
        }
        for (int i = 0; i < N; i++)
            magnitude[i] = (unsigned char)((raw[i] * 255) / maxVal);
    }

    // Stage 3b: L2 magnitude = sqrt(Gx^2+Gy^2), two-pass normalize to [0,255]
    void computeMagnitudeL2(const int16_t* gx, const int16_t* gy,
                             unsigned char* magnitude) {
        const int N = width * height;
        vector<float> raw(N);
        float maxVal = 1.0f; // avoid div-by-zero
        for (int i = 0; i < N; i++) {
            raw[i] = sqrt((float)gx[i]*gx[i]+(float)gy[i]*gy[i]);
            if (raw[i] > maxVal) maxVal = raw[i];
        }
        for (int i = 0; i < N; i++)
            magnitude[i] = (unsigned char)((raw[i]/maxVal)*255.0f);
    }

    // Stage 4: quantize gradient angle to {0,45,90,135} deg using integer cross-mult (no atan2)
    void computeDirection(const int16_t* gx, const int16_t* gy,
                          unsigned char* direction) {
        const int N = width * height;
        for (int i = 0; i < N; i++) {
            int ax = abs((int)gx[i]);
            int ay = abs((int)gy[i]);
            unsigned char dir;
            if      (ay*5 < ax*2)  dir = 0;  // < tan(22.5°)
            else if (ay*5 > ax*12) dir = 2;  // > tan(67.5°)
            else dir = ((int)gx[i]*(int)gy[i] >= 0) ? 1 : 3; // 45 vs 135 by sign
            direction[i] = dir;
        }
    }
};

// =====================================================================
// GAUSSIAN BLUR TESTS
// =====================================================================

// Tests that a uniform gray image behaves correctly under blur with zero-padding:
// zero-padding darkens the border pixels but never brightens any pixel,
// and the center must always be the brightest region
TEST(GaussianBlur, UniformImageStaysUniform) {
    // With zero-padding, a uniform image gets darker near borders.
    // This test verifies two correct behaviors:
    // 1. Output never exceeds input (zero-padding only reduces values)
    // 2. Center is brighter than corners (less zero-padding effect)
    // Image size chosen large enough that the center pixel is fully surrounded
    // and unaffected by zero-padding (at least 2 pixels away from every border)
    const int W = 50, H = 50;
    CannyEdgeDetector det(W, H);
    vector<unsigned char> input(W*H, 128), output(W*H, 0);
    det.applyGaussianBlur(input.data(), output.data());
    // Property 1: zero-padding never increases pixel values
    // Every output pixel must be <= 128 because missing border neighbors reduce the weighted sum
    for (int i = 0; i < W*H; i++)
        EXPECT_LE(output[i], 128) << "Output must not exceed input at pixel " << i;
    // Property 2: center must be brighter than corners
    // The center pixel at (25,25) has all 25 kernel neighbors inside the image,
    // so it receives the full weighted sum and produces a higher output than corners
    EXPECT_GT(output[25*W+25], output[0]) << "Center must be brighter than corner";
    // Property 3: output is symmetric
    // Pixel at column 10 and pixel at column 39 are equidistant from the center column (25),
    // so they must receive the same amount of zero-padding and produce equal outputs
    EXPECT_EQ(output[25*W+10], output[25*W+39]) << "Must be horizontally symmetric";
    // Same reasoning for vertical symmetry: row 10 and row 39 are equidistant from the center
    EXPECT_EQ(output[10*W+25], output[39*W+25]) << "Must be vertically symmetric";
}

// Tests that an all-black image (all zeros) stays all zeros after blurring
// A zero input multiplied by any kernel weight still sums to zero
TEST(GaussianBlur, AllBlackStaysBlack) {
    const int W = 20, H = 20;
    CannyEdgeDetector det(W, H);
    // Input: all pixels are 0 (completely black image)
    // Output: also initialized to 0 to detect any incorrect non-zero writes
    vector<unsigned char> input(W*H, 0), output(W*H, 0);
    det.applyGaussianBlur(input.data(), output.data());
    // Every output pixel must remain exactly 0
    for (int i = 0; i < W*H; i++)
        EXPECT_EQ(output[i], 0);
}

// Tests that a single bright pixel (impulse) spreads its energy to its neighbors
// and that the spread decreases with distance, matching the Gaussian bell shape
TEST(GaussianBlur, ImpulseSpreadToNeighbors) {
    // 9x9 gives enough room: the single bright pixel at (4,4) is 4 pixels from every border,
    // so all 25 kernel taps are inside the image and the full blur shape is visible
    const int W = 9, H = 9;
    CannyEdgeDetector det(W, H);
    // Input: all black except one bright pixel at the center position (4,4)
    vector<unsigned char> input(W*H, 0), output(W*H, 0);
    input[4*W+4] = 255;
    det.applyGaussianBlur(input.data(), output.data());
    // The center pixel must be nonzero: it receives the highest kernel weight (center tap)
    EXPECT_GT(output[4*W+4], 0)             << "Center must be nonzero";
    // The center must be brighter than its immediate neighbor one row above
    // because the center has the highest kernel coefficient
    EXPECT_GT(output[4*W+4], output[3*W+4]) << "Center > neighbor above";
    // Same check in the horizontal direction: center must outweigh its left neighbor
    EXPECT_GT(output[4*W+4], output[4*W+3]) << "Center > neighbor left";
    // Closer neighbors must be brighter than farther neighbors
    // row 3 is 1 step from center, row 2 is 2 steps from center -> row 3 must be brighter
    EXPECT_GT(output[3*W+4], output[2*W+4]) << "Near neighbor > far neighbor";
}

// Tests that Gaussian blur softens a hard left-right step edge
// The left half is 0 and the right half is 255, creating a sharp vertical boundary
// After blurring, the pixel just right of the boundary should NOT be 255 higher than the one just left
TEST(GaussianBlur, ReducesSharpEdge) {
    const int W = 20, H = 10;
    CannyEdgeDetector det(W, H);
    // Input initialized without a default value so we fill it manually below
    vector<unsigned char> input(W*H), output(W*H, 0);
    // Fill left half with 0 and right half with 255 to create a sharp vertical step edge
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            input[y*W+x] = (x < W/2) ? 0 : 255;
    det.applyGaussianBlur(input.data(), output.data());
    // Measure the contrast across the edge at the center row (H/2)
    // W/2 is the first pixel on the right (bright) side; W/2 - 1 is the last pixel on the left (dark) side
    int diff = (int)output[(H/2)*W + W/2] - (int)output[(H/2)*W + W/2 - 1];
    // If the blur worked, the step has been smoothed and the jump is less than the original 255
    EXPECT_LT(diff, 255) << "Blur must soften the sharp edge";
}

// =====================================================================
// SOBEL GRADIENT TESTS
// =====================================================================

// Tests that a completely uniform image produces zero gradients everywhere in the interior
// The Sobel kernel is antisymmetric: equal neighbors on both sides always cancel out
// Border pixels are skipped because zero-padding breaks the uniformity at the edges
TEST(SobelGradient, UniformImageZeroGradient) {
    const int W = 8, H = 8;
    CannyEdgeDetector det(W, H);
    // Input: all pixels set to the same value (100); any constant value works
    vector<unsigned char> input(W*H, 100);
    // Output buffers for horizontal and vertical gradients
    vector<int16_t> gx(W*H), gy(W*H);
    det.applySobel(input.data(), gx.data(), gy.data());
    // Check only interior pixels (y from 1 to H-2, x from 1 to W-2)
    // Border pixels have zero-padded neighbors that break the uniform neighborhood
    for (int y = 1; y < H-1; y++)
        for (int x = 1; x < W-1; x++) {
            // In a uniform image, the Sobel kernel sees equal values on both sides,
            // so the weighted sum cancels to exactly zero
            EXPECT_EQ(gx[y*W+x], 0) << "Gx must be 0 at (" << x << "," << y << ")";
            EXPECT_EQ(gy[y*W+x], 0) << "Gy must be 0 at (" << x << "," << y << ")";
        }
}

// Tests that a vertical step edge (left half black, right half white) produces
// a strong horizontal gradient (Gx) and a weaker vertical gradient (Gy) at the boundary
TEST(SobelGradient, VerticalEdgeStrongGx) {
    // Non-square dimensions to make the intended edge direction more obvious
    const int W = 10, H = 6;
    CannyEdgeDetector det(W, H);
    // Input buffer without default fill; filled manually row by row below
    vector<unsigned char> input(W*H);
    // Output gradient buffers
    vector<int16_t> gx(W*H), gy(W*H);
    // Left half is 0, right half is 255: creates a vertical step edge at x = W/2
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            input[y*W+x] = (x < W/2) ? 0 : 255;
    det.applySobel(input.data(), gx.data(), gy.data());
    // Sample the pixel at the center of the edge: row H/2, column W/2 (right at the boundary)
    int idx = (H/2)*W + W/2;
    // Gx must be large because the left-right intensity difference is maximal here
    EXPECT_GT(abs((int)gx[idx]), 100) << "Gx must be large at vertical edge";
    // Gy must be smaller than Gx because there is no vertical variation in this image
    EXPECT_LT(abs((int)gy[idx]), abs((int)gx[idx])) << "Gy must be less than Gx";
}

// Tests that a horizontal step edge (top half white, bottom half black) produces
// a strong vertical gradient (Gy) and a weaker horizontal gradient (Gx) at the boundary
TEST(SobelGradient, HorizontalEdgeStrongGy) {
    // Non-square dimensions; width is smaller to emphasize the horizontal nature of the test
    const int W = 6, H = 10;
    CannyEdgeDetector det(W, H);
    // Input buffer without default fill; filled manually row by row below
    vector<unsigned char> input(W*H);
    // Output gradient buffers
    vector<int16_t> gx(W*H), gy(W*H);
    // Top half is 255, bottom half is 0: creates a horizontal step edge at y = H/2
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            input[y*W+x] = (y < H/2) ? 255 : 0;
    det.applySobel(input.data(), gx.data(), gy.data());
    // Sample the pixel at the center of the edge: row H/2, column W/2 (right at the boundary)
    int idx = (H/2)*W + W/2;
    // Gy must be large because the top-bottom intensity difference is maximal here
    EXPECT_GT(abs((int)gy[idx]), 100) << "Gy must be large at horizontal edge";
    // Gx must be smaller than Gy because there is no horizontal variation in this image
    EXPECT_LT(abs((int)gx[idx]), abs((int)gy[idx])) << "Gx must be less than Gy";
}

// Tests that an all-black image produces zero gradients at every pixel, including borders
// There is no intensity variation anywhere so every Sobel response must be exactly zero
TEST(SobelGradient, AllBlackZeroOutput) {
    const int W = 8, H = 8;
    CannyEdgeDetector det(W, H);
    // Input: all pixels are 0 (completely black image)
    vector<unsigned char> input(W*H, 0);
    // Output gradient buffers
    vector<int16_t> gx(W*H), gy(W*H);
    det.applySobel(input.data(), gx.data(), gy.data());
    // Check every pixel including borders: zero input always gives zero gradient
    for (int i = 0; i < W*H; i++) {
        EXPECT_EQ(gx[i], 0);
        EXPECT_EQ(gy[i], 0);
    }
}

// =====================================================================
// MAGNITUDE TESTS
// =====================================================================

// Tests that the L1 normalization produces a maximum output value of exactly 255
// The two-pass normalization divides every raw value by the max,
// so the strongest pixel must always map to 255 after scaling
TEST(Magnitude, L1MaxIs255AfterNormalization) {
    // Reuse the same vertical edge setup from the Sobel tests to guarantee a strong gradient
    const int W = 10, H = 6;
    CannyEdgeDetector det(W, H);
    // Input and magnitude output buffers
    vector<unsigned char> input(W*H), mag(W*H);
    // Gradient buffers needed as intermediate results before computing magnitude
    vector<int16_t> gx(W*H), gy(W*H);
    // Create a vertical step edge: left half black, right half white
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            input[y*W+x] = (x < W/2) ? 0 : 255;
    // Compute Sobel gradients first, then pass them to L1 magnitude
    det.applySobel(input.data(), gx.data(), gy.data());
    det.computeMagnitudeL1(gx.data(), gy.data(), mag.data());
    // Find the largest value in the output magnitude buffer
    int maxVal = *max_element(mag.begin(), mag.end());
    // After normalization the maximum must be exactly 255; anything less means the scaling is wrong
    EXPECT_EQ(maxVal, 255) << "L1 max must be 255 after normalization";
}

// Tests that the L2 normalization produces a maximum output value of exactly 255
// Same reasoning as L1: the strongest pixel always maps to 255 after the two-pass scaling
TEST(Magnitude, L2MaxIs255AfterNormalization) {
    // Same vertical edge setup as above so both L1 and L2 tests use comparable inputs
    const int W = 10, H = 6;
    CannyEdgeDetector det(W, H);
    // Input and magnitude output buffers
    vector<unsigned char> input(W*H), mag(W*H);
    // Gradient buffers needed as intermediate results before computing magnitude
    vector<int16_t> gx(W*H), gy(W*H);
    // Create a vertical step edge: left half black, right half white
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            input[y*W+x] = (x < W/2) ? 0 : 255;
    // Compute Sobel gradients first, then pass them to L2 magnitude
    det.applySobel(input.data(), gx.data(), gy.data());
    det.computeMagnitudeL2(gx.data(), gy.data(), mag.data());
    // Find the largest value in the output magnitude buffer
    int maxVal = *max_element(mag.begin(), mag.end());
    // After normalization the maximum must be exactly 255; anything less means the scaling is wrong
    EXPECT_EQ(maxVal, 255) << "L2 max must be 255 after normalization";
}

// Tests that both L1 and L2 produce visibly bright values at a known strong edge location
// This checks that the normalization scales the edge pixel high enough to be meaningful,
// not just that the max is 255 (which could come from a different pixel)
TEST(Magnitude, BothL1AndL2NonzeroAtEdge) {
    // Same vertical edge setup as the normalization tests
    const int W = 10, H = 6;
    CannyEdgeDetector det(W, H);
    // Separate output buffers for L1 and L2 so both can be checked at the same pixel
    vector<unsigned char> input(W*H), mag_l1(W*H), mag_l2(W*H);
    // Gradient buffers shared by both magnitude computations
    vector<int16_t> gx(W*H), gy(W*H);
    // Create a vertical step edge: left half black, right half white
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            input[y*W+x] = (x < W/2) ? 0 : 255;
    // Compute Sobel gradients once and reuse them for both L1 and L2
    det.applySobel(input.data(), gx.data(), gy.data());
    det.computeMagnitudeL1(gx.data(), gy.data(), mag_l1.data());
    det.computeMagnitudeL2(gx.data(), gy.data(), mag_l2.data());
    // Sample the pixel at the center of the edge boundary
    int idx = (H/2)*W + W/2;
    // Both L1 and L2 must be visibly bright (> 100) at the strong edge location
    // The threshold 100 is arbitrary but confirms the edge is clearly detected
    EXPECT_GT(mag_l1[idx], 100) << "L1 must be bright at strong edge";
    EXPECT_GT(mag_l2[idx], 100) << "L2 must be bright at strong edge";
}

// =====================================================================
// DIRECTION TESTS
// =====================================================================

// Tests that a vertical step edge (pure left-right transition) produces direction 0 (0 degrees)
// A vertical edge creates a large Gx and near-zero Gy,
// so the angle is close to 0 degrees (horizontal gradient direction)
TEST(Direction, VerticalEdgeGives0Degrees) {
    // Reuse the same vertical edge setup from Sobel and magnitude tests
    const int W = 10, H = 6;
    CannyEdgeDetector det(W, H);
    // Input and direction output buffers
    vector<unsigned char> input(W*H), dir(W*H);
    // Gradient buffers needed before computing direction
    vector<int16_t> gx(W*H), gy(W*H);
    // Create a vertical step edge: left half black, right half white
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            input[y*W+x] = (x < W/2) ? 0 : 255;
    // Compute Sobel gradients, then pass them to direction quantization
    det.applySobel(input.data(), gx.data(), gy.data());
    det.computeDirection(gx.data(), gy.data(), dir.data());
    // The center of the edge at (W/2, H/2) must be classified as direction 0 (0 degrees)
    // because Gx dominates Gy there and the angle falls below tan(22.5°)
    EXPECT_EQ(dir[(H/2)*W + W/2], 0) << "Vertical edge must give direction 0 (0 deg)";
}

// Tests that a horizontal step edge (pure top-bottom transition) produces direction 2 (90 degrees)
// A horizontal edge creates a large Gy and near-zero Gx,
// so the angle is close to 90 degrees (vertical gradient direction)
TEST(Direction, HorizontalEdgeGives90Degrees) {
    // Non-square dimensions with height > width to emphasize the horizontal orientation
    const int W = 6, H = 10;
    CannyEdgeDetector det(W, H);
    // Input and direction output buffers
    vector<unsigned char> input(W*H), dir(W*H);
    // Gradient buffers needed before computing direction
    vector<int16_t> gx(W*H), gy(W*H);
    // Create a horizontal step edge: top half white, bottom half black
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            input[y*W+x] = (y < H/2) ? 255 : 0;
    // Compute Sobel gradients, then pass them to direction quantization
    det.applySobel(input.data(), gx.data(), gy.data());
    det.computeDirection(gx.data(), gy.data(), dir.data());
    // The center of the edge at (W/2, H/2) must be classified as direction 2 (90 degrees)
    // because Gy dominates Gx there and the angle exceeds tan(67.5°)
    EXPECT_EQ(dir[(H/2)*W + W/2], 2) << "Horizontal edge must give direction 2 (90 deg)";
}

// Tests that computeDirection never produces an invalid direction value
// The only legal output values are 0, 1, 2, and 3
// An arbitrary ramp input is used to exercise many different gradient combinations
TEST(Direction, OutputOnlyValidValues) {
    const int W = 8, H = 8;
    CannyEdgeDetector det(W, H);
    // Input and direction output buffers
    vector<unsigned char> input(W*H), dir(W*H);
    // Gradient buffers needed before computing direction
    vector<int16_t> gx(W*H), gy(W*H);
    // Fill with a ramp pattern (0, 1, 2, ..., 255, 0, 1, ...) to create varied gradients
    // i % 256 wraps around so the values stay in the valid unsigned char range
    for (int i = 0; i < W*H; i++) input[i] = (unsigned char)(i % 256);
    // Compute Sobel gradients from the ramp input, then quantize the direction
    det.applySobel(input.data(), gx.data(), gy.data());
    det.computeDirection(gx.data(), gy.data(), dir.data());
    // Every pixel must have a direction in {0, 1, 2, 3}; anything above 3 is a bug
    for (int i = 0; i < W*H; i++)
        EXPECT_LE(dir[i], 3) << "Direction must be 0,1,2,3 at pixel " << i;
}

// =====================================================================
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}