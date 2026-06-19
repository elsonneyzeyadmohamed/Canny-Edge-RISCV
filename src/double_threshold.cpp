// edge or no edge, that's the question

#include "double_threshold.h"
void double_threshold(const uint16_t* input, uint8_t* output,
                      int total_pixels,
                      uint16_t low_thresh, uint16_t high_thresh)
//const uint16_t* input: A pointer to the input array containing gradient magnitudes. 
//It's 16-bit (uint16_t) because adding or multiplying pixel values during the Sobel filter stage results in values larger than a standard 8-bit byte.
//uint8_t* output: A pointer to the output array where the categorized pixels are saved. 
//We use 8-bit (uint8_t) here because we only need to store three specific target values (0, 128, or 255).
//int total_pixels: The total count of pixels in the image (WxH)
//low_thresh, high_thresh: The boundary limits used to classify the pixels.
{
    const uint8_t STRONG_PIXEL = 255;
    const uint8_t WEAK_PIXEL   = 128;   // must match EDGE_WEAK in hysteresis
    const uint8_t NO_EDGE      =   0;

    for (int i = 0; i < total_pixels; i++) {
        if      (input[i] >= high_thresh) output[i] = STRONG_PIXEL;
        else if (input[i] >= low_thresh)  output[i] = WEAK_PIXEL;
        else                              output[i] = NO_EDGE;
    }
}
