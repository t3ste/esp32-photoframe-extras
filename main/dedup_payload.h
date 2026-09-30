#ifndef DEDUP_PAYLOAD_H
#define DEDUP_PAYLOAD_H

#include "dedup.h"

/**
 * @brief MD5 of what an image file shows rather than of how it is encoded (build option
 * `upload-dedup`, setting "Compare the pixels").
 *
 * - `.epdgz`: the bytes inside the gzip wrapper (two browsers, or two compression levels, give
 *   different gzip files for the same picture, but the same 4-bit pixel data).
 * - `.png`: the decoded pixels as 8-bit RGB, together with the image size (a palette PNG and a
 *   truecolour PNG of the same picture agree; alpha is dropped, 16-bit is reduced to 8).
 *
 * The file is read in small pieces, nothing image-sized is held in memory.
 *
 * @param name The file's real name, whose extension says how to read it - for a file that still
 * has a temporary name (an upload in `temp_full.png` may be an EPDGZ). NULL: use `path`.
 *
 * @return ESP_OK, ESP_ERR_NOT_FOUND for a missing file, ESP_ERR_NOT_SUPPORTED for a type this does
 * not decode (a `.bmp`, an interlaced PNG - the caller then compares the stored bytes instead),
 * ESP_FAIL for a file that is not what its name says (corrupt gzip or PNG).
 */
esp_err_t dedup_payload_md5(const char *path, const char *name, dedup_digest_t *out);

#endif
