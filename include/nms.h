#pragma once
#include <cstdint>

void non_maximum_suppression(
    const uint16_t* magnitude,
    const uint8_t*  direction,
    uint16_t*       output,
    int width,
    int height
);
