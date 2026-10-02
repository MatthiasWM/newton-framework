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
- R3d.3: a literal (a word the code loads PC-relative, romcode.py's 'l')
  whose value is an address is written as `DCD |label|+offset` (the
  nearest label at or below it): an address in the read-only part from
  0x10000 up (below, small numbers and the hand-written start of the ROM
  look alike: R3h), in the RAM data (RW, zero-init), with or without a
  NewtonScript tag (+1), or a jump-table slot (|VEC_Name|).
- R3f: the NewtonScript object area (gROMSoupData..gROMSoupDataSize),
  object by object (a header word: size << 8 | flags, bit 0 slotted, bit 1
  a frame; a GC word; then the class or map, then the slots; each object
  padded to 4 bytes): every object gets a label (its symbol, or
  |O_0x3AFDA8|); a Ref to an object (its address + 1, in a slot or as a
  binary's class) is written as `DCD |label|+1`; a slot whose value is the
  start of a function or a jump-table slot (native functions' C code) as
  `DCD |label|`. After the area, the R and RS constants (to the first
  other name): an R word holds a Ref to an object (`DCD |label|+1`) or a
  magic pointer (a number); an RS word the address of an R word (`DCD
  |label|`, a label made where the word has no symbol).
- R3g: the jump table (at the jump_table address, romsyms.py): entry i is
  a `B function` encoded from its virtual address (the MMU shows the table
  sparsely from 0x01A00000: 32 entries a 4 KB page, page p's entries at
  offset (p % 32) * 0x80), so it is written as `B |function|-&D`, D its
  virtual minus its physical address (a constant: the linker then encodes
  it right wherever the function is).
- R3h: the linker's values at the ROM's start: gPackageStart (0x3C) is
  |ROM$$Size|; the DataAreaTable (0x40: "data", then the RAM data's load
  address, run address, zero-init address, lengths) is |Load$$root$$Base|,
  |Image$$root$$Base|, |Image$$root$$ZI$$Base|, |Image$$root$$Length|,
  |Image$$root$$ZI$$Length| (imported: the linker makes them).
- R3e.1: a data word (in the read-only part, not code and not in the
  NewtonScript object area gROMSoupData..gROMSoupDataSize; or in the RW
  data) whose value is exactly where a symbol starts (from 0x10000 up), a
  RAM symbol, or a jump-table slot, is written as `DCD |label|`. Only this
  strong evidence: a word pointing inside a symbol is as often two 16-bit
  numbers (parser tables, dictionaries). Nor a target in the block of R
  and RS constants after the object area (a symbol every word, so every
  aligned value there "hits": the dictionaries' UTF-16 pairs did, as
  0x006E0027 is a symbol's address). Not in tables of numbers either,
  whose words hit symbols by chance: the recogniser's dictionaries and
  lexicons (gLex8..., gEnum80..., gSymb80...), the parser's tables (yy...),
  the DES S-boxes; nor in the R/RS block (R3f). data-pointers.txt lists
  them by the symbol they are in (single hits: to check).
"""

import argparse
import bisect
import json
import os
import re
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
        self.start_names = {}
        for s_ in symbols:
            if s_['class'] != 'abs' and s_['value'] < len(ro):
                self.start_names.setdefault(s_['value'], s_['name'])
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

    def literals(self, ram_labels, ram_low, ram_high):
        """R3d.3: literals that are addresses, as label+offset."""
        ro_points = sorted(self.ro_labels)
        ram_points = sorted(ram_labels)

        def near(points, labels, v):
            k = bisect.bisect_right(points, v) - 1
            if k < 0:
                return None
            p = points[k]
            return labels[p][0], v - p

        for i in range(len(self.ro) // 4):
            if self.kinds[i] != ord('l'):
                continue
            a = 4 * i
            v = struct.unpack_from('>I', self.ro, a)[0]
            if v in self.slot_names:
                ref = (self.label_for(v), 0)
                kind = 'literals: jump-table slots'
            elif 0x10000 <= v < len(self.ro):
                ref = near(ro_points, self.ro_labels, v)
                kind = 'literals: read-only addresses'
            elif ram_low <= v < ram_high:
                ref = near(ram_points, ram_labels, v)
                kind = 'literals: RAM data addresses'
            else:
                continue
            if ref is None:
                continue
            label, off = ref
            expr = '|%s|' % label + ('+%d' % off if off else '')
            self.lines[a] = ('        DCD      %s' % expr, label)
            self.counts[kind] += 1

    def jump_table(self, rom_address, count):
        """R3g: the jump table's entries, encoded from their virtual addresses."""
        def virtual(i):
            page, slot = divmod(i, 32)
            return 0x01A00000 + page * 0x1000 + (page % 32) * 0x80 + slot * 4
        for i in range(count):
            p = rom_address + 4 * i
            w = struct.unpack_from('>I', self.ro, p)[0]
            v = virtual(i)
            t = branch_target(w, v)
            label = self.label_for(t)
            self.lines[p] = ('        B        |%s|-&%X' % (label, v - p), label)
            self.counts['jump table entries'] += 1

    def object_area(self, lo, hi):
        """R3f: the NewtonScript objects in [lo, hi)."""
        objects = []
        a = lo
        while a < hi:
            w0 = struct.unpack_from('>I', self.ro, a)[0]
            size = w0 >> 8
            if size < 8:
                raise ValueError('no object at 0x%X (0x%08X)' % (a, w0))
            objects.append((a, size, w0 & 0xFF))
            a += (size + 3) & ~3
        starts = set(o for o, _, _ in objects)
        for o, _, _ in objects:
            if o not in self.ro_labels:
                self.ro_labels[o].append('O_0x%X' % o)
        for o, size, flags in objects:
            words = range(o + 8, o + size - 3, 4) if flags & 1 else [o + 8]
            for p in words:
                v = struct.unpack_from('>I', self.ro, p)[0]
                if v & 3 == 1 and (v - 1) in starts:
                    label = self.ro_labels[v - 1][0]
                    self.lines[p] = ('        DCD      |%s|+1' % label, label)
                    self.counts['object area: references'] += 1
                elif flags & 1 and v in self.slot_names:
                    label = self.label_for(v)
                    self.lines[p] = ('        DCD      |%s|' % label, label)
                    self.counts['object area: native functions (slots)'] += 1
                elif (flags & 1 and v & 3 == 0 and 0x10000 <= v < len(self.ro) and v in self.ro_labels
                      and self.kinds[v // 4] in (ord('c'), ord('n'))):
                    label = self.ro_labels[v][0]
                    self.lines[p] = ('        DCD      |%s|' % label, label)
                    self.counts['object area: native functions'] += 1
        self.counts['object area: objects'] = len(objects)
        return starts

    def r_constants(self, lo, hi, objects):
        """R3f: the R and RS constants in [lo, hi) after the object area."""
        for p in range(lo, hi, 4):
            v = struct.unpack_from('>I', self.ro, p)[0]
            if v & 3 == 1 and (v - 1) in objects:
                label = self.ro_labels[v - 1][0]
                self.lines[p] = ('        DCD      |%s|+1' % label, label)
                self.counts['R constants: references'] += 1
            elif v & 3 == 0 and lo <= v < hi:
                label = self.label_for(v)
                self.lines[p] = ('        DCD      |%s|' % label, label)
                self.counts['RS constants: addresses of R words'] += 1

    def data_pointers(self, data, base, ram_labels, ram_low, ram_high, skip, report, dense=(0, 0)):
        """R3e.1: data words that are exactly a symbol's address; base: data's
        address; skip(a): leave this word alone. Returns {address: line}."""
        lines = {}
        by_symbol = defaultdict(int)
        for i in range(len(data) // 4):
            a = base + 4 * i
            if skip(a):
                continue
            v = struct.unpack_from('>I', data, 4 * i)[0]
            if v in self.slot_names:
                label = self.label_for(v)
            elif 0x10000 <= v < len(self.ro) and v in self.ro_labels and not dense[0] <= v < dense[1]:
                label = self.ro_labels[v][0]
            elif ram_low <= v < ram_high and v in ram_labels:
                label = ram_labels[v][0]
            else:
                continue
            lines[a] = ('        DCD      |%s|' % label, label)
            k = bisect.bisect_right(self.starts, a) - 1 if a < len(self.ro) else -1
            by_symbol['%s 0x%X' % (self.start_names.get(self.starts[k], '?'), self.starts[k]) if k >= 0
                      else 'RW data'] += 1
            self.counts['data pointers'] += 1
        for name, n in sorted(by_symbol.items(), key=lambda x: -x[1]):
            report.append('%6d  %s' % (n, name))
        return lines

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
    names = {s['name']: s['value'] for s in symbols}
    soup_low, soup_high = names['gROMSoupData'], names['gROMSoupDataSize']
    objects = src.object_area(soup_low, soup_high)
    r_names = sorted((s['value'], s['name']) for s in symbols if soup_high <= s['value'] < len(ro))
    dense_end = next(v for v, n in r_names if v > soup_high and not (n[:1] == 'R' and len(n) > 1))
    src.r_constants(soup_high + 4, dense_end, objects)
    src.jump_table(info['jump_table']['rom_address'], info['jump_table']['count'])
    linker_words = {names['gPackageStart']: 'ROM$$Size'}
    for k, n in enumerate(('Load$$root$$Base', 'Image$$root$$Base', 'Image$$root$$ZI$$Base',
                           'Image$$root$$Length', 'Image$$root$$ZI$$Length')):
        linker_words[names['DataAreaTable'] + 4 + 4 * k] = n
    src.branches()
    rw_labels = label_names(symbols, rw_base, rw_base + len(rw), count)
    zi_base = rw_base + len(rw)
    zi_size = info['image']['zi_size']
    zi_labels = label_names(symbols, zi_base, zi_base + zi_size, count)
    ram_labels = defaultdict(list)
    for d in (rw_labels, zi_labels):
        for k, v in d.items():
            ram_labels[k] += v
    src.literals(ram_labels, rw_base, zi_base + zi_size)
    report = ['; data words that are exactly a symbol\'s address, by the symbol they are in (R3e.1)']

    numbers = re.compile(r'^(gLex8|gEnum80|gSymb80|yy|DESSBoxes)')

    def skip_ro(a):
        if kinds[a // 4] in (ord('c'), ord('n'), ord('l'), ord('v')) or soup_low <= a < dense_end:
            return True
        k = bisect.bisect_right(src.starts, a) - 1
        return k >= 0 and bool(numbers.match(src.start_names.get(src.starts[k], '')))

    dense = (soup_high, dense_end)     # the R and RS constants
    src.lines.update(src.data_pointers(ro, 0, ram_labels, rw_base, zi_base + zi_size, skip_ro, report, dense))
    rw_lines = src.data_pointers(rw, rw_base, ram_labels, rw_base, zi_base + zi_size, lambda a: False,
                                 report, dense)
    for a, n in linker_words.items():          # R3h, after R3e.1 (which saw some as RAM addresses)
        src.lines[a] = ('        DCD      |%s|' % n, n)
    with open(os.path.join(args.out, 'data-pointers.txt'), 'w') as f:
        f.write('\n'.join(report) + '\n')

    files = []
    for i in range(len(cuts) - 1):
        name = 'ro_%02d.a' % i
        write_area(os.path.join(args.out, name), 'ROM$$RO$$%02d' % i, 'CODE, READONLY',
                   ro, 0, ro_labels, cuts[i], cuts[i + 1], src.lines, pad)
        files.append(name)
    write_area(os.path.join(args.out, 'rw.a'), 'ROM$$RW', 'DATA',
               rw, rw_base, rw_labels, rw_base, rw_base + len(rw), rw_lines)
    files.append('rw.a')
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
    print('%d files, %d labels (%d made for targets), %d absolute symbols'
          % (len(files), nlabels, src.counts['labels made'], len(src.absolute)))
    for k in sorted(src.counts):
        if k != 'labels made':
            print('  %s: %d' % (k, src.counts[k]))
    return 0


if __name__ == '__main__':
    sys.exit(main())
