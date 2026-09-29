#include <gtest/gtest.h>
#include <png.h>
#include <sys/stat.h>
#include <unistd.h>
#include <zlib.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

extern "C" {
#include "dedup.h"
#include "dedup_payload.h"
}

namespace
{

using Bytes = std::vector<uint8_t>;

class DedupPayload : public ::testing::Test
{
   protected:
    std::string dir = "dedup_payload_test";

    void SetUp() override
    {
        std::string cmd = "rm -rf " + dir;
        ASSERT_EQ(system(cmd.c_str()), 0);
        ASSERT_EQ(mkdir(dir.c_str(), 0755), 0);
    }
    void TearDown() override
    {
        std::string cmd = "rm -rf " + dir;
        (void) !system(cmd.c_str());
    }

    std::string path(const std::string &name)
    {
        return dir + "/" + name;
    }

    // A PNG of the given colour type; `pixels` holds the rows exactly as libpng expects them.
    void write_png(const std::string &name, int w, int h, int color_type, int bit_depth,
                   const Bytes &pixels, int level = 6, int interlace = PNG_INTERLACE_NONE,
                   const std::vector<png_color> &palette = {})
    {
        FILE *fp = fopen(path(name).c_str(), "wb");
        ASSERT_NE(fp, nullptr);
        png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
        png_infop info = png_create_info_struct(png);
        ASSERT_FALSE(setjmp(png_jmpbuf(png)));
        png_init_io(png, fp);
        png_set_compression_level(png, level);
        png_set_IHDR(png, info, w, h, bit_depth, color_type, interlace,
                     PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
        if (color_type == PNG_COLOR_TYPE_PALETTE) {
            png_set_PLTE(png, info, palette.data(), (int) palette.size());
        }
        png_write_info(png, info);
        size_t stride = pixels.size() / (size_t) h;
        int passes = interlace == PNG_INTERLACE_NONE ? 1 : png_set_interlace_handling(png);
        for (int p = 0; p < passes; p++) {
            for (int y = 0; y < h; y++) {
                png_write_row(png, const_cast<uint8_t *>(pixels.data()) + y * stride);
            }
        }
        png_write_end(png, nullptr);
        png_destroy_write_struct(&png, &info);
        fclose(fp);
    }

    void write_gzip(const std::string &name, const Bytes &payload, int level)
    {
        z_stream s;
        memset(&s, 0, sizeof(s));
        ASSERT_EQ(deflateInit2(&s, level, Z_DEFLATED, 16 + MAX_WBITS, 8, Z_DEFAULT_STRATEGY), Z_OK);
        Bytes out(deflateBound(&s, (uLong) payload.size()) + 32);
        s.next_in = const_cast<uint8_t *>(payload.data());
        s.avail_in = (uInt) payload.size();
        s.next_out = out.data();
        s.avail_out = (uInt) out.size();
        ASSERT_EQ(deflate(&s, Z_FINISH), Z_STREAM_END);
        FILE *fp = fopen(path(name).c_str(), "wb");
        fwrite(out.data(), 1, out.size() - s.avail_out, fp);
        fclose(fp);
        deflateEnd(&s);
    }

    std::string hash(const std::string &name, esp_err_t expect = ESP_OK)
    {
        dedup_digest_t d;
        esp_err_t err = dedup_payload_md5(path(name).c_str(), &d);
        EXPECT_EQ(err, expect) << name;
        if (err != ESP_OK) {
            return "";
        }
        char hex[DEDUP_HEX_LEN + 1];
        dedup_digest_to_hex(&d, hex);
        return hex;
    }

    std::string stored_hash(const std::string &name)
    {
        dedup_digest_t d;
        EXPECT_EQ(dedup_md5_file(path(name).c_str(), &d), ESP_OK);
        char hex[DEDUP_HEX_LEN + 1];
        dedup_digest_to_hex(&d, hex);
        return hex;
    }

    // A varied w x h picture in RGB
    static Bytes rgb_picture(int w, int h)
    {
        Bytes px((size_t) w * h * 3);
        for (size_t i = 0; i < px.size(); i++) {
            px[i] = (uint8_t) ((i * 7 + i / 11) % 251);
        }
        return px;
    }
};

}  // namespace

TEST_F(DedupPayload, SamePixelsDifferentCompressionGiveOneHash)
{
    Bytes px = rgb_picture(64, 48);
    write_png("fast.png", 64, 48, PNG_COLOR_TYPE_RGB, 8, px, 1);
    write_png("small.png", 64, 48, PNG_COLOR_TYPE_RGB, 8, px, 9);
    EXPECT_NE(stored_hash("fast.png"), stored_hash("small.png"));  // the files differ ...
    EXPECT_EQ(hash("fast.png"), hash("small.png"));                // ... the pictures do not
}

TEST_F(DedupPayload, OnePixelChangedGivesAnotherHash)
{
    Bytes px = rgb_picture(64, 48);
    write_png("a.png", 64, 48, PNG_COLOR_TYPE_RGB, 8, px);
    px[1000] ^= 1;
    write_png("b.png", 64, 48, PNG_COLOR_TYPE_RGB, 8, px);
    EXPECT_NE(hash("a.png"), hash("b.png"));
}

TEST_F(DedupPayload, PaletteAndTruecolourAgree)
{
    std::vector<png_color> pal = {{255, 0, 0}, {0, 255, 0}, {0, 0, 255}, {255, 255, 255}};
    Bytes idx(16 * 8), rgb;
    for (size_t i = 0; i < idx.size(); i++) {
        idx[i] = (uint8_t) ((i * 5 + i / 3) % 4);
        rgb.push_back(pal[idx[i]].red);
        rgb.push_back(pal[idx[i]].green);
        rgb.push_back(pal[idx[i]].blue);
    }
    write_png("pal.png", 16, 8, PNG_COLOR_TYPE_PALETTE, 8, idx, 6, PNG_INTERLACE_NONE, pal);
    write_png("rgb.png", 16, 8, PNG_COLOR_TYPE_RGB, 8, rgb);
    EXPECT_EQ(hash("pal.png"), hash("rgb.png"));
}

TEST_F(DedupPayload, FourBitPaletteAgreesToo)
{
    // what the frame's own 16-level grey / 6-colour output looks like: a few bits per pixel
    std::vector<png_color> pal;
    for (int i = 0; i < 16; i++) {
        pal.push_back({(png_byte) (i * 17), (png_byte) (i * 17), (png_byte) (i * 17)});
    }
    Bytes packed(8 * 8 / 2), rgb;
    for (int i = 0; i < 8 * 8; i++) {
        int v = (i * 3 + i / 5) % 16;
        if (i % 2 == 0) {
            packed[i / 2] = (uint8_t) (v << 4);
        } else {
            packed[i / 2] |= (uint8_t) v;
        }
        rgb.push_back((png_byte) (v * 17));
        rgb.push_back((png_byte) (v * 17));
        rgb.push_back((png_byte) (v * 17));
    }
    write_png("pal4.png", 8, 8, PNG_COLOR_TYPE_PALETTE, 4, packed, 6, PNG_INTERLACE_NONE, pal);
    write_png("rgb.png", 8, 8, PNG_COLOR_TYPE_RGB, 8, rgb);
    EXPECT_EQ(hash("pal4.png"), hash("rgb.png"));
}

TEST_F(DedupPayload, GreyAndTruecolourAgree)
{
    Bytes grey(20 * 10), rgb;
    for (size_t i = 0; i < grey.size(); i++) {
        grey[i] = (uint8_t) ((i * 13) % 256);
        rgb.push_back(grey[i]);
        rgb.push_back(grey[i]);
        rgb.push_back(grey[i]);
    }
    write_png("grey.png", 20, 10, PNG_COLOR_TYPE_GRAY, 8, grey);
    write_png("rgb.png", 20, 10, PNG_COLOR_TYPE_RGB, 8, rgb);
    EXPECT_EQ(hash("grey.png"), hash("rgb.png"));
}

TEST_F(DedupPayload, AlphaIsIgnored)
{
    Bytes rgb = rgb_picture(10, 10), rgba;
    for (size_t i = 0; i < rgb.size(); i += 3) {
        rgba.insert(rgba.end(), rgb.begin() + i, rgb.begin() + i + 3);
        rgba.push_back((uint8_t) (i % 256));
    }
    write_png("rgba.png", 10, 10, PNG_COLOR_TYPE_RGB_ALPHA, 8, rgba);
    write_png("rgb.png", 10, 10, PNG_COLOR_TYPE_RGB, 8, rgb);
    EXPECT_EQ(hash("rgba.png"), hash("rgb.png"));
}

TEST_F(DedupPayload, SixteenBitIsReducedToEight)
{
    Bytes rgb8 = rgb_picture(8, 8), rgb16;
    for (uint8_t v : rgb8) {
        rgb16.push_back(v);  // high byte
        rgb16.push_back(v);  // low byte: 8-bit value v*257
    }
    write_png("deep.png", 8, 8, PNG_COLOR_TYPE_RGB, 16, rgb16);
    write_png("plain.png", 8, 8, PNG_COLOR_TYPE_RGB, 8, rgb8);
    EXPECT_EQ(hash("deep.png"), hash("plain.png"));
}

TEST_F(DedupPayload, SizeIsPartOfTheHash)
{
    Bytes px(18);
    for (size_t i = 0; i < px.size(); i++) {
        px[i] = (uint8_t) (i * 9 + 1);
    }
    write_png("wide.png", 3, 2, PNG_COLOR_TYPE_RGB, 8, px);  // 3x2 pixels
    write_png("tall.png", 2, 3, PNG_COLOR_TYPE_RGB, 8, px);  // same bytes, 2x3
    EXPECT_NE(hash("wide.png"), hash("tall.png"));
}

TEST_F(DedupPayload, InterlacedPngIsNotSupported)
{
    write_png("adam7.png", 16, 16, PNG_COLOR_TYPE_RGB, 8, rgb_picture(16, 16), 6,
              PNG_INTERLACE_ADAM7);
    hash("adam7.png", ESP_ERR_NOT_SUPPORTED);
}

TEST_F(DedupPayload, BrokenPngFails)
{
    write_png("whole.png", 32, 32, PNG_COLOR_TYPE_RGB, 8, rgb_picture(32, 32));
    FILE *fp = fopen(path("whole.png").c_str(), "rb");
    Bytes data(4096);
    size_t n = fread(data.data(), 1, data.size(), fp);
    fclose(fp);
    fp = fopen(path("cut.png").c_str(), "wb");
    fwrite(data.data(), 1, n / 2, fp);  // cut in the middle of the image data
    fclose(fp);
    hash("cut.png", ESP_FAIL);

    fp = fopen(path("fake.png").c_str(), "wb");
    fputs("this is not a png at all", fp);
    fclose(fp);
    hash("fake.png", ESP_FAIL);
}

TEST_F(DedupPayload, EpdgzIgnoresTheCompression)
{
    Bytes payload(192000);
    for (size_t i = 0; i < payload.size(); i++) {
        payload[i] = (uint8_t) (((i * 31) >> 3) % 97);
    }
    write_gzip("fast.epdgz", payload, 1);
    write_gzip("small.epdgz", payload, 9);
    EXPECT_NE(stored_hash("fast.epdgz"), stored_hash("small.epdgz"));
    EXPECT_EQ(hash("fast.epdgz"), hash("small.epdgz"));
    payload[100000] ^= 0x10;
    write_gzip("other.epdgz", payload, 6);
    EXPECT_NE(hash("other.epdgz"), hash("small.epdgz"));
}

TEST_F(DedupPayload, BrokenEpdgzFails)
{
    Bytes payload(50000, 0x5a);
    write_gzip("whole.epdgz", payload, 6);
    FILE *fp = fopen(path("whole.epdgz").c_str(), "rb");
    Bytes data(70000);
    size_t n = fread(data.data(), 1, data.size(), fp);
    fclose(fp);
    ASSERT_GT(n, 20u);
    fp = fopen(path("cut.epdgz").c_str(), "wb");
    fwrite(data.data(), 1, n - 10, fp);  // the gzip trailer is missing
    fclose(fp);
    hash("cut.epdgz", ESP_FAIL);

    fp = fopen(path("plain.epdgz").c_str(), "wb");
    fputs("not gzip data, just text", fp);
    fclose(fp);
    hash("plain.epdgz", ESP_FAIL);

    fp = fopen(path("empty.epdgz").c_str(), "wb");
    fclose(fp);
    hash("empty.epdgz", ESP_FAIL);
}

TEST_F(DedupPayload, OtherTypesAndMissingFiles)
{
    FILE *fp = fopen(path("x.bmp").c_str(), "wb");
    fputs("BM....", fp);
    fclose(fp);
    hash("x.bmp", ESP_ERR_NOT_SUPPORTED);
    hash("x.jpg", ESP_ERR_NOT_SUPPORTED);
    hash("missing.png", ESP_ERR_NOT_FOUND);
    hash("missing.epdgz", ESP_ERR_NOT_FOUND);
}
