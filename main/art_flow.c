#include "art_flow.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "album_manager.h"
#include "art_caption.h"
#include "art_select.h"
#include "art_sources.h"
#include "art_store.h"
#include "board_hal.h"
#include "config.h"
#include "config_manager.h"
#include "display_manager.h"
#include "esp_heap_caps.h"
#include "esp_littlefs.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_vfs_fat.h"
#include "http_fetch.h"
#include "image_processor.h"
#include "processing_settings.h"
#include "storage.h"
#include "utils.h"
#include "wifi_manager.h"

static const char *TAG = "art_flow";

// An identifying User-Agent with the repository this firmware was built for (like the weather
// fetch): the museums ask for contact information
#define ART_USER_AGENT "esp32-photoframe-artworks/1.0 github.com/" CONFIG_FORK_OTA_REPO

#define ART_JSON_TIMEOUT_MS 20000
#define ART_JSON_MAX_BYTES (64 * 1024)
#define ART_IMAGE_TIMEOUT_MS 40000
#define ART_IMAGE_MAX_BYTES (1536 * 1024)
#define ART_THUMBNAIL_MAX_DIMENSION 300
#define ART_PICKS_PER_SOURCE 3        // works tried per source (a year without works, no CC0 ...)
#define ART_SOURCES_PER_ROTATION 2    // sources asked per rotation
#define ART_ITEMS_MAX 1000            // pictures of this option an album keeps at most
#define ART_CAPTION_CHARS 60          // the caption is cut to the panel's width when it is drawn
#define ART_TEMP_BASE ".art_current"  // the picture of a rotation that is not kept

// The works the Smithsonian is known to have per kind (American Art Museum, 2026): a start inside
// always has a hit, so one request is enough (the demo key allows 10 an hour)
static const unsigned SMITHSONIAN_POOL[ART_TYPE_COUNT] = {3500, 2000, 0};

// ---------------------------------------------------------------------------------------------
// Fetching

// A JSON answer. `once`: ask only once and accept only a 200 (the Smithsonian's quota).
static bool get_json(const char *url, bool once, char **body, size_t *len)
{
    *body = NULL;
    *len = 0;
    if (once) {
        int status = 0;
        esp_err_t err = http_fetch_get_once(url, ART_JSON_TIMEOUT_MS, ART_JSON_MAX_BYTES, body, len,
                                            &status, ART_USER_AGENT);
        if (err != ESP_OK || status != 200 || !*body) {
            ESP_LOGW(TAG, "Request failed (HTTP %d)", status);
            free(*body);
            *body = NULL;
            return false;
        }
        return true;
    }
    bool truncated = false;
    esp_err_t err = http_fetch_get(url, ART_JSON_TIMEOUT_MS, ART_JSON_MAX_BYTES, body, len,
                                   &truncated, ART_USER_AGENT);
    if (err != ESP_OK || !*body || truncated) {
        ESP_LOGW(TAG, "Request failed: %s", esp_err_to_name(err));
        free(*body);
        *body = NULL;
        return false;
    }
    return true;
}

static bool fetch_rijks(art_type_t type, art_work_t *work)
{
    for (int pick = 0; pick < ART_PICKS_PER_SOURCE; pick++) {
        char url[ART_URL_MAX * 2];
        if (!art_rijks_search_url(type, art_select_year(esp_random()), url, sizeof(url))) {
            return false;
        }
        char *body;
        size_t len;
        if (!get_json(url, false, &body, &len)) {
            return false;
        }
        char record[ART_URL_MAX];
        int hits = art_rijks_pick_object(body, len, esp_random(), record, sizeof(record));
        free(body);
        if (hits == 0) {
            continue;  // a year without works: another year
        }

        char visual[ART_URL_MAX], digital[ART_URL_MAX];
        if (!get_json(record, false, &body, &len)) {
            return false;
        }
        bool ok = art_rijks_parse_object(body, len, work, visual, sizeof(visual));
        free(body);
        if (!ok) {
            continue;
        }
        if (!get_json(visual, false, &body, &len)) {
            return false;
        }
        ok = art_rijks_parse_visual_item(body, len, work, digital, sizeof(digital));
        free(body);
        if (!ok) {
            continue;  // the picture is not public domain: another work
        }
        if (!get_json(digital, false, &body, &len)) {
            return false;
        }
        ok = art_rijks_parse_digital_object(body, len, work);
        free(body);
        if (ok) {
            return true;
        }
    }
    return false;
}

