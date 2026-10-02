#include "art_sources.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "cJSON.h"

// An answer bigger than this is not read (the biggest real one, a Rijksmuseum record, is 17 KB)
#define ART_JSON_MAX_BYTES (96 * 1024)

// ---------------------------------------------------------------------------------------------
// Small helpers

static bool starts_with(const char *text, const char *prefix)
{
    return text && strncmp(text, prefix, strlen(prefix)) == 0;
}

// Copies text into a buffer, cut at a character boundary.
static void copy_text(char *dest, size_t size, const char *src)
{
    if (size == 0) {
        return;
    }
    dest[0] = '\0';
    if (!src) {
        return;
    }
    size_t len = strlen(src);
    if (len >= size) {
        len = size - 1;
        while (len > 0 && ((unsigned char) src[len] & 0xC0) == 0x80) {
            len--;
        }
    }
    memcpy(dest, src, len);
    dest[len] = '\0';
}

// A name that can be part of a file name: letters, digits, '-' and '_' (other characters become
// '_')
static void safe_id(const char *src, char *dest, size_t size)
{
    size_t used = 0;
    for (; src && *src && used + 1 < size; src++) {
        unsigned char c = (unsigned char) *src;
        dest[used++] = (isalnum(c) || c == '-' || c == '_') ? (char) c : '_';
    }
    if (size > 0) {
        dest[used] = '\0';
    }
}

static cJSON *parse(const char *json, size_t len)
{
    if (!json || len == 0 || len > ART_JSON_MAX_BYTES) {
        return NULL;
    }
    return cJSON_ParseWithLength(json, len);
}

static cJSON *member(const cJSON *object, const char *key)
{
    return object ? cJSON_GetObjectItemCaseSensitive((cJSON *) object, key) : NULL;
}

static const char *text_of(const cJSON *item)
{
    return (item && cJSON_IsString(item) && item->valuestring) ? item->valuestring : NULL;
}

static const char *member_text(const cJSON *object, const char *key)
{
    return text_of(member(object, key));
}

static cJSON *first_of(const cJSON *array)
{
    return (array && cJSON_IsArray(array)) ? cJSON_GetArrayItem((cJSON *) array, 0) : NULL;
}

// The pairs "classified_as": [{"id": "..."}] of a Linked Art element: does one of them have this
// id?
static bool classified_as(const cJSON *element, const char *id)
{
    const cJSON *entry;
    cJSON_ArrayForEach(entry, member(element, "classified_as"))
    {
        const char *entry_id = member_text(entry, "id");
        if (entry_id && strcmp(entry_id, id) == 0) {
            return true;
        }
    }
    return false;
}

// "https://id.rijksmuseum.nl/200100988" -> the same record on the data host, in the framed profile
static bool rijks_record_url(const char *id_url, char *out, size_t out_len)
{
    static const char PREFIX[] = "https://id.rijksmuseum.nl/";
    if (!starts_with(id_url, PREFIX)) {
        return false;
    }
    const char *tail = id_url + sizeof(PREFIX) - 1;
    size_t len = strlen(tail);
    if (len == 0 || len > 24) {
        return false;
    }
    for (size_t i = 0; i < len; i++) {
        if (!isdigit((unsigned char) tail[i])) {
            return false;
        }
    }
    int n = snprintf(out, out_len, "https://data.rijksmuseum.nl/%s?_profile=la-framed", tail);
    return n > 0 && (size_t) n < out_len;
}

// The year of "1500-01-01T00:00:00.000Z" or the text "1500-1550"
static void year_of(const char *text, char *out, size_t out_len)
{
    out[0] = '\0';
    if (!text) {
        return;
    }
    size_t len = strlen(text);
    bool four_digits = len >= 4 && isdigit((unsigned char) text[0]) &&
                       isdigit((unsigned char) text[1]) && isdigit((unsigned char) text[2]) &&
                       isdigit((unsigned char) text[3]);
    // a bare year, a full date or a date and time: the year; anything else ("1500-1550",
    // "ca. 1642") as it is
    if (four_digits &&
        (len == 4 || text[4] == 'T' || (len >= 10 && text[4] == '-' && text[7] == '-'))) {
        snprintf(out, out_len, "%.4s", text);
        return;
    }
    copy_text(out, out_len, text);
}

// ---------------------------------------------------------------------------------------------
// The key

