#!/bin/zsh
# run.sh <rom image> <seconds> <tag>: boot a ROM in Einstein (/Applications,
# a debug build: its monitor logs to /tmp/Einstein_log.txt), quit, print the
# milestones it reached (<image>.watch, from watch.py). Points Einstein's ROM
# and flash at the test files: save ~/Library/Preferences/robowerk.com/
# einstein.prefs first, put it back after.
E=${0:a:h:h:h}; OUT=$E/build/einstein; mkdir -p $OUT
ROM=$1; SECS=$2; TAG=$3
python3 - "$ROM" "$OUT/flash-$TAG.bin" <<'PY'
import sys
p = '/Users/matt/Library/Preferences/robowerk.com/einstein.prefs'
lines = open(p).read().split('\n')
out, sect, skip = [], None, False
for l in lines:
    if l.startswith('['):
        sect = l
    if skip and l.startswith('+'):
        continue
    skip = False
    if sect in ('[./ROM]', '[./Flash]') and l.startswith('path:'):
        l = 'path:' + (sys.argv[1] if sect == '[./ROM]' else sys.argv[2])
        skip = True
    out.append(l)
open(p, 'w').write('\n'.join(out))
PY
rm -f /tmp/Einstein_log.txt
/Applications/Einstein.app/Contents/MacOS/Einstein > $OUT/out-$TAG.txt 2>&1 &
PID=$!
sleep $SECS
osascript -e 'tell application id "org.messagepad.einstein" to quit' >/dev/null 2>&1 &
sleep 3
kill $PID 2>/dev/null; sleep 1; kill -9 $PID 2>/dev/null
cp /tmp/Einstein_log.txt $OUT/log-$TAG.txt
python3 ${0:a:h}/hits.py $OUT/log-$TAG.txt ${ROM:r}.watch
