#include "art_store.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "cJSON.h"

// The extensions of the files a picture can have
static const char *const PICTURE_EXTENSIONS[] = {".epdgz", ".png", ".bmp", ".jpg",
                                                 ART_CAPTION_SUFFIX};
#define PICTURE_EXTENSION_COUNT (sizeof(PICTURE_EXTENSIONS) / sizeof(PICTURE_EXTENSIONS[0]))

// Left-over caption files of deleted pictures that one scan cleans up (the rest follows next time)
#define ART_ORPHANS_PER_SCAN 8

void art_store_clamp_limits(int *free_min_pct, int *free_target_pct)
{
    if (*free_min_pct < 5) {
        *free_min_pct = 5;
    }
    if (*free_min_pct > 80) {
        *free_min_pct = 80;
    }
    if (*free_target_pct <= *free_min_pct) {
        *free_target_pct = *free_min_pct + 1;
    }
    if (*free_target_pct > 95) {
        *free_target_pct = 95;
        if (*free_min_pct >= 95) {
            *free_min_pct = 94;
        }
    }
}

art_plan_t art_store_plan(uint64_t total, uint64_t free_bytes, uint64_t new_bytes, int free_min_pct,
                          int free_target_pct, const art_item_t *items, int count)
{
    art_plan_t plan = {0, false};
    if (total == 0) {
        return plan;
    }
    art_store_clamp_limits(&free_min_pct, &free_target_pct);
    uint64_t min_bytes_x100 = (uint64_t) free_min_pct * total;
    uint64_t target_bytes_x100 = (uint64_t) free_target_pct * total;

    uint64_t available = free_bytes > new_bytes ? free_bytes - new_bytes : 0;
    if (available * 100 > min_bytes_x100) {
        plan.can_save = true;  // still above the minimum after saving
        return plan;
    }
    uint64_t gained = 0;
    for (int i = 0; i < count; i++) {
        gained += items[i].bytes;
        if ((available + gained) * 100 >= target_bytes_x100) {
            plan.delete_count = i + 1;
            plan.can_save = true;
            return plan;
        }
    }
    if ((available + gained) * 100 > min_bytes_x100) {
        plan.delete_count = count;  // not up to the target, but above the minimum
        plan.can_save = true;
    }
    return plan;  // otherwise: delete nothing, do not save
}

void art_store_file_base(const art_work_t *work, char *out, size_t out_len)
{
    snprintf(out, out_len, "%s-%s", art_source_tag(work->source), work->id);
}

// The path without its extension (the extension is what follows the last '.' after the last '/')
static bool without_extension(const char *path, char *out, size_t out_len)
{
    const char *slash = strrchr(path, '/');
    const char *dot = strrchr(path, '.');
    if (!dot || (slash && dot < slash)) {
        return false;
    }
    size_t len = (size_t) (dot - path);
    if (len + 1 > out_len) {
        return false;
    }
    memcpy(out, path, len);
    out[len] = '\0';
    return true;
}

bool art_store_caption_path(const char *picture_path, char *out, size_t out_len)
{
    char stem[256];
    if (!picture_path || !without_extension(picture_path, stem, sizeof(stem))) {
        return false;
    }
    int n = snprintf(out, out_len, "%s%s", stem, ART_CAPTION_SUFFIX);
    return n > 0 && (size_t) n < out_len;
}

bool art_store_write_caption(const char *dir, const char *base, uint32_t seq,
                             const art_work_t *work, const char *text)
{
    char path[256];
    if (snprintf(path, sizeof(path), "%s/%s%s", dir, base, ART_CAPTION_SUFFIX) >=
        (int) sizeof(path)) {
        return false;
    }
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        return false;
    }
    cJSON_AddStringToObject(root, "kind", "art");
    cJSON_AddNumberToObject(root, "v", 1);
    cJSON_AddNumberToObject(root, "n", (double) seq);
    cJSON_AddStringToObject(root, "text", text ? text : "");
    cJSON_AddStringToObject(root, "source", art_source_tag(work->source));
    cJSON_AddStringToObject(root, "id", work->id);
    cJSON_AddStringToObject(root, "rights", work->rights);
    if (work->width > 0 && work->height > 0) {
        cJSON_AddNumberToObject(root, "w", work->width);
        cJSON_AddNumberToObject(root, "h", work->height);
    }
    char *body = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!body) {
        return false;
    }
    FILE *file = fopen(path, "wb");
    bool ok = false;
    if (file) {
        size_t len = strlen(body);
        ok = fwrite(body, 1, len, file) == len;
        ok = (fclose(file) == 0) && ok;
    }
    free(body);
    if (!ok) {
        remove(path);
    }
    return ok;
}

// Reads a caption file (a few hundred bytes) into a JSON tree; NULL unless it is one of this option
static cJSON *read_caption_file(const char *path)
{
    FILE *file = fopen(path, "rb");
    if (!file) {
        return NULL;
    }
    char body[1024];
    size_t len = fread(body, 1, sizeof(body) - 1, file);
    fclose(file);
    if (len == 0) {
        return NULL;
    }
    cJSON *root = cJSON_ParseWithLength(body, len);
    if (!root) {
        return NULL;
    }
    const cJSON *kind = cJSON_GetObjectItemCaseSensitive(root, "kind");
    const cJSON *version = cJSON_GetObjectItemCaseSensitive(root, "v");
    if (!cJSON_IsString(kind) || strcmp(kind->valuestring, "art") != 0 ||
        !cJSON_IsNumber(version) || version->valueint != 1) {
        cJSON_Delete(root);
        return NULL;
    }
    return root;
}

