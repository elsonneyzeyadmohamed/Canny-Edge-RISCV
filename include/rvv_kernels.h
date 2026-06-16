#ifndef RVV_KERNELS_H
#define RVV_KERNELS_H

#include <cstdint>


void applyGaussianBlur_rvv(
    
// write here your includes

);

void computeDirection_rvv(
    const int16_t* gx,
    const int16_t* gy,
    uint8_t* direction,
    int total_pixels
);

void computeMagnitudeL1_rvv(
    const int16_t* gx,
    const int16_t* gy,
    uint8_t* mag,
    int total_pixels
);
#endif