bool art_si_key_valid(const char *key)
{
    if (!key) {
        return false;
    }
    size_t len = strlen(key);
    if (len < 4 || len > 64) {
        return false;
    }
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char) key[i];
        if (!isalnum(c) && c != '_') {
            return false;
        }
    }
    return true;
}

// ---------------------------------------------------------------------------------------------
// Rijksmuseum

bool art_rijks_search_url(art_type_t type, int year, char *out, size_t out_len)
{
    if (year < 0 || year > 2100) {
        return false;
    }
    int n = snprintf(out, out_len,
                     "https://data.rijksmuseum.nl/search/collection?type=%s&imageAvailable=true"
                     "&creationDate=%d",
                     art_type_name(type), year);
    return n > 0 && (size_t) n < out_len;
}

int art_rijks_pick_object(const char *json, size_t len, uint32_t rnd, char *object_url,
                          size_t url_len)
{
    if (url_len > 0) {
        object_url[0] = '\0';
    }
    cJSON *root = parse(json, len);
    if (!root) {
        return 0;
    }
    int count = 0;
    const cJSON *items = member(root, "orderedItems");
    if (items && cJSON_IsArray(items)) {
        count = cJSON_GetArraySize(items);
        if (count > 0) {
            const cJSON *pick = cJSON_GetArrayItem((cJSON *) items, (int) (rnd % (uint32_t) count));
            if (!rijks_record_url(member_text(pick, "id"), object_url, url_len)) {
                count = 0;
            }
        }
    }
    cJSON_Delete(root);
    return count;
}

// The English wording of a Linked Art text list, else the first one
#define AAT_ENGLISH "http://vocab.getty.edu/aat/300388277"
#define AAT_PRIMARY_TITLE "http://vocab.getty.edu/aat/300417200"
#define AAT_CREATOR_TEXT "http://vocab.getty.edu/aat/300435416"

static const char *creator_text(const cJSON *produced_by)
{
    const char *fallback = NULL;
    const cJSON *entry;
    cJSON_ArrayForEach(entry, member(produced_by, "referred_to_by"))
    {
        const char *content = member_text(entry, "content");
        if (!content || content[0] == '\0' || !classified_as(entry, AAT_CREATOR_TEXT)) {
            continue;
        }
        const cJSON *language;
        cJSON_ArrayForEach(language, member(entry, "language"))
        {
            const char *id = member_text(language, "id");
            if (id && strcmp(id, AAT_ENGLISH) == 0) {
                return content;
            }
        }
        if (!fallback) {
            fallback = content;
        }
    }
    return fallback;
}

bool art_rijks_parse_object(const char *json, size_t len, art_work_t *work, char *visual_url,
                            size_t url_len)
{
    if (url_len > 0) {
        visual_url[0] = '\0';
    }
    cJSON *root = parse(json, len);
    if (!root) {
        return false;
    }
    bool ok = false;
    do {
        // the number at the end of the record's own address names the work
        const char *self = member_text(root, "id");
        const char *slash = self ? strrchr(self, '/') : NULL;
        if (!slash || slash[1] == '\0') {
            break;
        }
        memset(work, 0, sizeof(*work));
        work->source = ART_SOURCE_RIJKS;
        safe_id(slash + 1, work->id, sizeof(work->id));

        // the first picture of the work -> its visual item
        const cJSON *shows = first_of(member(root, "shows"));
        if (!rijks_record_url(member_text(shows, "id"), visual_url, url_len)) {
            break;
        }

        // the title: a name that is the primary title, else the first name
        const char *title = NULL;
        const cJSON *name;
        cJSON_ArrayForEach(name, member(root, "identified_by"))
        {
            const char *type = member_text(name, "type");
            const char *content = member_text(name, "content");
            if (!type || strcmp(type, "Name") != 0 || !content || content[0] == '\0') {
                continue;
            }
            if (classified_as(name, AAT_PRIMARY_TITLE)) {
                title = content;
                break;
            }
            if (!title) {
                title = content;
            }
        }
        copy_text(work->title, sizeof(work->title), title ? title : "");

        const cJSON *produced_by = member(root, "produced_by");
        copy_text(work->artist, sizeof(work->artist), creator_text(produced_by));
        const cJSON *time_name = first_of(member(member(produced_by, "timespan"), "identified_by"));
        year_of(member_text(time_name, "content"), work->year, sizeof(work->year));
        ok = true;
    } while (0);
    cJSON_Delete(root);
    return ok;
}

