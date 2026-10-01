#include "art_select.h"

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
