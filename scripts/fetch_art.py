#!/usr/bin/env python3
"""Fetch curated public-domain artworks and render them for a frame.

A PC-side helper (not firmware, so it has no build feature): it asks the public
paperlesspaper art API (https://art.paperlesspaper.de, see docs/ART_FETCH.md) for
curated artworks, downloads a panel-suited JPEG of each, runs the repository's own
`process-cli` (cover/fit, tone mapping, dithering with the measured palette for the
chosen board) and either writes an album folder for the SD card or uploads the
result to a frame over its HTTP API.

An ATTRIBUTION.md listing title, artist, licence and source of every picture is
written next to the output - Creative Commons works require it, and it is cheap
for the public-domain ones.

Examples:
    python scripts/fetch_art.py --board waveshare_photopainter_73 --count 12
    python scripts/fetch_art.py --board seeedstudio_xiao_ee03 --source met,artic --query landscape
    python scripts/fetch_art.py --board waveshare_photopainter_73 --upload --host photoframe.local
    python scripts/fetch_art.py --board waveshare_photopainter_73 --dry-run
"""

import argparse
import json
import random
import re
import shutil
import subprocess
import sys
import tempfile
import time
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BOARDS_JSON = ROOT / "boards" / "boards.json"
PROCESS_CLI = ROOT / "process-cli" / "cli.js"

DEFAULT_API_BASE = "https://art.paperlesspaper.de"
USER_AGENT = (
    "esp32-photoframe-fetch-art/1.0 "
    "(+https://github.com/t3stier/esp32-photoframe-rebuild)"
)
# Sources whose images are raster artwork; svgrepo is an icon collection (SVG).
RASTER_SOURCES = ("met", "artic", "wikimedia")
MAX_COUNT = 200
PAGE_SIZE = 100
DOWNLOAD_PAUSE_S = 0.3
DEFAULT_MAX_MB = 25


class ArtError(Exception):
    """A problem worth a clear message instead of a traceback."""


# -- API ---------------------------------------------------------------------


def build_query_url(
    api_base, sources, query, min_rating, public_domain_only, limit, offset
):
    """The search URL for one page of curated artworks."""
    params = [("selected", "true"), ("limit", str(limit)), ("offset", str(offset))]
    if min_rating:
        params.append(("rating", str(min_rating)))
    if public_domain_only:
        params.append(("publicDomain", "true"))
    if query:
        params.append(("q", query))
    if len(sources) == 1:
        params.append(("source", sources[0]))
    return f"{api_base.rstrip('/')}/api/artworks?{urllib.parse.urlencode(params)}"


def pick_image_url(item, wanted_size):
    """A raster image URL for `item`, or None.

    Prefers the API's resized JPEGs (`image.resizedUrls`, keyed by width): the
    smallest one that is at least `wanted_size` wide, else the largest available,
    else the item's own `image.url`. SVG (and anything without an image) is skipped.
    """
    image = item.get("image") or {}
    resized = image.get("resizedUrls") or {}
    candidates = []
    for key, url in resized.items():
        try:
            candidates.append((int(key), url))
        except (TypeError, ValueError):
            continue
    candidates.sort()
    chosen = None
    for width, url in candidates:
        if width >= wanted_size:
            chosen = url
            break
    if chosen is None and candidates:
        chosen = candidates[-1][1]
    if chosen is None:
        chosen = image.get("url")
    if not chosen or urllib.parse.urlparse(chosen).path.lower().endswith(".svg"):
        return None
    return chosen


def select_items(items, count, seed, wanted_size, public_domain_only):
    """Up to `count` usable, distinct items in a seed-determined order."""
    seen = set()
    usable = []
    for item in sorted(items, key=lambda i: str(i.get("id", ""))):
        item_id = item.get("id")
        if not item_id or item_id in seen:
            continue
        if public_domain_only and not item.get("isPublicDomain"):
            continue
        url = pick_image_url(item, wanted_size)
        if not url:
            continue
        seen.add(item_id)
        usable.append((item, url))
    random.Random(seed).shuffle(usable)
    return usable[:count]


def fetch_json(url, opener=urllib.request.urlopen, timeout=30):
    request = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
    try:
        with opener(request, timeout=timeout) as response:
            return json.loads(response.read().decode("utf-8"))
    except (urllib.error.URLError, ValueError, TimeoutError) as error:
        raise ArtError(f"Could not read {url}: {error}") from error


