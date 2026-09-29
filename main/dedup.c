#include "dedup.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

#include "esp_log.h"
#include "esp_rom_md5.h"

static const char *TAG = "dedup";

#define DEDUP_PATH_MAX 512
#define DEDUP_READ_CHUNK 4096

bool dedup_is_image_name(const char *name)
{
    const char *ext = name ? strrchr(name, '.') : NULL;
    return ext && (strcasecmp(ext, ".png") == 0 || strcasecmp(ext, ".epdgz") == 0 ||
                   strcasecmp(ext, ".bmp") == 0);
}

void dedup_digest_to_hex(const dedup_digest_t *d, char out[DEDUP_HEX_LEN + 1])
{
    static const char hex[] = "0123456789abcdef";
    for (int i = 0; i < DEDUP_DIGEST_LEN; i++) {
        out[2 * i] = hex[d->b[i] >> 4];
        out[2 * i + 1] = hex[d->b[i] & 0x0F];
    }
    out[DEDUP_HEX_LEN] = '\0';
}

static int hex_value(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

bool dedup_digest_from_hex(const char *hex, dedup_digest_t *d)
{
    for (int i = 0; i < DEDUP_DIGEST_LEN; i++) {
        int hi = hex_value(hex[2 * i]);
        int lo = hi < 0 ? -1 : hex_value(hex[2 * i + 1]);
        if (hi < 0 || lo < 0) {
            return false;
        }
        d->b[i] = (uint8_t) ((hi << 4) | lo);
    }
    return true;
}

bool dedup_digest_equal(const dedup_digest_t *a, const dedup_digest_t *b)
{
    return memcmp(a->b, b->b, DEDUP_DIGEST_LEN) == 0;
}

esp_err_t dedup_md5_file(const char *path, dedup_digest_t *out)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        return ESP_ERR_NOT_FOUND;
    }
    uint8_t *buf = malloc(DEDUP_READ_CHUNK);
    if (!buf) {
        fclose(fp);
        return ESP_ERR_NO_MEM;
    }
    md5_context_t ctx;
    esp_rom_md5_init(&ctx);
    size_t n;
    while ((n = fread(buf, 1, DEDUP_READ_CHUNK, fp)) > 0) {
        esp_rom_md5_update(&ctx, buf, (uint32_t) n);
    }
    bool failed = ferror(fp) != 0;
    fclose(fp);
    free(buf);
    esp_rom_md5_final(out->b, &ctx);
    return failed ? ESP_FAIL : ESP_OK;
}

// One parsed index line; `name` points into the line buffer.
typedef struct {
    dedup_hash_t kind;
    dedup_digest_t digest;
    char *name;
} index_entry_t;

// "s 0123...cdef name.png\n" -> entry; false for anything else (the line is then ignored).
static bool parse_line(char *line, index_entry_t *e)
{
    size_t len = strlen(line);
    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
        line[--len] = '\0';
    }
    if (len < 2 + DEDUP_HEX_LEN + 1 + 1 || line[1] != ' ' || line[2 + DEDUP_HEX_LEN] != ' ') {
        return false;
    }
    if (line[0] == 's') {
        e->kind = DEDUP_HASH_STORED;
    } else if (line[0] == 'p') {
        e->kind = DEDUP_HASH_PAYLOAD;
    } else {
        return false;
    }
    if (!dedup_digest_from_hex(line + 2, &e->digest)) {
        return false;
    }
    e->name = line + 2 + DEDUP_HEX_LEN + 1;
    return true;
}

static void index_path(const char *album_dir, char *out, size_t out_len)
{
    snprintf(out, out_len, "%s/%s", album_dir, DEDUP_INDEX_NAME);
}

// Reads the next line into `line`; a line that does not fit is skipped whole. False at the end.
static bool next_line(FILE *fp, char *line, size_t line_len)
{
    while (fgets(line, (int) line_len, fp)) {
        size_t n = strlen(line);
        if (n > 0 && line[n - 1] == '\n') {
            return true;
        }
        if (feof(fp)) {
            return true;  // last line without a newline
        }
        int c;  // too long: drop the rest of it
        while ((c = fgetc(fp)) != EOF && c != '\n') {
        }
    }
    return false;
}

