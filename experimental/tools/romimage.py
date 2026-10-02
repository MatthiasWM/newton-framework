#!/usr/bin/env python3
"""The shipping ROM image from the link (R3g.2).

    romimage.py --aif <rom.aif> --rom <romsyms output> -o <image> [--compare <rom image>]

Takes the linked AIF's RO and RW (ROM$$Size bytes), appends the ROM
extension (rex.bin) at ROM$$Size, fills with 0xFF to 8 MB, and fills in the
checksum block ("DIAG" "XSUM" "HERE", then gDiagType, gPhysROMAcsum ..
gPhysROMHcsum; placeholders in the link) as Apple's build did:

    gDiagType      " Q  " (the product, forQ)
    gPhysROMBcsum  the ROM's size, 0x00800000
    gPhysROMCcsum .. Hcsum  0
    gPhysROMAcsum  the sum of all the image's 32-bit words (big-endian,
                   modulo 2^32), taken with these filled in and Acsum 0

Nothing in the ROM reads the block (diagnostics and factory tools do). The
extension is laid out for its own address (its header's `start` at +0x20
and some 40,000 references inside it are absolute; a few hundred refer to
the base ROM): if ROM$$Size is not where it was, the extension would have
to be relocated (the Rex tool's work, later), so this says so and stops.

With --compare, compares with a ROM image (such as ROMData/rom.image,
whose first 64 KB a live dump gets wrong: the MMU pages them).

With --einstein, an image to test in the Einstein emulator. Einstein knows
this ROM by a few words (gROMVersion, gROMStage, gHardwareType) and then
patches it at fixed addresses (its JIT ROM patches, the virtualised
__rt_udiv, __rt_sdiv and symcmp): in a ROM where code moved, those land in
the wrong code. So gROMVersion gets a value Einstein doesn't know
(0x00020102 for 0x00020002: the ROM only reports it, through Gestalt; not
gROMStage, which Einstein's own ROM extension needs to find its machine),
and the image gets
Einstein's plain patches itself, each where its code is now (Apple's symbol
plus offset, looked up in the link's symbols.txt next to the AIF): no
debugger (gDebuggerBits 1; DebugStr and Debugger, traps, return), stdio on
(gNewtConfig), no GeoPort beacon, no calibration screen, Einstein's time
base, setting the time ignored. Einstein's native calls (the clock, time
and date functions) and its logging injections are left out: the ROM's own
code runs. The checksum is then not the shipping one.
"""

import argparse
import json
import os
import struct
import sys

SIZE = 0x800000

MOV_PC_LR = 0xE1A0F00E
# Einstein's plain patches for this ROM (its JIT/Generic/TJITGenericROMPatch.cpp),
# by Apple's symbol and offset: (symbol, offset, word)
EINSTEIN = [
    ('gDebuggerBits', 0, 1),
    ('gNewtConfig', 0, 0x00000002 | 0x00000200 | 0x00008000),
    ('CheckTabletCalibration__Fv', 0x1C, 0xEA000009),
    ('BeaconDetect__17TGeoPortDebugLinkFl', 0, 0xE3A00000),
    ('BeaconDetect__17TGeoPortDebugLinkFl', 4, MOV_PC_LR),
    ('DebugStr', 0, MOV_PC_LR),
    ('Debugger', 0, MOV_PC_LR),
    ('SYMordinal', 0x4DC, 218799360),
    ('SYMordinal', 0x524, 218799360),
    ('SYMoptionsslip', 0xBC, 218799360),
    ('FSetSysAlarm', 0x19C, 3281990400),
    ('FSetTimeInSeconds', 0, MOV_PC_LR),
]


def einstein(image, aif_path, names):
    """Hide the ROM from Einstein's own patches, apply them where the code is."""
    import re
    linked = {}
    with open(os.path.join(os.path.dirname(aif_path), 'symbols.txt'), errors='replace') as f:
        for line in f:
            m = re.match(r'^(\S+)\s+([0-9a-fA-F]+)\s*$', line)
            if m:
                linked[m.group(1)] = int(m.group(2), 16)
    assert struct.unpack_from('>I', image, names['gROMVersion'])[0] == 0x00020002
    struct.pack_into('>I', image, names['gROMVersion'], 0x00020102)
    for name, offset, word in EINSTEIN:
        struct.pack_into('>I', image, linked.get(name, names[name]) + offset, word)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('--aif', required=True)
    ap.add_argument('--rom', required=True)
    ap.add_argument('-o', '--out', required=True)
    ap.add_argument('--compare')
    ap.add_argument('--einstein', action='store_true',
                    help='an image to test in Einstein (see above)')
    args = ap.parse_args()
    with open(args.aif, 'rb') as f:
        aif = f.read()
    ro_size, rw_size = struct.unpack_from('>2I', aif, 0x14)
    base = aif[0x80:0x80 + ro_size + rw_size]
    with open(os.path.join(args.rom, 'rex.bin'), 'rb') as f:
        rex = f.read()
    with open(os.path.join(args.rom, 'symbols.json')) as f:
        info = json.load(f)
    names = {s['name']: s['value'] for s in info['symbols']}
    rex_start = struct.unpack_from('>I', rex, 0x20)[0]
    if len(base) != rex_start:
        print('ROM$$Size is 0x%X, the extension is laid out for 0x%X: it would have to be '
              'relocated (not yet)' % (len(base), rex_start))
        return 1
    image = bytearray(base + rex)
    image += b'\xff' * (SIZE - len(image))
    if args.einstein:
        einstein(image, args.aif, names)
    block = names['gDiagType']
    struct.pack_into('>9I', image, block, 0x20512020, 0, SIZE, 0, 0, 0, 0, 0, 0)
    total = sum(struct.unpack('>%dI' % (SIZE // 4), bytes(image))) & 0xFFFFFFFF
    struct.pack_into('>I', image, names['gPhysROMAcsum'], total)
    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
    with open(args.out, 'wb') as f:
        f.write(image)
    print('%s: 8 MB, ROM$$Size 0x%X, the extension 0x%X bytes, checksum 0x%08X'
          % (args.out, len(base), len(rex), total))
    if args.compare:
        with open(args.compare, 'rb') as f:
            other = f.read()
        diff = [a for a in range(0, SIZE, 4) if image[a:a + 4] != other[a:a + 4]]
        low = [a for a in diff if a < 0x10000]
        print('against %s: %d words differ, %d of them in the first 64 KB%s'
              % (args.compare, len(diff), len(low),
                 '' if len(diff) == len(low) else '; others from 0x%X' % next(a for a in diff if a >= 0x10000)))
    return 0


if __name__ == '__main__':
    sys.exit(main())
