#include <stdint.h>

void double_threshold(uint8_t* input, uint8_t* output, int total_pixels, uint8_t low_thresh, uint8_t high_thresh) {
    const uint8_t STRONG_PIXEL = 255;
    const uint8_t WEAK_PIXEL = 25; 

    for (int i = 0; i < total_pixels; i++) {
        if (input[i] >= high_thresh) {
            output[i] = STRONG_PIXEL;
        } else if (input[i] >= low_thresh) {
            output[i] = WEAK_PIXEL;
        } else {
            output[i] = 0;
        }
    }
}
