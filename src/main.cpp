#include "image_types.hpp"
#include <iostream>

// Empty "stubs" so the compiler doesn't complain while your team works
void gaussian_blur(const Image& in, Image& out) { out = in; } // Just a copy for now
void sobel_filter(const Image& in, Image& gx, Image& gy) {}
void non_max_suppression(const Image& m, const Image& gx, const Image& gy, Image& out) {}
void double_threshold(Image& in, uint8_t l, uint8_t h) {}
void hysteresis(Image& in) {}

int main() {
    std::cout << "--- Canny Edge RISCV Pipeline Starting ---" << std::endl;

    // 1. Create containers (512x512 for Tiger)
    Image tiger(512, 512);
    Image result(512, 512);

    // 2. Load the image
    if (!load_raw("tiger.raw", tiger)) {
        return 1; 
    }
    std::cout << "Step 1: Tiger loaded successfully." << std::endl;

    // 3. Run Gaussian Blur (Using Student 1's future code)
    gaussian_blur(tiger, result);
    std::cout << "Step 2: Gaussian Blur applied." << std::endl;

    // 4. Save the result
    if (save_raw("output.raw", result)) {
        std::cout << "Step 3: Result saved to output.raw." << std::endl;
    }

    std::cout << "--- Pipeline Finished ---" << std::endl;
    return 0;
}
