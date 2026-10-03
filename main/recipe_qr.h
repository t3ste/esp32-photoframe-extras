#ifndef RECIPE_QR_H
#define RECIPE_QR_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @file recipe_qr.h
 * @brief The QR code of a recipe's address (build option `recipes`): the text is encoded at the
 * lowest error correction (7%, the address is short and the code is small on the display) in as
 * small a version as holds it, up to RECIPE_QR_MAX_VERSION. On the device the encoder is the
 * firmware's esp_qrcode component, on the host qrcodegen, the library it is made of.
 */

#define RECIPE_QR_MAX_VERSION 6
#define RECIPE_QR_MAX_SIZE (RECIPE_QR_MAX_VERSION * 4 + 17)

typedef struct {
    int size;  // modules on a side (0 = empty)
    uint8_t bits[(RECIPE_QR_MAX_SIZE * RECIPE_QR_MAX_SIZE + 7) / 8];
} recipe_qr_t;

/** @brief Encodes `text`; false (and an empty code) if it does not fit the largest version. */
bool recipe_qr_encode(const char *text, recipe_qr_t *out);

/** @brief Whether a module is dark; false outside the code. */
bool recipe_qr_module(const recipe_qr_t *qr, int x, int y);

#endif
