#!/usr/bin/env python3
"""Lift the ROM's graphics into PNG files (this directory), for newtc to
embed (cmake/EmbedImages.cmake, Host/FLTK/RomImages.h).

    Host/FLTK/Images/extract_rom_images.py [--force] path/to/newtc

Existing PNGs are kept (they may have been replaced by better ones, e.g.
twice the resolution: newtc draws every image at the ROM object's size);
--force writes them all again.

newtc gives the bytes: every magic pointer that is a picture (a QuickDraw
PICT, class 'picture) or a bitmap frame (a frame with a 'bits binary, as
an icon) is dumped as hex. Each becomes rom_NNNN[_name].png: NNNN is the
magic pointer index (@13 -> rom_0013), the name comes from
Frames/MagicPointers.h if it has one.

  PICTs: version 1, drawn by their bitmap opcodes (BitsRect, PackBitsRect);
      all of the ROM's are just bitmaps. 8-bit gray, opaque (as QuickDraw
      copies them: white is white).
  Bitmaps: a Newton 1-bit bitmap (16-byte header: rowBytes at 4, bounds at
      8). Gray and alpha: black where a bit is set, transparent elsewhere
      (as newtc draws icons; the 'mask slot isn't used).

Python's standard library only.
"""

import pathlib
import re
import struct
import subprocess
import sys
import tempfile
import zlib

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parent.parent.parent
MAX_MAGIC = 1200    # the ROM's table ends before this

NS_SCRIPT = """
func Hex(b) begin
  local digits := "0123456789ABCDEF";
  local s := "";
  for i := 0 to Length(b) - 1 do begin
    local v := ExtractByte(b, i);
    s := s & digits[v div 16] & digits[v mod 16];
  end;
  s
end;
DefGlobalVar('mps, [%s]);
for i := 0 to Length(mps) - 1 do begin
  local o := mps[i];
  try
    if IsBinary(o) and ClassOf(o) = 'picture then Print("pict " & i & " " & Hex(o))
    else if IsFrame(o) and HasSlot(o, 'bits) and IsBinary(o.bits) then Print("bits " & i & " " & Hex(o.bits))
  onexception |evt.ex| do nil;
end;
"""


def magic_names():
    names = {}
    for line in (ROOT / "Frames" / "MagicPointers.h").read_text(errors="replace").splitlines():
        m = re.match(r"#define\s+(\w+)\s+MAKEMAGICPTR\((\d+)\)", line)
        if m:
            names[int(m.group(2))] = m.group(1)
    return names


def unpack_bits(data, p, count):
    """PackBits: count bytes of data from p; returns the bytes."""
    out = bytearray()
    end = p + count
    while p < end:
        n = data[p]
        p += 1
        if n < 128:
            out += data[p:p + n + 1]
            p += n + 1
        elif n > 128:
            out += bytes([data[p]]) * (257 - n)
            p += 1
    return bytes(out)


