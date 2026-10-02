"""ARM Object Format (AOF), as Apple's tools write it: reading and writing (R4).

An AOF file is a chunk file: the word 0xC3CBC6C5 (its byte order is the
file's), the number of chunk slots and of chunks used, then per chunk an
8-character name, a file offset and a size. The chunks: OBJ_HEAD (the
header and the area headers), OBJ_AREA (each area's bytes, followed by its
relocations), OBJ_IDFN (the tool that made it), OBJ_SYMT (the symbols),
OBJ_STRT (the strings: a size word, then strings ending in 0, addressed by
offset from the table's start).

Relocations (Apple's tools write only the second form, bit 31 set): an
offset into the area, then a word with the symbol's or area's index in
bits 0..23, the field type in 24..25 (0 byte, 1 halfword, 2 word, 3 an
instruction), PC-relative in bit 26 and, in bit 27, whether the index is a
symbol's (1) or an area's (0).

Writing: `to_bytes()` lays the chunks out in the file's own order with its
own chunk slots, so an object read and written unchanged comes out byte
for byte (checked: `aof.py --check FILE...`). Strings keep their offsets;
new names go at the end of the table. What can be changed: an area's name
(`rename_area`: where the linker puts it, as it sorts areas by name), a
symbol's name (`rename_symbol`), references to a symbol sent to another
(`redirect`: calls to a function sent to its jump-table slot), an area
dropped (`drop_area`: its symbols become imports, so references to them
go to a definition elsewhere; relocations by area index are renumbered).
"""

import argparse
import struct
import sys
from dataclasses import dataclass, field
from typing import List, Optional

CHUNK_FILE_ID = 0xC3CBC6C5

# symbol attributes
SYM_DEFINED = 0x01
SYM_GLOBAL = 0x02
SYM_ABSOLUTE = 0x04
SYM_COMMON = 0x40

# relocations (the second form)
REL_TYPE2 = 0x80000000
REL_SYMBOL = 0x08000000
REL_INDEX = 0x00FFFFFF

# area attributes (in the word with the alignment, bits 8 and up)
AREA_ABSOLUTE = 0x0100
AREA_CODE = 0x0200
AREA_COMMON_DEF = 0x0400
AREA_COMMON_REF = 0x0800
AREA_ZERO_INIT = 0x1000
AREA_READ_ONLY = 0x2000


@dataclass
class Area:
    name: str
    attributes: int
    alignment: int
    size: int
    num_relocations: int
    base: int
    data: bytes = b''
    relocations: List[tuple] = field(default_factory=list)   # (offset, flags word)

    @property
    def is_code(self) -> bool:
        return bool(self.attributes & AREA_CODE)


@dataclass
class Symbol:
    name: str
    attributes: int
    value: int
    area: Optional[str]

    @property
    def is_defined(self) -> bool:
        return bool(self.attributes & SYM_DEFINED)

    @property
    def is_global(self) -> bool:
        return bool(self.attributes & SYM_GLOBAL)

    @property
    def is_import(self) -> bool:
        return not self.is_defined and self.is_global


