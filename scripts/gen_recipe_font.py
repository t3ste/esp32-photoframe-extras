#!/usr/bin/env python3
"""Generates main/recipe_font_data.c: the small proportional bitmap fonts of the recipe page.

The firmware's own font is one monospace 17x24 bitmap; a recipe needs 12-16 pixel text with
umlauts and the odd fraction. This renders Noto Sans (SIL Open Font License 1.1, see
docs/third_party/NotoSans-OFL.txt) once, with the font's own hinting at each size in
1-bit mode, and writes the bitmaps as C tables. The firmware needs no font engine and no
font file; the generated file is committed, so a build never runs this.

The character set is Windows-1252 (code = byte): ASCII, the Latin-1 letters (a-umlaut,
sharp s, accents), the typographic quotes, dashes, the ellipsis, the euro sign and the
vulgar fractions the recipes use.

    python scripts/gen_recipe_font.py <NotoSans-Regular.ttf> <NotoSans-Bold.ttf>

Requires Pillow (with FreeType).
"""

import hashlib
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "main" / "recipe_font_data.c"

# (style, pixel size): what the layout asks for. Body text goes 22..12, the title 30/24/19,
# the headings and the small lines have their own.
FONTS = [
    ("regular", 12),
    ("regular", 13),
    ("regular", 14),
    ("regular", 15),
    ("regular", 16),
    ("regular", 17),
    ("regular", 18),
    ("regular", 19),
    ("regular", 20),
    ("regular", 22),
    ("regular", 24),
    ("regular", 30),
    ("bold", 12),
    ("bold", 20),
]

FIRST, LAST = 0x20, 0xFF  # the codes of a table: Windows-1252 bytes


def code_to_char(code):
    try:
        return bytes([code]).decode("cp1252")
    except UnicodeDecodeError:
        return None  # 0x81, 0x8D, 0x8F, 0x90, 0x9D are not assigned


def render_glyph(font, char, ascent, pad=8):
    """Returns (advance, left, top, width, height, rows) for one character; rows are lists of 0/1."""
    advance = int(round(font.getlength(char)))
    size = ascent * 2 + pad * 2
    image = Image.new("L", (size * 2, size), 255)
    draw = ImageDraw.Draw(image)
    draw.fontmode = "1"
    baseline = pad + ascent
    draw.text((pad, baseline), char, font=font, fill=0, anchor="ls")
    box = image.point(lambda v: 255 if v < 128 else 0).getbbox()
    if box is None:
        return advance, 0, 0, 0, 0, []
    x0, y0, x1, y1 = box
    rows = []
    for y in range(y0, y1):
        rows.append([1 if image.getpixel((x, y)) < 128 else 0 for x in range(x0, x1)])
    return advance, x0 - pad, baseline - y0, x1 - x0, y1 - y0, rows


def pack_rows(rows, width):
    stride = (width + 7) // 8
    out = []
    for row in rows:
        for byte_index in range(stride):
            value = 0
            for bit in range(8):
                x = byte_index * 8 + bit
                value = (value << 1) | (row[x] if x < width else 0)
            out.append(value)
    return out


def build(paths):
    lines = []
    tables = []
    for style, size in FONTS:
        path = paths[style]
        font = ImageFont.truetype(str(path), size, layout_engine=ImageFont.Layout.BASIC)
        ascent, descent = font.getmetrics()
        bits = []
        glyphs = []
        for code in range(FIRST, LAST + 1):
            char = code_to_char(code)
            if char is None:
                glyphs.append((0, 0, 0, 0, 0, 0))
                continue
            advance, left, top, width, height, rows = render_glyph(font, char, ascent)
            offset = len(bits)
            bits.extend(pack_rows(rows, width))
            assert offset < 65536 and advance < 256 and width < 256 and height < 256
            glyphs.append((advance, left, top, width, height, offset))
        assert len(bits) < 65536, (style, size, len(bits))
        name = f"{style}_{size}"
        lines.append(f"static const uint8_t bits_{name}[] = {{")
        for i in range(0, len(bits), 16):
            lines.append(
                "    " + ", ".join(f"0x{b:02x}" for b in bits[i : i + 16]) + ","
            )
        if not bits:
            lines.append("    0,")
        lines.append("};")
        lines.append(f"static const recipe_glyph_t glyphs_{name}[] = {{")
        for code, g in zip(range(FIRST, LAST + 1), glyphs):
            lines.append(
                f"    {{{g[0]}, {g[1]}, {g[2]}, {g[3]}, {g[4]}, {g[5]}}},  // 0x{code:02X}"
            )
        lines.append("};")
        tables.append((style, size, ascent, descent, name, len(bits)))
    lines.append("")
    lines.append("const recipe_font_t RECIPE_FONTS[] = {")
    for style, size, ascent, descent, name, _ in tables:
        bold = "true" if style == "bold" else "false"
        lines.append(
            f"    {{{size}, {bold}, {ascent}, {descent}, glyphs_{name}, bits_{name}}},"
        )
    lines.append("};")
    lines.append(f"const int RECIPE_FONT_COUNT = {len(tables)};")
    total = sum(t[5] for t in tables)
    return lines, tables, total


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    paths = {"regular": Path(sys.argv[1]), "bold": Path(sys.argv[2])}
    digests = {k: hashlib.sha256(p.read_bytes()).hexdigest() for k, p in paths.items()}
    body, tables, total = build(paths)
    header = [
        "// GENERATED by scripts/gen_recipe_font.py - do not edit.",
        "//",
        "// Small proportional bitmap fonts for the recipe page (build option `recipes`): Noto Sans,",
        "// Copyright 2022 The Noto Project Authors, SIL Open Font License 1.1 (see",
        "// docs/third_party/NotoSans-OFL.txt), rendered at each size with the font's own hinting in",
        "// 1-bit mode. Code = Windows-1252 byte, 0x20..0xFF. The bitmaps are a derivative of the font;",
        "// the license permits that, and they are not sold on their own.",
        f"// Source files (sha256): Regular {digests['regular'][:16]}..., Bold {digests['bold'][:16]}...",
        "",
        '#include "recipe_font.h"',
        "",
        "// clang-format off",
        "",
    ]
    body.append("")
    body.append("// clang-format on")
    OUTPUT.write_text("\n".join(header + body) + "\n", encoding="utf-8", newline="\n")
    print(
        f"wrote {OUTPUT.relative_to(ROOT)}: {len(tables)} fonts, {total} bytes of bitmaps"
    )
    for style, size, ascent, descent, _, size_bytes in tables:
        print(
            f"  {style:8} {size:2} px: ascent {ascent}, descent {descent}, {size_bytes} B"
        )
    return 0


if __name__ == "__main__":
    sys.exit(main())
