#!/usr/bin/env python3
"""
Resolve NewtonOS ROM public jump table entries to function names.

Packages (like "NS Debug Tools.pkg") call the ROM through the public jump
table, which the MMU maps to virtual address 0x01800000:

  1. entry i is at virtual 0x01800000 + 4*i; the ROM stores it at
     gROMPublicJumpTable (0x13000) + 4*i. It is a `b` to a VEC_ address.
  2. the VEC_ address is the function's entry in the patchable table; the MMU
     maps it to ROM address (((a>>5) & 0xffffff80) | (a & 0x7f)) - 0xCE000
     (128 bytes of table per 4K page). That word is a `b` to the function.

newtonos.s (git-ignored, see Matt/CLAUDE.md) shows the ROM with calls already
resolved, so it has no labels for the 0x0180xxxx table; this script follows
the chain.

Usage:
  Matt/tools/rom_jumptable.py 2045 0x01801FF4 ...   entry index or address
"""

import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
ROM_SOURCE = REPO / "newtonos.s"

JUMP_TABLE_VIRTUAL = 0x01800000
JUMP_TABLE_ROM = 0x00013000
JUMP_TABLE_ROM_END = 0x00015E0C

WORD_RE = re.compile(r"^\s*\.word\s+0x([0-9A-Fa-f]{8})\s+@ 0x([0-9A-Fa-f]{8}) ")
LABEL_RE = re.compile(r"^(\S+):\s*@ 0x([0-9A-Fa-f]{8})")
VEC_RE = re.compile(r"^\s*\.equ\s+(VEC_\S+),\s*_start\+0x([0-9A-Fa-f]+)")


def patch_table_rom(vec_addr):
    return (((vec_addr >> 5) & 0xFFFFFF80) | (vec_addr & 0x7F)) - 0xCE000


def branch_target(pc, insn):
    if (insn & 0x0F000000) != 0x0A000000:      # not a b instruction
        return None
    off = insn & 0xFFFFFF
    if off & 0x800000:
        off -= 0x1000000
    return (pc + 8 + off * 4) & 0xFFFFFFFF


def load(wanted_words):
    """Read newtonos.s once: the words we need, all labels, all VEC_ names."""
    words, labels, vecs = {}, {}, {}
    with open(ROM_SOURCE, errors="replace") as f:
        for line in f:
            if line.startswith("\t.word"):
                m = WORD_RE.match(line)
                if m:
                    addr = int(m.group(2), 16)
                    if addr in wanted_words:
                        words[addr] = int(m.group(1), 16)
            elif line.startswith("\t.equ"):
                m = VEC_RE.match(line)
                if m:
                    vecs[int(m.group(2), 16)] = m.group(1)
            elif not line[:1].isspace():
                m = LABEL_RE.match(line)     # "Name:\t@ 0x..." (local labels have no @)
                if m:
                    labels.setdefault(int(m.group(2), 16), m.group(1))
    return words, labels, vecs


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    indices = []
    for arg in sys.argv[1:]:
        n = int(arg, 0)
        indices.append((n - JUMP_TABLE_VIRTUAL) // 4 if n >= JUMP_TABLE_VIRTUAL else n)

    # First pass: the jump table words we need
    wanted = {JUMP_TABLE_ROM + 4 * i for i in indices}
    words, labels, vecs = load(wanted)
    vec_addrs = {}
    for i in indices:
        rom = JUMP_TABLE_ROM + 4 * i
        if rom >= JUMP_TABLE_ROM_END or rom not in words:
            continue
        vec_addrs[i] = branch_target(JUMP_TABLE_VIRTUAL + 4 * i, words[rom])

    # Second pass: the patch table words behind the VEC_ addresses
    wanted2 = {patch_table_rom(v) for v in vec_addrs.values() if v is not None}
    words2, _, _ = load(wanted2)

    for i in indices:
        virt = JUMP_TABLE_VIRTUAL + 4 * i
        vec = vec_addrs.get(i)
        if vec is None:
            print(f"{i:5d}  0x{virt:08X}  (not in the jump table)")
            continue
        patch = patch_table_rom(vec)
        func = branch_target(vec, words2.get(patch, 0))
        name = labels.get(func, "?") if func is not None else "?"
        print(f"{i:5d}  0x{virt:08X} -> {vecs.get(vec, f'0x{vec:08X}')} "
              f"-> ROM 0x{patch:08X} -> 0x{func or 0:08X} {name}")


if __name__ == "__main__":
    main()
