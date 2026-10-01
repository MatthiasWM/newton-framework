# The Newton ROM from source, byte for byte (experimental)

A sub-project of decompiling, independent of newtc. The goal (Matt,
2026-10-02): the original ROM as source that Apple's own tools build into
Apple's image byte for byte, **and** that still works when things change
size, so the original ROM can be patched and extended. Every address in the
ROM becomes a symbol: in code, in data, in the NewtonScript objects. A
function that grows moves everything after it, and the linker puts every
pointer right. On that base, assembler and data are replaced step by step
with C++ (and C) that compiles to the same bytes; a function may also grow.
Everything lives in `experimental/`, with its own CMake setup; newtc's build
doesn't know about it.

## What is here

- `bin/`: the tools, 68k MPW tools wrapped in mosrun (Matt's, open source; he
  can change it): `ARMCpp` (Norcroft Newton C++ 0.43/C4.68, Jul 1996),
  `ARM6c` (Norcroft Newton C 4.62b1, Feb 1995), `ARM6asm` (ARM AOF Macro
  Assembler 2.21, May 1994), `ARMLink` (ARM Linker 5.04, Apr 1996),
  `DumpAOF`, `DumpAIF`, `Packer`, `Rex`, `BuildRex`, `DumpRex`, `AIFtoNTK`,
  `ProtocolGenTool`. `<tool> ---help` shows mosrun's own options.
