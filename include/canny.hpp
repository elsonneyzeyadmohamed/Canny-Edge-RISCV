#ifndef CANNY_HPP
#define CANNY_HPP

#include <vector>
#include <cstdint>

using namespace std;

class CannyEdgeDetector {
private:
    int width;
    int height;

public:
    CannyEdgeDetector(int w, int h);

    // Stage 1: Gaussian Blur (5x5 kernel, weight=273, zero-padding)
    void applyGaussianBlur(const unsigned char* input, unsigned char* output);

    // Stage 2: Sobel - returns Gx and Gy SEPARATELY (Structure of Arrays)
    void applySobel(const unsigned char* input,
                    int16_t* gx, int16_t* gy);

    // Stage 3a: L1 magnitude = |Gx| + |Gy|
    void computeMagnitudeL1(const int16_t* gx, const int16_t* gy,
                            unsigned char* magnitude);

    // Stage 3b: L2 magnitude = sqrt(Gx^2 + Gy^2)
    void computeMagnitudeL2(const int16_t* gx, const int16_t* gy,
                            unsigned char* magnitude);

    // Stage 4: Direction quantized to 4 angles (0=0deg, 1=45, 2=90, 3=135)
    // Uses integer arithmetic instead of atan2()
    void computeDirection(const int16_t* gx, const int16_t* gy,
                          unsigned char* direction);
};

#endif