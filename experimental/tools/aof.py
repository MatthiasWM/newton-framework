"""ARM Object Format (AOF), as Apple's tools write it: reading (R4).

An AOF file is a chunk file: the word 0xC3CBC6C5 (its byte order is the
file's), the number of chunk slots and of chunks used, then per chunk an
8-character name, a file offset and a size. The chunks: OBJ_HEAD (the
header and the area headers), OBJ_AREA (each area's bytes, followed by its
relocations), OBJ_IDFN (the tool that made it), OBJ_SYMT (the symbols),
OBJ_STRT (the strings: a size word, then strings ending in 0, addressed by
offset from the table's start).
"""

import struct
from dataclasses import dataclass, field
from typing import List, Optional

CHUNK_FILE_ID = 0xC3CBC6C5

# symbol attributes
SYM_DEFINED = 0x01
SYM_GLOBAL = 0x02
SYM_ABSOLUTE = 0x04
SYM_COMMON = 0x40

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

    def string(self, offset: int) -> str:
        end = self.strings.index(b'\0', offset)
        return self.strings[offset:end].decode('latin-1')

    @classmethod
    def read(cls, path: str) -> 'AOF':
        with open(path, 'rb') as f:
            return cls(f.read())

    def imports(self) -> List[str]:
        return [s.name for s in self.symbols if s.is_import]
