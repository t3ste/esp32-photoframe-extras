#ifndef ART_CAPTION_H
#define ART_CAPTION_H

/**
 * @file art_caption.h
 * @brief The caption of an artwork (build option `artworks`): the text folded to what the frame's
 * font can draw, and "Artist - Title (Year)" cut to the width of one line. Pure C, so the host
 * tests link it.
 */

#include <stddef.h>

/**
 * @brief The text as UTF-8 for the 17x24 font: the letters it has (a o u with two dots, capitals
 * of them, sharp s, degree, euro) stay as they are, other accented letters become their plain
 * letters (e acute -> e, o stroke -> o, ae ligature -> ae), dashes and curly quotes become ASCII,
 * everything else is dropped. Runs of blanks become one. Always ends in a 0; returns the length in
 * bytes.
 */
size_t art_caption_fold(const char *utf8, char *out, size_t out_len);

/** @brief The length of the UTF-8 text in characters (not bytes). */
int art_caption_char_count(const char *utf8);

/**
 * @brief "Artist - Title (Year)" of at most `max_chars` characters. Each part is folded first and
 * a missing part is left out. A text that is too long loses the end of the title, which is cut
 * with a "~" (the year and the artist stay while they fit); when even that does not fit the whole
 * text is cut.
 */
void art_caption_compose(const char *artist, const char *title, const char *year, int max_chars,
                         char *out, size_t out_len);

#endif