static bool fetch_smk(art_type_t type, art_work_t *work)
{
    char url[ART_URL_MAX * 2];
    char *body;
    size_t len;
    if (!art_smk_search_url(type, 0, 0, url, sizeof(url)) || !get_json(url, false, &body, &len)) {
        return false;
    }
    int found = art_smk_found(body, len);
    free(body);
    if (found <= 0) {
        return false;
    }
    for (int pick = 0; pick < ART_PICKS_PER_SOURCE; pick++) {
        if (!art_smk_search_url(type, esp_random() % (unsigned) found, 1, url, sizeof(url)) ||
            !get_json(url, false, &body, &len)) {
            return false;
        }
        bool ok = art_smk_parse_item(body, len, work);
        free(body);
        if (ok) {
            return true;
        }
    }
    return false;
}

static bool fetch_smithsonian(art_type_t type, art_work_t *work)
{
    if (SMITHSONIAN_POOL[type] == 0) {
        return false;
    }
    const char *key = config_manager_get_art_si_key();
    if (!art_si_key_valid(key)) {
        key = ART_SMITHSONIAN_DEMO_KEY;
    }
    char url[ART_URL_MAX * 3];
    char *body;
    size_t len;
    // one request: the quota is small (see SMITHSONIAN_POOL)
    if (!art_si_search_url(type, esp_random() % SMITHSONIAN_POOL[type], 1, key, url, sizeof(url)) ||
        !get_json(url, true, &body, &len)) {
        return false;
    }
    bool ok = art_si_parse_row(body, len, work);
    free(body);
    return ok;
}

// A work of a kind of work from the first sources that give one
static bool fetch_work(art_work_t *work)
{
    int type = art_select_type(config_manager_get_art_types(), esp_random());
    if (type < 0) {
        ESP_LOGW(TAG, "No kind of work is enabled");
        return false;
    }
    art_source_t sources[ART_SOURCE_COUNT];
    int count = art_select_sources(config_manager_get_art_sources(), (art_type_t) type, sources);
    if (count == 0) {
        ESP_LOGW(TAG, "No source is enabled for %s", art_type_name((art_type_t) type));
        return false;
    }
    for (int i = 0; i < count && i < ART_SOURCES_PER_ROTATION; i++) {
        ESP_LOGI(TAG, "Asking %s for a %s", art_source_name(sources[i]),
                 art_type_name((art_type_t) type));
        bool ok = false;
        switch (sources[i]) {
        case ART_SOURCE_RIJKS:
            ok = fetch_rijks((art_type_t) type, work);
            break;
        case ART_SOURCE_SMK:
            ok = fetch_smk((art_type_t) type, work);
            break;
        case ART_SOURCE_SMITHSONIAN:
            ok = fetch_smithsonian((art_type_t) type, work);
            break;
        }
        if (ok) {
            ESP_LOGI(TAG, "Work: %s - %s (%s), %s", work->artist, work->title, work->year,
                     work->rights);
            return true;
        }
    }
    return false;
}

