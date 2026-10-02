#!/usr/bin/env python3
"""Code and literals in the ROM, found by following the code (R3d).

    romcode.py <rom-dir> [--compare kinds.bin]

Starts at the exception vectors (0x00..0x1C: the CPU jumps there), at
every function the jump table names (each slot's target), at
every code symbol that looks like a function (below), at the static
constructors and destructors (the pointers between C$$ctorvec$$Base and
$$Limit, C$$dtorvec$$Base and $$Limit), and at every compiler prologue not
yet reached (`MOV ip, sp` then `STMDB sp!, {..., fp, ip, lr, pc}`:
functions without a symbol), and follows the
code from there; then again from what that found: function
pointers (a literal whose value is where a code symbol starts) and the
targets of vtables (runs of unconditional `B` not reached as code, each
to a function or a jump-table slot), until nothing new turns up. Such a
target counts only if it looks like a function: a mangled C++ function
name, a native function's name (`F` and a capital: `FGetVariable`, which
NewtonScript reaches through the native table), or the compiler's
prologue (`MOV ip, sp`) as its first word (Apple's
table calls data in code areas code too: parser tables, trigram data). Each
instruction reached is code; a `BL`'s
target is another function to follow; a `B`'s target is followed too
(and, if unconditional, ends this path); a return (`MOV pc, ...`,
`LDM ... {..., pc}`, `LDR pc, ...`) or any other unconditional write to
pc ends it, except a switch (`ADD pc, pc, Rn, LSL #2`), whose table of
branches follows it (the last case is not a branch but its code, right
after the table); a write to pc right after `MOV lr, pc` is a call
(virtual calls: `MOV lr, pc; ADD pc, r1, #n`, `LDR pc, ...`), so the path
goes on after it; so does an undefined instruction (a trap, as the jump
table's fillers have), and the start of the next symbol in Apple's table
(compiled code never runs from one function into the next). A word a PC-relative `LDR` loads is a literal; an
address an `ADR` (ADD/SUB Rd, pc, #n) makes is data the code refers to.

With --compare, newtonos.s's instructions (romkinds.py) inside a function
we followed (or a symbol whose first word we reached as code), where we
did not get to, are taken as code too ('n'): code
after a return that no branch we know reaches (computed jumps, tables of
addresses, callbacks); never in a symbol we did not follow as a function.

Writes <rom-dir>/code.bin, a byte per word of the read-only part: 'c'
code, 'n' code by newtonos.s inside a function, 'l' a literal word, 'd'
data the code points at (ADR, byte loads), 'v' a vtable entry, 0 not
reached. With --compare, lists where newtonos.s's marks
(romkinds.py) disagree.
"""

import argparse
import json
import os
import re
import struct
import sys
from collections import Counter


