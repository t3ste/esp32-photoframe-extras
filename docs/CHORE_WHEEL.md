# Chore wheel

> **Build option:** compiled in only with `python build.py --with chore-wheel` (needs `info-screens`, which needs `agenda` and
> `glyphs`; the build pulls them in). Without it the firmware is the upstream firmware (see [FEATURES.md](FEATURES.md)).

A page for the fridge-door frame: **who does which chore this week**. A donut wheel with one coloured sector per household member
on one side, one card per chore on the other; the chores go round the members week by week, so nobody has to remember whose turn it is.

<img src="screens/info-chore-wheel.png" width="480" alt="The chore wheel page: a donut with one coloured sector per household member and one card per chore">

*Sample data, drawn by the firmware's own code - [more pictures](SCREENSHOTS.md).*

## Settings

Settings -> Agenda -> Information screens: tick **Chore wheel**, then fill in two lists (separate the names with commas; semicolons
or one name per line work too):

| Field | Example | Limits |
| --- | --- | --- |
| Members | `Anna, Ben, Clara` | up to 5 names, each up to 23 bytes |
| Chores | `Bins, Dishes, Vacuum` | up to 6 names, each up to 23 bytes |

Umlauts and the other characters of the [glyphs](GLYPHS.md) option (ä ö ü Ä Ö Ü ß ° €) can be used in names; other accents are
dropped as everywhere else on the display. Each list is at most 159 bytes in all (longer text is cut). The page appears on the
schedule of the Agenda (see [INFO_SCREENS.md](INFO_SCREENS.md)); a schedule such as `0 6 *` draws it every morning at 06:00.

Without members or without chores the page shows a short message that says what to set instead of an empty wheel.

## Who has what

Chore number `t` (counting from 0 in the order of the list) of ISO calendar week `w` belongs to member `(t + w) mod members`.
So the first chore moves on to the next member every Monday, and with as many chores as members everybody has exactly one chore a
week. The week number is the ISO week (Monday is the first day, week 1 holds the year's first Thursday).

## What is on the page

- **The wheel:** one sector per member in the colours red, blue, green, yellow and black (in this order of the members list; a
  panel with fewer colours shows what it can, like any picture), the first letter of the name in each sector, a pointer at the
  top, and the week number with the word WEEK / WOCHE in the middle. The wheel is turned so that the member who has the first
  chore is under the pointer.
- **The heading** of the cards: `THIS WEEK 2026-W40` (German: `DIESE WOCHE`).
- **The cards:** one per chore, with its number, its name, a pill in the member's colour with the member's name, and - when the
  panel is tall enough for a third line - `NEXT WEEK: <name>` (German: `NÄCHSTE WOCHE`).
- Landscape panels put the wheel on the left and the cards on the right; portrait panels the wheel above the cards. The text gets
  smaller on small panels until the cards still hold about 18 characters.

## Notes

- A member keeps their colour: it follows the place in the members list, not the week.
- Names are shown as typed; one that does not fit its place is cut with a `~` at the end.
- The lists are stored on the frame like the other settings and appear in a config export; nothing in them is secret.
