#!/usr/bin/env python3
"""Try forms of a source until a function is identical (R6).

    variants.py <source> <variants file>

When a function is the ROM's but for registers or the order of
instructions, the cause is often a detail of the source anywhere in the
function. The variants file is a Python list of (function name prefix,
[(name, (old text, new text)), ...]); for each function, each variant is
tried on the current source (probe.py, every word compared); one that
lowers the function's differences (then the file's) is kept. The source
is left in its best form.
"""

import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))


def score(src, text, func):
    with open(src, 'w') as f:
        f.write(text)
    out = subprocess.run([sys.executable, os.path.join(HERE, 'probe.py'), '--bin', 'bin', '--includes', 'includes',
                          '--includes', 'src', '--rom', 'build/rom', '--out', '/tmp/variants', '--words', '--each', src],
                         capture_output=True, text=True).stdout
    if 'does not compile' in out:
        return 999, 999
    n, total, on = 0, 0, False
    for line in out.split('\n'):
        if line.startswith('  ') and not line.startswith('      '):
            on = line.startswith('  ' + func) and 'identical' not in line
            total += 'identical' not in line
            n += on
        elif on and line.startswith('      0x'):
            n += 1
    return n, total


def main():
    src, variants = sys.argv[1:3]
    with open(src) as f:
        current = f.read()
    with open(variants) as f:
        groups = eval(f.read())
    for func, tries in groups:
        best = score(src, current, func)
        print('%s: %s' % (func, best), flush=True)
        for name, (old, new) in tries:
            if old not in current:
                print('  %s: not in the source' % name)
                continue
            text = current.replace(old, new)
            s = score(src, text, func)
            print('  %s: %s%s' % (name, s, ' (kept)' if s < best else ''), flush=True)
            if s < best:
                best, current = s, text
    with open(src, 'w') as f:
        f.write(current)
    return 0


if __name__ == '__main__':
    sys.exit(main())
