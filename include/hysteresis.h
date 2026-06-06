#pragma once
#include <cstdint>

// Performs hysteresis edge tracing in-place on a classified map.
// Input pixels must be:  255 = STRONG,  128 = WEAK,  0 = NONE
// After the call:        255 = confirmed edge,        0 = background
void hysteresis(uint8_t* edges, int width, int height);
