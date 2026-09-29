# Uploading several images at once

> **Build option:** compiled in only with `python build.py --with multi-upload`; without it the firmware and its web
> UI are the upstream ones (see [FEATURES.md](FEATURES.md)).

The Web UI's upload takes a whole selection of files (up to 200) instead of one. Select several files in the file
dialog or drop them onto the upload area; a queue shows them together with a few options, and nothing is uploaded until
you press **Upload N files**. A single file still opens the editor exactly as before.

## What happens to each file

| File | What the frame receives |
| --- | --- |
| A photo (JPG, PNG, HEIC, WebP, GIF, BMP) | Converted in the browser with the **current processing settings** (Settings → Processing), one file after the other, exactly like a single upload - but without the crop editor. **Photos**: *Cover* crops to fill the panel (the default), *Fit* letterboxes. |
| `.epdgz` | Uploaded **as it is**, without converting. The file must be rendered for this panel: its expanded size has to be `width × height ÷ 2` bytes, otherwise it is refused with a message (an EPDGZ for another board would show garbage). |
| `.png` with **"PNG files are already rendered for this panel"** ticked | Uploaded as it is if it is exactly the panel's size (either orientation); a PNG of any other size is treated as a photo and converted. |

Pre-rendered files keep **their own name** (reduced to letters, digits and `._-`); uploading the same name again
replaces the earlier file. Converted photos get a new unique name each time, as with a single upload. A pre-rendered
EPDGZ has no preview a browser could draw, so it is stored **without a thumbnail** (the gallery shows its placeholder
icon); a PNG gets a thumbnail like any other upload.

The queue shows each file's state (waiting, uploading, done, failed with the reason, skipped) and a progress bar. **Stop
after this file** ends the batch early. The album is reloaded once, at the end.

Needs storage (an SD card or the internal flash) - without it there is nowhere to keep several images. Works in the
offline hotspot mode too: the converter is part of the web UI the frame serves.

## Pre-rendering on a PC

[`process-cli`](../process-cli/README.md) produces `.epdgz` files for a board (`--board <id>`), and
[`scripts/fetch_art.py`](ART_FETCH.md) fills a folder with art for one; select that folder's `.epdgz` files in the
upload dialog. Converting on the PC is faster for hundreds of photos than converting in a phone's browser.

## Notes

- The upload endpoint (`POST /api/upload`) accepts an image without a thumbnail when this option is built in; without it
  both are still required.
- A batch is sequential on purpose: the frame stores one upload at a time, and a phone's browser converts photos one at a time anyway.
- The same photo selected twice is uploaded twice; nothing compares the files.
