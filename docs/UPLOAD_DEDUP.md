# Duplicate detection at upload

> **Build option:** compiled in only with `python build.py --with upload-dedup`; without it the firmware and its web UI are
> the upstream ones (see [FEATURES.md](FEATURES.md)). Needs storage (an SD card or the internal flash).

Uploading the same photo twice used to give two files in the album. With this option the frame keeps, for every album, a
small list of the **MD5** of its images and compares each new upload against it.

## Settings

Settings → Maintenance → **Duplicate Images**:

| Setting | Choices | What it does |
| --- | --- | --- |
| When an upload is already in the album | **Refuse it** (default) / Store it, but say so / Do nothing | *Refuse*: the frame answers `409` with the name of the file it duplicates and stores nothing; the Web UI says so and offers **Upload anyway**. *Store it, but say so*: the image is stored and the reply names the duplicate. |
| What counts as the same image | **The file** (default) / The picture | *The file*: the same bytes. *The picture*: the same **pixels** - an EPDGZ is compared by what is inside its gzip wrapper, a PNG by its decoded 8-bit RGB pixels and size - so one photo converted by two browsers (whose compressed files differ) still counts as one. A `.bmp` or an interlaced PNG is compared by its bytes. It reads and decodes the upload, so it takes a moment longer. |
| Index the images that were there before, in the background | off (default) / on | Images uploaded before the option existed (or before the comparison was switched) are not in the list. When on, the frame indexes them - once, in the background - right when the switch is saved and after every start-up. |

**Index now** does the same on request for one album or all albums, with progress; **Find duplicates** lists what one
album has twice, with a delete button for each file (nothing is ever removed automatically).

In the **batch upload** ([MULTI_UPLOAD.md](MULTI_UPLOAD.md)) an image the album already has is *skipped* with a note in the list;
the option **Upload images the album already has, too** turns the check off for that batch.

## How it works

- Each album folder holds a text file `.dedup`, one line per image: `s <32 hex digits> <file name>` (`p` instead of `s` for the
  pixel comparison). It is scanned line by line - nothing is kept in memory - so an upload is compared without reading any image:
  about 60 bytes per image (an album of 450 images: about 27 KB).
- The upload is hashed while it is still in its temporary file, before it is stored. A name that is uploaded again replaces the
  old file and its entry; a delete removes the entry; entries of files that have gone are ignored.
- The pixel comparison and the file comparison keep separate lines, so switching between them is harmless: only the images
  without a line of the chosen kind need indexing.
- The index is **per album**: the same image in two albums is not reported. A file that the frame did not store itself (copied to
  the SD card by hand, a Telegram photo, the "combined" images of the portrait pairing) has no line until it is indexed.
- Telegram photos are not compared by this option: the bot keeps the original JPEG only for a moment, and what ends up in an album
  is a converted image, which could never match the original's MD5. The Telegram feature has its own repeat check.

## Interface

For scripts and other clients:

| Request | Answer |
| --- | --- |
| `POST /api/upload?album=NAME` (also `&duplicates=allow`) | `409` `{"status":"duplicate","existing":"file.png"}` for a refused duplicate; `200` with `"duplicate_of"` when a duplicate was stored anyway |
| `GET /api/dedup/duplicates?album=NAME` | `{"album","hash","images","indexed","groups":[["a.png","b.png"],...]}` |
| `POST /api/dedup/scan` `{"album":"NAME"}` (`""` = all albums) | `{"status":"started"}`, or `409` if one is running |
| `GET /api/dedup/status` | `{"running","finished","album","total","done","indexed","failed"}` |

The configuration keys are `dedup_mode` (`skip`, `warn`, `off`), `dedup_hash` (`stored`, `payload`) and
`dedup_index_existing` (bool).

## Notes

- MD5 is used to spot identical files, not to resist tampering; the frame uses the chip's ROM implementation.
- Indexing reads every image once, so a large album takes minutes; the frame stays awake meanwhile.
