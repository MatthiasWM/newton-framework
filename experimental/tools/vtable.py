#!/usr/bin/env python3
"""A class's vtable in the ROM: its entries, in order (R6, for headers).

    vtable.py <rom dir> CLASS [COUNT]
    vtable.py <rom dir> 0xADDRESS [COUNT]

A vtable is a run of `B function` instructions, one per virtual function in
declaration order (the destructor first, a base class's entries before the
derived class's). Given a class, the vtable is the word its constructor
stores at [this, #0]: the constructor's literals that point into the vtable
area (the read-only part below 0x22000) are taken. Each entry's target is
named from symbols.json (a jump-table slot by the function it stands for).
Vtables follow one another, so COUNT (default 32) is where to stop; the
entries of the next class read as unrelated names.
"""

import json
import os
import re
import struct
import sys


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    rom_dir, what = sys.argv[1], sys.argv[2]
    count = int(sys.argv[3]) if len(sys.argv) > 3 else 32
    with open(os.path.join(rom_dir, 'ro.bin'), 'rb') as f:
        ro = f.read()
    with open(os.path.join(rom_dir, 'symbols.json')) as f:
        info = json.load(f)
    symbols = info['symbols']
    by_address = {}
    for s in symbols:                   # slots too: Apple's table names them like the function
        by_address.setdefault(s['value'], s['name'])
    slot_names = {address: name for name, address in info['slots'].items()}
    ordered = sorted(set(s['value'] for s in symbols if s['class'] != 'abs' and s['value'] < len(ro)))

    def word(a):
        return struct.unpack_from('>I', ro, a)[0]

    if what.startswith('0x'):
        tables = [int(what, 16)]
    else:
        prefix = '__ct__%d%s' % (len(what), what)
        starts = sorted(s['value'] for s in symbols if s['name'].startswith(prefix)
                        and s['class'] != 'abs' and s['value'] < len(ro))
        if not starts:
            print('%s: no constructor in the ROM' % what)
            return 1
        tables = set()
        for start in starts:
            import bisect
            end = ordered[bisect.bisect_right(ordered, start)]
            for a in range(start, end, 4):
                w = word(a)
                if 0x10000 <= w < 0x22000 and w % 4 == 0 and (word(w) >> 24) == 0xEA:
                    tables.add(w)
        tables = sorted(tables)
    for t in tables:
        print('vtable at 0x%X' % t)
        for i in range(count):
            a = t + 4 * i
            w = word(a)
            if (w >> 24) != 0xEA:
                print('  (not a branch at 0x%X: end)' % a)
                break
            off = w & 0xFFFFFF
            if off & 0x800000:
                off -= 0x1000000
            target = a + 8 + 4 * off
            # a branch to a jump-table slot is encoded from the slot's
            # virtual address only in the jump table itself; here targets are
            # plain addresses: a slot (0x01A00000 on) or the function
            target &= 0xFFFFFFFF
            name = slot_names.get(target) or by_address.get(target) or '?'
            print('  +0x%03X  %-8s %s' % (4 * i, '0x%X' % target, name))
    return 0


if __name__ == '__main__':
    sys.exit(main())
