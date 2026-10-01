#ifndef ART_SOURCES_H
#define ART_SOURCES_H

/**
 * @file art_sources.h
 * @brief The three museum services of the artworks mode (build option `artworks`): the requests
 * to build and the answers to read. Pure C on top of cJSON, so the host tests link it and run it
 * against real answers (host_tests/data/art/); the fetching itself is in art_flow.c.
 *
 * Rijksmuseum (data.rijksmuseum.nl, no key): search by kind and year, then the object, its visual
 * item and its digital object, then the picture from the IIIF server. SMK (api.smk.dk, no key):
 * one search answer holds everything. Smithsonian (api.si.edu, a key - the public DEMO_KEY when
 * none is set): one search answer holds everything.
 *
 * A work is only taken when its record says that it is public domain or CC0, and a picture only
 * from the host that belongs to its service.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "art_select.h"

#define ART_ID_MAX 48
#define ART_URL_MAX 160

typedef struct {
    art_source_t source;
    char id[ART_ID_MAX];           // letters, digits, '-' and '_' only: part of the file name
    char image_base[ART_URL_MAX];  // the picture without its size (art_image_url() adds it)
    char artist[96];
    char title[160];
    char year[16];
    char rights[8];  // "PDM" (public domain mark), "CC0" or "PD"
} art_work_t;

/** @brief The public demo key of api.data.gov: 10 requests an hour for everybody. */
#define ART_SMITHSONIAN_DEMO_KEY "DEMO_KEY"

/** @brief Letters, digits and '_', 4 to 64 of them: what an api.data.gov key looks like. */
bool art_si_key_valid(const char *key);

// --- Rijksmuseum ---------------------------------------------------------------------------

/** @brief The search for works of a kind and a year that have a picture (at most 100 hits). */
bool art_rijks_search_url(art_type_t type, int year, char *out, size_t out_len);

/**
 * @brief One of the hits of a search answer (`rnd` picks): the address of its record. Returns the
 * number of hits (0 if the answer has none or is not what it should be); `object_url` is only
 * written when the pick is a record of the Rijksmuseum.
 */
int art_rijks_pick_object(const char *json, size_t len, uint32_t rnd, char *object_url,
                          size_t url_len);

/** @brief Title, artist, year and the address of the visual item from the record of a work. */
bool art_rijks_parse_object(const char *json, size_t len, art_work_t *work, char *visual_url,
                            size_t url_len);

/**
 * @brief The rights of the picture and the address of the digital object from the visual item.
 * False when the picture is not public domain or CC0.
 */
bool art_rijks_parse_visual_item(const char *json, size_t len, art_work_t *work, char *digital_url,
                                 size_t url_len);

/** @brief The IIIF address of the picture from the digital object (false if it is not for
 * download). */
bool art_rijks_parse_digital_object(const char *json, size_t len, art_work_t *work);

// --- SMK -----------------------------------------------------------------------------------

/** @brief The search for public-domain works of a kind with a picture: one hit at `offset` (`rows`
 * 0: only the count). */
bool art_smk_search_url(art_type_t type, unsigned offset, int rows, char *out, size_t out_len);

/** @brief How many works the search found (-1 if the answer is not what it should be). */
int art_smk_found(const char *json, size_t len);

/** @brief The work of a search answer with one hit. */
bool art_smk_parse_item(const char *json, size_t len, art_work_t *work);

// --- Smithsonian ---------------------------------------------------------------------------

/** @brief The search in the American Art Museum: one hit at `start` (`rows` 0: only the count).
 * False for a kind it has no term for. */
bool art_si_search_url(art_type_t type, unsigned start, int rows, const char *key, char *out,
                       size_t out_len);

/** @brief How many works the search found (-1 if the answer is not what it should be). */
int art_si_row_count(const char *json, size_t len);

/** @brief The work of a search answer with one hit (false unless its picture is CC0). */
bool art_si_parse_row(const char *json, size_t len, art_work_t *work);

// --- The picture ---------------------------------------------------------------------------

/**
 * @brief The address of the smallest picture that covers a panel of `max_w` x `max_h` pixels: the
 * IIIF servers fit it into the box (`!w,h`), the Smithsonian takes the longer side.
 */
bool art_image_url(const art_work_t *work, int max_w, int max_h, char *out, size_t out_len);

#endif
