#ifndef JSON_SCAN_H
#define JSON_SCAN_H

/**
 * @file json_scan.h
 * @brief Two helpers for pages that read a long JSON answer object by object instead of building
 * the tree of the whole text (build option `info-screens`): the fuel prices and the market quotes
 * take one station or one day at a time, so a big answer needs little memory and an answer that was
 * cut off still gives what came before the cut. Pure C, so the host tests link it.
 */

/** @brief `p` moved past spaces, tabs and line breaks. */
const char *json_skip_blanks(const char *p);

/**
 * @brief The end of the JSON object that starts at `start` (a '{'): the address of its closing
 * brace, or NULL if the text ends first. Braces inside strings do not count.
 */
const char *json_object_end(const char *start);

#endif
