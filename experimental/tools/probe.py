#!/usr/bin/env python3
"""Compile C++ and compare it with the ROM, function by function (R0).

    probe.py --bin <tools> --includes <dir> --rom <dir> --out <dir> source.cp ...

For each source: compile it with the standard options (Apple's ARMCpp, LF and
UTF-8 read through mosrun's input filter), give every symbol it imports an
address (its jump-table slot if it has one, else its own address in the
ROM; references to its own functions with a slot go to the slot too, as
in Apple's code), link it so that its functions land at their ROM addresses, and compare
each function it defines with Apple's image (`ro.bin` from romsyms.py), from
its address to the next symbol's. Exit status 0 if every function is
identical.
"""

import argparse
import bisect
import json
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from aof import AOF, AREA_COMMON_DEF  # noqa: E402

CXX_OPTIONS = ['---text=utf8', '-c', '-bigend', '-fc', '-zo', '-DforARM', '-DforQ', '-DQD_Gray']


def run(cmd):
    p = subprocess.run(cmd, capture_output=True, text=True, errors='replace')
    out = (p.stdout + p.stderr).replace('\r', '\n')
    return p.returncode, out


class ROM:
    def __init__(self, rom_dir):
        with open(os.path.join(rom_dir, 'ro.bin'), 'rb') as f:
            self.ro = f.read()
        with open(os.path.join(rom_dir, 'symbols.json')) as f:
            data = json.load(f)
        self.slots = data['slots']
        self.by_name = {}
        for s in data['symbols']:
            self.by_name.setdefault(s['name'], []).append(s)
        self.starts = sorted({s['value'] for s in data['symbols'] if s['value'] < len(self.ro)})

    def address(self, name):
        """A symbol's own address (one, or None if none or several)."""
        found = {s['value'] for s in self.by_name.get(name, ())}
        return found.pop() if len(found) == 1 else None

    def call_address(self, name):
        """Where other files reach it: its jump-table slot, else itself."""
        return self.slots.get(name, self.address(name))

    def extent(self, address):
        i = bisect.bisect_right(self.starts, address)
        return self.starts[i] if i < len(self.starts) else len(self.ro)


def link_symbols(listing):
    syms = {}
    with open(listing, 'rb') as f:
        text = f.read().decode('mac_roman').replace('\r', '\n')
    for line in text.split('\n'):
        m = re.match(r'^(\S+)\s+([0-9a-fA-F]+)\s*$', line)
        if m:
            syms[m.group(1)] = int(m.group(2), 16)
    return syms


