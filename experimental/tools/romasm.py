#!/usr/bin/env python3
"""The ROM as assembler source (R2, R3).

    romasm.py <rom-dir> <out-dir> [--chunk BYTES] [--pad ADDRESS:BYTES]

From romsyms.py's and romcode.py's output (ro.bin, rw.bin, symbols.json,
code.bin), writes ARM6asm source that ARMLink turns back into Apple's
image:

    ro_NN.a      the read-only part in pieces of about --chunk bytes (cut at
                 a symbol), one area each (|ROM$$RO$$NN|: the linker sorts
                 areas by name, so the pieces stay in order)
    rw.a         the read-write data (|ROM$$RW|, linked to run at its base)
    zi.a         the zero-initialised data (|ROM$$ZI|, NOINIT), after it
    abs.a        absolute symbols: the jump-table slots (|VEC_Name|) and
                 other fixed addresses (|A_0x01234567|) that code refers to
    scatter.txt  the scatter file, as Apple's link had it: the root region
                 at 0, its data at the RW base (so the linker makes Apple's
                 symbols: ROM$$Size, Image$$root$$Base, Load$$root$$Base, ...)
    files.txt    the files in link order
    link.txt     the linker's options

Every symbol becomes an exported label: its name if the ROM has it once,
else the name with its address (|Name@0x1234|: local functions of
different files). The linker's own symbols ($$ in the name) are left out:
the linker makes them. Words go out as DCD (only at word-aligned
addresses), other bytes as DCB, but where a word refers to an address
(R3): then it is written as what it means, so that it follows when things
move.

- R3c: a branch (B, BL) in code to outside its own symbol (code by
  romcode.py, which follows the code) is written as the instruction to a
  label: the function's, a jump-table slot's, or a label made for the
  target (|L_0x1234|); so is every vtable entry (romcode.py's 'v').
"""

import argparse
import bisect
import json
import os
import struct
import sys
from collections import defaultdict

CONDITIONS = ['EQ', 'NE', 'CS', 'CC', 'MI', 'PL', 'VS', 'VC',
              'HI', 'LS', 'GE', 'LT', 'GT', 'LE', '', 'NV']


def label_names(symbols, low, high, count):
    """Labels for the symbols in [low, high): address -> [label]."""
    at = defaultdict(list)
    for s in symbols:
        name, value = s['name'], s['value']
        if not (low <= value < high) or '$$' in name:
            continue
        at[value].append(name if count[name] == 1 else '%s@0x%X' % (name, value))
    return at


def branch_target(word, pc):
    imm = word & 0xFFFFFF
    if imm & 0x800000:
        imm -= 0x1000000
    return (pc + 8 + imm * 4) & 0xFFFFFFFF


def is_branch(word):
    return (word & 0x0E000000) == 0x0A000000 and (word >> 28) != 0xF