def decode_pict(b):
    """A version 1 PICT of bitmaps: (width, height, gray pixels)."""
    top, left, bottom, right = struct.unpack(">hhhh", b[2:10])
    width, height = right - left, bottom - top
    pixels = bytearray([255]) * (width * height)
    if b[10:12] != b"\x11\x01":
        raise ValueError("not a version 1 PICT")
    p = 12
    word = lambda at: struct.unpack(">H", b[at:at + 2])[0]
    rect = lambda at: struct.unpack(">hhhh", b[at:at + 8])
    while p < len(b):
        op = b[p]
        p += 1
        if op == 0xFF:          # end
            break
        elif op == 0x00 or op == 0x1E:   # nop, def hilite
            pass
        elif op == 0x01:        # clip region: its size counts itself
            p += word(p)
        elif op == 0xA0:        # short comment
            p += 2
        elif op == 0xA1:        # long comment
            p += 4 + word(p + 2)
        elif op in (0x90, 0x98):   # BitsRect, PackBitsRect
            row_bytes = word(p) & 0x7FFF
            b_top, b_left, b_bottom, _ = rect(p + 2)
            s_top, s_left, s_bottom, s_right = rect(p + 10)
            d_top, d_left, _, _ = rect(p + 18)
            p += 28             # rowBytes, bounds, srcRect, dstRect, mode
            for row in range(b_bottom - b_top):
                if op == 0x98 and row_bytes >= 8:
                    if row_bytes > 250:
                        count = word(p)
                        p += 2
                    else:
                        count = b[p]
                        p += 1
                    line = unpack_bits(b, p, count)
                    p += count
                else:
                    line = b[p:p + row_bytes]
                    p += row_bytes
                y = b_top + row
                if not s_top <= y < s_bottom:
                    continue
                for x in range(s_left, s_right):
                    bit = line[(x - b_left) >> 3] & (0x80 >> ((x - b_left) & 7))
                    px, py = d_left + x - s_left - left, d_top + y - s_top - top
                    if 0 <= px < width and 0 <= py < height:
                        pixels[py * width + px] = 0 if bit else 255
        else:
            raise ValueError("PICT opcode 0x%02X" % op)
    return width, height, pixels


def decode_bits(b):
    """A Newton 1-bit bitmap: (width, height, gray and alpha pixels)."""
    row_bytes = struct.unpack(">H", b[4:6])[0]
    top, left, bottom, right = struct.unpack(">hhhh", b[8:16])
    width, height = right - left, bottom - top
    if len(b) < 16 + row_bytes * height:
        raise ValueError("short bitmap (not 1 bit deep?)")
    pixels = bytearray()
    for y in range(height):
        line = b[16 + y * row_bytes:16 + (y + 1) * row_bytes]
        for x in range(width):
            set_ = line[x >> 3] & (0x80 >> (x & 7))
            pixels += b"\x00\xff" if set_ else b"\xff\x00"
    return width, height, pixels


def write_png(path, width, height, pixels, channels):
    """8-bit gray (1 channel) or gray and alpha (2)."""
    stride = width * channels
    raw = b"".join(b"\x00" + bytes(pixels[y * stride:(y + 1) * stride]) for y in range(height))
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))
    color_type = 0 if channels == 1 else 4
    path.write_bytes(b"\x89PNG\r\n\x1a\n"
                     + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, color_type, 0, 0, 0))
                     + chunk(b"IDAT", zlib.compress(raw, 9))
                     + chunk(b"IEND", b""))


def main():
    args = sys.argv[1:]
    force = "--force" in args
    args = [a for a in args if a != "--force"]
    if len(args) != 1:
        sys.exit(__doc__)
    newtc = args[0]
    names = magic_names()
    with tempfile.TemporaryDirectory() as tmp:
        script = pathlib.Path(tmp) / "dump.ns"
        script.write_text(NS_SCRIPT % ",".join("@%d" % i for i in range(MAX_MAGIC)))
        out = subprocess.run([newtc, "-stubs", "quiet", "-script", str(script)],
                             capture_output=True, text=True, check=True).stdout
    written = kept = 0
    for line in out.splitlines():
        m = re.match(r'"(pict|bits) (\d+) ([0-9A-F]+)"', line.strip())
        if not m:
            continue
        kind, index, data = m.group(1), int(m.group(2)), bytes.fromhex(m.group(3))
        name = "rom_%04d" % index + ("_" + names[index] if index in names else "") + ".png"
        existing = list(HERE.glob("rom_%04d*.png" % index))
        if existing and not force:
            kept += 1
            continue
        for old in existing:
            old.unlink()
        try:
            if kind == "pict":
                width, height, pixels = decode_pict(data)
                write_png(HERE / name, width, height, pixels, 1)
            else:
                width, height, pixels = decode_bits(data)
                write_png(HERE / name, width, height, pixels, 2)
            written += 1
        except ValueError as error:
            print("skipped @%d (%s): %s" % (index, kind, error))
    print("%d images written, %d kept, in %s" % (written, kept, HERE))


if __name__ == "__main__":
    main()
