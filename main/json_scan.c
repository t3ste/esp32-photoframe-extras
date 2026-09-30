#include "json_scan.h"

#include <stdbool.h>
#include <stddef.h>

const char *json_skip_blanks(const char *p)
{
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') {
        p++;
    }
    return p;
}

const char *json_object_end(const char *start)
{
    int depth = 0;
    bool in_string = false;
    for (const char *p = start; *p; p++) {
        if (in_string) {
            if (*p == '\\' && p[1]) {
                p++;
            } else if (*p == '"') {
                in_string = false;
            }
        } else if (*p == '"') {
            in_string = true;
        } else if (*p == '{') {
            depth++;
        } else if (*p == '}' && --depth == 0) {
            return p;
        }
    }
    return NULL;
}
