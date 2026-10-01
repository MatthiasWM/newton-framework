#!/usr/bin/env python3
"""Assemble the ROM's sources, link them, compare with Apple's image (R2).

    romlink.py --bin <tools> --src <romasm output> --rom <romsyms output> --out <dir>

Assembles every file in files.txt with ARM6asm (in parallel), links them in
that order with ARMLink (-BIN, the bases in link.txt), and compares the
image with Apple's read-only and read-write parts (ro.bin + rw.bin). Exit
status 0 if they are the same, byte for byte.
"""

import argparse
import os
import subprocess
import sys
import time
from concurrent.futures import ThreadPoolExecutor


def run(cmd):
    p = subprocess.run(cmd, capture_output=True, text=True, errors='replace')
    return p.returncode, (p.stdout + p.stderr).strip()


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('--bin', required=True)
    ap.add_argument('--src', required=True)
    ap.add_argument('--rom', required=True)
    ap.add_argument('--out', required=True)
    args = ap.parse_args()
    os.makedirs(args.out, exist_ok=True)
    with open(os.path.join(args.src, 'files.txt')) as f:
        files = [l.strip() for l in f if l.strip()]
    with open(os.path.join(args.src, 'link.txt')) as f:
        bases = f.read().split()

    asm = os.path.join(args.bin, 'ARM6asm')
    t = time.time()

    def assemble(name):
        obj = os.path.join(args.out, os.path.splitext(name)[0] + '.o')
        return name, obj, run([asm, '---text=utf8', '-bigend', os.path.join(args.src, name), obj])

    with ThreadPoolExecutor(max_workers=os.cpu_count()) as pool:
        results = list(pool.map(assemble, files))
    failed = [(n, msg) for n, _, (rc, msg) in results if rc != 0]
    for n, msg in failed:
        print('%s does not assemble:\n%s' % (n, msg))
    if failed:
        return 1
    print('assembled %d files in %.1f s' % (len(files), time.time() - t))

    t = time.time()
    image = os.path.join(args.out, 'rom.bin')
    rc, msg = run([os.path.join(args.bin, 'ARMLink'), '---text=utf8', '-BIN'] + bases
                  + ['-Symbols', os.path.join(args.out, 'symbols.txt'), '-o', image]
                  + [obj for _, obj, _ in results])
    if rc != 0:
        print('does not link:\n%s' % msg)
        return 1
    print('linked in %.1f s' % (time.time() - t))

    with open(image, 'rb') as f:
        ours = f.read()
    with open(os.path.join(args.rom, 'ro.bin'), 'rb') as f:
        ro = f.read()
    with open(os.path.join(args.rom, 'rw.bin'), 'rb') as f:
        rw = f.read()
    apple = ro + rw
    if ours == apple:
        print('identical to Apple\'s image: RO 0x%X + RW 0x%X bytes' % (len(ro), len(rw)))
        return 0
    n = min(len(ours), len(apple))
    first = next((i for i in range(n) if ours[i] != apple[i]), n)
    print('DIFFERENT: %d bytes, Apple\'s %d; first difference at 0x%X' % (len(ours), len(apple), first))
    return 1


if __name__ == '__main__':
    sys.exit(main())
