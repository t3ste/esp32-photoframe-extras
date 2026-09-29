#ifndef SOURCE_AUTH_H
#define SOURCE_AUTH_H

#include <stdbool.h>
#include <stddef.h>

/**
 * @file source_auth.h
 * @brief Login for the URLs the frame downloads from (build option `source-auth`).
 *
 * The user writes the login into the URL itself - `https://user:password@host/path`, with
 * special characters percent-encoded (`@` as `%40`, `:` as `%3A`, ...) - so the URL fields keep
 * their existing treatment as credentials: write-only in the Web UI, left out of a normal
 * config export. This file only splits such a URL; http_fetch.c hands the pieces to the HTTP
 * client, which answers the server's 401 with Basic or Digest authentication.
 *
 * Pure string handling with no ESP-IDF dependency, so the host tests link it.
 */

// Longest user name / password (decoded) and URL (without the login) that fit.
#define SOURCE_AUTH_USER_MAX_LEN 64
#define SOURCE_AUTH_PASS_MAX_LEN 128
#define SOURCE_AUTH_URL_MAX_LEN 512

typedef enum {
    SOURCE_AUTH_NO_LOGIN = 0,  // the URL has no `user@` part - use it unchanged
    SOURCE_AUTH_SPLIT,         // login found: clean URL and credentials were written
    SOURCE_AUTH_INVALID,       // malformed (bad %xx, a part too long for its buffer)
} source_auth_result_t;

/**
 * @brief Splits `scheme://user:password@host/path` into the URL without the login and the
 * percent-decoded credentials.
 *
 * Only the authority part (between `://` and the first `/`, `?` or `#`) is inspected, so an
 * `@` in the path or query (`?owner=me@example.org`) is not a login. The user name ends at the
 * first `:`; a password may contain further `:` characters. A `+` stays a `+`.
 *
 * The outputs are only written for SOURCE_AUTH_SPLIT.
 */
source_auth_result_t source_auth_split_url(const char *url, char *clean_url, size_t clean_len,
                                           char *user, size_t user_len, char *pass,
                                           size_t pass_len);

/** @brief True if the URL starts with `https://` (case-insensitive). */
bool source_auth_is_https(const char *url);

#endif
