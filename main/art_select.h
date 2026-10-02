#ifndef ART_SELECT_H
#define ART_SELECT_H

/**
 * @file art_select.h
 * @brief What the artworks mode asks for (build option `artworks`): the kind of work and the order
 * of the sources, chosen from the settings and a random number. Pure C, so the host tests link it.
 */

#include <stdbool.h>
#include <stdint.h>

// The kinds of work, chosen first with equal chance among the enabled ones
typedef enum {
    ART_TYPE_PAINTING = 0,
    ART_TYPE_DRAWING = 1,
    ART_TYPE_PRINT = 2,
} art_type_t;
#define ART_TYPE_COUNT 3
#define ART_TYPES_ALL 0x07u  // bit n = art_type_t n

// The sources, always tried in this order
typedef enum {
    ART_SOURCE_RIJKS = 0,
    ART_SOURCE_SMK = 1,
    ART_SOURCE_SMITHSONIAN = 2,
} art_source_t;
#define ART_SOURCE_COUNT 3
#define ART_SOURCES_ALL 0x07u  // bit n = art_source_t n

// The years the Rijksmuseum pick draws from (a year gives at most one page of hits)
#define ART_YEAR_MIN 1400
#define ART_YEAR_MAX 1950

/** @brief A kind of work among the enabled ones (`rnd` picks), or -1 when none is enabled. */
int art_select_type(unsigned type_mask, uint32_t rnd);

/** @brief Whether the source has works of that kind (the Smithsonian has no print term yet). */
bool art_source_supports(art_source_t source, art_type_t type);

/**
 * @brief The enabled sources that have works of the kind, in the fixed order. Returns how many were
 * written to `out`.
 */
int art_select_sources(unsigned source_mask, art_type_t type, art_source_t out[ART_SOURCE_COUNT]);

/** @brief A year from ART_YEAR_MIN to ART_YEAR_MAX. */
int art_select_year(uint32_t rnd);

// How a picture is put on the panel: the whole picture with bars where it does not fill the panel
// (fit), or the panel filled with the edges cut off (cover). Its own setting of this option, not
// the frame's general picture setting.
typedef enum {
    ART_SCALE_FIT = 0,
    ART_SCALE_COVER = 1,
} art_scale_t;

const char *art_scale_name(art_scale_t scale);      // "fit", "cover"
art_scale_t art_scale_from_name(const char *name);  // anything but "cover" is fit

/**
 * @brief The size of the picture area as the viewer sees it, in the orientation the frame is set to
 * (`want_landscape`): the longer side of the panel is the width of a landscape frame, the height of
 * a portrait one - the same rule the frame's picture pipeline follows.
 */
void art_panel_box(int native_w, int native_h, bool want_landscape, int *box_w, int *box_h);

/**
 * @brief Whether a picture of `w` x `h` pixels has the orientation of the frame: a square picture
 * and a picture of unknown size (a side of 0 or less) fit both.
 */
bool art_orientation_matches(int w, int h, bool want_landscape);

/**
 * @brief Which of `count` pictures to show: from the entry `start_rnd` picks, up to `probes`
 * entries in a row (wrapping round) are asked whether they match, and the first one that does is
 * the answer
 * (`*matched` is then true); if none does, the entry it started at (`*matched` false). -1 when
 * `count` is 0. `matches` may be NULL (the start wins at once); `matched` may be NULL.
 */
int art_pick_matching(int count, uint32_t start_rnd, int probes,
                      bool (*matches)(int index, void *context), void *context, bool *matched);

const char *art_type_name(art_type_t type);        // "painting", "drawing", "print"
const char *art_source_name(art_source_t source);  // "Rijksmuseum", "SMK", "Smithsonian"
const char *art_source_tag(art_source_t source);   // "rijks", "smk", "si" - file name prefix

#endif
