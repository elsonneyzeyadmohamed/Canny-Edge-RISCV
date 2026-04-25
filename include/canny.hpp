#ifndef CANNY_HPP
#define CANNY_HPP

#include <vector>
#include <cstdint>

class CannyEdgeDetector {
private:
    int width;
    int height;

public:
    CannyEdgeDetector(int w, int h);

    // Stage 1: Gaussian Blur (5x5 kernel, weight=273, zero-padding)
    void applyGaussianBlur(const unsigned char* input, unsigned char* output);

    // Stage 2: Sobel Operator - returns Gx and Gy SEPARATELY (SoA layout)
    void applySobel(const unsigned char* input,
                    int16_t* gx, int16_t* gy);

    // Stage 3a: Gradient Magnitude - L1 norm ( |Gx| + |Gy| )
    void computeMagnitudeL1(const int16_t* gx, const int16_t* gy,
                            unsigned char* magnitude);

    // Stage 3b: Gradient Magnitude - L2 norm ( sqrt(Gx^2 + Gy^2) )
    void computeMagnitudeL2(const int16_t* gx, const int16_t* gy,
                            unsigned char* magnitude);

    // Stage 4: Gradient Direction quantized to 4 angles (0, 45, 90, 135)
    // Uses integer arithmetic instead of atan2() - embedded-friendly
    // Output values: 0 -> 0deg, 1 -> 45deg, 2 -> 90deg, 3 -> 135deg
    void computeDirection(const int16_t* gx, const int16_t* gy,
                          unsigned char* direction);
};

#endif