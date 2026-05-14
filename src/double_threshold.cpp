#include "double_threshold.h"

void double_threshold(const uint16_t* input, uint8_t* output,
                      int total_pixels,
                      uint16_t low_thresh, uint16_t high_thresh)
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
