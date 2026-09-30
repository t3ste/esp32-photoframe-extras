#ifndef DEDUP_H
#define DEDUP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

/**
 * @file dedup.h
 * @brief Duplicate detection for uploaded images (build option `upload-dedup`).
 *
 * Every album keeps a small text file, `.dedup`, with one line per image:
 *
 *     <kind> <32 hex digits of the MD5> <file name>
 *
 * `kind` is `s` (MD5 of the stored file's bytes) or `p` (MD5 of the decoded pixel data, which is
 * the same for two encodings of one picture). An upload is compared against the lines of the
 * current kind - the file is scanned line by line, nothing is held in RAM - so a picture already
 * in the album is found without reading any image. Lines whose file has gone are ignored.
 *
 * Pure file and string handling (MD5 through the ESP ROM, or a stand-in on the host), so the host
 * tests link it. Locking and the HTTP glue are in dedup_service.c.
 */

#define DEDUP_DIGEST_LEN 16
#define DEDUP_HEX_LEN 32
#define DEDUP_INDEX_NAME ".dedup"
#define DEDUP_LINE_MAX 400  // one index line, name included

typedef struct {
    uint8_t b[DEDUP_DIGEST_LEN];
} dedup_digest_t;

typedef enum {
    DEDUP_HASH_STORED = 0,   // MD5 of the file as stored
    DEDUP_HASH_PAYLOAD = 1,  // MD5 of the decoded pixel data
} dedup_hash_t;

typedef enum {
    DEDUP_MODE_OFF = 0,   // no check
    DEDUP_MODE_SKIP = 1,  // refuse a duplicate (HTTP 409)
    DEDUP_MODE_WARN = 2,  // store it, but say so
} dedup_mode_t;

/** @brief True for the file types an album holds (.png, .epdgz, .bmp; case-insensitive). */
bool dedup_is_image_name(const char *name);

void dedup_digest_to_hex(const dedup_digest_t *d, char out[DEDUP_HEX_LEN + 1]);
bool dedup_digest_from_hex(const char *hex, dedup_digest_t *d);
bool dedup_digest_equal(const dedup_digest_t *a, const dedup_digest_t *b);

/** @brief MD5 of the bytes of a file. */
esp_err_t dedup_md5_file(const char *path, dedup_digest_t *out);

/**
 * @brief Looks for an entry of `kind` with this digest in the album's index.
 *
 * @param ignore_name An entry with this file name does not count (the file about to replace it);
 * may be NULL.
 * @param found Receives the name of the first matching file that still exists, if not NULL.
 * @return True on a match.
 */
bool dedup_index_find(const char *album_dir, dedup_hash_t kind, const dedup_digest_t *d,
                      const char *ignore_name, char *found, size_t found_len);

/**
 * @brief Records `name` with its digest, replacing every earlier entry of that file name (of any
 * kind - the file was just stored or replaced, so what the other kind said is stale).
 */
esp_err_t dedup_index_set(const char *album_dir, dedup_hash_t kind, const dedup_digest_t *d,
                          const char *name);

/**
 * @brief Adds the entry of `kind` for a file that is already in the album, replacing only an
 * earlier entry of that kind - the indexing of old images must keep what the other kind knows.
 */
esp_err_t dedup_index_add(const char *album_dir, dedup_hash_t kind, const dedup_digest_t *d,
                          const char *name);

/** @brief Drops every entry of a file name (the file was deleted or replaced). */
esp_err_t dedup_index_remove(const char *album_dir, const char *name);

/** @brief True if the index has an entry of `kind` for the file name. */
bool dedup_index_has(const char *album_dir, dedup_hash_t kind, const char *name);

// The most entries the duplicate report looks at (an album larger than this is reported in part).
#define DEDUP_REPORT_MAX_ENTRIES 4000

/** @brief Receives one set of two or more files that share a digest (names in sorted order). */
typedef void (*dedup_group_fn)(const char *const *names, int count, void *ctx);

/**
 * @brief Reports the duplicates of an album from its index: `fn` is called for every set of
 * files, of which the index (entries of `kind`, for files that still exist) says they are the same.
 *
 * @param images Receives the number of image files in the folder (may be NULL).
 * @param indexed Receives how many of them the index knows (may be NULL); the difference has
 * not been indexed yet, so the report cannot say anything about it.
 */
esp_err_t dedup_index_groups(const char *album_dir, dedup_hash_t kind, dedup_group_fn fn, void *ctx,
                             int *images, int *indexed);

#endif
