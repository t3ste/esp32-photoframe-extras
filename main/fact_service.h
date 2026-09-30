#ifndef FACT_SERVICE_H
#define FACT_SERVICE_H

#include <stdbool.h>
#include <stddef.h>

#include "esp_err.h"
#include "fact_pack.h"

/**
 * @file fact_service.h
 * @brief The user's own facts for the fact-of-the-day page (build option `fact-of-the-day`): the
 * pack lives as a text file in the storage (FACT_PACK_PATH, format in fact_pack.h), is edited from
 * the Web UI, and is read whenever the page is drawn. Without a file the built-in facts are used.
 */

/**
 * @brief Reads the pack file into a NUL-terminated buffer allocated with malloc (free it). Returns
 * ESP_ERR_NOT_FOUND when there is no file (or no storage) and ESP_ERR_INVALID_SIZE when it is
 * larger than FACT_PACK_MAX_BYTES.
 */
esp_err_t fact_service_load(char **out_text, size_t *out_len);

/**
 * @brief Replaces the pack file with `data` (written next to it first, so an interrupted write
 * keeps the old pack); an empty `data` removes the file. ESP_ERR_INVALID_SIZE if `len` is more than
 * FACT_PACK_MAX_BYTES.
 */
esp_err_t fact_service_store(const char *data, size_t len);

/**
 * @brief The fact of a day: from the user's pack if there is a usable one, else from the built-in
 * facts in the given language. `*from_pack` (may be NULL) tells which. Always fills `out` (with the
 * first built-in fact if even the built-in list could not be used).
 */
void fact_service_pick(bool german, long day_number, fact_t *out, bool *from_pack);

#endif