// The smallest picture that covers the panel (the panel as the frame is mounted)
static bool download_picture(const art_work_t *work, char **data, size_t *len)
{
    int width = BOARD_HAL_DISPLAY_WIDTH;
    int height = BOARD_HAL_DISPLAY_HEIGHT;
    int rotation = config_manager_get_display_rotation_deg() % 360;
    if (rotation == 90 || rotation == 270 || rotation == -90 || rotation == -270) {
        int swap = width;
        width = height;
        height = swap;
    }
    char url[ART_URL_MAX + 48];
    if (!art_image_url(work, width, height, url, sizeof(url))) {
        return false;
    }
    bool truncated = false;
    esp_err_t err = http_fetch_get(url, ART_IMAGE_TIMEOUT_MS, ART_IMAGE_MAX_BYTES, data, len,
                                   &truncated, ART_USER_AGENT);
    if (err != ESP_OK || !*data || truncated || *len < 1024) {
        ESP_LOGW(TAG, "Picture download failed: %s", esp_err_to_name(err));
        free(*data);
        *data = NULL;
        return false;
    }
    if (!art_jpeg_is_baseline((const uint8_t *) *data, *len)) {
        ESP_LOGW(TAG, "The picture is not a baseline JPEG, the frame cannot decode it");
        free(*data);
        *data = NULL;
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------------------------
// Keeping and showing

static bool storage_space(uint64_t *total, uint64_t *free_bytes)
{
    storage_type_t type = storage_get_type();
    if (type == STORAGE_TYPE_SDCARD) {
        uint64_t t = 0, f = 0;
        if (esp_vfs_fat_info(FS_MOUNT_POINT, &t, &f) == ESP_OK) {
            *total = t;
            *free_bytes = f;
            return true;
        }
    } else if (type == STORAGE_TYPE_LITTLEFS) {
        size_t t = 0, u = 0;
        if (esp_littlefs_info(LITTLEFS_PARTITION_LABEL, &t, &u) == ESP_OK) {
            *total = t;
            *free_bytes = (t > u) ? (t - u) : 0;
            return true;
        }
    }
    return false;
}

// The album folder; a new album is switched on for the rotation of the storage mode (the user can
// switch it off in the gallery), so the pictures are there for wakes without network
static bool ensure_album(char *dir, size_t dir_len)
{
    const char *album = config_manager_get_art_album();
    if (!album_manager_album_exists(album)) {
        if (album_manager_create_album(album) != ESP_OK) {
            return false;
        }
        album_manager_set_album_enabled(album, true);
    }
    return album_manager_get_album_path(album, dir, dir_len) == ESP_OK;
}

// The display-ready file of a picture that is already in the album, or false
static bool stored_picture(const char *dir, const char *base, char *path, size_t path_len)
{
    static const char *const EXTENSIONS[] = {".epdgz", ".png"};
    for (size_t i = 0; i < sizeof(EXTENSIONS) / sizeof(EXTENSIONS[0]); i++) {
        struct stat st;
        snprintf(path, path_len, "%s/%s%s", dir, base, EXTENSIONS[i]);
        if (stat(path, &st) == 0) {
            return true;
        }
    }
    return false;
}

// Makes room for the new picture `base` in the album by the free-space rule of the settings: false
// when it must not be kept. Only the older pictures of this option are ever deleted.
static bool make_room(const char *dir, const char *base)
{
    art_item_t *items = heap_caps_malloc(ART_ITEMS_MAX * sizeof(art_item_t), MALLOC_CAP_SPIRAM);
    if (!items) {
        return true;  // no memory to look: keep it, the next rotation tries again
    }
    int count = art_store_scan(dir, items, ART_ITEMS_MAX);
    int older = 0;  // the new picture is not one of the candidates
    for (int i = 0; i < count; i++) {
        if (strcmp(items[i].base, base) != 0) {
            items[older++] = items[i];
        }
    }
    count = older;
    int delete_count = 0;
    bool keep = true;
    uint64_t total = 0, free_bytes = 0;
    if (storage_space(&total, &free_bytes)) {
        art_plan_t plan = art_store_plan(total, free_bytes, 0, config_manager_get_art_free_min(),
                                         config_manager_get_art_free_target(), items, count);
        keep = plan.can_save;
        delete_count = plan.delete_count;
        ESP_LOGI(TAG, "Storage %llu of %llu bytes free: %s, %d old picture(s) to delete",
                 (unsigned long long) free_bytes, (unsigned long long) total,
                 keep ? "keeping the new one" : "not keeping the new one", delete_count);
    }
    if (keep && count >= ART_ITEMS_MAX - 1) {  // the album is never longer than this
        int extra = count - (ART_ITEMS_MAX - 1);
        if (extra > delete_count) {
            delete_count = extra;
        }
    }
    for (int i = 0; keep && i < delete_count; i++) {
        art_store_remove(dir, items[i].base);
    }
    heap_caps_free(items);
    return keep;
}

// Turns the downloaded JPEG into a display-ready picture named `base` in `dir` with its thumbnail
// and caption file. On success `path` names the display-ready file.
static bool make_picture(const art_work_t *work, const char *data, size_t len, const char *dir,
                         const char *base, uint32_t seq, char *path, size_t path_len)
{
    char jpg[320];
    snprintf(jpg, sizeof(jpg), "%s/%s.jpg", dir, base);
    snprintf(path, path_len, "%s/%s.epdgz", dir, base);

    FILE *file = fopen(jpg, "wb");
    if (!file) {
        ESP_LOGW(TAG, "Cannot write %s", jpg);
        return false;
    }
    bool written = fwrite(data, 1, len, file) == len;
    written = (fclose(file) == 0) && written;
    if (!written) {
        art_store_remove(dir, base);
        return false;
    }

    image_format_t actual = IMAGE_FORMAT_EPD_GZ;
    esp_err_t err = image_processor_process_fmt(
        jpg, path, processing_settings_get_dithering_algorithm(), IMAGE_FORMAT_EPD_GZ, &actual);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Processing failed: %s", esp_err_to_name(err));
        art_store_remove(dir, base);
        return false;
    }
    if (actual != IMAGE_FORMAT_EPD_GZ) {  // written as a PNG for lack of memory
        snprintf(path, path_len, "%s/%s.png", dir, base);
    }

    // The downloaded JPEG becomes the preview of the web gallery (the true colours of the original)
    if (image_processor_make_thumbnail_from_original(jpg, IMAGE_FORMAT_JPG,
                                                     ART_THUMBNAIL_MAX_DIMENSION, jpg) != ESP_OK) {
        unlink(jpg);
    }

    char caption[ART_CAPTION_TEXT_MAX];
    art_caption_compose(work->artist, work->title, work->year, ART_CAPTION_CHARS, caption,
                        sizeof(caption));
    if (!art_store_write_caption(dir, base, seq, work, caption)) {
        ESP_LOGW(TAG, "Cannot write the caption file");
        art_store_remove(dir, base);
        return false;
    }
    return true;
}

// The work is chosen: show its picture - the stored one, or a new one made from `data`
static bool show_new_work(const art_work_t *work)
{
    bool keep_wanted = config_manager_get_art_save() && storage_has_persistent_storage();
    char dir[256];
    char base[ART_BASE_MAX];
    if (keep_wanted && ensure_album(dir, sizeof(dir))) {
        art_store_file_base(work, base, sizeof(base));
    } else {
        keep_wanted = false;
        snprintf(dir, sizeof(dir), "%s", FS_MOUNT_POINT);
        snprintf(base, sizeof(base), "%s", ART_TEMP_BASE);
    }

    char path[320];
    if (keep_wanted && stored_picture(dir, base, path, sizeof(path))) {
        ESP_LOGI(TAG, "Already in the album: %s", path);
        display_manager_show_album_file(path);
        return true;
    }

    char *data = NULL;
    size_t len = 0;
    if (!download_picture(work, &data, &len)) {
        return false;
    }
    art_store_remove(FS_MOUNT_POINT, ART_TEMP_BASE);  // what a rotation that was cut short left
    bool made =
        make_picture(work, data, len, dir, base, config_manager_next_art_seq(), path, sizeof(path));
    free(data);
    if (!made) {
        return false;
    }
    bool keep = keep_wanted && make_room(dir, base);
    display_manager_show_album_file(path);
    if (!keep) {
        art_store_remove(dir, base);  // shown, not kept
    }
    return true;
}

// ---------------------------------------------------------------------------------------------

esp_err_t art_flow_rotate(void)
{
    const char *reason = "No new artwork";
    if (!wifi_manager_is_connected()) {
        reason = "WiFi unavailable";
    } else {
        art_work_t work;
        memset(&work, 0, sizeof(work));
        bool fetched = fetch_work(&work);
        if (fetched && show_new_work(&work)) {
            utils_record_internet_attempt(true);
            utils_set_last_fetch_error(NULL);
            return ESP_OK;
        }
        utils_record_internet_attempt(false);
        reason = fetched ? "Artwork picture failed" : "No artwork found";
    }

    // No network or anything failed: a picture of the album instead
    ESP_LOGW(TAG, "%s - showing a picture of the album", reason);
    if (display_manager_rotate_from_album(config_manager_get_art_album())) {
        utils_set_last_fetch_error(reason);
        return ESP_OK;
    }
    ESP_LOGW(TAG, "The album has no picture; keeping the current one");
    utils_set_last_fetch_error(reason);
    return ESP_FAIL;
}
