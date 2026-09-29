#!/usr/bin/env python3
"""Generate the placeholder photos of examples/waveshare_photopainter_73/photos/.

One-off/manual tool (like generate_weather_icons.py, not run by build.py). The
demo package (docs/DEMO_PACKAGE.md) needs a handful of pictures for the storage-
rotation profile that carry no licence or privacy question at all, so they are
drawn procedurally instead of sourced from anywhere - five flat gradients at
the Waveshare PhotoPainter's native 800x480, landscape, no text or people, one
named for each of the palette's accent colors so the difference is obvious on
the panel between rotations.
"""

import os

from PIL import Image, ImageDraw

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.dirname(SCRIPT_DIR)
OUTPUT_DIR = os.path.join(REPO_ROOT, "examples", "waveshare_photopainter_73", "photos")

WIDTH = 800
HEIGHT = 480

# (file stem, top color, bottom color, accent) - accent draws one simple
# shape so the five images stay visually distinct at a glance, not just by
# color. Colors are plain sRGB, chosen for a pleasant vertical gradient
# rather than to match the firmware's dithered output palette (the frame
# dithers whatever it is given, the same as any uploaded photo).
IMAGES = [
    ("demo-photo-sunrise", (255, 200, 130), (255, 120, 90), "sun"),
    ("demo-photo-ocean", (140, 210, 255), (20, 70, 140), "wave"),
    ("demo-photo-forest", (170, 220, 140), (30, 90, 40), "hill"),
    ("demo-photo-dusk", (120, 90, 160), (30, 20, 60), "moon"),
    ("demo-photo-meadow", (255, 240, 190), (110, 170, 90), "sun"),
]


def lerp(a, b, t):
    return tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(3))


def draw_gradient(draw, top, bottom):
    for y in range(HEIGHT):
        draw.line([(0, y), (WIDTH, y)], fill=lerp(top, bottom, y / (HEIGHT - 1)))


def draw_accent(draw, kind, top, bottom):
    cx, cy = WIDTH // 2, HEIGHT // 2
    if kind == "sun":
        r = 90
        draw.ellipse([cx - r, cy - r - 60, cx + r, cy + r - 60], fill=(255, 250, 230))
    elif kind == "moon":
        r = 70
        draw.ellipse(
            [cx + 150, cy - 150, cx + 150 + r, cy - 150 + r], fill=(235, 235, 245)
        )
    elif kind == "wave":
        for i, y in enumerate(range(HEIGHT - 140, HEIGHT, 28)):
            shade = lerp(top, bottom, 0.9)
            draw.line([(0, y), (WIDTH, y - 10)], fill=shade, width=6)
    elif kind == "hill":
        draw.polygon(
            [
                (0, HEIGHT),
                (0, HEIGHT - 160),
                (260, HEIGHT - 260),
                (520, HEIGHT - 150),
                (800, HEIGHT - 220),
                (800, HEIGHT),
            ],
            fill=lerp(top, bottom, 0.7),
        )


def main():
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    for stem, top, bottom, accent in IMAGES:
        image = Image.new("RGB", (WIDTH, HEIGHT))
        draw = ImageDraw.Draw(image)
        draw_gradient(draw, top, bottom)
        draw_accent(draw, accent, top, bottom)
        out_path = os.path.join(OUTPUT_DIR, f"{stem}.png")
        image.save(out_path, "PNG")
        print(f"wrote {out_path}")


if __name__ == "__main__":
    main()
