#!/usr/bin/env python3
"""
The macOS icon of a Newton app (the newtc_app target): the package's own
icon, a Newton bitmap, big and crisp (its pixels as squares) on a rounded
square of a Newton screen's green.

    make_app_icon.py <icon.txt> <AppIcon.icns>

icon.txt is what newtc prints for the package's icon frame (Print of
{bits: MakeBinaryFromHex("..."), bounds: {...}}). Without a bitmap in it,
nothing is written (the app gets the generic icon). Uses iconutil (macOS).
"""

import re
import struct
import subprocess
import sys
import tempfile
import zlib
from pathlib import Path

PAPER = (190, 201, 170)     # a Newton screen
INK = (28, 34, 24)
EDGE = (84, 94, 72)


def bitmap(text):
    """The icon's pixels: rows of booleans, or None."""
    m = re.search(r'bits:\s*MakeBinaryFromHex\("([0-9A-Fa-f]+)"', text)
    if not m:
        return None
    data = bytes.fromhex(m.group(1))
    if len(data) < 16:
        return None
    row_bytes = struct.unpack(">H", data[4:6])[0]
    top, left, bottom, right = struct.unpack(">hhhh", data[8:16])
    w, h = right - left, bottom - top
    if row_bytes <= 0 or w <= 0 or h <= 0 or len(data) < 16 + row_bytes * h:
        return None
    rows = []
    for y in range(h):
        row = data[16 + y * row_bytes:16 + (y + 1) * row_bytes]
        rows.append([bool(row[x >> 3] & (0x80 >> (x & 7))) for x in range(w)])
    # only the part that has pixels
    ys = [y for y in range(h) if any(rows[y])]
    xs = [x for x in range(w) if any(rows[y][x] for y in range(h))]
    if not ys:
        return None
    return [r[xs[0]:xs[-1] + 1] for r in rows[ys[0]:ys[-1] + 1]]


def rgba_png(size, pixels):
    """A PNG of size x size RGBA pixels."""
    raw = b"".join(b"\0" + bytes(c for p in row for c in p) for row in pixels)
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xffffffff)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", size, size, 8, 6, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


def icon(size, bits):
    """The icon at size: a rounded square (as macOS draws app icons: 80% of
    the canvas), the bitmap in its middle as big as fits in whole pixels."""
    margin = round(size * 0.1)
    side = size - 2 * margin
    radius = side * 0.225
    edge = max(1, round(size / 128))
    bh, bw = len(bits), len(bits[0])
    scale = max(1, int(side * 0.78) // max(bw, bh))
    ox = (size - bw * scale) // 2
    oy = (size - bh * scale) // 2
    rows = []
    for y in range(size):
        row = []
        for x in range(size):
            # inside the rounded square? (distance to the corner circles)
            cx = min(max(x + 0.5, margin + radius), margin + side - radius)
            cy = min(max(y + 0.5, margin + radius), margin + side - radius)
            d = ((x + 0.5 - cx) ** 2 + (y + 0.5 - cy) ** 2) ** 0.5 - radius
            if d > 0.5:
                row.append((0, 0, 0, 0))
                continue
            alpha = 255 if d <= -0.5 else round(255 * (0.5 - d))
            color = EDGE if d > -edge else PAPER
            bx, by = (x - ox) // scale, (y - oy) // scale
            if 0 <= bx < bw and 0 <= by < bh and 0 <= x - ox and 0 <= y - oy and bits[by][bx]:
                color = INK
            row.append(color + (alpha,))
        rows.append(row)
    return rgba_png(size, rows)


def main():
    text = Path(sys.argv[1]).read_text(errors="replace")
    out = Path(sys.argv[2])
    bits = bitmap(text)
    if bits is None:
        print("make_app_icon.py: the package has no icon bitmap; the app gets the generic icon")
        return 0
    with tempfile.TemporaryDirectory() as tmp:
        iconset = Path(tmp) / "AppIcon.iconset"
        iconset.mkdir()
        for base in (16, 32, 128, 256, 512):
            (iconset / f"icon_{base}x{base}.png").write_bytes(icon(base, bits))
            (iconset / f"icon_{base}x{base}@2x.png").write_bytes(icon(base * 2, bits))
        out.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run(["iconutil", "-c", "icns", str(iconset), "-o", str(out)], check=True)
    print(f"make_app_icon.py: wrote {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
