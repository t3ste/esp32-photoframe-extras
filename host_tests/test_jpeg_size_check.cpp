// The size check of esp_jpeg's answer (main/jpeg_size_check.h, build option fixes).

#include <gtest/gtest.h>

#include <cstdint>

extern "C" {
#include "jpeg_size_check.h"
}

TEST(JpegSizeCheck, TheSizeOfAnOrdinaryPictureIsAccepted)
{
    EXPECT_TRUE(jpeg_output_size_ok(800, 480, 0, 800 * 480 * 3));
    EXPECT_TRUE(jpeg_output_size_ok(4032, 3024, 0, 4032 * 3024 * 3));  // a phone photo
    EXPECT_TRUE(jpeg_output_size_ok(12000, 9000, 0,
                                    12000u * 9000u * 3u));  // 108 megapixels, 324 MB: no wrap
}

TEST(JpegSizeCheck, EachScaleDividesEachSideSeparately)
{
    EXPECT_TRUE(jpeg_output_size_ok(800, 480, 1, 400 * 240 * 3));
    EXPECT_TRUE(jpeg_output_size_ok(
        801, 481, 1, 400 * 240 * 3));  // the division of each side drops the odd pixel
    EXPECT_TRUE(jpeg_output_size_ok(801, 481, 2, 200 * 120 * 3));
    EXPECT_TRUE(jpeg_output_size_ok(4032, 3024, 2, 1008 * 756 * 3));
    EXPECT_TRUE(jpeg_output_size_ok(12000, 9000, 2, 3000 * 2250 * 3));
    EXPECT_TRUE(jpeg_output_size_ok(803, 483, 3, 100 * 60 * 3));
}

TEST(JpegSizeCheck, ASizeThatWrapsIn32BitIsRefused)
{
    // 40000 x 35792 pixels: 4 295 040 000 bytes, which 32-bit arithmetic reports as 72 704
    uint32_t wrapped = 40000u * 35792u * 3u;
    ASSERT_EQ(wrapped, 72704u);
    EXPECT_FALSE(jpeg_output_size_ok(40000, 35792, 0, wrapped));
    // the scaled form of the same header: (35792 / 4) * (40000 / 4) * 3 = 268 440 000, no wrap
    // here, and right
    EXPECT_TRUE(jpeg_output_size_ok(40000, 35792, 2, (size_t) 8948 * 10000 * 3));
    // a size that is not what the sides say
    EXPECT_FALSE(jpeg_output_size_ok(800, 480, 0, 800 * 480 * 3 + 1));
    EXPECT_FALSE(jpeg_output_size_ok(800, 480, 0, 800 * 480 * 2));
    EXPECT_FALSE(jpeg_output_size_ok(
        800, 480, 1, 800 * 480 * 3));  // the size of the unscaled picture for a scaled one
}

TEST(JpegSizeCheck, BadSidesAndScalesAreRefused)
{
    EXPECT_FALSE(jpeg_output_size_ok(0, 480, 0, 0));
    EXPECT_FALSE(jpeg_output_size_ok(800, 0, 0, 0));
    EXPECT_FALSE(jpeg_output_size_ok(-800, 480, 0, 800 * 480 * 3));
    EXPECT_FALSE(jpeg_output_size_ok(800, -480, 0, 800 * 480 * 3));
    EXPECT_FALSE(jpeg_output_size_ok(800, 480, -1, 800 * 480 * 3));
    EXPECT_FALSE(jpeg_output_size_ok(800, 480, 4, 0));
    EXPECT_FALSE(jpeg_output_size_ok(5, 5, 3, 0));  // nothing is left of it
    EXPECT_FALSE(jpeg_output_size_ok(7, 1000, 3, 0));
}
