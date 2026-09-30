#ifndef SCREEN_FACT_H
#define SCREEN_FACT_H

#include "fact_pack.h"
#include "info_screens_core.h"
#include "screen_canvas.h"

/**
 * @file screen_fact.h
 * @brief The fact-of-the-day page (build option `fact-of-the-day`): a red header with the heading
 * and the date, the topic of the fact as a blue pill, the fact in the biggest text that fits, and -
 * if the fact has one - a yellow box with a question to think about.
 *
 * Pure drawing with no ESP-IDF dependency, so the host tests and the render harness link it.
 */

/** @brief Draws the page for one fact (text of the fact as UTF-8, see fact_pack.h). */
void fact_screen_render(canvas_t *canvas, const info_now_t *now, const fact_t *fact);

#endif
