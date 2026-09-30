# Information screens

> **Build option:** compiled in only with `python build.py --with info-screens` (needs `agenda` and `glyphs`; the build pulls
> them in); without it the firmware is the upstream firmware (see [FEATURES.md](FEATURES.md)). On its own this option adds no
> new page - it is the base that pages such as the [chore wheel](CHORE_WHEEL.md) (`--with chore-wheel`) are built on.

The Agenda draws a page of ToDo items and calendar events whenever its schedule fires (Settings -> Agenda -> Schedule). With
this option the frame can also draw **other full-screen pages** on that same schedule: every time the schedule would draw the
Agenda, the next page of the rotation is drawn instead. The Agenda itself is one member of the rotation.

## Settings

Settings -> Agenda -> **Information screens**: tick the pages that take part. The Agenda is ticked by default, so a firmware
with this option behaves as before until a page is added.

- One ticked page: every run of the schedule draws that page. Tick only "Agenda" for the plain Agenda.
- Several ticked pages: they take turns in a fixed order (Agenda first, then the others in the order of the list). With a
  schedule of `0 */12 *` and the Agenda plus the chore wheel ticked, the frame shows the Agenda at 00:00 and the chore wheel at 12:00.
- Nothing ticked: the Agenda (a rotation cannot be empty).
- The schedule is active as soon as a page other than the Agenda is ticked, even if neither ToDo nor a calendar is switched on.
  An Agenda without ToDo and calendars has nothing to show and is skipped in the rotation.

Over the API: `GET /api/config` reports `info_screens` (the ticked pages, by name: `agenda`, `chore-wheel`) and
`info_screens_available` (the pages this firmware contains); `PATCH /api/config` accepts `info_screens` as a list of names
(unknown names are ignored).

## Language

The pages use the language of the on-display text - the overlay language setting (Settings -> Overlays -> Overlay language:
English or German), the same the Agenda follows; a build without the `overlays` option stays English. German month and weekday
names come with real umlauts thanks to the `glyphs` option.

## How it works

- **Rotation:** a counter kept in the settings memory (so it goes on after a sleep) selects the next page among the ticked
  ones. It only moves when there is more than one page in the rotation.
- **Drawing:** every page is a plain function that draws into an RGB canvas the size of the panel (`main/screen_canvas.c`: text in
  the frame's 17x24 font at 1x-4x, rectangles, discs, ring sectors, pills), which is written like the Agenda's picture and shown
  through the normal display path. The functions know nothing about the hardware, so they are tested on a PC and can be looked at
  as pictures for every board size (`host_tests/render_screens.cpp`).
- **Sizes:** the layout follows the panel: 800x480, 960x540, 480x800, 1200x1600 and 1872x1404 were checked, landscape and portrait.
- **Memory:** the canvas (width x height x 3 bytes) is allocated in PSRAM for the moment of drawing and freed after it.

## Adding a page

A page is a `screen_<name>.c` with a render function, one entry in `info_screens.h` (the ids are bits of a stored mask - do
not renumber) and `info_screens.c`, its own build option that requires `info-screens`, and its settings. See
`main/screen_chore_wheel.c` for a complete one.

## Limits

- The pages only redraw when the schedule fires; there is no live clock.
- Pages that need the internet (none is in yet) will draw what they have when the frame has no network on that wake.
