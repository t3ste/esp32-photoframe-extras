// Host-test stubs for what the optional image pipeline (CONFIG_FORK_IMAGE_PIPELINE) links
// on top of the upstream one: the streaming JPEG decoder and a few config accessors.
// Only the "fork" variants of the image pipeline / display flow tests compile this file.
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "config_manager.h"
#include "tjpgd.h"

JRESULT jd_prepare(JDEC *jd, unsigned int (*infunc)(JDEC *, uint8_t *, unsigned int), void *pool,
                   size_t sz_pool, void *dev)
{
    (void) infunc;
    (void) pool;
    (void) sz_pool;
    jd->device = dev;
    jd->width = 0;
    jd->height = 0;
    return JDR_INP;
}

JRESULT jd_decomp(JDEC *jd, int (*outfunc)(JDEC *, void *, JRECT *), uint8_t scale)
{
    (void) jd;
    (void) outfunc;
    (void) scale;
    return JDR_INP;
}

bool config_manager_get_caption_invert_colors_enabled(void)
{
    return false;
}

const char *config_manager_get_weather_icon_set(void)
{
    return "none";
}

bool config_manager_get_weather_icon_colored(void)
{
    return false;
}
