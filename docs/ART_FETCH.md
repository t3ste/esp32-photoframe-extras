# Art helper (`scripts/fetch_art.py`)

A PC-side helper, **not a firmware feature** (no build option): it fetches curated public-domain artworks from the
public [paperlesspaper art API](https://github.com/paperlesspaper/paperlesspaper-art) (`art.paperlesspaper.de`), renders
them for your board with this repository's own [`process-cli`](../process-cli/README.md) and either writes an album
folder you copy to the SD card or uploads the result to a frame.

The API lists roughly 12,000 works from the Met, the Art Institute of Chicago and Wikimedia Commons (plus an icon
collection that this helper ignores) with title, artist, licence and, for each, JPEG variants of 512, 1024 and 3000 px
width. Only the smallest variant that covers the board is downloaded - no multi-megabyte originals and no SVG.

## Requirements

- Python 3.8+ (standard library only) and Node.js with `npm ci` run once in `process-cli/` (see its README for the
  Cairo packages it needs on Linux/macOS).

## Usage

```bash
# 12 curated, public-domain pictures for a board, into ./art-out/Art/ (copy that folder to the SD card)
python scripts/fetch_art.py --board waveshare_photopainter_73

# a specific taste: sources, a search word, another album name, more pictures
python scripts/fetch_art.py --board seeedstudio_xiao_ee03 --source met,artic --query landscape --album Landscapes --count 30

# straight onto a frame over its HTTP API (creates the album if needed)
python scripts/fetch_art.py --board waveshare_photopainter_73 --upload --host photoframe.local

# look first, download nothing
python scripts/fetch_art.py --board waveshare_photopainter_73 --dry-run
```

| Option | Meaning |
| --- | --- |
| `--board ID` | Board from `boards/boards.json` (sets the resolution and, for the grey panels, 16-level output) |
| `--count N` | Pictures to fetch, 1-200 (default 12) |
| `--source LIST` | `met`, `artic`, `wikimedia` (default: all three) |
| `--query TEXT` | Free-text search over title, artist, tags |
| `--min-rating N` | The curators' rating, 1-5 (default 5; 0 = any) |
| `--allow-cc` | Also Creative Commons works (default: public domain only) |
| `--seed TEXT` | The same seed gives the same selection |
| `--scale-mode fit\|cover` | `fit` (default, letterboxed - paintings keep their edges) or `cover` |
| `--size 512\|1024\|3000` | Override the download width |
| `--album NAME`, `--out DIR` | Album name (default `Art`) and output folder (default `art-out`) |
| `--upload`, `--host H` | Upload instead of writing files, and the frame's address |
| `--max-mb N` | Refuse a single download larger than this (default 25) |
| `--dry-run` | List what would be fetched |

Photos are rotated to match the display (`--auto-orient`), so a portrait painting on a landscape panel is turned and
fitted; use `--scale-mode cover` if you prefer a cropped fill.

## Attribution

Every run writes an `ATTRIBUTION.md` next to the output (`ATTRIBUTION-<album>.md` in the output folder when uploading):
title, artist, licence and source page of each picture. Public-domain works need none, but Creative Commons ones
(`--allow-cc`) do - keep that file with the pictures.

## Being a good API citizen

This is someone else's free service: keep `--count` modest, do not schedule it, and note that it was written against
the API as documented in the linked README (`GET /api/artworks`). If the service changes or asks for something else,
the helper should change with it. Tests (`scripts/test_fetch_art.py`) never touch the network.
