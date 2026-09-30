#ifndef DEDUP_SERVICE_H
#define DEDUP_SERVICE_H

#include <stdbool.h>
#include <stddef.h>

#include "dedup.h"
#include "esp_err.h"

/**
 * @file dedup_service.h
 * @brief The settings-driven side of the duplicate detection (build option `upload-dedup`): what
 * the upload handler asks, the lock that keeps the index consistent between the web server and
 * the background indexing, and that indexing itself.
 */

/** @brief The configured reaction to a duplicate (Settings -> "When an upload is already in the
 * album"). */
dedup_mode_t dedup_service_mode(void);

/** @brief What is compared: the stored bytes or, where the file type allows, the decoded pixels. */
dedup_hash_t dedup_service_kind(void);

/**
 * @brief Digest of an image file as the settings say. A file the pixel comparison cannot decode
 * (a `.bmp`, an interlaced PNG) is compared by its bytes instead.
 *
 * @param name The name the file has (or will have): an upload is still in a temporary file whose
 * name says nothing about its type. NULL: the type of `path`.
 */
esp_err_t dedup_service_hash(const char *path, const char *name, dedup_digest_t *out);

/** @brief The name of an image already in the album with this digest (not `ignore_name`), if any.
 */
bool dedup_service_find(const char *album_dir, const dedup_digest_t *d, const char *ignore_name,
                        char *found, size_t found_len);

/** @brief Records a stored image. Best effort: a failure only means it is not found later. */
void dedup_service_record(const char *album_dir, const char *name, const dedup_digest_t *d);

/** @brief Forgets a deleted image. */
void dedup_service_forget(const char *album_dir, const char *name);

/**
 * @brief Indexes, in the background, the images of an album (or of every album if `album` is
 * NULL or empty) that the index does not know yet - those uploaded before the option was built
 * in, or before the comparison was switched from the file to the picture.
 *
 * @return ESP_OK if it started, ESP_ERR_INVALID_STATE if one is running, ESP_ERR_NOT_FOUND
 * without storage.
 */
esp_err_t dedup_service_scan_start(const char *album);

bool dedup_service_scan_running(void);

/** @brief Progress of the last or running indexing as JSON (free with free()). */
char *dedup_service_status_json(void);

/**
 * @brief The duplicates of an album as JSON:
 * `{"album","hash","images","indexed","groups":[[names]]}` (free with free()); NULL for an unknown
 * album.
 */
char *dedup_service_report_json(const char *album);

#endif
