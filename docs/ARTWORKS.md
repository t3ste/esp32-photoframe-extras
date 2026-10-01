# Artworks

> **Build option:** compiled in only with `python build.py --with artworks` (needs `overlays`; the build pulls it in), part of the
> `extras` bundle and of every full build. Without it the firmware is the upstream firmware (see [FEATURES.md](FEATURES.md)).
>
> **Status:** built, tested on a PC against real answers of the three museums, and compiled for every board; **not yet run on a
> frame** - please report what you see ([hardware test report](../.github/ISSUE_TEMPLATE/hardware-test-report.md)).

A rotation mode that shows **a painting, a drawing or a print from a museum**. At each Auto Rotate turn the frame picks the kind of
work first (equal chance among the kinds you allow), then a random work of that kind from the museum's open-access service, loads the
smallest picture that covers the panel, shows it with a small caption and keeps it in an album, so that it is still there when there
is no network.

> **Usage rights.** This firmware is intended for private use. The pictures come from the open-access services of museums and
> collections. Their terms of use differ by museum and by work: public domain or CC0 in most cases, but some works or services ask for
> attribution or limit use. You are responsible for checking and observing the terms of use and attribution requirements of the source
> you use. The optional caption shows the title, artist and source of a picture, which helps you to credit it. The authors of this
> firmware give no warranty that a picture may be used for your purpose.

The frame only takes a work whose record says that its picture is **public domain or CC0**, and only loads a picture from the museum's
own server.

## Switching it on

