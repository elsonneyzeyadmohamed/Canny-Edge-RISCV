#pragma once
#include <cstdint>
 
void double_threshold(const uint16_t* input, uint8_t* output,
                      int total_pixels,
                      uint16_t low_thresh, uint16_t high_thresh);
