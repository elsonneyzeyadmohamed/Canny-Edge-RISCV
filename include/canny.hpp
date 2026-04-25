#ifndef CANNY_HPP
#define CANNY_HPP

#include <vector>

class CannyEdgeDetector {
private:
    int width;
    int height;

public:
    CannyEdgeDetector(int w, int h);
    
    // Function 1: Apply Gaussian Blur to reduce noise
    void applyGaussianBlur(const unsigned char* input, unsigned char* output);
    
    // Function 2: Apply Sobel Operator to find gradients (edges)
    void applySobel(const unsigned char* input, unsigned char* output);
};

#endif