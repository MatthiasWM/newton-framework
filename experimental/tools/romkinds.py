#!/usr/bin/env python3
"""Which ROM words are instructions, from newtonos.s's disassembly (R3c).

    romkinds.py <newtonos.s> <out-dir>

Writes <out-dir>/kinds.bin: a byte per word of the ROM (0x800000 bytes,
so 0x200000 entries): 'i' an instruction, 'w' a data word (`.word`), 0 not
known (strings and bytes, which the disassembly shows otherwise).
newtonos.s disassembles the shipping ROM; its words are Apple's but for
the checksum block (romsyms.py's notes).
"""

import os
import re
import sys


def main():
    if len(sys.argv) != 3:
        print(__doc__.split('\n\n')[1])
        return 2
    src, out = sys.argv[1], sys.argv[2]
    kinds = bytearray(0x200000)
    inst = re.compile(r'^\t[a-z]\S*\s.*@ 0x([0-9A-F]{8}) 0x[0-9A-F]{8}')
    word = re.compile(r'^\t\.word\s+0x[0-9A-F]{8}\s+@ 0x([0-9A-F]{8})')
    n = {ord('i'): 0, ord('w'): 0}
    with open(src, errors='replace') as f:
        for line in f:
            if not line.startswith('\t'):
                continue
            m = word.match(line)
            k = ord('w')
            if not m:
                m = inst.match(line)
                k = ord('i')
            if m:
                a = int(m.group(1), 16)
                if a < 0x800000 and a % 4 == 0:
                    kinds[a // 4] = k
                    n[k] += 1
    os.makedirs(out, exist_ok=True)
    with open(os.path.join(out, 'kinds.bin'), 'wb') as f:
        f.write(kinds)
    print('%d instructions, %d data words' % (n[ord('i')], n[ord('w')]))
    return 0


if __name__ == '__main__':
    sys.exit(main())
