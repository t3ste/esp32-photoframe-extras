#include "dedup_service.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "album_manager.h"
#include "cJSON.h"
#include "config.h"
#include "config_manager.h"
#include "dedup_payload.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "power_manager.h"
#include "storage.h"

static const char *TAG = "dedup_service";

// One lock for every index access: the web server task and the background indexing both write.
static SemaphoreHandle_t index_lock(void)
{
    static SemaphoreHandle_t lock = NULL;
    static portMUX_TYPE init_mux = portMUX_INITIALIZER_UNLOCKED;
    if (!lock) {
        SemaphoreHandle_t fresh = xSemaphoreCreateMutex();
        portENTER_CRITICAL(&init_mux);
        if (!lock) {
            lock = fresh;
            fresh = NULL;
        }
        portEXIT_CRITICAL(&init_mux);
        if (fresh) {
            vSemaphoreDelete(fresh);
        }
    }
    return lock;
}

static void lock_index(void)
{
    SemaphoreHandle_t lock = index_lock();
    if (lock) {
        xSemaphoreTake(lock, portMAX_DELAY);
    }
}

static void unlock_index(void)
{
    SemaphoreHandle_t lock = index_lock();
    if (lock) {
        xSemaphoreGive(lock);
    }
}

dedup_mode_t dedup_service_mode(void)
{
    int mode = config_manager_get_dedup_mode();
    return (mode == DEDUP_MODE_OFF || mode == DEDUP_MODE_WARN) ? (dedup_mode_t) mode
                                                               : DEDUP_MODE_SKIP;
}

dedup_hash_t dedup_service_kind(void)
{
    return config_manager_get_dedup_hash() == 1 ? DEDUP_HASH_PAYLOAD : DEDUP_HASH_STORED;
}

esp_err_t dedup_service_hash(const char *path, dedup_digest_t *out)
{
    if (dedup_service_kind() == DEDUP_HASH_PAYLOAD) {
        esp_err_t err = dedup_payload_md5(path, out);
        if (err != ESP_ERR_NOT_SUPPORTED) {
            return err;
        }
        // not decodable here (a .bmp, an interlaced PNG): the bytes have to do
    }
    return dedup_md5_file(path, out);
}

bool dedup_service_find(const char *album_dir, const dedup_digest_t *d, const char *ignore_name,
                        char *found, size_t found_len)
{
    lock_index();
    bool hit = dedup_index_find(album_dir, dedup_service_kind(), d, ignore_name, found, found_len);
    unlock_index();
    return hit;
}

void dedup_service_record(const char *album_dir, const char *name, const dedup_digest_t *d)
{
    lock_index();
    esp_err_t err = dedup_index_set(album_dir, dedup_service_kind(), d, name);
    unlock_index();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Could not record %s in the duplicate index", name);
    }
}

void dedup_service_forget(const char *album_dir, const char *name)
{
    lock_index();
    dedup_index_remove(album_dir, name);
    unlock_index();
}

// ---- background indexing --------------------------------------------------------------------

#define SCAN_ALBUM_MAX 128
#define SCAN_TASK_STACK 8192

static struct {
    volatile bool running;
    bool finished;                   // a scan has completed since boot
    char requested[SCAN_ALBUM_MAX];  // "" = every album
    char current[SCAN_ALBUM_MAX];    // the album being worked on
    int total;                       // images to look at
    int done;                        // looked at so far
    int indexed;                     // newly hashed
    int failed;                      // could not be hashed
} scan;

static bool wanted_file(const char *name)
{
    return name[0] != '.' && dedup_is_image_name(name);
}

// Number of image files in an album folder.
static int count_album_images(const char *album_dir)
{
    DIR *dir = opendir(album_dir);
    if (!dir) {
        return 0;
    }
    int n = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        n += wanted_file(entry->d_name) ? 1 : 0;
    }
    closedir(dir);
    return n;
}

static void index_album(const char *album_name)
{
    char album_dir[256];
    if (album_manager_get_album_path(album_name, album_dir, sizeof(album_dir)) != ESP_OK) {
        return;
    }
    DIR *dir = opendir(album_dir);
    if (!dir) {
        return;
    }
    snprintf(scan.current, sizeof(scan.current), "%s", album_name);
    dedup_hash_t kind = dedup_service_kind();
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (!wanted_file(entry->d_name)) {
            continue;
        }
        char name[256], path[512];
        snprintf(name, sizeof(name), "%s", entry->d_name);
        snprintf(path, sizeof(path), "%s/%s", album_dir, name);

        lock_index();
        bool known = dedup_index_has(album_dir, kind, name);
        unlock_index();
        if (!known) {
            // hashed without the lock (it takes a while); if the file changed meanwhile - an upload
            // replaced it - the entry the upload recorded is the right one, so this one is dropped
            struct stat before, after;
            dedup_digest_t digest;
            bool ok = stat(path, &before) == 0 && dedup_service_hash(path, &digest) == ESP_OK &&
                      stat(path, &after) == 0 && before.st_mtime == after.st_mtime &&
                      before.st_size == after.st_size;
            if (ok) {
                lock_index();
                esp_err_t err = dedup_index_set(album_dir, kind, &digest, name);
                unlock_index();
                ok = (err == ESP_OK);
            }
            if (ok) {
                scan.indexed++;
            } else {
                scan.failed++;
            }
        }
        scan.done++;
        power_manager_reset_sleep_timer();  // do not go to sleep in the middle of it
        vTaskDelay(pdMS_TO_TICKS(10));      // let the web server and the idle task breathe
    }
    closedir(dir);
}

