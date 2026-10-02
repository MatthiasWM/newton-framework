# bisect.py <low> <high> [--skip-last]: the last symbol in [low, high) before
# which 16 bytes of padding keep the ROM from booting in Einstein (RunInitScripts
# not reached), and the first before which it boots
import json, os, subprocess, sys
S = os.path.dirname(os.path.abspath(__file__))
d = json.load(open(os.path.join(S, '..', '..', 'build', 'rom', 'symbols.json')))
lo_a, hi_a = int(sys.argv[1], 0), int(sys.argv[2], 0)
cand = sorted(set(s['value'] for s in d['symbols'] if s['class'] != 'abs' and lo_a <= s['value'] < hi_a and s['value'] % 4 == 0))
names = {}
for s in d['symbols']:
    names.setdefault(s['value'], s['name'])
def boots(a):
    out = subprocess.run([S + '/boot.sh', '0x%X' % a, '40'], capture_output=True, text=True).stdout
    ok = 'RunInitScripts' in out
    print('0x%X %-50s %s   %s' % (a, names[a][:50], 'BOOTS' if ok else 'fails', out.strip()), flush=True)
    return ok
lo, hi = 0, len(cand) - 1   # cand[lo] fails (known), cand[hi]: check
if '--skip-last' not in sys.argv and not boots(cand[hi]):
    print('even the last fails'); sys.exit()
while hi - lo > 1:
    mid = (lo + hi) // 2
    if boots(cand[mid]): hi = mid
    else: lo = mid
print('last failing: 0x%X %s; first booting: 0x%X %s' % (cand[lo], names[cand[lo]], cand[hi], names[cand[hi]]))
