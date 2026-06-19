#ifndef CANNY_HPP
#define CANNY_HPP
#include <vector>
#include <cstdint>

using namespace std;

// This class groups together all the core Canny edge detection stages
// (Gaussian Blur, Sobel, Magnitude, Direction) as methods operating on raw pixel buffers
class CannyEdgeDetector {
private:
    // Image width in pixels, stored once and reused by all stages
    int width;

    // Image height in pixels, stored once and reused by all stages
    int height;

public:
    // Constructor: takes the image dimensions and stores them
    // so each stage function doesn't need to receive width/height every time
    CannyEdgeDetector(int w, int h);

    // Stage 1: Gaussian Blur
    // Applies a 5x5 blur kernel (weight=273) with zero-padding at the borders
    // input  : pointer to the original grayscale image
    // output : pointer to the buffer that will hold the blurred image
    void applyGaussianBlur(const unsigned char* input, unsigned char* output);

    // Stage 2: Sobel operator
    // Computes horizontal (gx) and vertical (gy) gradients separately (SoA layout)
    // input : pointer to the blurred image
    // gx    : output buffer for the horizontal gradient
    // gy    : output buffer for the vertical gradient
    void applySobel(const unsigned char* input,
                    int16_t* gx, int16_t* gy);

    // Stage 3a: L1 magnitude
    // Computes magnitude as |Gx| + |Gy| (cheaper, no square root needed)
    // gx, gy    : input gradient buffers
    // magnitude : output buffer for the normalized magnitude (0-255)
    void computeMagnitudeL1(const int16_t* gx, const int16_t* gy,
                            unsigned char* magnitude);

    // Stage 3b: L2 magnitude
    // Computes magnitude as sqrt(Gx^2 + Gy^2) (more accurate, uses square root)
    // gx, gy    : input gradient buffers
    // magnitude : output buffer for the normalized magnitude (0-255)
    void computeMagnitudeL2(const int16_t* gx, const int16_t* gy,
                            unsigned char* magnitude);

    // Stage 4: Direction quantization
    // Converts the gradient angle into one of 4 categories: 0=0deg, 1=45deg, 2=90deg, 3=135deg
    // Uses integer comparisons instead of atan2() to avoid floating-point trig functions
    // gx, gy    : input gradient buffers
    // direction : output buffer for the quantized direction
    void computeDirection(const int16_t* gx, const int16_t* gy,
                          unsigned char* direction);
};

// End of header guard
#endif