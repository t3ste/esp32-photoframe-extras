#ifndef SCREEN_CHORE_WHEEL_H
#define SCREEN_CHORE_WHEEL_H

#include "info_screens_core.h"
#include "screen_canvas.h"

/**
 * @file screen_chore_wheel.h
 * @brief The chore wheel (build option `chore-wheel`): who does which chore this week.
 *
 * Up to CHORE_MAX_MEMBERS members and CHORE_MAX_TASKS chores. The chores go round the members by
 * ISO week: chore `t` of week `w` belongs to member `(t + w) mod members`. The screen shows a wheel
 * with one coloured sector per member, turned so that the member of the first chore is at the top
 * under a pointer, the week number in its middle, and one card per chore with its number, its name,
 * who has it and - room permitting - who has it next week.
 *
 * Pure drawing with no ESP-IDF dependency, so the host tests and the render harness link it.
 */

#define CHORE_MAX_MEMBERS 5
#define CHORE_MAX_TASKS 6
#define CHORE_NAME_MAX 24

typedef struct {
    int member_count;
    char members[CHORE_MAX_MEMBERS][CHORE_NAME_MAX];  // UTF-8, as typed
    int task_count;
    char tasks[CHORE_MAX_TASKS][CHORE_NAME_MAX];  // UTF-8, as typed
} chore_config_t;

/** @brief Builds the configuration from the two lists as users type them (see info_parse_list()).
 */
void chore_config_parse(const char *members_text, const char *tasks_text, chore_config_t *out);

/** @brief The member who has chore `task` in ISO week `iso_week`; -1 without members. */
int chore_assignee(int task, int iso_week, int member_count);

/** @brief The colour of a member (red, blue, green, yellow, black - the palette of the panels). */
canvas_color_t chore_member_color(int member);

/** @brief Draws the whole screen. */
void chore_wheel_render(canvas_t *canvas, const info_now_t *now, const chore_config_t *config);

#endif
