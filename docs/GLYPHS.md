# Umlauts, sharp s, degree and euro in the text on the display

> **Build option:** compiled in only with `python build.py --with glyphs`; without it the firmware is the upstream firmware
> (see [FEATURES.md](FEATURES.md)). It changes only text that another feature draws (overlays, the Agenda, Telegram captions).

The text the frame draws itself uses a fixed 17x24 pixel bitmap font that contains ASCII only. Text with other characters was
made ASCII first: **ä ö ü Ä Ö Ü ß** became `ae oe ue Ae Oe Ue ss` ("Käse" -> "Kaese"), and everything else - the degree
sign, the euro sign, other accents, emoji - was dropped. With this option nine characters are drawn as themselves:

| Character | Drawn as |
| --- | --- |
| ä ö ü | the font's own `a`, `o`, `u` with two dots above |
| Ä Ö Ü | the font's own `A`, `O`, `U` with two dots above |
| ß | a hand-drawn sharp s in the cell and stroke width of the font |
| ° | a small ring at the height of the capitals |
| € | the font's `C` with two bars |

Because the umlauts are built from the font's letters they always match it. Every other character that has no glyph (é, ñ,
emoji, ...) is still dropped, as before.

The fact-of-the-day page in German, with ä, ö, ü, Ü and ß drawn as themselves:

<img src="screens/info-fact-de.png" width="480" alt="The fact of the day page in German, with umlauts and a sharp s">

*Sample data, drawn by the firmware's own code - [more pictures](SCREENSHOTS.md).*

## Where it shows

Everything that goes through the frame's text drawing: the overlay bar with captions and headlines, the Agenda's ToDo and
Calendar columns (events, to-dos, names), Telegram captions on the picture. Text that the frame itself writes is unchanged
(it is English or ASCII already), so for instance the temperature in the Agenda header still reads `27C`.

## How it works

The text sanitizer keeps the nine characters as single bytes `0x80`-`0x88` instead of the digraph; the drawing code turns such a
byte into its bitmap. All other code that handles display text counts one byte per glyph, so wrapping, centring and the
`~` at a cut-off keep working. The bytes are not valid UTF-8: they exist only between the sanitizer and the drawing code
(and in the frame's own cache of expanded calendar events), never in anything sent over the network.

## Notes

- After switching this option **off** again (a firmware without it), a cache file written with it shows nothing at those
  places until the next refresh; the next fetch rewrites it.
- The bitmap of each glyph is in `main/glyph_extras.c`; `host_tests/test_glyph_extras.cpp` prints them as pictures
  (`--gtest_filter=*Pictures*`) and checks the umlauts against the real font table.