class AOF:
    def __init__(self, data: bytes):
        self.data = data
        (magic,) = struct.unpack_from('>I', data, 0)
        if magic == CHUNK_FILE_ID:
            self.e = '>'
        elif struct.unpack_from('<I', data, 0)[0] == CHUNK_FILE_ID:
            self.e = '<'
        else:
            raise ValueError('not a chunk file (AOF)')
        e = self.e
        _, max_chunks, num_chunks = struct.unpack_from(e + '3I', data, 0)
        self.chunks = {}
        for i in range(max_chunks):
            name, offset, size = struct.unpack_from(e + '8sII', data, 12 + 16 * i)
            name = name.decode('latin-1')
            if offset:
                self.chunks[name] = (offset, size)
        strt_off, strt_size = self.chunks['OBJ_STRT']
        self.strings = data[strt_off:strt_off + strt_size]
        head_off, _ = self.chunks['OBJ_HEAD']
        (self.file_type, self.version, num_areas, num_symbols,
         self.entry_area, self.entry_offset) = struct.unpack_from(e + '6I', data, head_off)
        self.areas: List[Area] = []
        for i in range(num_areas):
            name, attr, size, nrel, base = struct.unpack_from(e + '5I', data, head_off + 24 + 20 * i)
            self.areas.append(Area(self.string(name), attr >> 8 << 8, attr & 0xFF, size, nrel, base))
        area_off, _ = self.chunks['OBJ_AREA']
        pos = area_off
        for a in self.areas:
            if not a.attributes & AREA_ZERO_INIT:
                a.data = data[pos:pos + a.size]
                pos += a.size
            for _ in range(a.num_relocations):
                a.relocations.append(struct.unpack_from(e + '2I', data, pos))
                pos += 8
        self.symbols: List[Symbol] = []
        if 'OBJ_SYMT' in self.chunks:
            symt_off, _ = self.chunks['OBJ_SYMT']
            for i in range(num_symbols):
                name, attr, value, area = struct.unpack_from(e + '4I', data, symt_off + 16 * i)
                self.symbols.append(Symbol(self.string(name), attr, value,
                                           self.string(area) if area else None))

        # the file's chunk layout, kept for writing
        _, self.max_chunks, _ = struct.unpack_from(e + '3I', data, 0)
        self.slots = [struct.unpack_from(e + '8sII', data, 12 + 16 * i) for i in range(self.max_chunks)]
        idfn = self.chunks.get('OBJ_IDFN')
        self.idfn = data[idfn[0]:idfn[0] + idfn[1]] if idfn else None
        # the strings by offset (writing adds new ones at the end)
        self.offsets = {}
        pos = 4
        while pos < len(self.strings):
            end = self.strings.find(b'\0', pos)
            if end < 0:
                break
            self.offsets.setdefault(self.strings[pos:end].decode('latin-1'), pos)
            pos = end + 1

    def string(self, offset: int) -> str:
        end = self.strings.index(b'\0', offset)
        return self.strings[offset:end].decode('latin-1')

    def offset(self, name: str) -> int:
        """Where a string is in the table; a new one is added at its end."""
        if name not in self.offsets:
            size = struct.unpack_from(self.e + 'I', self.strings, 0)[0]
            table = bytearray(self.strings[:size])
            self.offsets[name] = len(table)
            table += name.encode('latin-1') + b'\0'
            struct.pack_into(self.e + 'I', table, 0, len(table))
            self.strings = bytes(table)
        return self.offsets[name]

    # changes (R5, R6)

    def rename_area(self, old: str, new: str):
        for a in self.areas:
            if a.name == old:
                a.name = new
        for s in self.symbols:
            if s.area == old:
                s.area = new

    def rename_symbol(self, old: str, new: str):
        for s in self.symbols:
            if s.name == old:
                s.name = new

    def redirect(self, name: str, new: str):
        """Every relocation to the symbol `name` goes to the import `new`
        instead (added if need be); the symbol itself stays."""
        index = next(i for i, s in enumerate(self.symbols) if s.name == name)
        target = next((i for i, s in enumerate(self.symbols) if s.name == new), None)
        if target is None:
            self.symbols.append(Symbol(new, SYM_GLOBAL, 0, None))
            target = len(self.symbols) - 1
        for a in self.areas:
            a.relocations = [(o, (w & ~REL_INDEX) | target)
                             if w & REL_SYMBOL and w & REL_INDEX == index else (o, w)
                             for o, w in a.relocations]

    def drop_area(self, name: str):
        """Leave an area out: its symbols become imports (global, not
        defined), references to the area itself must not exist."""
        index = next(i for i, a in enumerate(self.areas) if a.name == name)
        for a in self.areas:
            relocations = []
            for offset, word in a.relocations:
                if not word & REL_SYMBOL:
                    k = word & REL_INDEX
                    if k == index:
                        raise ValueError('area %s refers to %s itself' % (a.name, name))
                    if k > index:
                        word -= 1
                relocations.append((offset, word))
            a.relocations = relocations
        del self.areas[index]
        for s in self.symbols:
            if s.area == name:
                s.attributes = SYM_GLOBAL
                s.value = 0
                s.area = None
        self.prune()

    def prune(self):
        """Leave out imports no relocation refers to (relocations by symbol
        index are renumbered)."""
        used = set()
        for a in self.areas:
            for _, word in a.relocations:
                if word & REL_SYMBOL:
                    used.add(word & REL_INDEX)
        keep = [i for i, s in enumerate(self.symbols) if s.is_defined or i in used]
        new_index = {old: new for new, old in enumerate(keep)}
        for a in self.areas:
            a.relocations = [(o, (w & ~REL_INDEX) | new_index[w & REL_INDEX]) if w & REL_SYMBOL else (o, w)
                             for o, w in a.relocations]
        self.symbols = [self.symbols[i] for i in keep]

    # writing

    def to_bytes(self) -> bytes:
        e = self.e
        head = bytearray(struct.pack(e + '6I', self.file_type, self.version, len(self.areas),
                                     len(self.symbols), self.entry_area, self.entry_offset))
        body = bytearray()
        for a in self.areas:
            head += struct.pack(e + '5I', self.offset(a.name), a.attributes | a.alignment, a.size,
                                len(a.relocations), a.base)
            if not a.attributes & AREA_ZERO_INIT:
                body += a.data
            for r in a.relocations:
                body += struct.pack(e + '2I', *r)
        symt = bytearray()
        for s in self.symbols:
            symt += struct.pack(e + '4I', self.offset(s.name), s.attributes, s.value,
                                self.offset(s.area) if s.area else 0)
        strt = bytes(self.strings)              # (after every offset() above)
        contents = {'OBJ_HEAD': bytes(head), 'OBJ_AREA': bytes(body), 'OBJ_IDFN': self.idfn,
                    'OBJ_SYMT': bytes(symt), 'OBJ_STRT': strt}
        # the chunks in the file's own order, each word-aligned
        order = sorted((off, name) for name, (off, _) in self.chunks.items())
        out = bytearray(12 + 16 * self.max_chunks)
        placed = {}
        for _, name in order:
            data = contents[name]
            placed[name] = (len(out), len(data))
            out += data
            out += b'\0' * (-len(out) % 4)
        struct.pack_into(e + '3I', out, 0, CHUNK_FILE_ID, self.max_chunks, len(placed))
        for i, (raw, _, _) in enumerate(self.slots):
            name = raw.decode('latin-1')
            off, size = placed.get(name, (0, 0))
            struct.pack_into(e + '8sII', out, 12 + 16 * i, raw, off, size)
        return bytes(out)

    def write(self, path: str):
        with open(path, 'wb') as f:
            f.write(self.to_bytes())

    @classmethod
    def read(cls, path: str) -> 'AOF':
        with open(path, 'rb') as f:
            return cls(f.read())

    def imports(self) -> List[str]:
        return [s.name for s in self.symbols if s.is_import]


def main():
    ap = argparse.ArgumentParser(description='AOF objects: --check reads and writes each unchanged')
    ap.add_argument('--check', nargs='+', required=True, metavar='FILE')
    args = ap.parse_args()
    bad = 0
    for path in args.check:
        a = AOF.read(path)
        if a.to_bytes() != a.data:
            print('%s: written differently' % path)
            bad += 1
    print('%d objects read and written: %d the same' % (len(args.check), len(args.check) - bad))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
