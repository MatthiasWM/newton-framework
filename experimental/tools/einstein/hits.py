import sys, re, json
w = {int(k, 16): v for k, v in json.load(open(sys.argv[2])).items()}
seen = {}
for l in open(sys.argv[1], errors='replace'):
    m = re.match(r'Watch at ([0-9A-F]+)', l)
    if m:
        n = w.get(int(m.group(1), 16), m.group(1))
        seen[n] = seen.get(n, 0) + 1
print(' '.join('%s(%d)' % (k, v) for k, v in seen.items()))