- `includes/`: headers Apple published (not complete), converted to LF and
  UTF-8 (2026-10-01; they were CR and MacRoman; the compiler reads them
  through mosrun's input filter, see below). Their folders stay as they are
  (Matt); every `#include` names its header's path under `includes/`
  (`#include "Frames/objects.h"`, `"OS600/UserPorts.h"`; 194 directives in
  71 headers), so one `-I includes` finds them all: our sources do the
  same. Every C header compiles on its own without errors (2026-10-01) with
  the standard options (below): text after `#endif` became a comment (21
  lines: `#endif /* notFRAM */`), final line breaks added (15), headers that
  relied on `Newton.h` coming first include what they use (`NewtonExceptions.h`:
  `CLibrary/stddef.h`; `HAL/DelayTimer.h`: `NewtonTime.h`;
  `PSS/CompactState.h`, `Bootstrap/MemoryLanes.h`: `NewtonTypes.h`),
  `Toolbox/CompMath.h` declares its functions before it includes
  `ConfigToolbox.h` (which reaches `NewtonTime.h`, whose inlines call them),
  `OS600/UserSemaphore.h` casts `(void**)&fSem`, `Packages/PartHandler.h`
  says `friend class`. None of this can change the code the compiler makes.
  Not C: `Lantern/LanternNS.f.h`, `LanternNSEvents.f.h` (NewtonScript
  definitions, included by nothing). The html, latex and xml folders are
  Doxygen output.
- `src/`: the source tree we rebuild (empty so far).
- `CMakeLists.txt`, `tools/`, `probes/`: the build (below). `cmake -S
  experimental -B experimental/build`; `cmake --build experimental/build`
  writes the ROM's parts and symbols (`build/rom/`); `--target check` runs
  the probes. Needs Python 3 and newton-re (`NEWTON_RE`, default
  `../../newton-re` from here).
  - `tools/romsyms.py <newton-re> <out>`: from Apple's AIF (with
    newton-re's library, where it is): `ro.bin`, `rw.bin`, `rex.bin`,
    `symbols.json` (every symbol; each function's jump-table slot).
  - `tools/aof.py`: reads AOF objects (areas, relocations, symbols).
  - `tools/probe.py`: compiles each `probes/*.cp` with the standard
    options, gives its imports their slot (or own) addresses, links it so
    its functions land at their ROM addresses, compares each function with
    Apple's image up to the next symbol.
  - `tools/romasm.py <rom> <out>`: the ROM as assembler source
    (`build/romasm/`: `ro_NN.a`, `rw.a`, `zi.a`, `scatter.txt`,
    `files.txt`, `link.txt`).
  - `tools/romlink.py`: assembles them (in parallel), links an AIF with the
    scatter file, compares with Apple's: header sizes, RO and RW, the
    linker's own symbols.
- The ROM: **the target is Apple's linker output**, the AIF image `Senior
  CirrusNoDebug image` in newton-re (`DebugRom/MP2x00 US/`, 2.1 build
  717006; see "The target image"). `newtonos.s` (132 MB) is the disassembly
  of the shipping ROM, every label, calls shown as `bl VEC_Name`;
  `symbols.txt` (52,836 symbols, with the linker's own). `ROMData/rom.image`
  (8 MB) is the same build but has its first 64 KB changed: not a target.
- **newton-re** (`/Users/matt/dev/newton-re`, github dparnell/newton-re,
  published 2026-10): a reconstruction of the same ROM as host C++ that boots
  on 64-bit Linux, macOS and Windows, with very good notes. What it gives
  this project: the debug ROM images (AIF with Apple's symbol table) and the
  ROM extension (`... high`); `tools/newton-rom` (Python: AIF and symbol
  table reader, jump-table decoder, REx and package parser, demangler, many
  analysis tools: vtables, globals, xrefs, ROM tables); `romsrc/` (the ROM's
  NewtonScript world, 46,538 objects, as editable source that builds back
  byte for byte, and moves objects with `--relayout`); `docs/` (per
  subsystem: kernel, frames, views, memory map, compiler idioms in
  `curiosities.md`); `src/` (host C++, not compiled to the ROM's bytes, but
  every function cites its ROM address: a starting point like the port's
  code). **No license file** yet: we use its tools and findings where they
  are, and copy no code until dparnell says we may.

## Findings (2026-09-30, first probes)

**The compiler makes the ROM's code.** Four ROM functions compiled from C++
with `ARMCpp -bigend` (all other options default) are byte-identical:
`TPictureView::ClassID` and `TPictureView::DerivedFrom` (0x188D38..0x188D74),
`FGetOrientation` and `FSetOrientation` (0x202AE4..0x202B3C, with Apple's
`objects.h`: its inline `RINT` gives the ROM's `tst/moveq/beq/bl
_RINTError`). Code areas come out as `Code{32bit,FPIS3,NoSWStackCheck}`, APCS
with a frame pointer, as in the ROM.

**Running the tools** (mosrun rebuilt by Matt, release build, 2026-10-01;
every object the same as with the first tools).
- **Standard compiler options**: `ARMCpp ---text=utf8 -c -bigend -fc
  -DforARM -DforQ -I<experimental>/includes` (the probes: the same objects
  as with `-bigend` alone). `forQ` is the MessagePad 2x00
  (`ConfigGlobal.h`: Voyager, ARM7, sound input, internal mic); `forARM`
  gives `TTime` and more. `-fc` (limited pcc compatibility) allows `$` in
  identifiers (`OS600/ROMExtension.h`: `ROM$$Size`, the linker's symbol)
  and text after `#endif`. Other error switches exist: `-Ep` (text after
  preprocessor lines), `-Ec` (implicit casts). `-zu` changes the code: not
  to use. Apple's own options are unknown; so far these match.
- **Text** (mosrun as of 2026-10-02; `<tool> ---help`): our files and
  Apple's headers are LF and UTF-8 (Matt's preference). `---text=utf8`
  converts the text files a tool reads and writes (by extension: `.c .h
  .cc .cp .cpp .cxx .c++ .hh .hpp .hxx .i .ii .s .a .asm .inc .r .exp .def
  .make .mk .txt`, more with `---text-ext=`); files opened as binary or
  holding NUL bytes are never converted, and a file already MacRoman/CR
  stays as it is. Every tool can take it. Checked (2026-10-02): the probes'
  objects as before; the 128 MacRoman characters in a string literal; a
  MacRoman/CR source unchanged; the linker with a 7.4 MB object (the same
  image); a `-Symbols` listing named `.txt` comes out LF (`.lst` does not:
  not on the list); `-S` listings; the assembler on LF sources; `DumpAOF`.
- **At most 16 `-I` folders**: with 17, even `objects.h` isn't found (the
  compiler's limit, it seems). With paths in the `#include`s, one is enough.
  The compiler turns `/` in an `#include` into the Mac's `:` itself
  (`"Sub/b.h"` opens `::inc:Sub:b.h`), with quotes or angle brackets.
- Unix paths work (mosrun converts them). Exit codes come through (0 ok,
  non-zero on errors). The tools print only their own messages now.
