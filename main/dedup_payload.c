#include "dedup_payload.h"

#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "esp_log.h"
#include "esp_rom_md5.h"
#include "png.h"
#include "zlib.h"

static const char *TAG = "dedup_payload";

#define PAYLOAD_IN_CHUNK 2048
#define PAYLOAD_OUT_CHUNK 4096

static esp_err_t md5_gunzipped(FILE *fp, dedup_digest_t *out)
{
    z_stream strm;
    memset(&strm, 0, sizeof(strm));
    if (inflateInit2(&strm, 16 + MAX_WBITS) != Z_OK) {  // 16: expect the gzip wrapper
        return ESP_ERR_NO_MEM;
    }
    uint8_t *in = malloc(PAYLOAD_IN_CHUNK);
    uint8_t *chunk = malloc(PAYLOAD_OUT_CHUNK);
    if (!in || !chunk) {
        free(in);
        free(chunk);
        inflateEnd(&strm);
        return ESP_ERR_NO_MEM;
    }
    md5_context_t ctx;
    esp_rom_md5_init(&ctx);
    esp_err_t result = ESP_FAIL;
    int zret = Z_OK;
    while (zret != Z_STREAM_END) {
        if (strm.avail_in == 0) {
            strm.avail_in = (uInt) fread(in, 1, PAYLOAD_IN_CHUNK, fp);
            strm.next_in = in;
            if (strm.avail_in == 0) {
                break;  // ran out of data before the end of the stream: not a whole gzip file
            }
        }
        strm.avail_out = PAYLOAD_OUT_CHUNK;
        strm.next_out = chunk;
        zret = inflate(&strm, Z_NO_FLUSH);
        if (zret != Z_OK && zret != Z_STREAM_END) {
            break;
        }
        esp_rom_md5_update(&ctx, chunk, PAYLOAD_OUT_CHUNK - strm.avail_out);
    }
    if (zret == Z_STREAM_END) {
        result = ESP_OK;
    }
    inflateEnd(&strm);
    free(in);
    free(chunk);
    esp_rom_md5_final(out->b, &ctx);
    return result;
}

static esp_err_t md5_png_pixels(FILE *fp, dedup_digest_t *out)
{
    uint8_t sig[8];
    if (fread(sig, 1, 8, fp) != 8 || png_sig_cmp(sig, 0, 8) != 0) {
        return ESP_FAIL;
    }
    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop info = png ? png_create_info_struct(png) : NULL;
    if (!png || !info) {
        png_destroy_read_struct(&png, &info, NULL);
        return ESP_ERR_NO_MEM;
    }
    uint8_t *volatile row = NULL;
    volatile esp_err_t result = ESP_FAIL;
    md5_context_t ctx;
    esp_rom_md5_init(&ctx);

    if (setjmp(png_jmpbuf(png))) {
        goto done;  // corrupt PNG
    }
    png_init_io(png, fp);
    png_set_sig_bytes(png, 8);
    png_read_info(png, info);

    png_uint_32 width = png_get_image_width(png, info);
    png_uint_32 height = png_get_image_height(png, info);
    if (png_get_interlace_type(png, info) != PNG_INTERLACE_NONE) {
        result = ESP_ERR_NOT_SUPPORTED;  // rows arrive in several passes: no streaming hash
        goto done;
    }

    // Whatever the file's colour type, look at it as 8-bit RGB.
    png_byte color_type = png_get_color_type(png, info);
    png_byte bit_depth = png_get_bit_depth(png, info);
    if (color_type == PNG_COLOR_TYPE_PALETTE) {
        png_set_palette_to_rgb(png);
    }
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) {
        png_set_expand_gray_1_2_4_to_8(png);
    }
    if (bit_depth == 16) {
        png_set_strip_16(png);
    }
    if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
        png_set_gray_to_rgb(png);
    }
    png_set_strip_alpha(png);
    png_read_update_info(png, info);

    size_t row_bytes = png_get_rowbytes(png, info);
    row = malloc(row_bytes);
    if (!row) {
        result = ESP_ERR_NO_MEM;
        goto done;
    }
    char size_prefix[32];
    int prefix_len =
        snprintf(size_prefix, sizeof(size_prefix), "%ux%u:", (unsigned) width, (unsigned) height);
    esp_rom_md5_update(&ctx, size_prefix, (uint32_t) prefix_len);
    for (png_uint_32 y = 0; y < height; y++) {
        png_read_row(png, row, NULL);
        esp_rom_md5_update(&ctx, row, (uint32_t) row_bytes);
    }
    result = ESP_OK;

done:
    png_destroy_read_struct(&png, &info, NULL);
    free(row);
    if (result == ESP_OK) {  // (ctx is not trusted after a longjmp out of libpng)
        esp_rom_md5_final(out->b, &ctx);
    }
    return result;
}

esp_err_t dedup_payload_md5(const char *path, const char *name, dedup_digest_t *out)
{
    const char *ext = strrchr(name ? name : path, '.');
    bool is_epdgz = ext && strcasecmp(ext, ".epdgz") == 0;
    bool is_png = ext && strcasecmp(ext, ".png") == 0;
    if (!is_epdgz && !is_png) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        return ESP_ERR_NOT_FOUND;
    }
    esp_err_t err = is_epdgz ? md5_gunzipped(fp, out) : md5_png_pixels(fp, out);
    fclose(fp);
    if (err != ESP_OK && err != ESP_ERR_NOT_SUPPORTED) {
        ESP_LOGW(TAG, "Cannot decode %s for the pixel comparison", path);
    }
    return err;
}
