#ifndef ROUTE_SERVICE_H
#define ROUTE_SERVICE_H

/**
 * @file route_service.h
 * @brief The device side of the travel time (build option `route-time`): reads the keys and places
 * of the settings, asks TomTom first and HERE second (a provider without a key is skipped; one that
 * does not answer, refuses the key or has no quota left is followed by the next), and keeps the
 * last answer for five minutes so that a page drawn twice in a row asks once.
 *
 * The requests hold the personal API keys: they are never logged, and neither are addresses.
 */

#include <stdbool.h>
#include <stddef.h>

#include "route_time.h"

typedef enum {
    ROUTE_STATUS_OK = 0,
    ROUTE_STATUS_NO_KEY,       // no key of any provider
    ROUTE_STATUS_NO_PLACES,    // the places are not taken over and checked
    ROUTE_STATUS_NO_NETWORK,   // the frame is offline on this wake
    ROUTE_STATUS_KEY_REFUSED,  // every provider with a key refused it
    ROUTE_STATUS_QUOTA,        // every provider with a key is out of requests
    ROUTE_STATUS_NOT_FOUND,    // nothing found for the address, or no route between the places
    ROUTE_STATUS_IMPLAUSIBLE,  // a route that cannot be right (see route_plausible())
    ROUTE_STATUS_FAILED,       // no answer, or answers that cannot be read
} route_status_t;

/** @brief "ok", "no_key", "no_places", "no_network", "key_refused", "quota", "not_found", ... */
const char *route_status_name(route_status_t status);

typedef struct {
    route_leg_t there;  // start -> destination
    route_leg_t back;   // destination -> start
    char source[12];    // "TomTom" or "HERE": the provider that answered
    char message[96];   // the provider's own words for the last error ("" if none)
} route_times_t;

/**
 * @brief The places an address can be: up to `max` addresses or streets for `text`, best match
 * first. `*count` receives how many; `source` the provider that answered; `message` the provider's
 * words if everything failed.
 */
route_status_t route_service_geocode(const char *text, route_place_t *out, int max, int *count,
                                     char *source, size_t source_len, char *message,
                                     size_t message_len);

/**
 * @brief Takes over two places and checks them: the route is calculated both ways between them and
 * must be plausible. Only then the places (with their texts) are stored in the settings and marked
 * as checked, and `out` holds the times of now; otherwise nothing is stored and the settings keep
 * their state (an earlier check stays valid).
 */
route_status_t route_service_check(const char *from_text, const route_place_t *from,
                                   const char *to_text, const route_place_t *to,
                                   route_times_t *out);

/**
 * @brief The travel time there and back between the places of the settings, with the traffic of
 * now: what the fuel page shows. Needs the places checked, a key and a network (`wifi_connected`).
 */
route_status_t route_service_times(route_times_t *out, bool wifi_connected);

/** @brief Forgets the kept answer (the settings changed). */
void route_service_forget(void);

#endif
