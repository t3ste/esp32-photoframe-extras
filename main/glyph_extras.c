#include "glyph_extras.h"

#include <string.h>

#define GLYPH_WIDTH 17

// Hand-drawn glyphs, one string per row of the 17x24 cell ('#' = ink). The font's own capitals fill
// rows 3-16 and its lowercase letters rows 6-16, with strokes 2-3 pixels wide; these follow that.
#define BLANK "................."

static const char *const SHARP_S_ART[GLYPH_ROWS] = {
    BLANK,
    BLANK,
    BLANK,                // 0-2
    "....#######......",  // 3
    "...#########.....",  // 4
    "..###.....###....",  // 5
    "..###......###...",  // 6
    "..###......###...",  // 7
    "..###....#####...",  // 8
    "..###....#####...",  // 9
    "..###......#####.",  // 10
    "..###.......####.",  // 11
    "..###.......####.",  // 12
    "..###......#####.",  // 13
    "..###...#######..",  // 14
    "..###...######...",  // 15
    "..###............",  // 16
    BLANK,
    BLANK,
    BLANK,
    BLANK,
    BLANK,
    BLANK,
    BLANK,  // 17-23
};

static const char *const DEGREE_ART[GLYPH_ROWS] = {
    BLANK,
    BLANK,
    BLANK,                // 0-2
    ".....#####.......",  // 3
    "....##...##......",  // 4
    "....##...##......",  // 5
    "....##...##......",  // 6
    ".....#####.......",  // 7
    BLANK,
    BLANK,
    BLANK,
    BLANK,
    BLANK,
    BLANK,
    BLANK,
    BLANK,
    BLANK,
    BLANK,
    BLANK,
    BLANK,
    BLANK,
    BLANK,
    BLANK,
    BLANK,  // 8-23
};

static void set_pixel(uint8_t *glyph, int row, int col)
{
    if (row >= 0 && row < GLYPH_ROWS && col >= 0 && col < GLYPH_WIDTH) {
        glyph[row * GLYPH_BYTES_PER_ROW + col / 8] |= (uint8_t) (0x80 >> (col % 8));
    }
}

static void from_art(uint8_t *glyph, const char *const art[GLYPH_ROWS])
{
    memset(glyph, 0, GLYPH_BYTES);
    for (int row = 0; row < GLYPH_ROWS; row++) {
        for (int col = 0; col < GLYPH_WIDTH && art[row][col] != '\0'; col++) {
            if (art[row][col] == '#') {
                set_pixel(glyph, row, col);
            }
        }
    }
}

static void copy_letter(uint8_t *glyph, const uint8_t *font_table, char letter)
{
    memcpy(glyph, font_table + (letter - ' ') * GLYPH_BYTES, GLYPH_BYTES);
}

// Two dots, 2x2 pixels, centred over the letter, starting at `top_row`.
static void add_diaeresis(uint8_t *glyph, int top_row)
{
    static const int columns[] = {5, 6, 10, 11};
    for (int r = top_row; r < top_row + 2; r++) {
        for (unsigned i = 0; i < sizeof(columns) / sizeof(columns[0]); i++) {
            set_pixel(glyph, r, columns[i]);
        }
    }
}

// A bar of two rows from column 0 to `last_col`.
static void add_bar(uint8_t *glyph, int top_row, int last_col)
{
    for (int r = top_row; r < top_row + 2; r++) {
        for (int c = 0; c <= last_col; c++) {
            set_pixel(glyph, r, c);
        }
    }
}

bool glyph_extras_is_code(uint8_t byte)
{
    return byte >= GLYPH_EXTRA_FIRST && byte <= GLYPH_EXTRA_LAST;
}

uint8_t glyph_extras_code_for_codepoint(uint32_t cp)
{
    switch (cp) {
    case 0x00E4:
        return GLYPH_A_UMLAUT;
    case 0x00F6:
        return GLYPH_O_UMLAUT;
    case 0x00FC:
        return GLYPH_U_UMLAUT;
    case 0x00C4:
        return GLYPH_CAPITAL_A_UMLAUT;
    case 0x00D6:
        return GLYPH_CAPITAL_O_UMLAUT;
    case 0x00DC:
        return GLYPH_CAPITAL_U_UMLAUT;
    case 0x00DF:
        return GLYPH_SHARP_S;
    case 0x00B0:
        return GLYPH_DEGREE;
    case 0x20AC:
        return GLYPH_EURO;
    default:
        return 0;
    }
}

bool glyph_extras_compose(uint8_t code, const uint8_t *font_table, uint8_t out[GLYPH_BYTES])
{
    memset(out, 0, GLYPH_BYTES);
    switch (code) {
    case GLYPH_A_UMLAUT:
        copy_letter(out, font_table, 'a');
        add_diaeresis(out, 3);
        return true;
    case GLYPH_O_UMLAUT:
        copy_letter(out, font_table, 'o');
        add_diaeresis(out, 3);
        return true;
    case GLYPH_U_UMLAUT:
        copy_letter(out, font_table, 'u');
        add_diaeresis(out, 3);
        return true;
    case GLYPH_CAPITAL_A_UMLAUT:
        copy_letter(out, font_table, 'A');
        add_diaeresis(out, 0);
        return true;
    case GLYPH_CAPITAL_O_UMLAUT:
        copy_letter(out, font_table, 'O');
        add_diaeresis(out, 0);
        return true;
    case GLYPH_CAPITAL_U_UMLAUT:
        copy_letter(out, font_table, 'U');
        add_diaeresis(out, 0);
        return true;
    case GLYPH_SHARP_S:
        from_art(out, SHARP_S_ART);
        return true;
    case GLYPH_DEGREE:
        from_art(out, DEGREE_ART);
        return true;
    case GLYPH_EURO:
        copy_letter(out, font_table, 'C');
        add_bar(out, 7, 9);
        add_bar(out, 11, 9);
        return true;
    default:
        return false;
    }
}
