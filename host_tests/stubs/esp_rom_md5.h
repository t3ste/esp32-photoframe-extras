// Host stand-in for ESP-IDF's ROM MD5 (esp_rom_md5.h): the same three calls, a plain RFC 1321
// implementation. The firmware uses the ROM's; this only lets the host tests hash files. Checked
// against the RFC's test vectors in test_dedup.cpp.
#pragma once

#include <math.h>
#include <stdint.h>
#include <string.h>

#define ESP_ROM_MD5_DIGEST_LEN 16

typedef struct md5_context {
    uint32_t state[4];
    uint64_t bytes;
    uint8_t buffer[64];
} md5_context_t;

static inline uint32_t md5_rotl(uint32_t x, int c)
{
    return (x << c) | (x >> (32 - c));
}

static inline void md5_block(md5_context_t *ctx, const uint8_t *p)
{
    static const int shifts[4][4] = {
        {7, 12, 17, 22}, {5, 9, 14, 20}, {4, 11, 16, 23}, {6, 10, 15, 21}};
    uint32_t m[16];
    for (int i = 0; i < 16; i++) {
        m[i] = (uint32_t) p[4 * i] | ((uint32_t) p[4 * i + 1] << 8) |
               ((uint32_t) p[4 * i + 2] << 16) | ((uint32_t) p[4 * i + 3] << 24);
    }
    uint32_t a = ctx->state[0], b = ctx->state[1], c = ctx->state[2], d = ctx->state[3];
    for (int i = 0; i < 64; i++) {
        uint32_t f;
        int g;
        if (i < 16) {
            f = (b & c) | (~b & d);
            g = i;
        } else if (i < 32) {
            f = (d & b) | (~d & c);
            g = (5 * i + 1) % 16;
        } else if (i < 48) {
            f = b ^ c ^ d;
            g = (3 * i + 5) % 16;
        } else {
            f = c ^ (b | ~d);
            g = (7 * i) % 16;
        }
        uint32_t k = (uint32_t) floor(fabs(sin((double) (i + 1))) * 4294967296.0);
        uint32_t tmp = d;
        d = c;
        c = b;
        b = b + md5_rotl(a + f + k + m[g], shifts[i / 16][i % 4]);
        a = tmp;
    }
    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
}

static inline void esp_rom_md5_init(md5_context_t *ctx)
{
    ctx->state[0] = 0x67452301;
    ctx->state[1] = 0xefcdab89;
    ctx->state[2] = 0x98badcfe;
    ctx->state[3] = 0x10325476;
    ctx->bytes = 0;
}

static inline void esp_rom_md5_update(md5_context_t *ctx, const void *buf, uint32_t len)
{
    const uint8_t *p = (const uint8_t *) buf;
    while (len > 0) {
        uint32_t used = (uint32_t) (ctx->bytes % 64);
        uint32_t take = 64 - used < len ? 64 - used : len;
        memcpy(ctx->buffer + used, p, take);
        ctx->bytes += take;
        p += take;
        len -= take;
        if (used + take == 64) {
            md5_block(ctx, ctx->buffer);
        }
    }
}

static inline void esp_rom_md5_final(uint8_t *digest, md5_context_t *ctx)
{
    uint64_t bits = ctx->bytes * 8;
    static const uint8_t pad = 0x80;
    static const uint8_t zero = 0;
    esp_rom_md5_update(ctx, &pad, 1);
    while (ctx->bytes % 64 != 56) {
        esp_rom_md5_update(ctx, &zero, 1);
    }
    uint8_t len_bytes[8];
    for (int i = 0; i < 8; i++) {
        len_bytes[i] = (uint8_t) (bits >> (8 * i));
    }
    esp_rom_md5_update(ctx, len_bytes, 8);
    for (int i = 0; i < 4; i++) {
        digest[4 * i] = (uint8_t) ctx->state[i];
        digest[4 * i + 1] = (uint8_t) (ctx->state[i] >> 8);
        digest[4 * i + 2] = (uint8_t) (ctx->state[i] >> 16);
        digest[4 * i + 3] = (uint8_t) (ctx->state[i] >> 24);
    }
}