def decode_target(w, pc):
    imm = w & 0xFFFFFF
    if imm & 0x800000:
        imm -= 0x1000000
    return pc + 8 + imm * 4


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('rom')
    ap.add_argument('--compare')
    args = ap.parse_args()
    with open(os.path.join(args.rom, 'ro.bin'), 'rb') as f:
        ro = f.read()
    with open(os.path.join(args.rom, 'symbols.json')) as f:
        info = json.load(f)
    size = len(ro)
    nwords = size // 4
    words = struct.unpack('>%dI' % nwords, ro[:nwords * 4])
    mark = bytearray(nwords)
    C, L, D = ord('c'), ord('l'), ord('d')

    # the functions the jump table names
    low = {}
    for s in info['symbols']:
        low.setdefault(s['name'], set()).add(s['value'])
    seeds = set()
    for name in info['slots']:
        for v in low.get(name, ()):
            if 0 <= v < size and v % 4 == 0:
                seeds.add(v)
    seeds |= set(range(0, 0x20, 4))      # the exception vectors
    work = sorted(seeds)
    functions = set(seeds)
    counts = Counter()
    V = ord('v')
    # where symbols start (a path stops there), and which start code
    starts = set(s['value'] for s in info['symbols'] if s['class'] != 'abs' and 0 <= s['value'] < size)
    code_starts = set(s['value'] for s in info['symbols'] if s['class'] == 'code' and 0 <= s['value'] < size
                      and s['value'] % 4 == 0)
    slots = set(info['slots'].values())
    mangled = re.compile(r'__(Q\d+_)?(\d+[A-Za-z_]\w*?)*S?C?F')
    native = re.compile(r'^F[A-Z][A-Za-z0-9]*$')
    name_at = {}
    for s_ in info['symbols']:
        name_at.setdefault(s_['value'], []).append(s_['name'])

    def looks_like_function(a):
        if a in seeds:
            return True
        if any(mangled.search(n) or native.match(n) for n in name_at.get(a, ())):
            return True
        return 0 <= a < size and a % 4 == 0 and words[a // 4] == 0xE1A0C00D

    def follow():
      while work:
          pc = start = work.pop()
          table = False                      # in a switch's table of branches
          while 0 <= pc < size:
              i = pc // 4
              if mark[i] == C:
                  break                      # been here
              if pc in starts and pc != start:
                  counts['stopped where the next symbol starts'] += 1
                  break
              if mark[i] == L:
                  counts['ran into a literal'] += 1
                  break
              w = words[i]
              cond = w >> 28
              if cond == 0xF:
                  counts['stopped at an undefined instruction'] += 1
                  break
              if (w >> 25) & 7 == 3 and w & 0x10:          # undefined: a trap
                  mark[i] = C
                  counts['ended at a trap'] += 1
                  break
              mark[i] = C
              # a write to pc right after MOV lr, pc is a call: the path goes on
              always = cond == 0xE and not (i > 0 and words[i - 1] == 0xE1A0E00F and mark[i - 1] == C)
              op = (w >> 25) & 7
              if table and op != 5:
                  table = False      # the table's last case is its code, right after the branches
              if op == 5:                                    # B, BL
                  t = decode_target(w, pc)
                  if 0 <= t < size:
                      if w & 0x01000000:
                          if t not in functions:
                              functions.add(t)
                              work.append(t)
                      else:
                          work.append(t)
                  if table:
                      pc += 4
                      continue
                  if always and not (w & 0x01000000):
                      break
              elif op in (2, 3) and not (op == 3 and w & 0x10):    # LDR/STR
                  load = w & 0x00100000
                  rn = (w >> 16) & 15
                  rd = (w >> 12) & 15
                  if load and rn == 15 and op == 2 and (w & 0x01000000):
                      a = pc + 8 + ((w & 0xFFF) if w & 0x00800000 else -(w & 0xFFF))
                      if 0 <= a < size:
                          if not (w & 0x00400000) and a % 4 == 0:
                              if mark[a // 4] != C:
                                  mark[a // 4] = L
                          elif mark[a // 4] == 0:
                              mark[a // 4] = D
                  if load and rd == 15 and always:
                      break                                  # LDR pc, ...
              elif op == 4:                                  # LDM/STM
                  if (w & 0x00100000) and (w & 0x8000) and always:
                      break                                  # LDM ..., {.., pc}
              elif op in (0, 1):                             # data processing (or multiply etc.)
                  if op == 0 and (w & 0x90) == 0x90:
                      pass                                   # multiply, swap, halfword: no pc
                  else:
                      opcode = (w >> 21) & 15
                      rn = (w >> 16) & 15
                      rd = (w >> 12) & 15
                      if rn == 15 and op == 1 and opcode in (2, 4):     # ADR
                          imm = w & 0xFF
                          rot = ((w >> 8) & 15) * 2
                          imm = ((imm >> rot) | (imm << (32 - rot))) & 0xFFFFFFFF
                          a = pc + 8 + (imm if opcode == 4 else -imm)
                          if 0 <= a < size and mark[a // 4] == 0:
                              mark[a // 4] = D
                      if rd == 15 and opcode == 4 and rn == 15 and op == 0 and not always:
                          table = True                       # ADDLS pc, pc, Rn, LSL #2
                      if rd == 15 and opcode not in (8, 9, 10, 11) and always:
                          if opcode == 4 and rn == 15 and op == 0:
                              table = True                   # ADD pc, pc, Rn, LSL #2: a switch
                          else:
                              break
              pc += 4

    vec = {s_['name']: s_['value'] for s_ in info['symbols']}
    for table in ('C$$ctorvec', 'C$$dtorvec'):
        for a in range(vec[table + '$$Base'], vec[table + '$$Limit'], 4):
            f = words[a // 4]
            mark[a // 4] = D
            if 0 <= f < size and f not in functions:
                functions.add(f)
                work.append(f)
                counts['static constructors and destructors'] += 1
    for i in range(nwords - 1):
        if words[i] == 0xE1A0C00D and (words[i + 1] & 0xFFFFD800) == 0xE92DD800 and 4 * i not in functions:
            functions.add(4 * i)
            work.append(4 * i)
            counts['prologues (functions without a symbol, or not yet found)'] += 1
    for a in sorted(code_starts):
        if a not in functions and looks_like_function(a):
            functions.add(a)
            work.append(a)
            counts['symbols that look like functions'] += 1
    follow()
    found = Counter()
    while True:
        new = []
        # function pointers: literals that point at the start of a code symbol
        for i in range(nwords):
            if (mark[i] == L and words[i] in code_starts and words[i] not in functions
                    and looks_like_function(words[i])):
                functions.add(words[i])
                new.append(words[i])
                found['function pointers'] += 1
        # vtables: runs of unconditional B, not code, to functions or slots
        i = 0
        while i < nwords:
            run = 0
            while (i + run < nwords and mark[i + run] in (0, V) and words[i + run] >> 24 == 0xEA):
                t = decode_target(words[i + run], 4 * (i + run))
                if not (t in slots or (t in code_starts and looks_like_function(t))):
                    break
                run += 1
            if run >= 2:
                for k in range(i, i + run):
                    if mark[k] == 0:
                        mark[k] = V
                        found['vtable entries'] += 1
                    t = decode_target(words[k], 4 * k)
                    if 0 <= t < size and t not in functions:
                        functions.add(t)
                        new.append(t)
                        found['vtable targets'] += 1
                i += run
            else:
                i += 1
        if not new:
            break
        work.extend(new)
        follow()
    for k, v in found.items():
        print('  found by %s: %d' % (k, v))

    if args.compare:
        with open(args.compare, 'rb') as f:
            kinds = f.read()
        followed = sorted(a for a in functions if a in starts)
        all_starts = sorted(starts)
        import bisect
        for i in range(nwords):
            if mark[i] == 0 and kinds[i] == ord('i'):
                a = 4 * i
                k = bisect.bisect_right(all_starts, a) - 1
                if k >= 0 and (all_starts[k] in functions or mark[all_starts[k] // 4] == C):
                    mark[i] = ord('n')
                    counts['code by newtonos.s inside a followed function'] += 1

    with open(os.path.join(args.rom, 'code.bin'), 'wb') as f:
        f.write(mark)
    n = Counter(mark)
    print('functions followed: %d (from %d in the jump table)' % (len(functions), len(seeds)))
    print('code words: %d, literals: %d, data the code points at: %d, vtable entries: %d, not reached: %d'
          % (n[C], n[L], n[D], n[V], n[0]))
    for k, v in counts.items():
        print('  %s: %d times' % (k, v))
    if args.compare:
        with open(args.compare, 'rb') as f:
            kinds = f.read()
        table = Counter()
        for i in range(nwords):
            table[(chr(mark[i]) if mark[i] else '-', chr(kinds[i]) if kinds[i] else '-')] += 1
        print('ours (c code, l literal, d data, - not reached) against newtonos.s (i, w, -):')
        for (a, b), v in sorted(table.items()):
            print('  %s %s %8d' % (a, b, v))
    return 0


if __name__ == '__main__':
    sys.exit(main())