bool art_rijks_parse_visual_item(const char *json, size_t len, art_work_t *work, char *digital_url,
                                 size_t url_len)
{
    if (url_len > 0) {
        digital_url[0] = '\0';
    }
    cJSON *root = parse(json, len);
    if (!root) {
        return false;
    }
    bool ok = false;
    work->rights[0] = '\0';
    const cJSON *right;
    cJSON_ArrayForEach(right, member(root, "subject_to"))
    {
        const cJSON *entry;
        cJSON_ArrayForEach(entry, member(right, "classified_as"))
        {
            const char *id = member_text(entry, "id");
            if (id && strstr(id, "publicdomain/mark/1.0")) {
                copy_text(work->rights, sizeof(work->rights), "PDM");
            } else if (id && strstr(id, "publicdomain/zero/1.0")) {
                copy_text(work->rights, sizeof(work->rights), "CC0");
            }
        }
    }
    if (work->rights[0] != '\0') {
        const cJSON *shown_by = first_of(member(root, "digitally_shown_by"));
        ok = rijks_record_url(member_text(shown_by, "id"), digital_url, url_len);
    }
    cJSON_Delete(root);
    return ok;
}

bool art_rijks_parse_digital_object(const char *json, size_t len, art_work_t *work)
{
    cJSON *root = parse(json, len);
    if (!root) {
        return false;
    }
    bool downloadable = false;
    const cJSON *statement;
    cJSON_ArrayForEach(statement, member(root, "referred_to_by"))
    {
        const char *content = member_text(statement, "content");
        if (content && (strcasecmp(content, "downloadbaar") == 0 ||
                        strcasecmp(content, "downloadable") == 0)) {
            downloadable = true;
        }
    }
    bool ok = false;
    if (downloadable) {
        static const char PREFIX[] = "https://iiif.micr.io/";
        const cJSON *point;
        cJSON_ArrayForEach(point, member(root, "access_point"))
        {
            const char *url = member_text(point, "id");
            if (!starts_with(url, PREFIX)) {
                continue;
            }
            const char *name = url + sizeof(PREFIX) - 1;
            size_t name_len = strcspn(name, "/");
            bool clean = name_len > 0 && name_len <= 40;
            for (size_t i = 0; clean && i < name_len; i++) {
                clean = isalnum((unsigned char) name[i]) || name[i] == '-' || name[i] == '_';
            }
            if (clean) {
                snprintf(work->image_base, sizeof(work->image_base), "%.*s",
                         (int) (sizeof(PREFIX) - 1 + name_len), url);
                ok = true;
                break;
            }
        }
    }
    cJSON_Delete(root);
    return ok;
}

// ---------------------------------------------------------------------------------------------
// SMK

static const char *smk_term(art_type_t type)
{
    switch (type) {
    case ART_TYPE_DRAWING:
        return "tegning";
    case ART_TYPE_PRINT:
        return "grafik";
    case ART_TYPE_PAINTING:
    default:
        return "maleri";
    }
}

bool art_smk_search_url(art_type_t type, unsigned offset, int rows, char *out, size_t out_len)
{
    if (rows < 0 || rows > 10 || offset > 1000000) {
        return false;
    }
    // filters=[has_image:true],[public_domain:true],[object_names:<term>] with the brackets, commas
    // and colons encoded
    int n =
        snprintf(out, out_len,
                 "https://api.smk.dk/api/v1/art/search/?keys=*&filters=%%5Bhas_image%%3Atrue%%5D"
                 "%%2C%%5Bpublic_domain%%3Atrue%%5D%%2C%%5Bobject_names%%3A%s%%5D&offset=%u"
                 "&rows=%d",
                 smk_term(type), offset, rows);
    return n > 0 && (size_t) n < out_len;
}

int art_smk_found(const char *json, size_t len)
{
    cJSON *root = parse(json, len);
    if (!root) {
        return -1;
    }
    const cJSON *found = member(root, "found");
    int count = (found && cJSON_IsNumber(found) && found->valuedouble >= 0 &&
                 found->valuedouble < 100000000)
                    ? (int) found->valuedouble
                    : -1;
    cJSON_Delete(root);
    return count;
}

