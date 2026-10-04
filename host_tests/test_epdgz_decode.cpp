// How the real image_processor.c reads an .epdgz back (decode_epdgz_buffer, reached through
// image_processor_compose_pair_to_rgb): a stream of exactly the panel's size is read, a stream that
// ends early, is cut or is too long is refused - an early end used to leave the rest of the buffer
// as whatever the memory held, and that was drawn as pixels.

#include <gtest/gtest.h>
#include <zlib.h>

#include <cstdint>
#include <vector>

extern "C" {
#include "image_processor.h"
}

namespace
{

constexpr int kWidth = 64;
constexpr int kHeight = 48;
constexpr size_t kPackedSize = ((size_t) kWidth * kHeight + 1) / 2;

// gzip of `size` bytes, each the 4-bit pair `value` (what epdgz_writer produces, framing included)
std::vector<uint8_t> gz_of(size_t size, uint8_t value)
{
    std::vector<uint8_t> raw(size, value);
    z_stream strm = {};
    EXPECT_EQ(deflateInit2(&strm, Z_BEST_SPEED, Z_DEFLATED, 16 + MAX_WBITS, 8, Z_DEFAULT_STRATEGY),
              Z_OK);
    std::vector<uint8_t> out(deflateBound(&strm, (uLong) size) + 64);
    strm.next_in = raw.data();
    strm.avail_in = (uInt) raw.size();
    strm.next_out = out.data();
    strm.avail_out = (uInt) out.size();
    EXPECT_EQ(deflate(&strm, Z_FINISH), Z_STREAM_END);
    out.resize(strm.total_out);
    deflateEnd(&strm);
    return out;
}

esp_err_t compose(const std::vector<uint8_t> &a, const std::vector<uint8_t> &b,
                  image_process_rgb_result_t *result)
{
    return image_processor_compose_pair_to_rgb(a.data(), a.size(), IMAGE_FORMAT_EPD_GZ, b.data(),
                                               b.size(), IMAGE_FORMAT_EPD_GZ, false,
                                               DITHER_FLOYD_STEINBERG, result);
}

class EpdgzDecode : public ::testing::Test
{
   protected:
    void SetUp() override
    {
        test_board_display_width = kWidth;
        test_board_display_height = kHeight;
        test_board_display_type = "spectra6";
    }
};

}  // namespace

TEST_F(EpdgzDecode, AStreamOfExactlyThePanelsSizeIsRead)
{
    auto good = gz_of(kPackedSize, 0x00);
    image_process_rgb_result_t result = {};
    ASSERT_EQ(compose(good, good, &result), ESP_OK);
    EXPECT_EQ(result.width, kWidth);
    EXPECT_EQ(result.height, kHeight);
    EXPECT_EQ(result.rgb_size, (size_t) kWidth * kHeight * 3);
    free(result.rgb_data);
}

TEST_F(EpdgzDecode, AStreamThatEndsEarlyIsRefused)
{
    auto good = gz_of(kPackedSize, 0x00);
    for (size_t shorter : {(size_t) 1, kPackedSize / 2, kPackedSize - 1}) {
        auto short_stream = gz_of(kPackedSize - shorter, 0x00);
        image_process_rgb_result_t result = {};
        EXPECT_NE(compose(short_stream, good, &result), ESP_OK) << "first, " << shorter << " short";
        EXPECT_NE(compose(good, short_stream, &result), ESP_OK)
            << "second, " << shorter << " short";
    }
}

TEST_F(EpdgzDecode, AStreamThatIsTooLongIsRefused)
{
    auto good = gz_of(kPackedSize, 0x00);
    auto too_long = gz_of(kPackedSize + 1, 0x00);
    image_process_rgb_result_t result = {};
    EXPECT_NE(compose(too_long, good, &result), ESP_OK);
}

TEST_F(EpdgzDecode, ACutStreamAndGarbageAreRefused)
{
    auto good = gz_of(kPackedSize, 0x00);
    std::vector<uint8_t> cut(good.begin(), good.begin() + good.size() / 2);
    std::vector<uint8_t> garbage(64, 0x5A);
    std::vector<uint8_t> empty;
    image_process_rgb_result_t result = {};
    EXPECT_NE(compose(cut, good, &result), ESP_OK);
    EXPECT_NE(compose(garbage, good, &result), ESP_OK);
    EXPECT_NE(compose(good, empty, &result), ESP_OK);
}