def probe(source, args, rom):
    base = os.path.splitext(os.path.basename(source))[0]
    out = os.path.join(args.out, base)
    os.makedirs(out, exist_ok=True)
    obj = os.path.join(out, base + '.o')
    rc, msg = run([os.path.join(args.bin, 'ARMCpp')] + CXX_OPTIONS
                  + ['-I' + i for i in args.includes] + ['-o', obj, source])
    if rc != 0:
        print('%s: does not compile\n%s' % (source, msg))
        return False
    if args.warnings:
        print(msg.strip())
    # every reference to a function with a jump-table slot goes to the slot
    # (R5: Apple's code does so in its own file too)
    aof = AOF.read(obj)
    # the inline copies the code refers to (Common areas C$$i$...): where
    # the ROM has them, read off where the code refers to them
    places, at = {}, None
    for a in aof.areas:
        if a.is_code and not a.attributes & AREA_COMMON_DEF:
            spots = [rom.address(s.name) - s.value for s in aof.symbols
                     if s.area == a.name and s.is_defined and s.is_global and rom.address(s.name) is not None]
            if spots or at is not None:
                places[a.name] = spots[0] if spots else at
                at = places[a.name] + ((a.size + 3) & ~3)
    inline = aof.inline_copies(rom.ro, places)
    for a in [a for a in aof.areas if a.attributes & AREA_COMMON_DEF]:
        aof.drop_area(a.name)                  # vtables, inline copies: not compared
    # -zo: an area per function, in ROM order (where its function is; an
    # area without a known function follows the one before it in the
    # object), with room between them where the source leaves a function
    # out (filler areas, below)
    spot, at = {}, None
    for a in aof.areas:
        if a.is_code:
            places = [rom.address(s.name) - s.value for s in aof.symbols
                      if s.area == a.name and s.is_defined and s.is_global and rom.address(s.name) is not None]
            spot[a.name] = places[0] if places else (at or 0)
            at = spot[a.name] + ((a.size + 3) & ~3)
    gaps, at = [], None
    for j, a in enumerate(sorted(aof.areas, key=lambda a: (not a.is_code, spot.get(a.name, 0)))):
        if a.is_code:
            if at is not None and spot[a.name] > at:
                gaps.append(('C$$probe$$%04d' % j, spot[a.name] - at))
            at = spot[a.name] + ((a.size + 3) & ~3)
        aof.rename_area(a.name, 'C$$probe$$%04d$$area' % j)
    # the file's data where the ROM has it (its first global's address)
    rw_base = []
    for a in aof.areas:
        if not a.is_code:
            rw_base += [rom.address(s.name) - s.value for s in aof.symbols
                        if s.area == a.name and s.is_defined and s.is_global and rom.address(s.name) is not None]
    rw_option = ['-RW-base', '0x%X' % rw_base[0]] if rw_base else []
    for s in list(aof.symbols):
        if s.is_global and s.name in rom.slots and s.name not in inline:
            aof.redirect(s.name, 'VEC_' + s.name)
    aof.prune()
    aof.write(obj)
    defined = [s for s in aof.symbols if s.is_defined and s.is_global
               and any(a.name == s.area and a.is_code for a in aof.areas)]
    # the imports at their addresses in the ROM, as absolute symbols
    lines = ['        AREA |ROM$$Absolute|, CODE, READONLY']
    missing = []
    for name in aof.imports():
        a = rom.slots[name[4:]] if name.startswith('VEC_') and name[4:] in rom.slots else rom.address(name)
        a = inline.get(name, a)
        if a is None:
            missing.append(name)
            continue
        lines += ['        EXPORT %s' % name, '%s EQU &%08X' % (name, a)]
    if missing:
        print('%s: imports with no single address in the ROM: %s' % (source, ', '.join(missing)))
        return False
    for name, size in gaps:
        lines += ['        AREA |%s|, CODE, READONLY' % name, '        %% %d' % size]
    lines += ['        END', '']
    absolute = os.path.join(out, base + '-imports.s')
    with open(absolute, 'w') as f:
        f.write('\n'.join(lines))
    absobj = os.path.join(out, base + '-imports.o')
    rc, msg = run([os.path.join(args.bin, 'ARM6asm'), '---text=utf8', '-bigend', absolute, absobj])
    if rc != 0:
        print('%s: the imports do not assemble\n%s' % (source, msg))
        return False
    # link once to see where the functions land, then where they belong
    image = os.path.join(out, base + '.bin')
    listing = os.path.join(out, base + '.symbols')
    link = [os.path.join(args.bin, 'ARMLink'), '-BIN', '-Symbols', listing, '-o', image, obj, absobj]
    rc, msg = run(link[:2] + ['-RO-base', '0'] + rw_option + link[2:])
    if rc != 0:
        print('%s: does not link\n%s' % (source, msg))
        return False
    linked = link_symbols(listing)
    first = defined[0].name
    want = rom.address(first)
    if want is None:
        print('%s: %s is not (once) in the ROM' % (source, first))
        return False
    ro_base = want - linked[first]
    rc, msg = run(link[:2] + ['-RO-base', '0x%X' % ro_base] + rw_option + link[2:])
    if rc != 0:
        print('%s: does not link at 0x%X\n%s' % (source, ro_base, msg))
        return False
    linked = link_symbols(listing)
    with open(image, 'rb') as f:
        bytes_ = f.read()
    ok = True
    for s in sorted(defined, key=lambda s: rom.address(s.name) or 0):     # in ROM order
        at = rom.address(s.name)
        if at is None:
            print('  %-40s not (once) in the ROM' % s.name)
            ok = False
            continue
        if linked.get(s.name) != at:
            print('  %-40s lands at 0x%X, the ROM has it at 0x%X' % (s.name, linked.get(s.name, 0), at))
            ok = False
            continue
        end = rom.extent(at)
        ours = bytes_[at - ro_base:end - ro_base]
        theirs = rom.ro[at:end]
        if ours == theirs:
            print('  %-40s 0x%06X..0x%06X identical' % (s.name, at, end))
        else:
            diff = next((i for i in range(0, min(len(ours), len(theirs)), 4)
                         if ours[i:i + 4] != theirs[i:i + 4]), min(len(ours), len(theirs)))
            print('  %-40s 0x%06X..0x%06X DIFFERENT at 0x%06X: %s, ROM %s%s'
                  % (s.name, at, end, at + diff, ours[diff:diff + 4].hex(), theirs[diff:diff + 4].hex(),
                     '' if len(ours) == len(theirs) else ' (%d bytes, ROM %d)' % (len(ours), len(theirs))))
            if args.words:
                for i in range(0, max(len(ours), len(theirs)), 4):
                    if ours[i:i + 4] != theirs[i:i + 4]:
                        print('      0x%06X  ours %-8s  ROM %s' % (at + i, ours[i:i + 4].hex(), theirs[i:i + 4].hex()))
            ok = False
    return ok


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('--bin', required=True, help="Apple's tools (experimental/bin)")
    ap.add_argument('--includes', required=True, action='append',
                    help='the headers (experimental/includes; again for src/)')
    ap.add_argument('--words', action='store_true', help='list every word that differs')
    ap.add_argument('--warnings', action='store_true', help="show the compiler's messages")
    ap.add_argument('--rom', required=True, help="romsyms.py's output (ro.bin, symbols.json)")
    ap.add_argument('--out', required=True, help='where objects and images go')
    ap.add_argument('sources', nargs='+')
    args = ap.parse_args()
    rom = ROM(args.rom)
    good = 0
    for source in args.sources:
        print(os.path.basename(source))
        if probe(source, args, rom):
            good += 1
    print('%d of %d sources identical to the ROM' % (good, len(args.sources)))
    return 0 if good == len(args.sources) else 1


if __name__ == '__main__':
    sys.exit(main())