bool art_smk_parse_item(const char *json, size_t len, art_work_t *work)
{
    cJSON *root = parse(json, len);
    if (!root) {
        return false;
    }
    bool ok = false;
    do {
        const cJSON *item = first_of(member(root, "items"));
        const cJSON *public_domain = member(item, "public_domain");
        const char *iiif = member_text(item, "image_iiif_id");
        const char *number = member_text(item, "object_number");
        if (!item || !public_domain || !cJSON_IsTrue(public_domain) ||
            !starts_with(iiif, "https://iip.smk.dk/iiif/") || !number || number[0] == '\0') {
            break;
        }
        memset(work, 0, sizeof(*work));
        work->source = ART_SOURCE_SMK;
        safe_id(number, work->id, sizeof(work->id));
        copy_text(work->image_base, sizeof(work->image_base), iiif);

        // the title: one in English if there is one, else the first
        const char *title = NULL;
        const cJSON *entry;
        cJSON_ArrayForEach(entry, member(item, "titles"))
        {
            const char *text = member_text(entry, "title");
            const char *language = member_text(entry, "language");
            if (!text || text[0] == '\0') {
                continue;
            }
            if (language &&
                (strcasecmp(language, "engelsk") == 0 || strcasecmp(language, "english") == 0)) {
                title = text;
                break;
            }
            if (!title) {
                title = text;
            }
        }
        copy_text(work->title, sizeof(work->title), title ? title : "");

        const cJSON *artist = first_of(member(item, "artist"));
        copy_text(work->artist, sizeof(work->artist), text_of(artist));

        const cJSON *made = first_of(member(item, "production_date"));
        const char *period = member_text(made, "period");
        year_of(period ? period : member_text(made, "start"), work->year, sizeof(work->year));

        // the size of the original: lets the frame skip a picture of the wrong orientation before
        // it loads it
        const cJSON *image_w = member(item, "image_width");
        const cJSON *image_h = member(item, "image_height");
        if (cJSON_IsNumber(image_w) && cJSON_IsNumber(image_h) && image_w->valueint > 0 &&
            image_h->valueint > 0) {
            work->width = image_w->valueint;
            work->height = image_h->valueint;
        }

        const char *rights = member_text(item, "rights");
        copy_text(work->rights, sizeof(work->rights),
                  rights && strstr(rights, "mark/1.0")   ? "PDM"
                  : rights && strstr(rights, "zero/1.0") ? "CC0"
                                                         : "PD");
        ok = true;
    } while (0);
    cJSON_Delete(root);
    return ok;
}

// ---------------------------------------------------------------------------------------------
// Smithsonian

static const char *si_term(art_type_t type)
{
    switch (type) {
    case ART_TYPE_DRAWING:
        return "Drawings";
    case ART_TYPE_PAINTING:
        return "Paintings";
    case ART_TYPE_PRINT:
    default:
        return NULL;
    }
}

bool art_si_search_url(art_type_t type, unsigned start, int rows, const char *key, char *out,
                       size_t out_len)
{
    const char *term = si_term(type);
    if (!term || rows < 0 || rows > 10 || start > 1000000 || !art_si_key_valid(key)) {
        return false;
    }
    // q=unit_code:"SAAM" AND online_media_type:"Images" AND object_type:"<term>", encoded
    int n = snprintf(out, out_len,
                     "https://api.si.edu/openaccess/api/v1.0/search?q=unit_code%%3A%%22SAAM%%22%%20"
                     "AND%%20online_media_type%%3A%%22Images%%22%%20AND%%20object_type%%3A%%22%s"
                     "%%22&start=%u&rows=%d&api_key=%s",
                     term, start, rows, key);
    return n > 0 && (size_t) n < out_len;
}

int art_si_row_count(const char *json, size_t len)
{
    cJSON *root = parse(json, len);
    if (!root) {
        return -1;
    }
    const cJSON *count = member(member(root, "response"), "rowCount");
    int rows = (count && cJSON_IsNumber(count) && count->valuedouble >= 0 &&
                count->valuedouble < 100000000)
                   ? (int) count->valuedouble
                   : -1;
    cJSON_Delete(root);
    return rows;
}

