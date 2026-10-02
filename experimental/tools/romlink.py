#!/usr/bin/env python3
"""Assemble the ROM's sources, link them, compare with Apple's image (R2).

    romlink.py --bin <tools> --src <romasm output> --rom <romsyms output> --out <dir>

Assembles every file in files.txt with ARM6asm (in parallel; objects, .o,
are taken as they are), links them in
that order with ARMLink (the options in link.txt: an AIF image, with the
scatter file), and compares with Apple's AIF: its read-only and read-write
parts (ro.bin + rw.bin), the header's sizes, and the linker's own symbols
(ROM$$Size, Image$$root$$Base, _end, ...). Exit status 0 if all are the
same.
"""

import argparse
import json
import os
import re
import struct
import subprocess
import sys
import time
from concurrent.futures import ThreadPoolExecutor


def run(cmd):
    p = subprocess.run(cmd, capture_output=True, text=True, errors='replace')
    return p.returncode, (p.stdout + p.stderr).strip()


def build(bin_dir, src, out):
    """Assemble and link src (romasm.py's output) into out; return the AIF's
    bytes and the linker's symbols, or None (the errors printed)."""
    os.makedirs(out, exist_ok=True)
    with open(os.path.join(src, 'files.txt')) as f:
        files = [l.strip() for l in f if l.strip()]
    with open(os.path.join(src, 'link.txt')) as f:
        options = [os.path.join(src, o) if o.endswith('.txt') else o for o in f.read().split()]
    asm = os.path.join(bin_dir, 'ARM6asm')
    t = time.time()

    def assemble(name):
        if name.endswith('.o'):                    # a compiled object, as it is (R6)
            return name, os.path.join(src, name), (0, '')
        obj = os.path.join(out, os.path.splitext(name)[0] + '.o')
        return name, obj, run([asm, '---text=utf8', '-bigend', os.path.join(src, name), obj])

    with ThreadPoolExecutor(max_workers=os.cpu_count()) as pool:
        results = list(pool.map(assemble, files))
    failed = [(n, msg) for n, _, (rc, msg) in results if rc != 0]
    for n, msg in failed:
        print('%s does not assemble:\n%s' % (n, msg))
    if failed:
        return None
    print('assembled %d files in %.1f s' % (len(files), time.time() - t))
    t = time.time()
    image = os.path.join(out, 'rom.aif')
    listing = os.path.join(out, 'symbols.txt')
    rc, msg = run([os.path.join(bin_dir, 'ARMLink'), '---text=utf8'] + options
                  + ['-Symbols', listing, '-o', image] + [obj for _, obj, _ in results])
    if rc != 0:
        print('does not link:\n%s' % msg)
        return None
    print('linked in %.1f s' % (time.time() - t))
    with open(image, 'rb') as f:
        aif = f.read()
    linked = {}
    with open(listing, encoding='utf-8', errors='replace') as f:
        for line in f:
            m = re.match(r'^(\S+)\s+([0-9a-fA-F]+)\s*$', line)
            if m:
                linked[m.group(1)] = int(m.group(2), 16)
    return aif, linked


def image_parts(aif):
    """An AIF's header numbers and its RO + RW bytes."""
    (ro_size, rw_size, _, zi_size, _, image_base, _, _, data_base) = struct.unpack_from('>9I', aif, 0x14)
    return (ro_size, rw_size, zi_size, image_base, data_base), aif[0x80:0x80 + ro_size + rw_size]


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('--bin', required=True)
    ap.add_argument('--src', required=True)
    ap.add_argument('--rom', required=True)
    ap.add_argument('--out', required=True)
    args = ap.parse_args()
    built = build(args.bin, args.src, args.out)
    if built is None:
        return 1
    aif, linked = built
    header, ours = image_parts(aif)
    ro_size, rw_size, zi_size, image_base, data_base = header
    with open(os.path.join(args.rom, 'ro.bin'), 'rb') as f:
        ro = f.read()
    with open(os.path.join(args.rom, 'rw.bin'), 'rb') as f:
        rw = f.read()
    with open(os.path.join(args.rom, 'symbols.json')) as f:
        info = json.load(f)
    ok = True
    im = info['image']
    header = (ro_size, rw_size, zi_size, image_base, data_base)
    apple_header = (im['ro_size'], im['rw_size'], im['zi_size'], im['ro_base'], im['rw_base'])
    if header == apple_header:
        print('AIF header: RO 0x%X, RW 0x%X, ZI 0x%X, base 0x%X, data base 0x%X, as Apple\'s' % header)
    else:
        print('AIF header DIFFERENT: %s, Apple\'s %s' % (header, apple_header))
        ok = False
    apple = ro + rw
    if ours == apple:
        print('identical to Apple\'s image: RO 0x%X + RW 0x%X bytes' % (len(ro), len(rw)))
    else:
        n = min(len(ours), len(apple))
        first = next((i for i in range(n) if ours[i] != apple[i]), n)
        print('DIFFERENT: %d bytes, Apple\'s %d; first difference at 0x%X' % (len(ours), len(apple), first))
        ok = False
    # the linker's own symbols, against Apple's symbol table
    apple_syms = {}
    for s in info['symbols']:
        apple_syms.setdefault(s['name'], set()).add(s['value'])
    own = sorted(n for n in apple_syms if ('$$' in n or n in ('_etext', '_edata', '_end')) and n in linked)
    wrong = [n for n in own if linked[n] not in apple_syms[n]]
    print('the linker\'s own symbols in Apple\'s table: %d, at Apple\'s values: %d%s'
          % (len(own), len(own) - len(wrong),
             ''.join('\n  %s 0x%X, Apple\'s %s' % (n, linked[n], ', '.join('0x%X' % v for v in apple_syms[n]))
                     for n in wrong)))
    return 0 if ok and not wrong else 1


if __name__ == '__main__':
    sys.exit(main())
