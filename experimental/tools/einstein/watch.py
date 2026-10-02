#!/usr/bin/env python3
# watch.py <symbols.txt or symbols.json> <rom image>: write <image>.monitorrc,
# which has Einstein's monitor log each time the boot milestones run
# ("Watch at ADDR"), and <image>.watch (address -> name, for hits.py)
import sys, json, re, os
NAMES = '''ROMBoot InitGlobalsThatLiveAcrossReboot InitKernelHeapArea InitToolbox InitScreen__16TQDLibraryDriverFv
InitObjects__Fv InitInterpreter__Fv CheckTabletCalibration__Fv DrawSplashScreen__9TNotebookFv
InitToolbox__9TNotebookFv RunInitScripts__Fv UserInit__Fv PowerOffSystem__Fv PowerOffSystem__16TVoyagerPlatformFv
PowerOffAndReboot__Fl WarmBoot ResumeImage SleepTask__Fv PowerOffSystemKernelGlue__Fv InitPSSManager__FUlT1'''.split()
src, image = sys.argv[1:3]
addr = {}
if src.endswith('.json'):
    for s in json.load(open(src))['symbols']:
        if s['value'] < 0x800000: addr.setdefault(s['name'], s['value'])
else:
    for l in open(src, errors='replace'):
        m = re.match(r'^(\S+)\s+([0-9a-fA-F]+)\s*$', l)
        if m and int(m.group(2), 16) < 0x800000: addr.setdefault(m.group(1), int(m.group(2), 16))
rc = os.path.splitext(image)[0] + '.monitorrc'
with open(rc, 'w') as f:
    for n in NAMES:
        f.write('# %s\nwatch 0 %X\n' % (n, addr[n]))
with open(os.path.splitext(image)[0] + '.watch', 'w') as f:
    json.dump({'%X' % addr[n]: n for n in NAMES}, f)
print(rc)