bool dedup_index_find(const char *album_dir, dedup_hash_t kind, const dedup_digest_t *d,
                      const char *ignore_name, char *found, size_t found_len)
{
    char path[DEDUP_PATH_MAX];
    index_path(album_dir, path, sizeof(path));
    FILE *fp = fopen(path, "r");
    if (!fp) {
        return false;
    }
    char line[DEDUP_LINE_MAX];
    bool hit = false;
    while (!hit && next_line(fp, line, sizeof(line))) {
        index_entry_t e;
        if (!parse_line(line, &e) || e.kind != kind || !dedup_digest_equal(&e.digest, d)) {
            continue;
        }
        if (ignore_name && strcmp(e.name, ignore_name) == 0) {
            continue;
        }
        char file[DEDUP_PATH_MAX];
        struct stat st;
        snprintf(file, sizeof(file), "%s/%s", album_dir, e.name);
        if (stat(file, &st) != 0) {
            continue;  // the file has gone since
        }
        if (found && found_len > 0) {
            snprintf(found, found_len, "%s", e.name);
        }
        hit = true;
    }
    fclose(fp);
    return hit;
}

bool dedup_index_has(const char *album_dir, dedup_hash_t kind, const char *name)
{
    char path[DEDUP_PATH_MAX];
    index_path(album_dir, path, sizeof(path));
    FILE *fp = fopen(path, "r");
    if (!fp) {
        return false;
    }
    char line[DEDUP_LINE_MAX];
    bool has = false;
    while (!has && next_line(fp, line, sizeof(line))) {
        index_entry_t e;
        has = parse_line(line, &e) && e.kind == kind && strcmp(e.name, name) == 0;
    }
    fclose(fp);
    return has;
}

// True if the index has an entry of any kind for the file name.
static bool name_in_index(const char *path, const char *name)
{
    FILE *fp = fopen(path, "r");
    if (!fp) {
        return false;
    }
    char line[DEDUP_LINE_MAX];
    bool has = false;
    while (!has && next_line(fp, line, sizeof(line))) {
        index_entry_t e;
        has = parse_line(line, &e) && strcmp(e.name, name) == 0;
    }
    fclose(fp);
    return has;
}

// Copies the index without the entries of `name` (only if it has any). The new file goes to
// `.dedup.tmp` and replaces the index at the end.
static esp_err_t rewrite_without(const char *album_dir, const char *name)
{
    char path[DEDUP_PATH_MAX], tmp[DEDUP_PATH_MAX + 8];
    index_path(album_dir, path, sizeof(path));
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    if (!name_in_index(path, name)) {
        return ESP_OK;  // nothing to drop
    }
    FILE *in = fopen(path, "r");
    if (!in) {
        return ESP_FAIL;
    }
    FILE *out = fopen(tmp, "w");
    if (!out) {
        fclose(in);
        ESP_LOGW(TAG, "Cannot write the duplicate index");
        return ESP_FAIL;
    }
    char line[DEDUP_LINE_MAX];
    bool ok = true;
    while (next_line(in, line, sizeof(line))) {
        index_entry_t e;
        if (parse_line(line, &e)) {
            if (strcmp(e.name, name) == 0) {
                continue;
            }
            char hex[DEDUP_HEX_LEN + 1];
            dedup_digest_to_hex(&e.digest, hex);
            ok = ok && fprintf(out, "%c %s %s\n", e.kind == DEDUP_HASH_STORED ? 's' : 'p', hex,
                               e.name) > 0;
        }  // an unreadable line is dropped
    }
    fclose(in);
    ok = (fclose(out) == 0) && ok;
    if (!ok) {
        unlink(tmp);
        return ESP_FAIL;
    }
    unlink(path);  // FAT cannot rename over an existing file
    return rename(tmp, path) == 0 ? ESP_OK : ESP_FAIL;
}

esp_err_t dedup_index_remove(const char *album_dir, const char *name)
{
    return rewrite_without(album_dir, name);
}

