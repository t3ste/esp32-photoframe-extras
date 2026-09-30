#ifndef INFO_SCREENS_H
#define INFO_SCREENS_H

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

/**
 * @file info_screens.h
 * @brief Full-screen information pages besides the agenda (build option `info-screens`, and the
 * screens built on it: `chore-wheel`, ...).
 *
 * The screens share the agenda's schedule (Settings -> Agenda -> schedule): each time the agenda
 * would be drawn, the next screen of the rotation is drawn instead - the agenda itself is one
 * member of the rotation, and the others are the info screens that are switched on. The rotation
 * counter is kept in the settings memory, so it goes on where it stopped after a sleep.
 *
 * Every screen is drawn by a pure function into an RGB canvas (screen_canvas.h) and written like
 * any picture; this file only chooses, feeds them settings and data, and shows the result.
 */

// The screens, as bits of the mask of the rotation. Bit 0 is the agenda; the others exist only if
// their build option is in. Do not renumber: the mask is stored.
typedef enum {
    INFO_SCREEN_AGENDA = 0,
    INFO_SCREEN_CHORE_WHEEL = 1,
    INFO_SCREEN_WEATHER = 2,
    INFO_SCREEN_FACT = 3,
    INFO_SCREEN_FINANCE = 4,
    INFO_SCREEN_FUEL = 5,
    INFO_SCREEN_MARKETS = 6,
    INFO_SCREEN_COUNT
} info_screen_id_t;

/** @brief The name used in the settings ("agenda", "chore-wheel", "weather", "fact", "finance",
 * "fuel", "markets"), NULL for an unknown id. */
const char *info_screen_name(int id);

/** @brief The id for a name of the settings, -1 if unknown. */
int info_screen_id_from_name(const char *name);

/** @brief The screens built into this firmware, as a mask (the agenda is always one). */
uint32_t info_screens_compiled_mask(void);

/**
 * @brief Whether any screen other than the agenda is in the rotation - what makes the agenda
 * schedule run even when neither the ToDo list nor a calendar is switched on.
 */
bool info_screens_extra_enabled(void);

/**
 * @brief Chooses the screen for this run of the schedule and moves the rotation on. Returns the
 * screen's id, INFO_SCREEN_AGENDA for the agenda (also whenever the rotation is empty or only the
 * agenda is in it - so a build with the option behaves as before until a screen is switched on).
 *
 * @param agenda_has_content Whether the agenda has anything switched on (ToDo or a calendar); an
 * agenda without content is skipped in the rotation.
 */
int info_screens_next(bool agenda_has_content);

/**
 * @brief Draws one screen (not the agenda) and shows it.
 *
 * @param wifi_connected Whether this wake has a network (screens with data from the internet then
 * draw what they have).
 */
esp_err_t info_screens_show(int id, bool wifi_connected);

#endif