class Source:
    """The words written as what they mean: address -> (line, symbol used)."""

    def __init__(self, ro, kinds, symbols, slots, ro_labels):
        self.ro = ro
        self.kinds = kinds
        self.lines = {}
        self.ro_labels = ro_labels
        self.absolute = {}                       # name -> value
        self.slot_names = {v: n for n, v in slots.items()}
        # where functions (and other symbols) start, and which are code
        located = sorted((s['value'], s['class']) for s in symbols
                         if s['class'] != 'abs' and s['value'] < len(ro))
        self.starts = [v for v, _ in located]
        self.classes = [c for _, c in located]
        self.counts = defaultdict(int)

    def region(self, a):
        """(start, end, class) of the symbol a is in."""
        i = bisect.bisect_right(self.starts, a) - 1
        if i < 0:
            return 0, self.starts[0] if self.starts else len(self.ro), 'data'
        j = bisect.bisect_right(self.starts, self.starts[i])
        end = self.starts[j] if j < len(self.starts) else len(self.ro)
        return self.starts[i], end, self.classes[i]

    def label_for(self, t):
        """A label for an address: a function's, a slot's, one made for it."""
        if t in self.slot_names:
            name = 'VEC_' + self.slot_names[t]
            self.absolute[name] = t
            return name
        if 0 <= t < len(self.ro):
            if t not in self.ro_labels:
                self.ro_labels[t].append('L_0x%X' % t)
                self.counts['labels made'] += 1
            return self.ro_labels[t][0]
        name = 'A_0x%08X' % t
        self.absolute[name] = t
        return name

    def branches(self):
        """R3c: branches in code that leave their function."""
        for i in range(len(self.ro) // 4):
            a = 4 * i
            if self.kinds[i] not in (ord('c'), ord('n'), ord('v')):
                continue
            w = struct.unpack_from('>I', self.ro, a)[0]
            if not is_branch(w):
                continue
            start, end, cls = self.region(a)
            t = branch_target(w, a)
            if start <= t < end and self.kinds[i] != ord('v'):
                continue
            label = self.label_for(t)
            op = ('BL' if w & 0x01000000 else 'B') + CONDITIONS[w >> 28]
            self.lines[a] = ('        %-8s |%s|' % (op, label), label)
            self.counts['branches'] += 1


def emit_bytes(out, data, start, end, origin, lines):
    """data[start-origin:end-origin] as DCB/DCD lines, but the words in
    `lines` as their own lines; start, end: addresses."""
    a = start
    while a < end and a % 4:
        n = min(end, (a + 4) & ~3) - a
        out.append('        DCB ' + ','.join('&%02X' % b for b in data[a - origin:a - origin + n]))
        a += n
    run = []
    while a + 4 <= end:
        if a in lines:
            if run:
                out.append('        DCD ' + ','.join(run))
                run = []
            out.append(lines[a][0])
        else:
            run.append('&%08X' % struct.unpack_from('>I', data, a - origin)[0])
            if len(run) == 8:
                out.append('        DCD ' + ','.join(run))
                run = []
        a += 4
    if run:
        out.append('        DCD ' + ','.join(run))
    if a < end:
        out.append('        DCB ' + ','.join('&%02X' % b for b in data[a - origin:end - origin]))


def write_area(path, area, attrs, data, origin, labels, start, end, lines, pad=None):
    body = []
    points = sorted(a for a in labels if start <= a < end)
    defined = set()
    a = start
    for p in points:
        emit_bytes(body, data, a, p, origin, lines)
        if pad and pad[0] == p:
            body.append('        %% %d          ; the shift test\'s padding' % pad[1])
        for n in labels[p]:
            body.append('|%s|' % n)
            defined.add(n)
        a = p
    emit_bytes(body, data, a, end, origin, lines)
    used = sorted({lines[x][1] for x in lines if start <= x < end} - defined)
    out = ['; generated by tools/romasm.py from Apple\'s ROM image: do not edit',
           '        AREA |%s|, %s' % (area, attrs)]
    out += ['        EXPORT |%s|' % n for p in points for n in labels[p]]
    out += ['        IMPORT |%s|' % n for n in used]
    out += body + ['        END', '']
    with open(path, 'w') as f:
        f.write('\n'.join(out))


def write_zi(path, area, labels, start, end):
    out = ['; generated by tools/romasm.py from Apple\'s ROM image: do not edit',
           '        AREA |%s|, NOINIT' % area]
    out += ['        EXPORT |%s|' % n for p in sorted(labels) for n in labels[p]]
    a = start
    for p in sorted(labels):
        if p > a:
            out.append('        %% %d' % (p - a))
        out += ['|%s|' % n for n in labels[p]]
        a = p
    if end > a:
        out.append('        %% %d' % (end - a))
    out += ['        END', '']
    with open(path, 'w') as f:
        f.write('\n'.join(out))


def write_absolute(path, absolute):
    out = ['; generated by tools/romasm.py: fixed addresses the ROM refers to',
           '        AREA |ROM$$Absolute|, CODE, READONLY']
    for n in sorted(absolute):
        out.append('        EXPORT |%s|' % n)
    for n in sorted(absolute):
        out.append('|%s| EQU &%08X' % (n, absolute[n]))
    out += ['        END', '']
    with open(path, 'w') as f:
        f.write('\n'.join(out))


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('rom')
    ap.add_argument('out')
    ap.add_argument('--chunk', type=int, default=0x40000)
    ap.add_argument('--pad', help='ADDRESS:BYTES: zero bytes put in before the symbol at ADDRESS '
                    '(the shift test, tools/shifttest.py)')
    args = ap.parse_args()
    with open(os.path.join(args.rom, 'ro.bin'), 'rb') as f:
        ro = f.read()
    with open(os.path.join(args.rom, 'rw.bin'), 'rb') as f:
        rw = f.read()
    with open(os.path.join(args.rom, 'code.bin'), 'rb') as f:
        kinds = f.read()
    with open(os.path.join(args.rom, 'symbols.json')) as f:
        info = json.load(f)
    symbols = info['symbols']
    count = defaultdict(int)
    for s in symbols:
        count[s['name']] += 1
    pad = None
    if args.pad:
        a, n = args.pad.split(':')
        pad = (int(a, 0), int(n, 0))
    rw_base = info['image']['rw_base']
    os.makedirs(args.out, exist_ok=True)

    ro_labels = label_names(symbols, 0, len(ro), count)
    if pad and (pad[0] not in ro_labels or pad[0] % 4 or pad[1] % 4):
        print('--pad: 0x%X is no word-aligned symbol, or %d no multiple of 4' % pad)
        return 2
    # the cuts between files, at symbols (before labels are made for targets)
    starts = sorted(a for a in ro_labels if a % 4 == 0)
    cuts = [0]
    target = args.chunk
    for a in starts:
        if a >= target:
            cuts.append(a)
            target = a + args.chunk
    cuts.append(len(ro))

    src = Source(ro, kinds, symbols, info['slots'], ro_labels)
    src.branches()

    files = []
    for i in range(len(cuts) - 1):
        name = 'ro_%02d.a' % i
        write_area(os.path.join(args.out, name), 'ROM$$RO$$%02d' % i, 'CODE, READONLY',
                   ro, 0, ro_labels, cuts[i], cuts[i + 1], src.lines, pad)
        files.append(name)
    rw_labels = label_names(symbols, rw_base, rw_base + len(rw), count)
    write_area(os.path.join(args.out, 'rw.a'), 'ROM$$RW', 'DATA',
               rw, rw_base, rw_labels, rw_base, rw_base + len(rw), {})
    files.append('rw.a')
    zi_base = rw_base + len(rw)
    zi_size = info['image']['zi_size']
    zi_labels = label_names(symbols, zi_base, zi_base + zi_size, count)
    write_zi(os.path.join(args.out, 'zi.a'), 'ROM$$ZI', zi_labels, zi_base, zi_base + zi_size)
    files.append('zi.a')
    write_absolute(os.path.join(args.out, 'abs.a'), src.absolute)
    files.append('abs.a')
    with open(os.path.join(args.out, 'scatter.txt'), 'w') as f:
        f.write('ROOT 0x%X\nROOT-DATA 0x%X\n' % (info['image']['ro_base'], rw_base))
    with open(os.path.join(args.out, 'files.txt'), 'w') as f:
        f.write('\n'.join(files) + '\n')
    with open(os.path.join(args.out, 'link.txt'), 'w') as f:
        f.write('-AIF -NOZEROpad -Entry 0x%X -SCATTER scatter.txt\n' % info['image']['ro_base'])
    nlabels = sum(len(v) for d in (ro_labels, rw_labels, zi_labels) for v in d.values())
    print('%d files, %d labels (%d made for targets), %d absolute symbols; '
          'branches to labels: %d'
          % (len(files), nlabels, src.counts['labels made'], len(src.absolute), src.counts['branches']))
    return 0


if __name__ == '__main__':
    sys.exit(main())