def collect_candidates(args, wanted_size, opener=urllib.request.urlopen):
    """Pages through the API until enough usable items are known (or it runs out)."""
    sources = args.source
    items = []
    offset = 0
    # A few extra pages: some items are filtered out again (svg, duplicates).
    while offset < PAGE_SIZE * 5:
        url = build_query_url(
            args.api_base,
            sources,
            args.query,
            args.min_rating,
            not args.allow_cc,
            PAGE_SIZE,
            offset,
        )
        payload = fetch_json(url, opener)
        page = payload.get("items") or []
        items.extend(i for i in page if i.get("source") in sources)
        if len(page) < PAGE_SIZE:
            break
        chosen = select_items(
            items, args.count, args.seed, wanted_size, not args.allow_cc
        )
        if len(chosen) >= args.count:
            return chosen
        offset += PAGE_SIZE
    return select_items(items, args.count, args.seed, wanted_size, not args.allow_cc)


# -- boards and process-cli --------------------------------------------------


def load_boards(path=BOARDS_JSON):
    with open(path, encoding="utf-8") as f:
        return {b["id"]: b for b in json.load(f)}


def board_target(board_id, boards):
    """(width, height, grayscale) of a board as process-cli should render it."""
    board = boards.get(board_id)
    if board is None:
        raise ArtError(f"Unknown board '{board_id}' (see boards/boards.json)")
    width, height = board["resolution"]
    return width, height, str(board.get("display_type", "")).startswith("gc")


def build_cli_command(
    node, cli_path, input_dir, output_dir, board, grayscale, scale_mode, host, upload
):
    command = [
        node,
        str(cli_path),
        str(input_dir),
        "-o",
        str(output_dir),
        "--board",
        board,
        "--scale-mode",
        scale_mode,
        "--auto-orient",
    ]
    if grayscale:
        command.append("--grayscale")
    if upload:
        command += ["--upload", "--host", host]
    return command


# -- files -------------------------------------------------------------------


def safe_name(item, url):
    """`<source>-<id>.<ext>`, restricted to characters that are safe everywhere."""
    ext = Path(urllib.parse.urlparse(url).path).suffix.lower() or ".jpg"
    base = f"{item.get('source', 'art')}-{item.get('sourceId') or item['id']}"
    return re.sub(r"[^A-Za-z0-9._-]", "_", base) + ext


def download(url, destination, max_bytes, opener=urllib.request.urlopen, timeout=60):
    """Streams `url` to `destination`; refuses anything larger than `max_bytes`."""
    request = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
    total = 0
    try:
        with opener(request, timeout=timeout) as response, open(
            destination, "wb"
        ) as out:
            while True:
                chunk = response.read(64 * 1024)
                if not chunk:
                    break
                total += len(chunk)
                if total > max_bytes:
                    raise ArtError(
                        f"{url} is larger than {max_bytes // 1024 // 1024} MB"
                    )
                out.write(chunk)
    except (urllib.error.URLError, TimeoutError) as error:
        raise ArtError(f"Could not download {url}: {error}") from error
    return total


def attribution_markdown(entries):
    """ATTRIBUTION.md text for [(filename, item)] - one row per picture."""
    lines = [
        "# Attribution",
        "",
        "Artworks fetched with `scripts/fetch_art.py` from the paperlesspaper art API.",
        "",
        "| File | Title | Artist | Licence | Source |",
        "| --- | --- | --- | --- | --- |",
    ]

    def cell(value):
        return str(value or "-").replace("|", "/").replace("\n", " ").strip() or "-"

    for filename, item in entries:
        lines.append(
            "| {} | {} | {} | {} | {} |".format(
                cell(filename),
                cell(item.get("title")),
                cell(item.get("artist")),
                cell(item.get("license")),
                cell(item.get("sourceUrl")),
            )
        )
    lines.append("")
    return "\n".join(lines)


# -- command line ------------------------------------------------------------


