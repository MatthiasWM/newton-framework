#!/usr/bin/env python3
"""The shift test (R3b): does everything follow when the ROM grows?

    shifttest.py --bin <tools> --rom <romsyms output> --out <dir> [--at ADDRESS] [--bytes N]

Builds the ROM twice from romasm.py's source: as it is, and with N bytes of
padding put in before the symbol at ADDRESS. Everything after that point
moves by N; the read-write data runs where it did. Then, word by word (each
word mapped to where it is in the padded image):

- branches (B, BL) in code whose one end moved and the other did not (a
  jump-table slot does not move): they must be encoded anew; an
  instruction by newtonos.s (romkinds.py) is a branch, a data word with
  a branch's bits is listed apart (the public jump table at 0x13000 is
  such: real branches the disassembly shows as `.word`; the others are
  data, e.g. in the NewtonScript area);
- words whose value is an address in the part that moved: they must grow
  by N (some are data that only looks like an address: candidates);
- anything else that changed: an error.

It prints how many of each followed and how many did not, and writes the
ones that did not to <out>/report.txt (address, the symbol it is in, the
word, what it should be).
"""

import argparse
import bisect
import json
import os
import struct
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from romlink import build, image_parts  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))


def branch_target(word, pc):
    imm = word & 0xFFFFFF
    if imm & 0x800000:
        imm -= 0x1000000
    return (pc + 8 + imm * 4) & 0xFFFFFFFF


