#!/usr/bin/env python3
"""The ROM's parts and symbols, from Apple's linker output (R1).

    romsyms.py <newton-re> <out-dir>

Reads the debug ROM image in newton-re (`DebugRom/MP2x00 US/Senior
CirrusNoDebug image`, an AIF file) with newton-re's own library
(`tools/newton-rom/newtonrom`, used where it is: not copied), and writes to
<out-dir>:

    ro.bin        the read-only area (address 0 up to Image$$RO$$Limit)
    rw.bin        the read-write area (loaded after RO, runs at its data base)
    rex.bin       the ROM extension (`... high`), appended at ROM$$Size
    symbols.json  the symbol table: every symbol (name, value, class,
                  global), and per function its jump-table slot (the
                  virtual address that other files call), with the AIF
                  header's numbers and the jump table's place and size

and checks: every jump-table slot branches to the function of the same name.
"""

import json
import os
import sys


def main():
    if len(sys.argv) != 3:
        print(__doc__.strip().split('\n\n')[1])
        return 2
    newton_re, out = sys.argv[1], sys.argv[2]
    sys.path.insert(0, os.path.join(newton_re, 'tools', 'newton-rom'))
    from newtonrom.aif import AIFImage
    from newtonrom.symbols import read_symbols
    from newtonrom.jumptable import JumpTable, VIRTUAL_BASE, is_virtual_slot

    rom_dir = os.path.join(newton_re, 'DebugRom', 'MP2x00 US')
    image = AIFImage.from_file(os.path.join(rom_dir, 'Senior CirrusNoDebug image'))
    h = image.header
    os.makedirs(out, exist_ok=True)
    with open(os.path.join(out, 'ro.bin'), 'wb') as f:
        f.write(image.ro)
    with open(os.path.join(out, 'rw.bin'), 'wb') as f:
        f.write(image.rw)
    with open(os.path.join(rom_dir, 'Senior CirrusNoDebug high'), 'rb') as f:
        rex = f.read()
    with open(os.path.join(out, 'rex.bin'), 'wb') as f:
        f.write(rex)

    symbols = read_symbols(image)
    table = JumpTable(image)
    matched, mismatches = table.verify(symbols)

    slots = {}      # name -> virtual slot address
    entries = []
    for s in symbols:
        if is_virtual_slot(s.value, table.count):
            slots[s.name] = s.value
        else:
            entries.append({'name': s.name, 'value': s.value,
                            'class': s.sym_class, 'global': s.is_global})
    data = {
        'image': {'ro_size': h.ro_area_size, 'rw_size': h.rw_area_size,
                  'zi_size': h.zero_init_area_size, 'rw_base': h.data_base,
                  'ro_base': h.image_base, 'rex_size': len(rex)},
        'jump_table': {'rom_address': table.rom_address, 'count': table.count,
                       'virtual_base': VIRTUAL_BASE},
        'symbols': entries,
        'slots': slots,
    }
    with open(os.path.join(out, 'symbols.json'), 'w') as f:
        json.dump(data, f, indent=0)

    print('RO 0x%X, RW 0x%X (runs at 0x%08X), ZI 0x%X, REx 0x%X bytes'
          % (h.ro_area_size, h.rw_area_size, h.data_base, h.zero_init_area_size, len(rex)))
    print('%d symbols, %d jump-table slots (table at 0x%X, %d entries)'
          % (len(entries), len(slots), table.rom_address, table.count))
    print('slots that branch to the function of their name: %d, others: %d'
          % (matched, len(mismatches)))
    for entry, name in mismatches[:10]:
        print('  slot 0x%08X %s -> 0x%08X' % (entry.virtual, name, entry.target))
    return 0


if __name__ == '__main__':
    sys.exit(main())
