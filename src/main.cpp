#include "image_types.hpp"
#include "canny.hpp"
#include <iostream>
#include <string>

using namespace std;

// Declarations
bool load_raw(const string& path, Image& img);
bool save_raw(const string& path, const Image& img);

int main() {
    cout << "--- Canny Edge RISCV Pipeline Starting ---" << endl;

    const int W = 512;
    const int H = 512;

    // 1. Create containers
    Image tiger(W, H);
    Image blurred(W, H);
    Image edge_result(W, H);

    // 2. Load the image
    // Note: Make sure the file is in 'data/' folder or update the path
    if (!load_raw("data/tiger.raw", tiger)) {
        return 1; 
    }
    cout << "Step 1: Tiger loaded successfully." << endl;

    // 3. Initialize your Detector
    CannyEdgeDetector detector(W, H);

    // 4. Run your Gaussian Blur
    detector.applyGaussianBlur(tiger.data.data(), blurred.data.data());
    cout << "Step 2: Gaussian Blur applied." << endl;

    // 5. Run your Sobel Gradient
    detector.applySobel(blurred.data.data(), edge_result.data.data());
    cout << "Step 3: Sobel Gradient applied." << endl;

    // 6. Save the result
    if (save_raw("output.raw", edge_result)) {
        cout << "Step 4: Result saved to output.raw." << endl;
    }

    cout << "--- Pipeline Finished ---" << endl;
    return 0;
}