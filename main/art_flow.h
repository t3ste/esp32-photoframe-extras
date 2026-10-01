#ifndef ART_FLOW_H
#define ART_FLOW_H

/**
 * @file art_flow.h
 * @brief One rotation of the artworks mode (build option `artworks`): ask a museum for a work, keep
 * the picture in an album for wakes without network, show it with its caption. See
 * docs/ARTWORKS.md.
 */

#include "esp_err.h"

/**
 * @brief The rotation of the artworks mode. The kind of work is chosen first, then the sources are
 * asked in their fixed order (at most two per rotation) for a work that is public domain or CC0;
 * its picture is loaded in the smallest size that covers the panel, made display-ready, kept in the
 * album (the oldest pictures made by this option are deleted to keep the free space the settings
 * name) and shown with its caption. Without network, or when anything fails, a random picture of
 * the album is shown instead; when there is none the panel keeps its picture.
 *
 * @return ESP_OK when a picture was shown (a new one or one of the album), ESP_FAIL when not.
 */
esp_err_t art_flow_rotate(void);

#endif
