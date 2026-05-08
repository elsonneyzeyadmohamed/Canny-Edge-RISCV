#include <gtest/gtest.h>
#include "nms.h"
#include <vector>

TEST(NMS, VerticalEdgeThinned) {
    const int W = 5, H = 5;
    uint16_t mag[W * H] = {
        0, 50, 100, 50, 0,
        0, 50, 100, 50, 0,
        0, 50, 100, 50, 0,
        0, 50, 100, 50, 0,
        0, 50, 100, 50, 0,
    };
    uint8_t dir[W * H];
    for (int i = 0; i < W * H; i++) dir[i] = 0;

    uint16_t out[W * H] = {};
    non_maximum_suppression(mag, dir, out, W, H);

    EXPECT_GT(out[1 * W + 2], 0);
    EXPECT_EQ(out[1 * W + 1], 0);
}

TEST(NMS, UniformImageProducesNoSuppression) {
    const int W = 5, H = 5;
    uint16_t mag[W * H];
    uint8_t  dir[W * H];
    uint16_t out[W * H] = {};

    for (int i = 0; i < W * H; i++) { mag[i] = 100; dir[i] = 0; }

    non_maximum_suppression(mag, dir, out, W, H);

    EXPECT_EQ(out[0], 0);
}
