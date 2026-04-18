#ifndef IMAGE_TYPES_HPP
#define IMAGE_TYPES_HPP

#include <vector>
#include <cstdint>

// The universal image structure for the team
struct Image {
    int width;
    int height;
    std::vector<uint8_t> data;

    Image(int w, int h) : width(w), height(h), data(w * h, 0) {}
};

// --- THE CANNY PIPELINE ---
// Assign these to your 4 teammates to ensure modular development:

// 1. Noise Reduction
void gaussian_blur(const Image& input, Image& output);

// 2. Finding Gradients
void sobel_filter(const Image& input, Image& grad_x, Image& grad_y);

// 3. Edge Thinning
void non_max_suppression(const Image& mag, const Image& grad_x, const Image& grad_y, Image& output);

// 4. Double Thresholding
void double_threshold(Image& input, uint8_t low, uint8_t high);

// 5. Edge Tracking by Hysteresis (The Final Step)
// This turns "weak" pixels into "strong" ones if they are connected to strong edges
void hysteresis(Image& input);

#endif