static void scan_task(void *arg)
{
    (void) arg;
    char **albums = NULL;
    int count = 0;
    if (scan.requested[0] != '\0') {
        albums = malloc(sizeof(char *));
        if (albums) {
            albums[0] = strdup(scan.requested);
            count = albums[0] ? 1 : 0;
        }
    } else if (album_manager_list_albums(&albums, &count) != ESP_OK) {
        albums = NULL;
        count = 0;
    }

    for (int i = 0; i < count; i++) {
        char album_dir[256];
        if (album_manager_get_album_path(albums[i], album_dir, sizeof(album_dir)) == ESP_OK) {
            scan.total += count_album_images(album_dir);
        }
    }
    ESP_LOGI(TAG, "Indexing %d image(s) in %d album(s)", scan.total, count);
    for (int i = 0; i < count; i++) {
        index_album(albums[i]);
    }
    ESP_LOGI(TAG, "Indexing finished: %d new, %d could not be read", scan.indexed, scan.failed);
    album_manager_free_album_list(albums, count);

    scan.current[0] = '\0';
    scan.finished = true;
    scan.running = false;
    vTaskDelete(NULL);
}

esp_err_t dedup_service_scan_start(const char *album)
{
    if (!storage_has_persistent_storage()) {
        return ESP_ERR_NOT_FOUND;
    }
    lock_index();  // also guards the check-and-set below
    if (scan.running) {
        unlock_index();
        return ESP_ERR_INVALID_STATE;
    }
    memset(&scan, 0, sizeof(scan));
    if (album && album[0] != '\0') {
        snprintf(scan.requested, sizeof(scan.requested), "%s", album);
    }
    scan.running = true;
    unlock_index();
    if (xTaskCreate(scan_task, "dedup_scan", SCAN_TASK_STACK, NULL, tskIDLE_PRIORITY + 1, NULL) !=
        pdPASS) {
        scan.running = false;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

bool dedup_service_scan_running(void)
{
    return scan.running;
}

char *dedup_service_status_json(void)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        return NULL;
    }
    cJSON_AddBoolToObject(root, "running", scan.running);
    cJSON_AddBoolToObject(root, "finished", scan.finished);
    cJSON_AddStringToObject(root, "album", scan.current);
    cJSON_AddNumberToObject(root, "total", scan.total);
    cJSON_AddNumberToObject(root, "done", scan.done);
    cJSON_AddNumberToObject(root, "indexed", scan.indexed);
    cJSON_AddNumberToObject(root, "failed", scan.failed);
    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json;
}

// ---- the duplicate report -------------------------------------------------------------------

static void add_group(const char *const *names, int count, void *ctx)
{
    cJSON *group = cJSON_CreateArray();
    if (!group) {
        return;
    }
    for (int i = 0; i < count; i++) {
        cJSON_AddItemToArray(group, cJSON_CreateString(names[i]));
    }
    cJSON_AddItemToArray((cJSON *) ctx, group);
}

char *dedup_service_report_json(const char *album)
{
    char album_dir[256];
    if (!album || album_manager_get_album_path(album, album_dir, sizeof(album_dir)) != ESP_OK) {
        return NULL;
    }
    cJSON *root = cJSON_CreateObject();
    cJSON *groups = cJSON_CreateArray();
    if (!root || !groups) {
        cJSON_Delete(root);
        cJSON_Delete(groups);
        return NULL;
    }
    int images = 0, indexed = 0;
    lock_index();
    esp_err_t err =
        dedup_index_groups(album_dir, dedup_service_kind(), add_group, groups, &images, &indexed);
    unlock_index();
    if (err != ESP_OK) {
        cJSON_Delete(root);
        cJSON_Delete(groups);
        return NULL;
    }
    cJSON_AddStringToObject(root, "album", album);
    cJSON_AddStringToObject(root, "hash",
                            dedup_service_kind() == DEDUP_HASH_PAYLOAD ? "payload" : "stored");
    cJSON_AddNumberToObject(root, "images", images);
    cJSON_AddNumberToObject(root, "indexed", indexed);
    cJSON_AddItemToObject(root, "groups", groups);
    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json;
}