esp_err_t dedup_index_set(const char *album_dir, dedup_hash_t kind, const dedup_digest_t *d,
                          const char *name)
{
    if (!name || name[0] == '\0' || strpbrk(name, "\r\n") != NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t err = rewrite_without(album_dir, name);
    if (err != ESP_OK) {
        return err;
    }
    char path[DEDUP_PATH_MAX], hex[DEDUP_HEX_LEN + 1];
    index_path(album_dir, path, sizeof(path));
    dedup_digest_to_hex(d, hex);
    // A last line without its newline (a hand-edited file) must not get the new entry glued on.
    bool needs_newline = false;
    FILE *probe = fopen(path, "rb");
    if (probe) {
        if (fseek(probe, -1, SEEK_END) == 0) {
            needs_newline = fgetc(probe) != '\n';
        }
        fclose(probe);
    }
    FILE *fp = fopen(path, "a");
    if (!fp) {
        return ESP_FAIL;
    }
    bool ok = !needs_newline || fputc('\n', fp) != EOF;
    ok = ok && fprintf(fp, "%c %s %s\n", kind == DEDUP_HASH_STORED ? 's' : 'p', hex, name) > 0;
    ok = (fclose(fp) == 0) && ok;
    return ok ? ESP_OK : ESP_FAIL;
}

// ---- the duplicate report -------------------------------------------------------------------

typedef struct {
    dedup_digest_t digest;
    char *name;
} report_entry_t;

static int compare_report_entries(const void *a, const void *b)
{
    const report_entry_t *x = a, *y = b;
    int by_digest = memcmp(x->digest.b, y->digest.b, DEDUP_DIGEST_LEN);
    return by_digest != 0 ? by_digest : strcmp(x->name, y->name);
}

// Image files of the directory (not hidden, not macOS "._" leftovers).
static int count_images(const char *album_dir)
{
    DIR *dir = opendir(album_dir);
    if (!dir) {
        return 0;
    }
    int count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] != '.' && dedup_is_image_name(entry->d_name)) {
            count++;
        }
    }
    closedir(dir);
    return count;
}

esp_err_t dedup_index_groups(const char *album_dir, dedup_hash_t kind, dedup_group_fn fn, void *ctx,
                             int *images, int *indexed)
{
    if (images) {
        *images = count_images(album_dir);
    }
    if (indexed) {
        *indexed = 0;
    }
    char path[DEDUP_PATH_MAX];
    index_path(album_dir, path, sizeof(path));
    FILE *fp = fopen(path, "r");
    if (!fp) {
        return ESP_OK;  // no index: nothing is known yet
    }
    report_entry_t *entries = NULL;
    int count = 0, cap = 0;
    esp_err_t result = ESP_OK;
    char line[DEDUP_LINE_MAX];
    while (next_line(fp, line, sizeof(line))) {
        index_entry_t e;
        if (!parse_line(line, &e) || e.kind != kind) {
            continue;
        }
        char file[DEDUP_PATH_MAX];
        struct stat st;
        snprintf(file, sizeof(file), "%s/%s", album_dir, e.name);
        if (stat(file, &st) != 0) {
            continue;  // gone
        }
        if (count == DEDUP_REPORT_MAX_ENTRIES) {
            ESP_LOGW(TAG, "More than %d indexed images - the report is cut off",
                     DEDUP_REPORT_MAX_ENTRIES);
            break;
        }
        if (count == cap) {
            int new_cap = cap ? cap * 2 : 64;
            report_entry_t *grown = realloc(entries, (size_t) new_cap * sizeof(*entries));
            if (!grown) {
                result = ESP_ERR_NO_MEM;
                break;
            }
            entries = grown;
            cap = new_cap;
        }
        entries[count].digest = e.digest;
        entries[count].name = strdup(e.name);
        if (!entries[count].name) {
            result = ESP_ERR_NO_MEM;
            break;
        }
        count++;
    }
    fclose(fp);

    if (result == ESP_OK && count > 0) {
        qsort(entries, (size_t) count, sizeof(*entries), compare_report_entries);
        const char **names = malloc((size_t) count * sizeof(*names));
        if (!names) {
            result = ESP_ERR_NO_MEM;
        } else {
            for (int i = 0; i < count; i++) {
                names[i] = entries[i].name;
            }
            for (int start = 0; start < count;) {
                int end = start + 1;
                while (end < count &&
                       dedup_digest_equal(&entries[start].digest, &entries[end].digest)) {
                    end++;
                }
                if (end - start >= 2 && fn) {
                    fn(&names[start], end - start, ctx);
                }
                start = end;
            }
            free(names);
        }
    }
    if (indexed) {
        *indexed = count;
    }
    for (int i = 0; i < count; i++) {
        free(entries[i].name);
    }
    free(entries);
    return result;
}