def is_branch(word):
    return (word & 0x0E000000) == 0x0A000000 and (word >> 28) != 0xF


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('--bin', required=True)
    ap.add_argument('--rom', required=True)
    ap.add_argument('--out', required=True)
    ap.add_argument('--at', type=lambda s: int(s, 0), default=0x188D38,
                    help='the symbol the padding goes before (default: TPictureView::ClassID)')
    ap.add_argument('--bytes', type=lambda s: int(s, 0), default=16)
    args = ap.parse_args()
    with open(os.path.join(args.rom, 'symbols.json')) as f:
        info = json.load(f)
    ro_size = info['image']['ro_size']
    P, N = args.at, args.bytes

    images = {}
    for name, pad in (('base', None), ('padded', '0x%X:%d' % (P, N))):
        src = os.path.join(args.out, name, 'src')
        cmd = [sys.executable, os.path.join(HERE, 'romasm.py'), args.rom, src]
        if pad:
            cmd += ['--pad', pad]
        p = subprocess.run(cmd, capture_output=True, text=True)
        if p.returncode != 0:
            print(p.stdout + p.stderr)
            return 1
        print('%s:' % name, end=' ')
        built = build(args.bin, src, os.path.join(args.out, name, 'obj'))
        if built is None:
            return 1
        images[name] = image_parts(built[0])
    (base_header, base), (pad_header, padded) = images['base'], images['padded']
    if pad_header[0] != base_header[0] + N:
        print('the padded RO is 0x%X bytes, not 0x%X + %d' % (pad_header[0], base_header[0], N))
        return 1

    # which symbol a word is in, and whether that is code
    syms = sorted((s['value'], s['name'], s['class']) for s in info['symbols'] if s['value'] < ro_size)
    starts = [s[0] for s in syms]

    def where(a):
        i = bisect.bisect_right(starts, a) - 1
        if i < 0:
            return '(start)', 'data'
        v, n, c = syms[i]
        return '%s+0x%X' % (n, a - v), c

    rw_size = info['image']['rw_size']

    def moves(addr):
        # in the read-only part, and its end: where the RW data is loaded,
        # and the image's end (Load$$root$$Base, ROM$$Size)
        return P <= addr <= ro_size + rw_size

    with open(os.path.join(args.rom, 'kinds.bin'), 'rb') as f:
        kinds = f.read()
    counts = {k: [0, 0] for k in ('branch', 'pointer', 'branch-like data', 'jump table')}
    jt = info['jump_table']
    jt_low, jt_high = jt['rom_address'], jt['rom_address'] + 4 * jt['count']
    names = {s_['name']: s_['value'] for s_ in info['symbols']}
    pjt_low, pjt_high = names['gROMPublicJumpTable'], names['gROMPublicJumpTableEnd']

    def virtual(i):
        page, slot = divmod(i, 32)
        return 0x01A00000 + page * 0x1000 + (page % 32) * 0x80 + slot * 4
    errors = 0
    report = []
    total = len(base) // 4
    for i in range(total):
        a = i * 4                              # in the base image (RO, then RW)
        a2 = a + N if a >= P else a            # in the padded image
        w = struct.unpack_from('>I', base, a)[0]
        w2 = struct.unpack_from('>I', padded, a2)[0]
        expected = None
        kind = None
        name, cls = where(a) if a < ro_size else ('(RW data)', 'data')
        if jt_low <= a < jt_high:             # the jump table: from virtual addresses
            t = branch_target(w, virtual((a - jt_low) // 4))
            expected = (w & 0xFF000000) | ((w + (N // 4 if moves(t) else 0)) & 0xFFFFFF)
            if w2 == expected:
                counts['jump table'][0] += 1
            else:
                counts['jump table'][1] += 1
                report.append('jtable   0x%07X %-60s 0x%08X became 0x%08X, should be 0x%08X'
                              % (a, name, w, w2, expected))
            continue
        if pjt_low <= a < pjt_high:           # the public jump table: to slots, which stay
            if w2 != w:
                errors += 1
                report.append('ERROR    0x%07X %-60s 0x%08X became 0x%08X' % (a, name, w, w2))
            continue
        if a < ro_size and cls == 'code' and is_branch(w) and kinds[a // 4] != ord('i'):
            t = branch_target(w, a)
            if moves(a) != moves(t):
                kind = 'branch-like data'
                delta = (N if moves(t) else -N) // 4
                expected = (w & 0xFF000000) | ((w + delta) & 0xFFFFFF)
            if w2 != w and w2 != expected:
                errors += 1
                report.append('ERROR    0x%07X %-60s 0x%08X became 0x%08X' % (a, name, w, w2))
                continue
            if kind and w2 == w:
                counts[kind][1] += 1
                report.append('%-8s 0x%07X %-60s 0x%08X (a branch to 0x%X if it is one)'
                              % ('bdata', a, name, w, t))
            elif kind:
                counts[kind][0] += 1
            continue
        if a < ro_size and cls == 'code' and is_branch(w):
            t = branch_target(w, a)
            pc_moves, t_moves = moves(a), moves(t)
            if pc_moves != t_moves:
                kind = 'branch'
                delta = (N if t_moves else -N) // 4
                expected = (w & 0xFF000000) | ((w + delta) & 0xFFFFFF)
        elif moves(w):
            kind = 'pointer'
            expected = w + N
        if kind:
            if w2 == expected:
                counts[kind][0] += 1
            elif w2 == w:
                counts[kind][1] += 1
                report.append('%-8s 0x%07X %-60s 0x%08X should be 0x%08X' % (kind, a, name, w, expected))
            else:
                errors += 1
                report.append('ERROR    0x%07X %-60s 0x%08X became 0x%08X, should be 0x%08X'
                              % (a, name, w, w2, expected))
        elif w2 != w:
            errors += 1
            report.append('ERROR    0x%07X %-60s 0x%08X became 0x%08X' % (a, name, w, w2))
    with open(os.path.join(args.out, 'report.txt'), 'w') as f:
        f.write('\n'.join(report) + '\n')
    print('padding: %d bytes before 0x%X (%s)' % (N, P, where(P)[0]))
    print('branches that cross it:   %6d followed, %6d did not' % tuple(counts['branch']))
    print('data words shaped so:     %6d followed, %6d did not (data)'
          % tuple(counts['branch-like data']))
    print('jump-table entries:       %6d right,    %6d wrong' % tuple(counts['jump table']))
    print('addresses past it:        %6d followed, %6d did not (candidates)' % tuple(counts['pointer']))
    print('other changes (errors):   %6d' % errors)
    print('the ones that did not: %s' % os.path.join(args.out, 'report.txt'))
    return 0 if errors == 0 and counts['branch'][1] == 0 and counts['pointer'][1] == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
