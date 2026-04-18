#include "image_types.hpp"
#include <iostream>

int main() {
    // 1. Setup dimensions (e.g., for your 512x512 Tiger)
    Image tiger(512, 512);
    Image blurred(512, 512);

    // 2. Load the image
    if (!load_raw("tiger.raw", tiger)) {
        std::cerr << "Error: Could not find tiger.raw" << std::endl;
        return 1;
    }

    // 3. Run the pipeline (Teammates fill these in)
    gaussian_blur(tiger, blurred);
    
    // 4. Save result
    save_raw("output.raw", blurred);

    std::cout << "Processing complete. Check output.raw" << std::endl;
    return 0;
}
