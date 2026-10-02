#!/usr/bin/env python3
"""The ROM as assembler source (R2, R3).

    romasm.py <rom-dir> <out-dir> [--chunk BYTES] [--pad ADDRESS:BYTES] [--object FILE ...]

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
    *.o          with --object: compiled objects, changed to go in place (R6)
    files.txt    the files in link order (sources, and objects as they are)
    link.txt     the linker's options

R6, compiled code in place (--object, an AOF object from ARMCpp): its
code areas (one per function with -zo) go where their functions are in
the ROM, so a source file may leave out a function (it stays generated
assembler between) (the address of a
function it defines in Apple's table, less its offset in the area); the
generated source leaves those bytes and labels out, and the area is
renamed to sort between the pieces around it (`ROM$$RO$$05`, then
`ROM$$RO$$05$$01` the object, `ROM$$RO$$05$$02` the rest: the linker
orders areas by name). Its vtables (Common areas) are left out: the
generated source has the ROM's. A reference to a function with a jump-table
slot goes to the slot (`VEC_Name`), whether the function is in this file or
not: Apple's calls do (`FSetOrientation` calls `SetOrientation`, right
after it, through its slot; the compiler makes a relocation for every
call, even in the file). Anything outside that still needs a label inside
(a branch into the middle, a PC-relative load) stops it.

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
- R3h.3: a PC-relative load or store (LDR/STR(B) Rd, [pc, #n]) or ADR
  (ADD/SUB Rd, pc, #n) in code whose target is in another symbol (hand-
  written assembler shares literal pools across symbols; the compiler's
  ClassInfo points at data before it) is written as `LDR Rd, |label|`
  (`ADR Rd, |label|`): the assembler works out the offset, so padding
  between them is fine; the files are never cut between the two.
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
- R3e.1b: tables of code addresses. In a run of at least three data words
  where at least one is already a pointer (R3e.1), or which starts where a
  literal points (the code loads the table's address), every word that is the
  address of an instruction (code by romcode.py, or by newtonos.s; in a
  table whose start a literal holds, any aligned address in the read-only
  part: the SWI table's hand-written handlers are code to neither) is
  written as `DCD |label|` (a label made where there is none): the SWI
  dispatch table, WarmBoot's `LDR pc, [pc, Rm, LSL #2]` table, handler
  tables whose handlers have no symbol. The static constructors' and
  destructors' tables (C$$ctorvec, C$$dtorvec) entirely: functions without
  a symbol.
- R3h: the linker's values at the ROM's start: gPackageStart (0x3C) is
  |ROM$$Size|; the DataAreaTable (0x40: "data", then the RAM data's load
  address, run address, zero-init address, lengths) is |Load$$root$$Base|,
  |Image$$root$$Base|, |Image$$root$$ZI$$Base|, |Image$$root$$Length|,
  |Image$$root$$ZI$$Length| (imported: the linker makes them).
- R3h.2: alignment: a symbol at a multiple of 0x100 or more with at least
  16 bytes of zeros before it (in the read-only part from 0x10000, and the
  hand-written start: gROMPublicJumpTable 0x13000, gROMPatchTablePageTable
  0x16000, AsmTraceAddAddrEvent 0x18400, gROMMagicPointerTable 0x3AF000)
  starts an area of its own with ALIGN=n, and the zeros before it are
  left out: the linker aligns the area and fills with zeros. Padding put
  in before such a symbol is absorbed there (aligned.json says where).
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
  IrMaxTurnTimeTable, nbcut..., xr_type_merits (times, cut values, pairs),
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


def pc_relative(word, pc):
    """(target, kind) of a PC-relative LDR/STR(B) with an immediate offset
    ('ldr') or ADR, ADD/SUB Rd, pc, #imm ('adr'); else None."""
    if word >> 28 == 0xF or (word >> 16) & 15 != 15:
        return None
    op = (word >> 25) & 7
    if op == 2 and word & 0x01000000 and not word & 0x00200000:     # pre-indexed, no write-back
        off = word & 0xFFF
        return pc + 8 + (off if word & 0x00800000 else -off), 'ldr'
    if op == 1 and (word >> 21) & 15 in (2, 4) and not word & 0x00100000:
        imm, rot = word & 0xFF, ((word >> 8) & 15) * 2
        imm = ((imm >> rot) | (imm << (32 - rot))) & 0xFFFFFFFF
        return pc + 8 + (imm if (word >> 21) & 15 == 4 else -imm), 'adr'
    return None


