# Fact of the day

> **Build option:** compiled in only with `python build.py --with fact-of-the-day` (needs `info-screens`; the build pulls it in
> together with what that needs). Without it the firmware is the upstream firmware (see [FEATURES.md](FEATURES.md)).

A page for the [information screens](INFO_SCREENS.md) that shows one fact a day: a red header with the heading and the date, the
topic of the fact as a blue pill, the fact itself in the biggest text that fits, and - if the fact has one - a yellow box with a
question to think about. The fact changes at midnight (local time) and is the same all day, however often the page is drawn.

## Settings

- Settings -> Agenda -> Information screens: tick **Fact of the day**.
- The language of the heading, the date and the built-in facts is the overlay language (Settings -> Overlays), English or German.
- **Your own facts** (optional): a text box under the tick box. Without it the frame uses its 24 built-in facts (space, animals,
  the body, history, ...; the same list in English and German). With it the frame uses only yours, in the language you wrote them.

## Your own facts

One fact per line; the file is plain UTF-8 text:

```
Honey almost never spoils.
Animals|An octopus has three hearts.
Space|A day on Venus is longer than its year.|Which way does Venus spin compared with Earth?
# a line starting with a hash is a comment
```

- `Fact`, `Topic|Fact` or `Topic|Fact|Question`; a bar inside the question is part of the question.
- Empty lines and comment lines are ignored; a line without fact text is skipped (the Web UI tells how many were).
- Umlauts, the sharp s, the degree and the euro sign are drawn as themselves; other accented letters and emoji are left out (as
  everywhere on the display).
- Up to 64 facts and 16 KB. A topic is cut after 47 bytes, a fact after 255 and a question after 127.
- The facts go round in the order of the file: fact number `day mod count` of the days since 1 January 1970, so a longer list
  simply lasts longer before it repeats.

The pack is saved as `facts.txt` in the frame's storage (SD card or flash) by the Web UI's Save button (`PUT /api/facts`;
`GET /api/facts` returns it). Saving an empty text removes it. A frame without storage keeps to the built-in facts.

## Notes

- The built-in facts are original wording of well-known facts; the pack files of other projects are not used (their licences do not
  allow it).
- The text is as big as it can be: up to four times the body text size, fewer lines and smaller text for a longer fact; a fact that
  does not fit even at the smallest size ends with `~`.
