#ifndef ART_STORE_H
#define ART_STORE_H

/**
 * @file art_store.h
 * @brief The album of the artworks mode (build option `artworks`): the file names, the caption
 * file next to each picture and the rule that keeps free space. Plain C on the C library's file
 * functions, so the host tests link it and run it on a temporary directory.
 *
 * A picture of the album is `<base>.epdgz` or `<base>.png` (the display-ready file) with the
 * thumbnail `<base>.jpg` and the caption file `<base>.caption.json`; `<base>` is the source and the
 * id of the work (`rijks-200100988`). The caption file also marks the picture as one this option
 * made: only such pictures are ever deleted.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "art_sources.h"

#define ART_BASE_MAX 64
#define ART_CAPTION_TEXT_MAX 120
#define ART_CAPTION_SUFFIX ".caption.json"

// Keep this much free (percent of the whole storage), and clean up to this much (setting)
#define ART_FREE_MIN_DEFAULT 20
#define ART_FREE_TARGET_DEFAULT 30

typedef struct {
    char base[ART_BASE_MAX];
    uint64_t bytes;  // all files of the picture
    uint32_t seq;    // when it was saved: the lower, the older
} art_item_t;

typedef struct {
    int delete_count;  // how many of the oldest pictures to delete before saving
    bool can_save;
} art_plan_t;

/** @brief The two limits in range: the minimum 5-80, the target above it and at most 95. */
void art_store_clamp_limits(int *free_min_pct, int *free_target_pct);

/**
 * @brief What to do before saving a picture of `new_bytes` bytes. While the free space after saving
 * would be at or below `free_min_pct` of `total`, the oldest pictures (`items` sorted oldest first)
 * are deleted until the free space is at least `free_target_pct`. When that is not possible - all
 * of them gone and still not above the minimum - nothing is deleted and the picture is not saved.
 */
art_plan_t art_store_plan(uint64_t total, uint64_t free_bytes, uint64_t new_bytes, int free_min_pct,
                          int free_target_pct, const art_item_t *items, int count);

/** @brief `<source>-<id>`, e.g. `rijks-200100988`. */
void art_store_file_base(const art_work_t *work, char *out, size_t out_len);

/** @brief The caption file that belongs to a picture file (same name, `.caption.json`). */
bool art_store_caption_path(const char *picture_path, char *out, size_t out_len);

/**
 * @brief Writes the caption file of a picture into `dir`. The size of the original (`work->width`
 * x `work->height`, when known) goes into it too: it tells the frame whether the picture is
 * landscape or portrait, which the display-ready file does not.
 */
bool art_store_write_caption(const char *dir, const char *base, uint32_t seq,
                             const art_work_t *work, const char *text);

/** @brief The size of the original of a picture, if its caption file of this option has one. */
bool art_store_read_size(const char *picture_path, int *width, int *height);

/** @brief The caption text of a picture, if it has a caption file of this option. */
bool art_store_read_caption(const char *picture_path, char *text, size_t text_len);

/** @brief The pictures of this option in `dir`, oldest first. Returns how many (at most
 * `max_items`). */
int art_store_scan(const char *dir, art_item_t *items, int max_items);

/** @brief Deletes all files of a picture. */
void art_store_remove(const char *dir, const char *base);

#endif