def parse_args(argv=None):
    parser = argparse.ArgumentParser(
        description="Fetch curated public-domain artworks and render them for a frame.",
        epilog="Uses a public API - keep --count modest and be considerate.",
    )
    parser.add_argument(
        "--board", required=True, help="board id from boards/boards.json"
    )
    parser.add_argument("--count", type=int, default=12, help="pictures (default 12)")
    parser.add_argument(
        "--source",
        default=",".join(RASTER_SOURCES),
        type=lambda s: [x for x in s.split(",") if x],
        help="comma list of met, artic, wikimedia (default: all three)",
    )
    parser.add_argument(
        "--query", default="", help="free-text search (title, artist, tag)"
    )
    parser.add_argument(
        "--min-rating",
        type=int,
        default=5,
        help="curation rating 1-5 (default 5, 0 = any)",
    )
    parser.add_argument(
        "--allow-cc",
        action="store_true",
        help="also take Creative Commons works (default: public domain only)",
    )
    parser.add_argument("--seed", default="0", help="same seed = same selection")
    parser.add_argument(
        "--size",
        type=int,
        choices=(512, 1024, 3000),
        help="download width (default: the smallest that covers the board)",
    )
    parser.add_argument(
        "--scale-mode", choices=("fit", "cover"), default="fit", help="default fit"
    )
    parser.add_argument("--album", default="Art", help="album name (default Art)")
    parser.add_argument(
        "--out", default="art-out", help="output folder (default art-out)"
    )
    parser.add_argument(
        "--upload", action="store_true", help="upload to a frame (needs --host)"
    )
    parser.add_argument("--host", default="photoframe.local")
    parser.add_argument("--max-mb", type=int, default=DEFAULT_MAX_MB)
    parser.add_argument("--api-base", default=DEFAULT_API_BASE)
    parser.add_argument("--node", default="node")
    parser.add_argument("--dry-run", action="store_true", help="list, download nothing")
    args = parser.parse_args(argv)
    if not 1 <= args.count <= MAX_COUNT:
        parser.error(f"--count must be 1-{MAX_COUNT}")
    unknown = [s for s in args.source if s not in RASTER_SOURCES]
    if unknown or not args.source:
        parser.error(f"--source must be a list of {', '.join(RASTER_SOURCES)}")
    if not 0 <= args.min_rating <= 5:
        parser.error("--min-rating must be 0-5")
    return args


def wanted_width(args, width, height):
    return args.size or max(width, height)


def run(args, opener=urllib.request.urlopen, runner=subprocess.run):
    width, height, grayscale = board_target(args.board, load_boards())
    wanted = wanted_width(args, width, height)
    chosen = collect_candidates(args, wanted, opener)
    if not chosen:
        raise ArtError("The API returned nothing usable for these filters")
    print(f"{len(chosen)} artwork(s) for {args.board} ({width}x{height})")
    if args.dry_run:
        for item, url in chosen:
            print(f"  {item['id']}: {item.get('title') or '-'} - {url}")
        return 0
    if not PROCESS_CLI.is_file() or not (PROCESS_CLI.parent / "node_modules").is_dir():
        raise ArtError(
            "process-cli is not installed: run `npm ci` in the process-cli folder first"
        )
    if shutil.which(args.node) is None:
        raise ArtError(f"'{args.node}' not found - process-cli needs Node.js")

    with tempfile.TemporaryDirectory(prefix="fetch-art-") as tmp:
        album_in = Path(tmp) / args.album
        album_in.mkdir()
        entries = []
        for number, (item, url) in enumerate(chosen, 1):
            name = safe_name(item, url)
            print(f"  [{number}/{len(chosen)}] {name}")
            download(url, album_in / name, args.max_mb * 1024 * 1024, opener)
            entries.append((name, item))
            time.sleep(DOWNLOAD_PAUSE_S)

        out_dir = Path(args.out)
        out_dir.mkdir(parents=True, exist_ok=True)
        command = build_cli_command(
            args.node,
            PROCESS_CLI,
            tmp,
            out_dir,
            args.board,
            grayscale,
            args.scale_mode,
            args.host,
            args.upload,
        )
        print("Running process-cli ...")
        result = runner(command, cwd=PROCESS_CLI.parent)
        if result.returncode != 0:
            raise ArtError(f"process-cli failed (exit {result.returncode})")

    attribution = (out_dir / args.album if not args.upload else out_dir) / (
        "ATTRIBUTION.md" if not args.upload else f"ATTRIBUTION-{args.album}.md"
    )
    attribution.parent.mkdir(parents=True, exist_ok=True)
    attribution.write_text(attribution_markdown(entries), encoding="utf-8")
    print(f"Attribution written to {attribution}")
    return 0


def main(argv=None):
    # Titles are arbitrary Unicode; a legacy console codepage must not crash the run.
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(errors="replace")
    try:
        return run(parse_args(argv))
    except ArtError as error:
        print(f"error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
