#include "nms.h"
#include <algorithm>

void non_maximum_suppression(
    const uint16_t* magnitude,
    const uint8_t*  direction,
    uint16_t*       output,
    int width,
    int height
) {
    for (int i = 0; i < width * height; i++)
        output[i] = 0;

    for (int y = 1; y < height - 1; y++) {
        for (int x = 1; x < width - 1; x++) {
            int idx = y * width + x;
            uint16_t mag = magnitude[idx];
            uint8_t  dir = direction[idx];

            uint16_t neighbor1, neighbor2;

            switch (dir) {
                case 0:
                    neighbor1 = magnitude[idx - 1];
                    neighbor2 = magnitude[idx + 1];
                    break;
                case 1:
                    neighbor1 = magnitude[(y-1)*width + (x+1)];
                    neighbor2 = magnitude[(y+1)*width + (x-1)];
                    break;
                case 2:
                    neighbor1 = magnitude[(y-1)*width + x];
                    neighbor2 = magnitude[(y+1)*width + x];
                    break;
                case 3:
                    neighbor1 = magnitude[(y-1)*width + (x-1)];
                    neighbor2 = magnitude[(y+1)*width + (x+1)];
                    break;
                default:
                    neighbor1 = neighbor2 = 0;
            }

            if (mag >= neighbor1 && mag >= neighbor2)   
             output[idx] = mag;
        }
    }
}