- `ARMLink` knows `-Debug` but not `-NODebug` (its help lists `-[NO]Debug`;
  the linker's own parser). `-BIN` has no debug data anyway.
- **Speed**: a small C++ file with Apple's `objects.h` 0.34 s (was 18 s);
  `ARM6asm` about 21 µs a `DCD` word; the whole read-only ROM as one 21 MB
  LF source file 22.5 s with the default cache (mosrun reads a text file
  into memory when it is opened; before, a file larger than the 8 MB cache
  needed `-MaxCache 33554432` and crawled without it). `ARMLink` links the
  7.4 MB object in 0.4 s.

**The target image** (2026-10-02, from newton-re's debug ROM).
- `Senior CirrusNoDebug image`: AIF, big-endian. Header: RO 0x71A95C bytes
  at 0, RW 0x52F0 bytes to run at 0x0C100800, ZI 0x2324, debug area
  0x1CADF0 (the symbol table). Its RO and RW are **the shipping ROM up to
  `ROM$$Size`** but for one block: the checksums at 0x18420 ("DIAG XSUM
  HERE", then `gDiagType` and `gPhysROMAcsum`..`Hcsum`: placeholders
  "NONE", "ASUM", ... in the linker's output; " Q  ", 0x87E01095,
  0x00800000, 0, ... in the shipping ROM). So the shipping ROM is the link,
  plus a checksum step, plus the ROM extension (`Senior CirrusNoDebug high`,
  0xCE3FC bytes) at 0x71FC4C. newtonos.s agrees with that everywhere.
- Our `ROMData/rom.image` differs from both in its first 64 KB (15,698
  words: zeros at the vectors, 0x400..0x804, 0x1000..0x10000): changed by
  whatever made it. Use the AIF.

**How the ROM was linked** (ARMLink, as the symbols show).
- Read-only image from 0: `Image$$RO$$Limit` 0x71A95C. A scatter-loaded
  region `root` (the RW data): loaded at 0x71A95C (`Load$$root$$Base`), runs
  at 0x0C100800 (`Image$$root$$Base`), 0x52F0 bytes, then 0x2324 bytes of
  zero-init at 0x0C105AF0. `ROM$$Size` 0x71FC4C. The table at 0x40
  (`DataAreaTable`, "data") tells the boot code what to copy.
- After `ROM$$Size` (`gPackageStart`, 0x71FC4C) up to 8 MB: the built-in
  packages, appended after the link.
- Static constructors: `C$$ctorvec$$Base` 0x38A744..0x38A7A0,
  `C$$dtorvec$$Base` 0x38A940..0x38A974 (the compiler's `C$$ctor` areas).
- **Area order.** ARMLink sorts areas of the same kind by name, then keeps
  input order (tested: `aa.o` before `zz.o` whatever the command line says).
  The compiler names a file's code area `C$$c_<file name>` (the `-S`
  listing says `C$$code`) and each vtable `C$$__VTABLE__<class>`, a
  **Common** area (merged, so a class's vtable emitted in many files appears
  once). In the ROM the vtables are together near the start, in alphabetical
  order (0x1C000: `TPictureView`, after `TPartHandler`). Code, though, is not
  in alphabetical order of class names; how Apple named or ordered the code
  areas is open (file names unlike the classes? partial links per subsystem?
  an option that names every area `C$$code`, so input order counts?).
- **Vtables are code**: a vtable is a list of `B function` instructions.

**The jump table.** 16,919 functions have an entry (a slot at a virtual
address from 0x01A00000; the MMU maps them to a table of `b function` at ROM
0x2000; see "How to read the ARM code" in Matt/CLAUDE.md). newtonos.s calls
them `VEC_<name>` (its 17,976 `VEC_` names include 1,057 that are no slot:
fixed high addresses such as `kFlashBank1`, `gTimerVirt`). A call goes
through the jump table exactly when its target has an entry: 83,231 calls
do, 13,296 calls to 9,069 functions without an entry are direct (counted
with newtonos.s's `VEC_` names). Only 12 targets with an entry are also
called directly (384 calls:
`_EnterAtomicFast`, `_ExitAtomicFast` and their FIQ versions, `__rt_udiv`,
`__rt_sdiv`, `SetAlarm1`, `DisableAlarm1`, 4 recognizer functions): hand
written assembler, or calls the tool didn't touch. Vtable entries and data
pointers use `VEC_` addresses too (0x1C00C: `b VEC_Key__8TxObjectCFv`; 0x34:
`gInitHardware` = `VEC_InitCirrusHW`). How it works (newton-re,
`tools/newton-rom/newtonrom/jumptable.py`): the table lies at ROM 0x2000,
one `B function` per entry (16,723 in the German ROM); the MMU shows it at
0x01A00000 in a sparse layout (32 virtual pages share one 4 KB ROM page,
each owning a 128-byte slice), so a ROM extension can patch it a page at a
time; each `B` is encoded relative to its **virtual** address. Apple's
symbol table has every exported function **twice**, at its real address and
at its slot, under the same name. That fits a link where a function's
definition is local to its object and a jump-table object exports the name
at the slot address: calls within a file stay direct (the 12 exceptions),
every other reference gets the slot. For us: a callee still in the ROM's
bytes is an absolute symbol at its `VEC_` address (that is how
`FGetOrientation` came out identical); for our own objects the same trick
(definitions made local, R5).

## Plan (revised 2026-10-02: relocatable)

Two checks run after every step. **Identical**: nothing moved, so the build
is Apple's image byte for byte. **Shift test**: the same build with N bytes
of padding put in at some point (before a function, inside the object area,
...): every word that holds an address past that point must differ by N
from the unshifted build, and every other word must not change; a word that
looks like such an address but didn't move is a pointer we missed, and goes
on the list to classify. Then (Matt) Einstein boots the shifted ROM.

- [x] **R0 The probe, repeatable.** Done (2026-10-02): `CMakeLists.txt`,
      `tools/probe.py`, `tools/aof.py` (reading), `probes/PictureView.cp`,
      `probes/Orientation.cp`: `--target check` says the four functions are
      identical to Apple's image.
- [x] **R1 The inputs.** Done (2026-10-02): `tools/romsyms.py` reads the
      AIF with newton-re's library where it is. 35,831 symbols and 16,920
      slot symbols; the table at 0x2000 has 16,919 entries, each branching
      to the function of its name (`_DebugStr` to `DebugStr`, an alias).
      Our `symbols.txt` is the same table (slots included; it lacks only
      `airusResult`). RO + RW + the ROM extension against newtonos.s's
      1,295,780 words: the same but for the 9 words of the checksum block
      (0x1842C..0x1844C). The checksums themselves (`gDiagType`,
      `gPhysROMAcsum`..`Hcsum`, computed by
      `OSCalibrationParameters::CalculateROMREXCheckSums` 0x1A71B8 per
      newton-re) are for the post-link step (R3).
- [x] **R2 The ROM as assembler, verbatim.** Done (2026-10-02):
      `tools/romasm.py` writes 26 files of about 256 KB (cut at a symbol;
      one area each, `|ROM$$RO$$NN|`, which the linker keeps in name order)
      and `rw.a` (`|ROM$$RW|`, DATA); words as `DCD`, other bytes as `DCB`;
      35,763 labels, a name the ROM has once exported, a repeated name
      local as `|Name@0xADDR|`, the linker's `$$` symbols left out.
      `tools/romlink.py`: assembled in 2.5 s (in parallel), linked in 0.7 s
      (`-BIN -RO-base 0 -RW-base 0x0C100800`: the RW data follows RO in the
      image), identical to Apple's RO + RW. Every exported label lands at
      Apple's address (35,765; `_end` is the linker's: R2 has no zero-init
      area, whose `-BIN` output would be zeros at the end). For R3: the
      zero-init area and a scatter file, so the linker's symbols are
      Apple's (`Image$$root$$Base`, `Load$$root$$Base`, `_end`).
- [ ] **R3 Every address a symbol.** The core: a word that holds an address
      becomes `DCD label+offset`, a branch to another function `BL label`
      (or `BL VEC_label`). In parts, each its own commit (Matt: logical
      parts he can follow):
  - [x] **R3a Apple's link setup.** Done (2026-10-02): the zero-init area
    (`zi.a`, `|ROM$$ZI|`, NOINIT, its 45 labels) and Apple's scatter file:
    `ROOT 0x0` / `ROOT-DATA 0x0C100800` (ARMLink 5.04's format; `root` is
    its keyword, not a name: the linker then makes Apple's symbols
    `ROM$$Size`, `Image$$root$$Base`, `Image$$root$$Length`,
    `Load$$root$$Base`, `Image$$root$$ZI$$Base`, `...$$ZI$$Length`).
    Linked as an AIF (`-AIF -NOZEROpad -Entry 0 -SCATTER`): with `-BIN` the
    linker wants a folder per load region, and mosrun has no mkdir yet;
    `-AIF` with a scatter file is a plain binary with an AIF header, in
    Apple's layout (the same header words, addressing type 0x120). The
    header's sizes, RO, RW and the linker's 10 own symbols in Apple's
    table are all Apple's.
  - [ ] **R3b The shift test** as a tool: padding put in at a point, two
    links compared, the words that should have moved and didn't listed.
  - [ ] **R3c Calls and branches** that leave a function: `BL`/`B` to a
    label (or to the jump-table slot).
  - [ ] **R3d Literal pools**: addresses loaded from a function's pool.
  - [ ] **R3e Data**: vtables, pointer tables, C++ static data, the RW
    data's pointers, the linker's symbols in the ROM (`DataAreaTable`).
  - [ ] **R3f The NewtonScript object area.**
  - [ ] **R3g The jump table and the checksums**, built after the link.
  - [ ] **R3h The hand-written assembler** (vectors, boot, the first 64 KB).

  What each part covers:
  - **Code**: per function (its literal pool with it), disassembled: `BL`
    and `B` that leave the function, literal-pool words that are addresses
    (`LDR rX, =addr`), switch tables; branches and `ADR`/`LDR` within the
    function stay as they are (they move with it). newtonos.s's
    annotations as a cross-check.
  - **Data**: vtables (`B` entries), tables of function and string
    pointers, C++ static data, the RW data's initial values (pointers into
    ROM), the linker's own (`ROM$$Size` at 0x3C, `Image$$...`, the
    `DataAreaTable` at 0x40).
  - **The NewtonScript object area** (`gROMSoupData` 0x3AFDA8, 2.9 MB):
    every object a label, every Ref to an object `DCD label+1`, native
    function entries pointing at their C function, the magic-pointer
    table; from newton-re's `romsrc` layout (it knows every object and
    every Ref), emitted as our assembler instead of its host object file.
  - **The jump table** (0x2000): built after the link from the symbol
    pairs (a `B` to each function's new address, encoded from its virtual
    slot), with the checksum block; the slots themselves stay where they
    are, so `VEC_` addresses are fixed.
  - **Hand-written assembler** (vectors, boot, SWI, atomic helpers, the
    parameter block at 0x1000) the same way, later as `ARM6asm` source.