def cross_references(ro, kinds, starts):
    """R3h.3: PC-relative loads and ADRs in code whose target is in another
    symbol (hand-written assembler sharing a literal pool; ClassInfo):
    [(address, target, kind)]."""
    out = []
    for i in range(len(ro) // 4):
        if kinds[i] not in (ord('c'), ord('n')):
            continue
        a = 4 * i
        r = pc_relative(struct.unpack_from('>I', ro, a)[0], a)
        if r is None or not 0 <= r[0] < len(ro):
            continue
        k = bisect.bisect_right(starts, a)
        lo = starts[k - 1] if k else 0
        hi = starts[k] if k < len(starts) else len(ro)
        if not lo <= r[0] < hi:
            out.append((a, r[0], r[1]))
    return out


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

    def cross(self, refs):
        """R3h.3: PC-relative loads and ADRs to another symbol, as
        `LDR Rd, |label|` / `ADR Rd, |label|`: the assembler works out the
        offset (both in one area: the cuts keep them so)."""
        for a, t, kind in refs:
            w = struct.unpack_from('>I', self.ro, a)[0]
            label = self.label_for(t)
            rd = 'r%d' % ((w >> 12) & 15)
            cond = CONDITIONS[w >> 28]
            if kind == 'ldr':
                op = ('LDR' if w & 0x00100000 else 'STR') + cond + ('B' if w & 0x00400000 else '')
            else:
                op = 'ADR' + cond
            self.lines[a] = ('        %-8s %s, |%s|' % (op, rd, label), label)
            self.counts['PC-relative loads and ADRs to another symbol'] += 1

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


def write_area(path, area, attrs, data, origin, labels, start, end, lines, pad=None, data_end=None):
    """data_end: where the bytes end (before alignment filler, left out);
    labels at data_end go at the end."""
    body = []
    if data_end is None:
        data_end = end
    points = sorted(a for a in labels if start <= a < end and a <= data_end)
    end = data_end
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
    # (a label at the very end, before left-out filler)
    out = ['; generated by tools/romasm.py from Apple\'s ROM image: do not edit',
           '        AREA |%s|, %s' % (area, attrs)]
    out += ['        EXPORT |%s|' % n for p in points for n in labels[p]]
    out += ['        IMPORT |%s|' % n for n in used]
    out += body + ['        END', '']
    with open(path, 'w') as f:
        f.write('\n'.join(out))


def place_object(path, names_once, slots, size):
    """R6: read a compiled object for its place in the ROM: (runs, the AOF
    changed, the names it defines, slots used); runs: [(start, end, [area
    names])], areas that follow each other in the ROM. With -zo every
    function is an area of its own and goes where its function is (Apple's
    table): a source file may leave a function out (it stays generated in
    between); an area without a function of a known name (a static one)
    follows the area before it."""
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    from aof import AOF, AREA_COMMON_DEF
    obj = AOF.read(path)
    for a in [a for a in obj.areas if a.attributes & AREA_COMMON_DEF]:
        obj.drop_area(a.name)
    if not obj.areas or not all(a.is_code for a in obj.areas):
        raise ValueError('%s: code areas only, not %s' % (path, [a.name for a in obj.areas]))
    runs, at = [], None
    for a in obj.areas:
        places = {names_once[s.name] - s.value for s in obj.symbols
                  if s.is_defined and s.is_global and s.area == a.name and s.name in names_once}
        if len(places) > 1:
            raise ValueError('%s: %s has no single place in the ROM (%s)'
                             % (path, a.name, sorted(hex(v) for v in places)))
        if places:
            start = places.pop()
        elif at is not None:
            start = at
        else:
            raise ValueError('%s: %s has no function the ROM names' % (path, a.name))
        if not 0 <= start and start + a.size <= size:
            raise ValueError('%s: %s outside the read-only part' % (path, a.name))
        if runs and runs[-1][1] == start:
            runs[-1][1] = start + ((a.size + 3) & ~3)
            runs[-1][2].append(a.name)
        else:
            runs.append([start, start + ((a.size + 3) & ~3), [a.name]])
        at = start + ((a.size + 3) & ~3)
    defined = {s.name for s in obj.symbols if s.is_defined and s.is_global}
    used_slots = {}
    for s in list(obj.symbols):
        if s.is_global and s.name in slots:      # defined here or not: Apple's calls go through the slot
            used_slots['VEC_' + s.name] = slots[s.name]
            obj.redirect(s.name, 'VEC_' + s.name)
    obj.prune()
    return [tuple(r) for r in runs], obj, defined, used_slots


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
    ap.add_argument('--object', action='append', default=[],
                    help='a compiled object (AOF) to put in place of its functions (R6)')
    ap.add_argument('--pad', help='ADDRESS:BYTES: zero bytes put in before the symbol at ADDRESS '
                    '(the shift test, tools/shifttest.py)')
    args = ap.parse_args()
    with open(os.path.join(args.rom, 'ro.bin'), 'rb') as f:
        ro = f.read()
    with open(os.path.join(args.rom, 'rw.bin'), 'rb') as f:
        rw = f.read()
    with open(os.path.join(args.rom, 'code.bin'), 'rb') as f:
        kinds = f.read()
    with open(os.path.join(args.rom, 'kinds.bin'), 'rb') as f:
        kinds_ns = f.read()
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
    sym_starts = sorted(set(s['value'] for s in symbols if s['class'] != 'abs' and s['value'] < len(ro)))
    refs = cross_references(ro, kinds, sym_starts)
    spanned = set()                       # symbols a cut would put between such a pair
    for a, t_, _ in refs:
        lo, hi = min(a, t_), max(a, t_)
        spanned.update(sym_starts[bisect.bisect_right(sym_starts, lo):bisect.bisect_right(sym_starts, hi)])
    cuts = [0]
    target = args.chunk
    for a in starts:
        if a >= target and a not in spanned:
            cuts.append(a)
            target = a + args.chunk
    cuts.append(len(ro))
    # R3h.2: aligned symbols with zeros before them start an aligned area
    aligned = {}                          # symbol address -> (alignment, where the zeros start)
    for v in starts:
        for al in (0x1000, 0x400, 0x100):
            if v % al == 0:
                break
        else:
            continue
        b = v
        while b > 0 and ro[b - 4:b] == b'\0\0\0\0':
            b -= 4
        if v - b >= 16 and v >= 0x10000 or v in (0x13000, 0x16000, 0x18400):
            if v - b >= 16:
                aligned[v] = (al, b)
    cuts = sorted(set(cuts) | set(aligned))
    if spanned & set(aligned):
        print('an aligned area would cut a PC-relative reference: 0x%X' % min(spanned & set(aligned)))
        return 2
    with open(os.path.join(args.out, 'aligned.json'), 'w') as f:
        json.dump([{'symbol': v, 'alignment': al, 'zeros_from': b} for v, (al, b) in sorted(aligned.items())], f)

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
    src.cross(refs)
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

    numbers = re.compile(r'^(gLex8|gEnum80|gSymb80|yy|DESSBoxes|IrMaxTurnTimeTable|nbcut|xr_type_merits)')

    def skip_ro(a):
        if kinds[a // 4] in (ord('c'), ord('n'), ord('l'), ord('v')) or soup_low <= a < dense_end:
            return True
        k = bisect.bisect_right(src.starts, a) - 1
        return k >= 0 and bool(numbers.match(src.start_names.get(src.starts[k], '')))

    dense = (soup_high, dense_end)     # the R and RS constants
    src.lines.update(src.data_pointers(ro, 0, ram_labels, rw_base, zi_base + zi_size, skip_ro, report, dense))
    rw_lines = src.data_pointers(rw, rw_base, ram_labels, rw_base, zi_base + zi_size, lambda a: False,
                                 report, dense)
    # R3e.1b: tables of code addresses
    def code_address(v):
        return (0x10000 <= v < len(ro) and v % 4 == 0
                and (kinds[v // 4] in (ord('c'), ord('n')) or kinds_ns[v // 4] == ord('i')))

    loaded = set()                 # addresses the code's literals hold
    for i in range(len(ro) // 4):
        if kinds[i] == ord('l'):
            loaded.add(struct.unpack_from('>I', ro, 4 * i)[0])
    forced = set()
    for t in ('C$$ctorvec', 'C$$dtorvec'):
        forced.update(range(names[t + '$$Base'], names[t + '$$Limit'], 4))
    i = 0
    nwords = len(ro) // 4
    while i < nwords:
        a = 4 * i
        if skip_ro(a) and a not in forced:
            i += 1
            continue
        j = i
        strong = 0
        table = a in loaded
        while j < nwords and (not skip_ro(4 * j) or 4 * j in forced):
            b = 4 * j
            v = struct.unpack_from('>I', ro, b)[0]
            if b in src.lines:
                strong += 1
            elif not (code_address(v) or (table and 0x10000 <= v < len(ro) and v % 4 == 0)):
                break
            j += 1
        if j - i >= 3 and (strong or a in forced or a in loaded):
            for k in range(i, j):
                b = 4 * k
                if b not in src.lines:
                    v = struct.unpack_from('>I', ro, b)[0]
                    label = src.label_for(v)
                    src.lines[b] = ('        DCD      |%s|' % label, label)
                    src.counts['tables of code addresses'] += 1
        i = max(j, i + 1)
    for a, n in linker_words.items():          # R3h, after R3e.1 (which saw some as RAM addresses)
        src.lines[a] = ('        DCD      |%s|' % n, n)
    with open(os.path.join(args.out, 'data-pointers.txt'), 'w') as f:
        f.write('\n'.join(report) + '\n')

    # R6: compiled objects in place
    names_once = {s['name']: s['value'] for s in symbols if count[s['name']] == 1 and s['value'] < len(ro)}
    placed = []                          # (start, end, AOF, [area names], path)
    objects = []                         # (AOF, file name)
    for n, path in enumerate(args.object):
        runs, obj, defined, used_slots = place_object(path, names_once, info['slots'], len(ro))
        for start, end, areas in runs:
            for other in placed:
                if start < other[1] and other[0] < end:
                    print('%s overlaps %s at 0x%X' % (path, other[4], start))
                    return 2
            if any(start < c < end for c in cuts):
                print('%s: 0x%X..0x%X crosses a file boundary' % (path, start, end))
                return 2
            for a, t_, _ in refs:
                if (start <= a < end) != (start <= t_ < end):
                    print('%s: a PC-relative reference between 0x%X and 0x%X crosses its edge' % (path, a, t_))
                    return 2
            inside = {n_ for p in ro_labels if start <= p < end for n_ in ro_labels[p]}
            needed = {lab for a, (_, lab) in src.lines.items() if not start <= a < end} & (inside - defined)
            if needed:
                print('%s: labels needed from outside but not defined: %s' % (path, ', '.join(sorted(needed))))
                return 2
            placed.append((start, end, obj, areas, path))
            src.counts['bytes from compiled objects'] += end - start
        src.absolute.update(used_slots)
        objects.append((obj, 'obj_%02d.o' % n))
        src.counts['compiled objects in place'] += 1

    files = []
    for i in range(len(cuts) - 1):
        attrs = 'CODE, READONLY'
        if cuts[i] in aligned:
            attrs = 'CODE, READONLY, ALIGN=%d' % (aligned[cuts[i]][0].bit_length() - 1)
        data_end = aligned[cuts[i + 1]][1] if cuts[i + 1] in aligned else None
        # the pieces: generated source, compiled code between
        pieces, a = [], cuts[i]
        for start, end, obj, areas, path in sorted((p for p in placed if cuts[i] <= p[0] < cuts[i + 1]),
                                                   key=lambda p: p[0]):
            pieces += [('src', a, start), ('obj', obj, areas)]
            a = end
        pieces.append(('src', a, cuts[i + 1]))
        for k, piece in enumerate(pieces):
            area_name = 'ROM$$RO$$%02d' % i + ('$$%02d' % k if k else '')
            if piece[0] == 'obj':
                for j, old_name in enumerate(piece[2]):      # in order: ...$$01$$000, $$01$$001
                    piece[1].rename_area(old_name, '%s$$%03d' % (area_name, j))
            else:
                name = 'ro_%02d.a' % i if not k else 'ro_%02d_%02d.a' % (i, k)
                last = k == len(pieces) - 1
                write_area(os.path.join(args.out, name), area_name, attrs if not k else 'CODE, READONLY',
                           ro, 0, ro_labels, piece[1], piece[2], src.lines, pad,
                           data_end if last else None)
                files.append(name)
    for obj, name in objects:                    # each object once, its areas renamed into place
        obj.write(os.path.join(args.out, name))
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