bool art_si_parse_row(const char *json, size_t len, art_work_t *work)
{
    cJSON *root = parse(json, len);
    if (!root) {
        return false;
    }
    bool ok = false;
    do {
        const cJSON *row = first_of(member(member(root, "response"), "rows"));
        const cJSON *content = member(row, "content");
        const cJSON *media = first_of(
            member(member(member(content, "descriptiveNonRepeating"), "online_media"), "media"));
        const char *access = member_text(member(media, "usage"), "access");
        const char *type = member_text(media, "type");
        const char *url = member_text(media, "content");
        const char *ids_id = member_text(media, "idsId");
        if (!row || !media || !access || strcmp(access, "CC0") != 0 || !type ||
            strcmp(type, "Images") != 0 ||
            !starts_with(url, "https://ids.si.edu/ids/deliveryService?id=") || !ids_id ||
            ids_id[0] == '\0') {
            break;
        }
        memset(work, 0, sizeof(*work));
        work->source = ART_SOURCE_SMITHSONIAN;
        safe_id(ids_id, work->id, sizeof(work->id));
        copy_text(work->image_base, sizeof(work->image_base), url);
        copy_text(work->title, sizeof(work->title), member_text(row, "title"));
        copy_text(work->rights, sizeof(work->rights), "CC0");

        // "Name, born Place 1875-died Place 1950": the name is what comes before the first comma
        const cJSON *freetext = member(content, "freetext");
        const cJSON *artist = first_of(member(freetext, "name"));
        copy_text(work->artist, sizeof(work->artist), member_text(artist, "content"));
        char *comma = strchr(work->artist, ',');
        if (comma) {
            *comma = '\0';
        }
        const cJSON *date = first_of(member(freetext, "date"));
        year_of(member_text(date, "content"), work->year, sizeof(work->year));
        ok = true;
    } while (0);
    cJSON_Delete(root);
    return ok;
}

// ---------------------------------------------------------------------------------------------
// The picture

bool art_image_url(const art_work_t *work, int max_w, int max_h, char *out, size_t out_len)
{
    if (!work || work->image_base[0] == '\0' || max_w < 16 || max_h < 16 || max_w > 8192 ||
        max_h > 8192) {
        return false;
    }
    int n;
    if (work->source == ART_SOURCE_SMITHSONIAN) {
        n = snprintf(out, out_len, "%s&max=%d", work->image_base, max_w > max_h ? max_w : max_h);
    } else {
        n = snprintf(out, out_len, "%s/full/!%d,%d/0/default.jpg", work->image_base, max_w, max_h);
    }
    return n > 0 && (size_t) n < out_len;
}

// The frame header of a JPEG: its marker and the size. False when there is none before the scan.
static bool jpeg_frame(const uint8_t *data, size_t len, uint8_t *marker_out, int *width,
                       int *height)
{
    if (!data || len < 4 || data[0] != 0xFF || data[1] != 0xD8) {
        return false;
    }
    size_t i = 2;
    while (i + 3 < len) {
        if (data[i] != 0xFF) {
            return false;  // not at a marker
        }
        uint8_t marker = data[i + 1];
        if (marker == 0xFF) {  // a fill byte
            i++;
            continue;
        }
        if (marker == 0x01 || (marker >= 0xD0 && marker <= 0xD8)) {  // markers without a length
            i += 2;
            continue;
        }
        if (marker == 0xD9 || marker == 0xDA) {
            return false;  // the end, or the scan, before any frame header
        }
        size_t segment = ((size_t) data[i + 2] << 8) | data[i + 3];
        if (segment < 2) {
            return false;
        }
        // a frame header: SOF0-SOF15 without DHT (C4), JPG (C8) and DAC (CC)
        if (marker >= 0xC0 && marker <= 0xCF && marker != 0xC4 && marker != 0xC8 &&
            marker != 0xCC) {
            if (segment < 8 || i + 9 > len) {
                return false;  // too short for the size
            }
            *marker_out = marker;
            *height = ((int) data[i + 5] << 8) | data[i + 6];
            *width = ((int) data[i + 7] << 8) | data[i + 8];
            return true;
        }
        i += 2 + segment;
    }
    return false;
}

bool art_jpeg_is_baseline(const uint8_t *data, size_t len)
{
    uint8_t marker = 0;
    int width = 0, height = 0;
    // baseline or extended sequential; progressive, lossless and arithmetic coding are refused
    return jpeg_frame(data, len, &marker, &width, &height) && (marker == 0xC0 || marker == 0xC1);
}

bool art_jpeg_size(const uint8_t *data, size_t len, int *width, int *height)
{
    uint8_t marker = 0;
    int w = 0, h = 0;
    if (!jpeg_frame(data, len, &marker, &w, &h) || w <= 0 || h <= 0) {
        return false;
    }
    *width = w;
    *height = h;
    return true;
}