- [ ] **R4 AOF in Python.** `tools/aof.py`: read ARM Object Format (areas,
      attributes, symbols, relocations), checked against `DumpAOF`; write
      it, to change symbol attributes (R5).
- [ ] **R5 The jump table for our own objects.** As Apple's: a function in
      the table is local in its object, a jump-table object exports its
      name at the slot, so other files call the slot and its own file calls
      it directly. Test: two compiled functions, one calling the other.
- [ ] **R6 C++ in place.** A function's compiled object replaces its
      assembler unit (R3 makes that possible at any size; the same size
      keeps the image identical). Vtables: a class's vtable comes from the
      file with its functions (Common areas, so the generated one gives
      way). Then whole source files: find the original files' extents
      (function order, literal pools and strings, static data), start from
      the port's C++ and newton-re's `src/` (both cite ROM addresses),
      turned into 32-bit code with Apple's headers; missing headers
      reconstructed in `src/`. Track how many bytes come from source.
- [ ] **R7 C and assembler.** C files with `ARM6c` (older code generator:
      check it matches), hand-written assembler with `ARM6asm`.
- [ ] **R8 NewtonScript as source.** The object area from an editable tree
      (newton-re's `romsrc` form, or ours), assembled into the link, so a
      NewtonScript edit relinks too.

## Open questions

- Apple's compiler options: so far defaults and `-bigend` match. `-O`
  settings, `-zo` (an area per function), `-fc`, debug tables, profiling:
  find out as functions stop matching.
- How the code areas were named and ordered (see above); whether Apple
  linked subsystems separately first (`ARMLink -AOF`).
- Whether a C++ 0.43 era compiler built all of it (2.1 build 717006,
  1997; it matched so far).
- How Apple built the jump table: two links (the first gives the
  addresses), a tool on the objects, ARMLink's shared-library entry vectors
  (`-SHL`)? Our way only needs the same bytes.
- What else Apple did after the link (the checksums are one; the ROM
  extension appended; anything else?).
- newton-re's license, before any of its code comes here.
- The `root` data and static constructors (`C$$ctor`, `C$$ctorvec`).
- String literals: where Norcroft puts them (the code area, after a file's
  functions?), and so what a single replaced function can take along.

## Wishes for mosrun (for Matt)

Done: quiet output, speed (release build), the busy cursor (2026-10-01);
text conversion by extension for input and output, binary files never
touched, text files read into memory when opened (2026-10-02). None open.