bool art_store_read_caption(const char *picture_path, char *text, size_t text_len)
{
    if (text_len == 0) {
        return false;
    }
    text[0] = '\0';
    char path[256];
    if (!art_store_caption_path(picture_path, path, sizeof(path))) {
        return false;
    }
    cJSON *root = read_caption_file(path);
    if (!root) {
        return false;
    }
    const cJSON *value = cJSON_GetObjectItemCaseSensitive(root, "text");
    bool ok = cJSON_IsString(value) && value->valuestring && value->valuestring[0] != '\0';
    if (ok) {
        snprintf(text, text_len, "%s", value->valuestring);
    }
    cJSON_Delete(root);
    return ok;
}

bool art_store_read_size(const char *picture_path, int *width, int *height)
{
    char path[256];
    if (!art_store_caption_path(picture_path, path, sizeof(path))) {
        return false;
    }
    cJSON *root = read_caption_file(path);
    if (!root) {
        return false;
    }
    const cJSON *w = cJSON_GetObjectItemCaseSensitive(root, "w");
    const cJSON *h = cJSON_GetObjectItemCaseSensitive(root, "h");
    bool ok = cJSON_IsNumber(w) && cJSON_IsNumber(h) && w->valueint > 0 && h->valueint > 0;
    if (ok) {
        *width = w->valueint;
        *height = h->valueint;
    }
    cJSON_Delete(root);
    return ok;
}

static uint64_t file_size(const char *dir, const char *base, const char *extension)
{
    char path[256];
    struct stat st;
    snprintf(path, sizeof(path), "%s/%s%s", dir, base, extension);
    return stat(path, &st) == 0 ? (uint64_t) st.st_size : 0;
}

static int compare_items(const void *a, const void *b)
{
    const art_item_t *x = (const art_item_t *) a;
    const art_item_t *y = (const art_item_t *) b;
    if (x->seq != y->seq) {
        return x->seq < y->seq ? -1 : 1;
    }
    return strcmp(x->base, y->base);
}

int art_store_scan(const char *dir, art_item_t *items, int max_items)
{
    DIR *handle = opendir(dir);
    if (!handle) {
        return 0;
    }
    int count = 0;
    char orphans[ART_ORPHANS_PER_SCAN][ART_BASE_MAX];
    int orphan_count = 0;
    size_t suffix_len = strlen(ART_CAPTION_SUFFIX);
    struct dirent *entry;
    while (count < max_items && (entry = readdir(handle)) != NULL) {
        size_t len = strlen(entry->d_name);
        if (len <= suffix_len || len - suffix_len >= ART_BASE_MAX ||
            strcmp(entry->d_name + len - suffix_len, ART_CAPTION_SUFFIX) != 0) {
            continue;
        }
        char base[ART_BASE_MAX];
        memcpy(base, entry->d_name, len - suffix_len);
        base[len - suffix_len] = '\0';

        char path[256];
        if (snprintf(path, sizeof(path), "%s/%s", dir, entry->d_name) >= (int) sizeof(path)) {
            continue;  // a path that long is not one this option wrote
        }
        cJSON *root = read_caption_file(path);
        if (!root) {
            continue;  // not a caption file of this option: not ours, never listed
        }
        const cJSON *seq = cJSON_GetObjectItemCaseSensitive(root, "n");
        uint64_t picture_bytes = 0;  // the display-ready file: without it there is no picture
        for (size_t i = 0; i < 3; i++) {
            picture_bytes += file_size(dir, base, PICTURE_EXTENSIONS[i]);
        }
        if (picture_bytes == 0) {
            // The picture was deleted (in the web gallery, say): its caption file and thumbnail are
            // left behind and no picture any more. Removed after the walk, not while it runs.
            if (orphan_count < ART_ORPHANS_PER_SCAN) {
                snprintf(orphans[orphan_count++], ART_BASE_MAX, "%s", base);
            }
            cJSON_Delete(root);
            continue;
        }
        art_item_t *item = &items[count++];
        snprintf(item->base, sizeof(item->base), "%s", base);
        item->seq =
            (cJSON_IsNumber(seq) && seq->valuedouble >= 0) ? (uint32_t) seq->valuedouble : 0;
        item->bytes = 0;
        for (size_t i = 0; i < PICTURE_EXTENSION_COUNT; i++) {
            item->bytes += file_size(dir, base, PICTURE_EXTENSIONS[i]);
        }
        cJSON_Delete(root);
    }
    closedir(handle);
    for (int i = 0; i < orphan_count; i++) {
        art_store_remove(dir, orphans[i]);
    }
    qsort(items, (size_t) count, sizeof(art_item_t), compare_items);
    return count;
}

void art_store_remove(const char *dir, const char *base)
{
    for (size_t i = 0; i < PICTURE_EXTENSION_COUNT; i++) {
        char path[256];
        snprintf(path, sizeof(path), "%s/%s%s", dir, base, PICTURE_EXTENSIONS[i]);
        remove(path);
    }
}
