#include "art_select.h"

#include <string.h>

static int count_bits(unsigned mask)
{
    int n = 0;
    for (int i = 0; i < 8; i++) {
        if (mask & (1u << i)) {
            n++;
        }
    }
    return n;
}

int art_select_type(unsigned type_mask, uint32_t rnd)
{
    type_mask &= ART_TYPES_ALL;
    int enabled = count_bits(type_mask);
    if (enabled == 0) {
        return -1;
    }
    int pick = (int) (rnd % (uint32_t) enabled);
    for (int type = 0; type < ART_TYPE_COUNT; type++) {
        if (type_mask & (1u << type)) {
            if (pick == 0) {
                return type;
            }
            pick--;
        }
    }
    return -1;
}

bool art_source_supports(art_source_t source, art_type_t type)
{
    // SAAM has no print term that returns works; left out until one is found
    return !(source == ART_SOURCE_SMITHSONIAN && type == ART_TYPE_PRINT);
}

int art_select_sources(unsigned source_mask, art_type_t type, art_source_t out[ART_SOURCE_COUNT])
{
    int count = 0;
    for (int source = 0; source < ART_SOURCE_COUNT; source++) {
        if ((source_mask & (1u << source)) && art_source_supports((art_source_t) source, type)) {
            out[count++] = (art_source_t) source;
        }
    }
    return count;
}

int art_select_year(uint32_t rnd)
{
    return ART_YEAR_MIN + (int) (rnd % (uint32_t) (ART_YEAR_MAX - ART_YEAR_MIN + 1));
}

const char *art_scale_name(art_scale_t scale)
{
    return scale == ART_SCALE_COVER ? "cover" : "fit";
}

art_scale_t art_scale_from_name(const char *name)
{
    return (name && strcmp(name, "cover") == 0) ? ART_SCALE_COVER : ART_SCALE_FIT;
}

void art_panel_box(int native_w, int native_h, bool want_landscape, int *box_w, int *box_h)
{
    int longer = native_w > native_h ? native_w : native_h;
    int shorter = native_w > native_h ? native_h : native_w;
    *box_w = want_landscape ? longer : shorter;
    *box_h = want_landscape ? shorter : longer;
}

bool art_orientation_matches(int w, int h, bool want_landscape)
{
    if (w <= 0 || h <= 0 || w == h) {
        return true;
    }
    return (w > h) == want_landscape;
}

int art_pick_matching(int count, uint32_t start_rnd, int probes,
                      bool (*matches)(int index, void *context), void *context, bool *matched)
{
    if (matched) {
        *matched = false;
    }
    if (count <= 0) {
        return -1;
    }
    int start = (int) (start_rnd % (uint32_t) count);
    if (!matches) {
        return start;
    }
    for (int step = 0; step < probes && step < count; step++) {
        int index = (start + step) % count;
        if (matches(index, context)) {
            if (matched) {
                *matched = true;
            }
            return index;
        }
    }
    return start;
}

const char *art_type_name(art_type_t type)
{
    switch (type) {
    case ART_TYPE_DRAWING:
        return "drawing";
    case ART_TYPE_PRINT:
        return "print";
    case ART_TYPE_PAINTING:
    default:
        return "painting";
    }
}

const char *art_source_name(art_source_t source)
{
    switch (source) {
    case ART_SOURCE_SMK:
        return "SMK";
    case ART_SOURCE_SMITHSONIAN:
        return "Smithsonian";
    case ART_SOURCE_RIJKS:
    default:
        return "Rijksmuseum";
    }
}

const char *art_source_tag(art_source_t source)
{
    switch (source) {
    case ART_SOURCE_SMK:
        return "smk";
    case ART_SOURCE_SMITHSONIAN:
        return "si";
    case ART_SOURCE_RIJKS:
    default:
        return "rijks";
    }
}
