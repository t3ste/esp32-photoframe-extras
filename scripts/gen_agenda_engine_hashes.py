#!/usr/bin/env python3
"""Regenerate webapp/src/utils/agendaEngine.hashes.json: what the firmware's Agenda renderer draws.

agendaEngine.test.js draws the scenes of agendaEngine.scenes.js with the JavaScript port of the renderer
(webapp/src/utils/agendaEngine.js) and compares the SHA-256 of every picture with the one the real
agenda_renderer.c drew for the same Agenda. The real renderer runs in the host render harness (the program
behind the pictures of docs/SCREENSHOTS.md: agenda_renderer.c on a PC with made-up sample data), which writes PNG
files; this script reads them and writes the hashes of their RGB pixels.

    render_agenda <dir>/default all 800x480           (also 480x800, 960x540, 1600x1200 and 1200x1600)
    render_agenda <dir>/bw all 800x480 examples/waveshare_photopainter_73/color_profiles/color-profile-slot1-BW.json
    ... y: color-profile-slot2_Y.json, b: color-profile-slot3_B.json
    python scripts/gen_agenda_engine_hashes.py <dir>

Needs Pillow (pip install pillow). Run it again when agenda_renderer.c, agenda_order.c or the text drawing of
image_processor.c changes the picture - then agendaEngine.js has to follow, and the test says where it does not.
"""

import hashlib
import json
import os
import re
import sys

from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "webapp", "src", "utils", "agendaEngine.hashes.json")
CASES = [
    "agenda-grid-a",
    "agenda-grid-b",
    "agenda-grid-a-marked",
    "agenda-grid-a-prio-d",
    "agenda-grid-a-noweek",
    "agenda-grid-a-weather-right",
    "agenda-grid-a-climate",
]
PROFILES = ["default", "bw", "y", "b"]
SIZES = ["800x480", "480x800", "960x540", "1600x1200", "1200x1600"]


def digest(path):
    image = Image.open(path).convert("RGB")
    return hashlib.sha256(image.tobytes()).hexdigest()


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    base = sys.argv[1]
    hashes = {}
    for profile in PROFILES:
        folder = os.path.join(base, profile)
        for size in SIZES:
            if profile != "default" and size != "800x480":
                continue  # the other sizes are drawn with the built-in profile only
            for case in CASES:
                suffix = "" if profile == "default" else "-profile"
                path = os.path.join(folder, "%s%s_%s.png" % (case, suffix, size))
                if not os.path.exists(path):
                    sys.exit("missing %s" % path)
                hashes["%s|%s|%s" % (case, profile, size)] = digest(path)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        json.dump(hashes, f, indent=2, sort_keys=True)
        f.write("\n")
    print("%d hashes written to %s" % (len(hashes), OUT))
    assert re.match(r"^[0-9a-f]{64}$", next(iter(hashes.values())))


if __name__ == "__main__":
    main()
