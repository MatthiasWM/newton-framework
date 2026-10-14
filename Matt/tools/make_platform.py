#!/usr/bin/env python3
"""
Make newtc's built-in platform (plan 4.3): Matt/Platform/Newton21.ns.

NTK's platform file "Newton 2.1" (Apple's, never in this repo) holds the
constants a NewtonScript program for NewtonOS 2.1 may use: magic pointers
(protoFloatNGo: @180), numbers (vjParentCenterH: 16), strings, characters,
symbols. This script takes only those names and simple values from it, one
`constant name := value;` line each, so newtc knows them without the file.
Constants whose values are whole objects (protos, functions, bitmaps: NTK
copies them into each package that uses one) are left out and listed at the
end of the file. The magic pointers are checked against the ROM's own names
for them (ROMData/forNewton/RSData.s).

Run it once, or again for a newer platform file; after that Newton21.ns is
newtc's source, edited by hand like any other.

Usage: Matt/tools/make_platform.py "<path>/Platforms/Newton 2.1" [--newtc path]
"""

import argparse
import re
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
OUT = REPO / "Matt" / "Platform" / "Newton21.ns"

HEADER = """\
// Newton21.ns: newtc's built-in NewtonOS 2.1 platform (plan 4.3).
//
// The constants a NewtonScript program for NewtonOS 2.0 and 2.1 may use, as
// NTK's platform file "Newton 2.1" defines them: magic pointers (ROM
// objects: protoFloatNGo is @180 in every 2.x ROM), view flags,
// justification, fonts, error codes, key codes, ... newtc runs this file
// at startup (embedded, like DAP.ns), so `_proto: protoFloatNGo` compiles
// to @180 without any Apple file.
//
// Made by Matt/tools/make_platform.py from the names and simple values in
// NTK's "Newton 2.1" (no objects, no code); edited by hand since. The
// constants whose values are objects in NTK's file (protos and functions
// NTK copies into each package that uses them, e.g. the mail protos) are
// listed at the end; use NTK's file for them (plan 4.8).

"""


def parse_dump(text):
    """The -print output of newtc: DefineGlobalConstant('Ref_N, value); blocks."""
    lines = text.split("\n")
    starts = {}
    for i, line in enumerate(lines):
        m = re.match(r"DefineGlobalConstant\('(Ref_\d+), ", line)
        if m:
            starts[m.group(1)] = i
    order = sorted(starts.items(), key=lambda x: x[1])
    blocks = {}
    for k, (name, i) in enumerate(order):
        end = order[k + 1][1] if k + 1 < len(order) else len(lines)
        blocks[name] = lines[i:end]
    return lines, blocks


def simple_value(ref, blocks):
    """A Ref_N defined as one literal (a shared real, a string): its text."""
    block = [l for l in blocks.get(ref, []) if l.strip()]
    if len(block) == 1:
        m = re.match(r"DefineGlobalConstant\('Ref_\d+, (.*)\);$", block[0])
        if m and not m.group(1).startswith(("{", "[", "Ref_", "MakeBinaryFromHex")):
            return m.group(1)
    return None


def rom_names():
    names = {}
    for line in open(REPO / "ROMData" / "forNewton" / "RSData.s", encoding="latin-1"):
        m = re.match(r"^_R(\w+):\s*Ref\s*MAKEMAGICPTR\((\d+)\)", line)
        if m:
            names.setdefault(int(m.group(2)), set()).add(m.group(1).lower())
    return names


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("platform", help="NTK's platform file, e.g. .../Platforms/Newton 2.1")
    ap.add_argument("--newtc", default=str(REPO / "build" / "VSCode" / "newtc"))
    args = ap.parse_args()

    dump = subprocess.run([args.newtc, "-nsof", args.platform, "-print"],
                          capture_output=True, text=True, stdin=subprocess.DEVNULL).stdout
    lines, blocks = parse_dump(dump)
    m = re.search(r"^\s*platformConstants: (Ref_\d+),?$", dump, re.M)
    if not m:
        sys.exit("make_platform: no platformConstants in that file")
    frame = blocks[m.group(1)]

    magic, other, objects = [], [], []
    for line in frame[1:]:
        m = re.match(r"^  ([A-Za-z_|][^:]*): (.*?),?$", line)
        if not m:
            continue                        # inside an inline object
        name, value = m.group(1), m.group(2)
        if value.startswith("Ref_"):
            value = simple_value(value, blocks) or value
        if value.startswith("@"):
            magic.append((int(value[1:]), name))
        elif value in ("inf", "-inf", "nan"):
            expr = {"inf": "1.0/0.0", "-inf": "-1.0/0.0", "nan": "0.0/0.0"}[value]
            other.append((name, "DefineGlobalConstant('%s, %s);   // %s: no literal for it" % (name, expr, value)))
        elif value.startswith(("{", "[", "Ref_", "MakeBinaryFromHex")):
            objects.append(name)
        else:
            other.append((name, "constant %s := %s;" % (name, value)))

    names = rom_names()
    unknown = [(i, n) for i, n in magic if i not in names]
    out = [HEADER, "// Magic pointers (ROM objects), by number\n"]
    out += ["constant %s := @%d;\n" % (n, i) for i, n in sorted(magic)]
    out += ["\n// Numbers, strings, characters, symbols, by name\n"]
    out += [text + "\n" for _, text in sorted(other, key=lambda x: x[0].lower())]
    out += ["\n// Objects in NTK's file, not here (NTK copies them into the packages\n",
            "// that use them; plan 4.8 reads NTK's file):\n"]
    row = "//"
    for name in sorted(objects, key=str.lower):
        if len(row) + len(name) + 1 > 78:
            out.append(row + "\n")
            row = "//"
        row += " " + name
    out.append(row + "\n")
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text("".join(out), encoding="utf-8")
    print("%s: %d magic pointers, %d other constants, %d objects left out"
          % (OUT.relative_to(REPO), len(magic), len(other), len(objects)))
    if unknown:
        print("magic pointers the ROM has no name for: %s" % unknown)


if __name__ == "__main__":
    main()
