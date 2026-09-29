#include "source_auth.h"

#include <string.h>
#include <strings.h>

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

// Percent-decodes src[0..src_len) into out (NUL-terminated). False on a bad escape, a decoded NUL
// or when out is too small.
static bool percent_decode(const char *src, size_t src_len, char *out, size_t out_len)
{
    if (out_len == 0) {
        return false;
    }
    size_t o = 0;
    for (size_t i = 0; i < src_len; i++) {
        char c = src[i];
        if (c == '%') {
            if (i + 2 >= src_len) {
                return false;
            }
            int hi = hex_value(src[i + 1]);
            int lo = hex_value(src[i + 2]);
            if (hi < 0 || lo < 0) {
                return false;
            }
            c = (char) ((hi << 4) | lo);
            if (c == '\0') {
                return false;
            }
            i += 2;
        }
        if (o + 1 >= out_len) {
            return false;
        }
        out[o++] = c;
    }
    out[o] = '\0';
    return true;
}

source_auth_result_t source_auth_split_url(const char *url, char *clean_url, size_t clean_len,
                                           char *user, size_t user_len, char *pass, size_t pass_len)
{
    if (!url) {
        return SOURCE_AUTH_NO_LOGIN;
    }
    const char *scheme_end = strstr(url, "://");
    if (!scheme_end) {
        return SOURCE_AUTH_NO_LOGIN;
    }
    const char *authority = scheme_end + 3;
    const char *authority_end = authority + strcspn(authority, "/?#");

    // The login ends at the last '@' of the authority (a password may contain a raw '@' only
    // when it is not percent-encoded, which is ambiguous anyway - the last one wins).
    const char *at = NULL;
    for (const char *p = authority; p < authority_end; p++) {
        if (*p == '@') {
            at = p;
        }
    }
    if (!at) {
        return SOURCE_AUTH_NO_LOGIN;
    }

    const char *colon = memchr(authority, ':', (size_t) (at - authority));
    const char *user_end = colon ? colon : at;
    if (!percent_decode(authority, (size_t) (user_end - authority), user, user_len)) {
        return SOURCE_AUTH_INVALID;
    }
    if (colon) {
        if (!percent_decode(colon + 1, (size_t) (at - colon - 1), pass, pass_len)) {
            return SOURCE_AUTH_INVALID;
        }
    } else if (pass_len > 0) {
        pass[0] = '\0';
    }

    // scheme:// + host part + the rest, without "user:password@"
    size_t head_len = (size_t) (authority - url);
    size_t tail_len = strlen(at + 1);
    if (head_len + tail_len + 1 > clean_len) {
        return SOURCE_AUTH_INVALID;
    }
    memcpy(clean_url, url, head_len);
    memcpy(clean_url + head_len, at + 1, tail_len + 1);
    return SOURCE_AUTH_SPLIT;
}

bool source_auth_is_https(const char *url)
{
    return url && strncasecmp(url, "https://", 8) == 0;
}
