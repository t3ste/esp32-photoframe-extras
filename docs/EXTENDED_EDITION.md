# Extended edition

The **extended edition** is this firmware plus a group of newer options that are **not** part of the base project
([t3stier/esp32-photoframe-rebuild](https://github.com/t3stier/esp32-photoframe-rebuild), branch `main`): `webcal`, `source-auth`,
`caldav`, `caldav-todo`, `multi-upload`, `upload-dedup`, `glyphs` and the information pages (`info-screens`, `chore-wheel`,
`weather-screen`, `fact-of-the-day`, `finance-snapshot`, `fuel-prices`, `market-quotes`) and the rotation mode `artworks`.
Together they are the bundle
**`extras`**. How to use them: [EXTRAS_USER_GUIDE.md](EXTRAS_USER_GUIDE.md).

With no option chosen the firmware is still the upstream firmware, exactly as in the base project; the extras only exist when
you switch them on (or build the full firmware, which contains everything the board supports).

What the pages and the Agenda look like: [SCREENSHOTS.md](SCREENSHOTS.md).

## Why a separate edition

The extras are a large change for a small audience, and only one board was available for testing them. They are therefore kept
out of the base project and offered here, next to it, as a variant that is merged from the base regularly
([EXTENDED_LINE.md](EXTENDED_LINE.md) is the maintainer's guide).

## Where to get it

| | |
| --- | --- |
| **Source** | branch `extras` of [t3stier/esp32-photoframe-rebuild](https://github.com/t3stier/esp32-photoframe-rebuild/tree/extras) (next to `main`) - and the same code as `main` of the repository below |
| **Releases, web flasher, update feed** | [t3ste/esp32-photoframe-extras](https://github.com/t3ste/esp32-photoframe-extras) |

A frame that runs a build from the extended repository updates itself from **that** repository's releases and keeps the extras.

## Building it yourself

```bash
python build.py --board waveshare_photopainter_73 --with extras      # the extras and what they need
python build.py --board waveshare_photopainter_73 --all-features     # everything the board supports
```

> **Important - the update feed.** A build made from source points its update check at the **base project's** releases by
> default (`FORK_OTA_REPO` in `main/Kconfig`). If such a frame installs an update, it is **replaced by the base project's firmware,
> which does not contain the extras.** So a frame you built yourself should either
>
> - be built with `--ota-repo t3ste/esp32-photoframe-extras` (the extended edition's own feed), or
> - have the **automatic update check switched off** (Settings -> Power -> Enable automatic update checks) and never press "install update".
>
> `build.py` prints a reminder of this whenever a feature is built in.

## What has been tested

Honest state: **one board**.

| Board | Status |
| --- | --- |
| Waveshare PhotoPainter 7.3" (`waveshare_photopainter_73`) | **Flashed and used** with the full firmware: every extra of the first release was run on the frame (calendars with logins, CalDAV, batch upload, duplicate detection, all information pages with real data, the Web UI in Chrome). **Not yet run on a frame: the artworks mode** ([ARTWORKS.md](ARTWORKS.md)) - built, tested on a PC against the museums' real answers, compiled for every board |
| the other seven boards (M5Paper, XIAO EE02/EE03/EE04, reTerminal E1002/E1003/E1004) | **Compiled** (locally for several, in CI for all) but **never flashed** - drawing sizes were checked on the PC for 800x480, 960x540, 480x800, 1200x1600 and 1872x1404 |

The pages are drawn by plain functions that are tested on a PC for every panel size, so layout errors on other boards are
unlikely - but colours on a real panel, memory use on the plain ESP32 (M5Paper) and the big panels have not been seen.

**Please tell us what you find**: open an issue with the *Hardware test report* template, also when everything works.
A first release for a board that has not been flashed is published as a pre-release.
The same holds for a feature that has not been run on a frame: a release that contains it is a pre-release until it has.

## What is not in it

Tree/bird/dinosaur "of the day" packs and a newsstand of front pages were considered and not built (the content or a WebP
decoder is missing). Everything else of the extras is in.

## Relation to the base project

The base project's `main` is merged into this edition regularly, so its fixes and features arrive here; nothing flows back
unless it is a general fix, which is applied to the base first. Upstream (`aitjcize/esp32-photoframe`) is merged into the base
project, not here. The [MIT license](../LICENSE) applies to everything.
