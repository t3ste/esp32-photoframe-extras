#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <strings.h>

/*
 * Cross-site request check for the device's HTTP API.
 *
 * A web page opened in any browser on the same network can make that browser send a POST to the
 * frame (a "simple" request needs no preflight, and the API does not look at Content-Type), e.g.
 * /api/factory-reset or /api/config. Browsers always put an Origin header on such a request, and it
 * names the page's site, not the frame. The frame's own Web UI sends the Origin of the frame
 * itself, so the two are the same as the Host header the request was sent to.
 *
 * Requests without an Origin header (curl, Home Assistant, the photoframe server, a browser
 * navigating with GET) are not browser-driven cross-site requests and pass.
 *
 * Header-only and free of ESP-IDF headers so host tests can include it.
 */

// The end of `authority` once a default port for the scheme (":80" for http, ":443" for https) is
// dropped: browsers leave the default port out of Origin, other clients may write it in Host.
static inline size_t http_origin_authority_len(const char *authority, const char *default_port)
{
    size_t len = strlen(authority);
    size_t port_len = strlen(default_port);
    if (len > port_len && strcmp(authority + len - port_len, default_port) == 0) {
        len -= port_len;
    }
    return len;
}

/**
 * @brief Whether a request's Origin header allows it to be served.
 * @param host    The request's Host header value; may be NULL.
 * @param origin  The request's Origin header value; NULL or empty when there is none.
 * @return true when there is no Origin, or when it is http(s):// plus exactly the Host.
 *         "null" (sandboxed frames, file:// pages), other schemes and other hosts are refused.
 */
static inline bool http_origin_matches_host(const char *host, const char *origin)
{
    if (origin == NULL || origin[0] == '\0') {
        return true;
    }
    if (host == NULL || host[0] == '\0') {
        return false;
    }
    const char *authority;
    const char *default_port;
    if (strncasecmp(origin, "http://", 7) == 0) {
        authority = origin + 7;
        default_port = ":80";
    } else if (strncasecmp(origin, "https://", 8) == 0) {
        authority = origin + 8;
        default_port = ":443";
    } else {
        return false;
    }
    if (authority[0] == '\0') {
        return false;
    }
    size_t origin_len = http_origin_authority_len(authority, default_port);
    size_t host_len = http_origin_authority_len(host, default_port);
    return origin_len == host_len && strncasecmp(authority, host, origin_len) == 0;
}
