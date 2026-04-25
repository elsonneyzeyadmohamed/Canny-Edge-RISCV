#include "image_types.hpp"
#include "canny.hpp"
#include <cstdio>

/**
 * Main Entry Point for the Canny Edge Detection Pipeline.
 * This version uses Standard I/O (stdin/stdout) to handle image data,
 * providing a reliable way to process images in a RISC-V emulated environment.
 */
int main() {
    // Image dimensions (Fixed for this stage of the project)
    const int W = 512;
    const int H = 512;

    // Initialize image buffers
    Image tiger(W, H);
    Image blurred(W, H);
    Image edge_result(W, H);

    // Step 1: Read raw pixel data from standard input (stdin)
    // This allows us to pipe image data directly into the emulator
    std::fread(tiger.data.data(), 1, W * H, stdin);

    // Step 2: Initialize the Canny Edge Detector
    CannyEdgeDetector detector(W, H);

    // Step 3: Execute the pipeline stages
    // 3a. Noise reduction using Gaussian Blur
    detector.applyGaussianBlur(tiger.data.data(), blurred.data.data());
    
    // 3b. Gradient calculation using Sobel Operator
    detector.applySobel(blurred.data.data(), edge_result.data.data());

    // Step 4: Write the final processed pixels to standard output (stdout)
    // This data can be piped back to a converter to view the image
    std::fwrite(edge_result.data.data(), 1, W * H, stdout);

    return 0;
}