Settings -> **Auto Rotate**: tick *Auto Rotate*, choose the mode **Artworks - A painting, drawing or print from a museum**, set the
schedule as usual (Auto Rotate's own), and fill in the card that appears:

| Setting | Default | What it does |
| --- | --- | --- |
| Kinds of work | all three | Paintings, drawings, prints. The kind is drawn first, with equal chance among the ticked ones |
| Sources | all three | Rijksmuseum, SMK, Smithsonian: asked in this order, at most two per rotation |
| Smithsonian API key | empty | Empty: the shared demo key (10 requests an hour for everybody). A free key from [api.data.gov](https://api.data.gov/signup/) is stored write-only; *Remove* deletes it |
| Keep the pictures in an album | on | Off: the picture is shown and not kept |
| Album | `Art` | The album's folder name (letters, digits, blank, `-`, `_`) |
| Keep at least this much free (%) | 20 | The free space of the frame's storage that is not touched (5-80) |
| Clean up to this much free (%) | 30 | Where the clean-up stops; above the first level, at most 95 |
| Show a small caption | on | Artist, title and year at the bottom left |

## What one rotation does

1. Pick the kind of work, then the first source that has works of that kind (the Smithsonian has no prints).
2. Ask it for a random work (a few small requests, see below) and check that the picture is public domain or CC0.
3. Load the picture in the smallest size that covers the panel: the museum's IIIF server fits it into the panel's box, the Smithsonian
   takes the longer side. A JPEG of about 50-250 KB; it must be a baseline JPEG (the frame cannot decode progressive ones) and at most 1.5 MB.
4. Make it display-ready with the frame's usual picture settings (dithering, scale mode), keep a thumbnail for the web gallery and a
   caption file, show it.
5. If the free space is at or below the first level, **delete the oldest pictures this mode saved** until it is back at the second level
   (see below) - before keeping the new one.

**No network, or anything fails** (no work found, a refused picture, a museum that does not answer): the frame shows **a random picture
of the album** instead; with an empty album it keeps the picture on the panel. The Web UI's last fetch error says why.

## Sources

| | Rijksmuseum | SMK (Statens Museum for Kunst) | Smithsonian American Art Museum |
| --- | --- | --- | --- |
| Needs | nothing | nothing | nothing (a key is optional) |
| Works with a picture (October 2026) | paintings 4,874, drawings 48,554, prints 386,892 | 4,637 / 13,929 / 16,540 | 4,067 paintings, 2,379 drawings |
| How a work is picked | a random year (1400-1950) of the kind, then one of its (up to 100) works | a random work of the kind among all of them | a random work among the first 3,500 / 2,000 |
| Requests per rotation | 4 small answers (~30 KB) and the picture | 2 small answers and the picture | 1 answer (~5 KB) and the picture |
| The picture is taken when | its record says public domain mark or CC0 and it is downloadable | the work is flagged public domain | its picture is CC0 |
| Titles | English or Dutch | mostly Danish | English |

The services are free and belong to the museums: the frame asks for one work per rotation and sends a `User-Agent` that names this
firmware and the repository it was built for. The museums see the frame's IP address, nothing else.

## The album and the free space

New pictures are kept in the album `Art` (a folder like any other album): `<source>-<id>.epdgz` (or `.png`), its thumbnail `.jpg`, and
`<source>-<id>.caption.json`. The album is created on first use and **switched on for the storage mode's Auto Rotate**, so a frame that
has no network - or that you switch to the *Storage* mode - shows these pictures with the others; switch the album off in the Gallery if
you do not want that.

The rule that keeps the free space (both levels are settings):

- before a new picture is kept, if the free space of the storage is **at or below 20 %**, the oldest pictures **made by this mode** are
  deleted until it is **at least 30 %** free again;
- **only pictures in the album `Art` that have their caption file are ever deleted.** Photos you put into that album yourself have no
  such file and are never touched, nor are other albums;
- if deleting all of them would not bring the free space above the first level (the storage is full of something else), nothing is
  deleted, the new picture is shown and **not kept**;
- the rule looks at the whole storage, so a card that is already 80 % full gets no new pictures kept, and an album never holds more than
  1,000 pictures of this mode.

On the boards that store on their internal flash (the XIAO EE02/EE03/EE04, 8.9 MiB of storage) 20 % free leaves room for only a handful of
pictures; the clean-up keeps working. The SD card boards are limited by the rule, not by size.

## The caption

<img src="screens/artworks-caption.png" width="560" alt="A painting by Jan Toorop on the panel with its caption at the bottom left: white text with a black border">

*Jan Toorop, Misty Sea (1899), Rijksmuseum, public domain mark. The caption is drawn by the firmware's own code; the picture is not
dithered here, on the panel it is made of the panel's inks.*

One line at the bottom left, no bar: **white text with a one-pixel black border**, drawn in the frame's font (17 x 24 pixels), as
`Artist - Title (Year)`, cut with `~` to the width of the picture. The font has the ASCII letters and `ä ö ü Ä Ö Ü ß ° €`; other accented
letters are written as plain letters (`é` as `e`, `ø` as `o`), anything else is left out. The text is read from the caption file next to
the picture when the picture is shown - so it also appears when Auto Rotate shows an album picture in the storage mode - and the switch
*Show a small caption* turns it off everywhere. Pictures without a caption file show none.

## Notes

- **The Smithsonian's demo key** is shared by everybody who uses it and allows 10 requests an hour; with a short schedule it runs out and
  the next source (or the album) takes over. A personal key (free) is much more generous.
- A work's **title and artist** are the museum's text; some are in Danish or Dutch.
- **Memory**: loading and converting a picture needs the frame's PSRAM like any other photo; a picture that does not fit is refused and the
  album's picture is shown.
- **Not part of this option**: the *Art helper* `scripts/fetch_art.py` ([ART_FETCH.md](ART_FETCH.md)) is a separate PC program that fills an
  album from a curated list; it does not need this option.

## Settings in the API

`GET/PATCH /api/config`: `rotation_mode` (`"artworks"`), `art_types` and `art_sources` (bit masks: kinds 1 painting, 2 drawing, 4 print;
sources 1 Rijksmuseum, 2 SMK, 4 Smithsonian), `art_save`, `art_album`, `art_free_min`, `art_free_target`, `art_caption`, and the key as
`art_si_key` (write-only: `GET` answers only `art_si_key_configured`; `art_si_key_clear: true` removes it; the export with credentials
includes it).

## How it was checked

Pure code (choice of kind and source, caption text, the readers of the three services, the album rule) is host-tested against real answers
of the museums recorded on 2026-10-01 (`host_tests/data/art/`), with a run that changes bytes of those answers under ASan/UBSan; the caption
drawing is tested on a canvas with guard bytes. The firmware compiles for every board. A rotation on a real frame has not been run yet.
