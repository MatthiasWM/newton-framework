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
  One change does, to match the ROM (2026-10-02): `OS600/NewtonGestalt.h`'s
  `TGestaltSystemInfo` ends with `fManufactureDate` (the 2.1 ROM's is 60
  bytes; the published header is older).
  Not C: `Lantern/LanternNS.f.h`, `LanternNSEvents.f.h` (NewtonScript
  definitions, included by nothing). The html, latex and xml folders are
  Doxygen output.
- `BUGS.md`: bugs found in Apple's code while rebuilding it (where, what
  it does, what was probably meant, a fix): every one found goes there
  (Matt, 2026-10-02), so they can be fixed later.
- `src/`: the source tree we rebuild (R6): C++ compiled with Apple's
  compiler and put in place of its functions; the rest of the ROM is
  generated assembler.
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
  - `tools/romkinds.py <newtonos.s> <out>`: `kinds.bin`, a byte per ROM
    word: an instruction, a data word, or not known (from the
    disassembly; `NEWTONOS_S`, default the repository's `newtonos.s`).
  - `tools/romcode.py <rom> [--compare kinds.bin]`: `code.bin`, a byte
    per word of the read-only part: code, a literal, data the code points
    at, a vtable entry, or not reached; found by following the code
    (R3d).
  - `tools/romasm.py <rom> <out>`: the ROM as assembler source
    (`build/romasm/`: `ro_NN.a`, `rw.a`, `zi.a`, `abs.a`, `scatter.txt`,
    `files.txt`, `link.txt`).
  - `tools/romlink.py`: assembles them (in parallel), links an AIF with the
    scatter file, compares with Apple's: header sizes, RO and RW, the
    linker's own symbols.
- **newtonos.s is an early reverse-engineering attempt** (Matt,
  2026-10-02): its symbols and comments are not necessarily right or
  complete, and many words are not classified (fonts, data, tables,
  dictionaries). Use it as a cross-check, not as the truth; Apple's symbol
  table (in the AIF) is the truth for names and addresses.
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
- **Standard compiler options**: `ARMCpp ---text=utf8 -c -bigend -fc -zo
  -DforARM -DforQ -DQD_Gray -I<experimental>/includes -I<experimental>/src`
  (the probes: the same objects as with `-bigend` alone; `QD_Gray`, the
  2x00's gray screen, since 2026-10-02: `PixelMap` has its `grayTable`,
  as the ROM). **`-zo`** (an AOF area per function; 2026-10-02): without
  it the compiler loads a literal from the previous function's pool when
  it is in range (`FResetPowerStats` took `gGlobalsThatLiveAcrossReboot`
  from `FGetPowerStats`' pool, 4 bytes shorter); Apple's compiled code
  never does (no compiled function in the ROM loads from another's pool),
  and with `-zo` every function so far is identical. `forQ` is the MessagePad 2x00
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
at its slot, under the same name. **Every** reference to a function with
an entry goes to the slot, in its own file too (`FSetOrientation` calls
`SetOrientation`, right after it, through `VEC_SetOrientation__Fl`): only
the 12 hand-written exceptions above call directly. The compiler makes a
relocation for every call, even within a file (to a static function too),
so the link can send them all to the slot (R5). For us: a reference to a
function with a slot is to its absolute symbol `VEC_Name` (that is how
`FGetOrientation` came out identical).

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
    Linked as an AIF (`-AIF -NOZEROpad -Entry 0 -SCATTER`): a plain binary
    with an AIF header, in Apple's layout (the same header words,
    addressing type 0x120), so the header can be compared too. `-BIN` with
    the scatter file works as well since mosrun has mkdir (2026-10-02): the
    output is a folder with one file per load region, here `root`, which is
    RO + RW, `ROM$$Size` bytes: the ROM below the ROM extension. The
    header's sizes, RO, RW and the linker's 10 own symbols in Apple's
    table are all Apple's.
  - [x] **R3b The shift test.** Done (2026-10-02): `tools/shifttest.py`
    (target `shift`): `romasm.py --pad ADDRESS:BYTES` puts zero bytes in
    before a symbol; two builds, compared word by word (a word after the
    padding looked up N bytes on). Branches in code (by Apple's symbol
    classes) with one end moved and not the other (jump-table slots don't
    move) must be encoded anew; words whose value is an address in the
    moved part must grow by N (candidates: some are data, e.g. 0x00200000
    in the MMU tables); anything else that changes is an error. Prints the
    counts, writes the misses to `build/shift/report.txt`. Verbatim, 16
    bytes before 0x188D38: 81,004 branches and 244,529 address candidates,
    none followed yet; 0 errors. Builds both in about 9 s.
  - [x] **R3c Calls and branches** that leave a function. Done
    (2026-10-02): a `B`/`BL` that is an instruction by newtonos.s
    (`romkinds.py`) and in a code symbol by Apple's table, with its target
    outside its own symbol, is written as the instruction to a label:
    101,586 of them. Targets: a function's label; a jump-table slot as an
    absolute symbol `|VEC_Name|` (14,243, in `abs.a`); 449 labels made
    where a target has no symbol (`|L_0x1234|`). Every label is exported
    now (a repeated name with its address: `|Name@0x1234|`). Identical to
    Apple's; the shift test: every branch instruction that crosses the
    padding follows (47,554 at 0x188D38, 88,354 at 0x3011C), no errors.
    Branch-shaped data words in code symbols are listed apart (33,450: the
    public jump table at 0x13000, real branches newtonos.s shows as
    `.word`, for R3g; the rest data: the NewtonScript area, recogniser
    tables).
  - [ ] **R3d Code and literal pools, our own way.** Follow the code from
    every function entry in Apple's table (instruction by instruction,
    along branches, until a return or a branch away): what is reached is
    code, a word a PC-relative `LDR` loads is a literal, the rest is data
    until shown otherwise. Then R3c's branches come from this, not from
    newtonos.s (its instruction marks only as a cross-check, the
    disagreements listed), and literal-pool words that are addresses are
    written as `DCD label+offset`. In steps:
    - [x] **R3d.1 Following the code** (2026-10-02, `tools/romcode.py`).
      Starts: the 16,567 functions the jump table names, the code symbols
      that look like functions (a mangled C++ function name, or `MOV ip,
      sp` first: 2,166 more), the static constructors and destructors (36,
      from `C$$ctorvec`/`C$$dtorvec`); then, until nothing new, function
      pointers in literals (same test) and vtables (runs of unconditional
      `B` to functions or slots: 8,746 entries). A path ends at a return,
      an unconditional branch, a write to pc, an undefined instruction (the
      jump table's fillers are traps), or where the next symbol starts; a
      switch (`ADDLS pc, pc, Rn, LSL #2`) is followed by its table of
      branches. The test for "looks like a function" matters: Apple's table
      calls data in code areas code too (`yytable`, `bpWeight`, trigram
      and NewtonScript data), and literals point at them. Result: 19,209
      functions, 773,396 code words, 13,029 literals, 1,394 data words the
      code points at. Against newtonos.s: 567,203 code in both, 12,971
      literals its data, 5 words our code its data; 205,971 of our code it
      has not classified; 54,413 of its instructions we don't reach:
      computed jumps (`MOV pc, r6`, `ADD pc, r1, #n`, `LDR pc, [r0, r1, LSL
      #2]` with a table of addresses), the floating-point emulator
      (`FP_UndefHandlers_Start`, hand-written), code after the destructor
      vector, and code nothing branches to (after a switch's table in
      `TParagraphView::RealDoCommand`: dead, or reached in a way neither
      sees).
    - [x] **R3d.2 R3c from this** (2026-10-02). Branches now come from
      `code.bin`; vtable entries are written as `B |function|` too. Fixes
      to the following on the way: the last case of a switch is its code
      right after the table of branches; a write to pc right after `MOV
      lr, pc` is a call (virtual calls: `MOV lr, pc; ADD pc, r1, #n`), so
      the path goes on (this alone took the unreached instructions from
      52,198 to 8,768); every compiler prologue (`MOV ip, sp`, `STMDB sp!,
      {..., fp, ip, lr, pc}`) is a start (1,199 functions, some without a
      symbol); native function names (`F` and a capital) look like
      functions. Fallback: newtonos.s's instructions inside a function we
      followed, where we did not get to, count as code ('n', 4,331 words:
      code after a return that no known branch reaches). Now: 19,500-odd
      functions, 838,058 code words, 13,633 literals; 2,983 of newtonos.s's
      instructions not reached. 101,649 branches to labels; identical to
      Apple's; the shift test: 47,545 crossing branches follow and 9 do
      not at 0x188D38, 88,344 and 10 at 0x3011C: 7 in hand-written
      assembler (`Reset`, `ROMBoot`, `DataAbortHandler`,
      `ExitCPUFIQAtomic`, `DoSchedulerSWI`: R3h), 3 in code without symbol
      or prologue after `C$$dtorvec$$Limit`.
    - [x] **R3d.3 Literals** (2026-10-02). A literal whose value is an
      address is written as `DCD |label|+offset` (the nearest label at or
      below): read-only addresses from 0x10000 up (7,276; below, numbers
      like 0x100 and the symbols at the ROM's hand-written start look
      alike: R3h), RAM data (RW and zero-init, 3,472), with a NewtonScript
      tag (+1) or without, jump-table slots (423, `|VEC_Name|`). Of 13,633
      literals, the rest are numbers (hardware addresses such as
      0x0F181800). Identical to Apple's; the shift test at 0x3011C: 6,666
      address candidates follow; of the 274,352 that don't, 274,202 are in
      words not reached as code (data: R3e, R3f), 113 in code, 37 in data
      the code points at (to look at).
  - [ ] **R3e Data**: vtables, pointer tables, C++ static data, the RW
    data's pointers, the linker's symbols in the ROM (`DataAreaTable`).
    - [x] **R3e.1 Strong evidence** (2026-10-02). A data word (read-only
      outside code and the object area, or RW) whose value is exactly a
      symbol's start (from 0x10000), a RAM symbol, or a jump-table slot is
      written as `DCD |label|`: 2,997 (the magic-pointer table 873, `rat0`
      ..`rat3` 1,024, `Functions`, `OpcodeProcs`, `gRDPHandlers`, the
      recogniser grammars `BiGS...`/`BiSL...`, the patterns' `...Ptr`, RW
      289). Not counted as evidence: a word pointing inside a symbol (as
      often two 16-bit numbers: parser tables, dictionaries); a target in
      the R/RS block after the object area (0x67FA44..0x6853DC, a symbol
      every word: the dictionaries' UTF-16 pairs such as 0x006E0027 "hit"
      it). Not looked in: tables of numbers that hit symbols by chance
      (`gLex8...`, `gEnum80...`, `gSymb80...`, `yy...`, `DESSBoxes`) and
      the R/RS block. `build/romasm/data-pointers.txt` lists them by symbol;
      single hits to check when Einstein runs a shifted ROM
      (`displayAngle`, `IrMaxTurnTimeTable`, `blackCompleteTbl`, ...).
      Identical to Apple's; the shift test at 0x3011C: 9,203 address
      candidates follow, no errors. Left: 236,225 in the object area,
      4,847 in the R/RS block (R3f), 28,287 in other read-only data (most
      numbers), 2,456 in the RW data.
    - [x] **R3e.1b Tables of code addresses** (2026-10-02, after Einstein
      hung on the first shifted ROM: PC 0x38D1F4, LR 0x38D1E8, i.e. by
      `WarmBoot`, whose `LDR pc, [pc, r1, LSL #2]` table of 18 addresses
      was still numbers). In a run of at least 3 data words with a pointer
      from R3e.1 in it, or starting where a literal points (the code loads
      the table's address), every word that is the address of an
      instruction (romcode.py or newtonos.s; in a table a literal points at,
      any aligned read-only address: the SWI handlers are code to neither)
      becomes `DCD |label|` (labels made); the static constructors' and
      destructors' tables (`C$$ctorvec`, `C$$dtorvec`: functions without a
      symbol) entirely. 185 entries: WarmBoot's table, the SWI dispatch
      table (`FlushEntireTLB+0x24`, 35 entries), the exception names
      (`exRootException`.. pointers to their strings), `gRDPHandlers`,
      `AAtables`. `romcode.py` writes `functions.json` (the functions it
      followed: 687 without a symbol). More tables of numbers left out:
      `IrMaxTurnTimeTable`, `nbcut...`, `xr_type_merits`. Identical to
      Apple's; shift test: no errors.
    - [ ] **R3e.2 Table by table**: pointers inside symbols, where a
      table's layout says so (newton-re's tools know many tables).
  - [x] **R3f The NewtonScript object area.** Done (2026-10-02). Walked
    object by object from `gROMSoupData` to `gROMSoupDataSize` (a header
    word size << 8 | flags, bit 0 slotted, bit 1 frame; a GC word; class or
    map; slots; padded to 4 with 0xBA): 46,538 objects, newton-re's census
    exactly (12,838 frames, 16,506 arrays, 8,623 symbols, 8,571 binaries),
    ending exactly at the area's end. Every object a label (its symbol, or
    `|O_0x3AFDA8|`); every Ref to an object (address + 1; 168,891, all to
    object starts, none outside the area) `DCD |label|+1`; slots holding a
    native function: 219 its address (`DCD |label|`, a function start that
    our analysis calls code), 1,143 its jump-table slot (`|VEC_Name|`).
    After the area, the R and RS constants (0x67FA44..0x6853DC): R words a
    Ref to an object (1,979) or a magic pointer (a number, 888); RS words
    the address of an R word (2,867; labels made for R words without a
    symbol). Identical to Apple's; the shift test at 0x3011C: 183,165
    address candidates follow, no errors. What doesn't follow in the area
    is numbers: bytes in binaries (55,372: strings, bitmaps, bytecode,
    fonts, tables) and symbols (8,648), headers (81), NewtonScript integers
    in slots (round values such as 0x40004, 0x300000: flags; none at a
    function's start). Left elsewhere: 28,281 in other read-only data,
    2,456 in the RW data (R3e.2).
  - [ ] **R3g The jump table and the checksums.**
    - [x] **R3g.1 The jump table** (2026-10-02): entry i at 0x2000 + 4i is a
      `B` encoded from its virtual address (0x01A00000 + page * 0x1000 +
      (page % 32) * 0x80 + slot * 4, page = i / 32), so it is written as
      `B |function|-&D` with D its virtual minus its physical address: a
      branch to a label with a constant subtracted, which ARM6asm and
      ARMLink take (tested), so the linker encodes it right wherever the
      function is; no tool after the link. 16,919 entries; identical to
      Apple's; the shift test: all 16,919 right in the shifted ROM. The
      public jump table (`gROMPublicJumpTable` 0x13000..0x15E0C, entry i a
      `B` from 0x01800000 + 4i to a slot of the first table) never
      changes: both ends fixed; the shift test checks it stays so.
    - [x] **R3g.2 The checksums and the shipping image** (2026-10-02):
      `tools/romimage.py` (target `image`): the link's RO + RW, the ROM
      extension at `ROM$$Size`, 0xFF to 8 MB, and the block at 0x18420 as
      Apple's tool filled it: `gDiagType` " Q  " (the product), 
      `gPhysROMBcsum` the size 0x00800000, C..H 0, `gPhysROMAcsum` the sum of
      all the image's 32-bit words with those filled in and A at 0
      (0x87E01095: worked out from the shipping values). Nothing in the ROM
      reads the block (no reference to it). Our image is the shipping ROM:
      against `ROMData/rom.image` only the 15,698 words of the first 64 KB
      differ, which a live dump gets wrong (the MMU pages them; Matt), and
      which agree with newtonos.s and Apple's AIF.
      **The ROM extension is position-dependent**: its header's `start`
      (+0x20) is its own address (= `ROM$$Size`), some 40,000 references
      inside it are absolute (NewtonScript objects, address + 1), and it
      refers to the base ROM in about 300 places (106 Refs to base objects,
      201 values exactly a base symbol, 19 jump-table slots). Moving it, or
      moving what it refers to, needs its packages relocated (its
      structure: newton-re's romsrc handles it; the Rex tool later, Matt);
      a plain "add N" won't do (UTF-16 text pairs such as 0x00720065 fall
      in its address range). So `romimage.py` stops if `ROM$$Size` moved.
      How it was made (Matt, 2026-10-02): the extension's packages were
      `.pkg` files, relocated into place by Apple's `Rex` tool (in `bin/`).
      So: un-relocate each back into its `.pkg`, and rebuild the extension
      with a Rex script; then it can move with the core ROM. Later: the
      core ROM has priority.
      This decides how to test a shifted ROM in Einstein (below).
  - [ ] **R3h The hand-written assembler** (vectors, boot, the first 64 KB).
    - [x] **R3h.1 Vectors and the linker's values** (2026-10-02): the
      exception vectors (0x00..0x1C) are starts for `romcode.py`; the
      newtonos.s fallback also counts in symbols whose first word we reached
      as code (`ROMBoot`, `DataAbortHandler`, `DoSchedulerSWI`, reached by
      branches, not calls: 6,871 words now); `gPackageStart` (0x3C) is
      `|ROM$$Size|`, the `DataAreaTable` (0x40) `|Load$$root$$Base|`,
      `|Image$$root$$Base|`, `|Image$$root$$ZI$$Base|`,
      `|Image$$root$$Length|`, `|Image$$root$$ZI$$Length|` (the linker's
      own, imported). The shift test counts the RW data's load address and
      the image's end as moving too (they grow with RO). Identical to
      Apple's; the shift test at 0x3011C: 5 crossing branches don't follow
      (3 in code without symbol or prologue after `C$$dtorvec$$Limit`, 2 in
      `ExitCPUFIQAtomic`, which nothing we follow reaches), no errors.
    - [x] **R3h.2a Alignment** (2026-10-02): a symbol at a multiple of
      0x100 or more with at least 16 bytes of zeros before it starts an
      area of its own with `ALIGN=n` (ARM6asm's area attribute; the linker
      aligns and fills with zeros), the zeros left out: four of them,
      `gROMPublicJumpTable` 0x13000, `gROMPatchTablePageTable` 0x16000,
      `AsmTraceAddAddrEvent` 0x18400, `gROMMagicPointerTable` 0x3AF000
      (2,676 bytes of zeros before it, right before the object area). So
      padding put in between 0x18400 and 0x3AF000 (up to 2,676 bytes) is
      taken up there, and the magic-pointer table, the object area, the
      R/RS constants, the lexicons, the RW data, `ROM$$Size` and the ROM
      extension stay where they are: a shifted ROM can keep the extension
      as it is. `romasm.py` writes `aligned.json`; the shift test moves
      only the window [padding, aligned symbol). Identical to Apple's; at
      0x3011C: 88,349 crossing branches follow (5 not), all 16,919
      jump-table entries right, 3,604 address candidates in the window
      follow, no errors; what doesn't is numbers (object binaries 42,386,
      slots 2,837, tables 5,714) and the RW data (2,450: character maps,
      strings; perhaps a few real pointers into the middle of something:
      `gPrintLiterals`, `gAlertGlyphPixMap+0xC`: R3e.2).
      **For Einstein** (Matt): `build/romimage/shifted-16-at-0x3011C.image`,
      16 bytes put in before `TAlertDialog::TAlertDialog` (0x3011C), so
      nearly all code moves (914,491 words differ from the shipping
      image); made with `shifttest.py --at 0x3011C` and `romimage.py --aif
      build/shift_0x3011C/padded/obj/rom.aif`.
    - [ ] **R3i Booting shifted ROMs in Einstein** (2026-10-02; parked,
      see "Where it stopped").
      **Einstein patches this ROM at fixed addresses**: it knows it by
      `gROMVersion`, `gROMStage`, `gHardwareType` (0x13DC, 0x13E0, 0x13EC)
      and then writes its JIT patches (native calls for `DebugStr`,
      `Debugger`, clock and date functions; logging injections; plain words:
      `gDebuggerBits`, `gNewtConfig`, no calibration screen, no GeoPort
      beacon, its time base) and the virtualised `__rt_udiv`, `__rt_sdiv`,
      `symcmp` (5 words each). In a shifted ROM these land in the wrong
      code: the first hangs were that. `romimage.py --einstein` makes an
      image for Einstein: `gROMVersion` 0x00020102 (Einstein doesn't know
      it, so it patches nothing; the ROM only reports it through Gestalt;
      not `gROMStage`, which Einstein's own REx, `Drivers/Glue.s`, needs to
      pick its machine: else "Unsupported platform"), and its plain
      patches applied where their code is now (Apple's symbol + offset, in
      the link's `symbols.txt`); `DebugStr` and `Debugger` (traps) return.
      The native clock and date calls are left out (the ROM's code runs).
      The unshifted image made so boots in Einstein like the original.
      **Testing without Matt**: `/Applications/Einstein.app` (a debug
      build: its monitor logs to `/tmp/Einstein_log.txt`) runs
      `<image>.monitorrc` at start; `watch 0 ADDR` logs "Watch at ADDR"
      each time and goes on, so a list of boot milestones (`ROMBoot`,
      `UserInit__Fv`, `InitObjects__Fv`, `DrawSplashScreen__9TNotebookFv`,
      `RunInitScripts__Fv`, ...) at their addresses in the link shows how
      far a ROM gets. The ROM and flash paths are in
      `~/Library/Preferences/robowerk.com/einstein.prefs`.
      Shifted 16 at 0x3011C: gets to `UserInit__Fv`, not to `SleepTask`.
      **Bisecting** (the padding moved later until the ROM boots: what
      breaks lies between the last that fails and the first that boots;
      about a minute per boot): 16 bytes before `PatchLoaderStub` boot,
      before `_BadExit` (0x3AE158) not. `_BadExit` is one `B TaskKillSelf`
      (a task's return address, `TTask::Init` loads it from a literal),
      which `romcode.py` never reached: a function pointer counted only if
      its target looks like a function. Now also when newtonos.s shows an
      instruction there (only `_BadExit` more; 20 more branches). Then
      before `_BadExit` boots too. Next: before `SWIBoot` (0x3AD698).
      Two causes. (1) The SWI handlers (`EnterCPUIRQAtomic+0x48`, ...) are
      entered only through the SWI table, so their code wasn't followed
      and 8 branches into `SWIBoot` stayed numbers: `romcode.py` now takes
      tables of code addresses as starts (a literal pointing at 3 or more
      aligned addresses, 3 in 4 of them instructions, all within 128 KB:
      the SWI table, `gRDPHandlers`; `AAtables`, `yydgoto`, `nbcut0`,
      `IrMaxTurnTimeTable` are numbers). (2) **R3h.3** `SWIBoot` loads the
      SWI table's address PC-relative from a literal before it (in
      `FlushEntireTLB`): 280 PC-relative loads and ADRs reach into another
      symbol (hand-written assembler shares literal pools: `ROMBoot` and
      others use `ResetFromResetSwitch`'s; the compiler's `ClassInfo` ADRs
      the data before it). `romasm.py` writes them `LDR Rd, |label|` /
      `ADR Rd, |label|` (the assembler works out the offset; same bytes)
      and never cuts files between such a pair. Then before `SWIBoot`
      boots. Identical to Apple's throughout.
      **Where it stopped** (Matt, 2026-10-02: the source comes first; the
      remaining places that need labels will turn up while the source is
      filled in, and growing code gets tested then): 16 bytes before
      0x3011C get through `InitInterpreter__Fv`, not to the Notebook's
      `InitToolbox`. Known left: Einstein's own REx calls
      `PSoundDriver::OutputIntHandlerDispatcher` (0x1E60FC) and
      `InputIntHandlerDispatcher` (0x1E6130) at their real addresses
      (`Drivers/Glue.s`), so a shift before them breaks sound in Einstein
      whatever the ROM does (Einstein would have to find them by name);
      3 branches after `C$$dtorvec$$Limit`; R3e.2's tables; R3h.2b.
      To go on: `tools/einstein/bisect.py 0x3011C 0x3AD698` (saves
      nothing of Einstein's settings: copy `einstein.prefs` first, put it
      back after).
      Tools (`tools/einstein/`): `watch.py` (the milestones as a
      `.monitorrc`), `run.sh` (boot one image, quit, print the milestones
      reached; output in `build/einstein/`), `boot.sh ADDRESS` (build with
      16 bytes before a symbol, make the Einstein image, boot it),
      `bisect.py LOW HIGH`, `hits.py`.
    - [ ] **R3h.2b** The rest of the first 64 KB (tables, literals below
      0x10000 that are addresses), those 5 branches, and a check that the
      floating-point emulator holds no absolute addresses of itself.
    Not the floating-point emulator (`FP_UndefHandlers_Start` 0x38D8DC on):
    ARM's fallback for a CPU without floating point, likely a library
    module linked in (Matt): it stays a block of bytes; check only that it
    holds no absolute addresses of itself, so it can move.

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
- [x] **R4 AOF in Python.** Done (2026-10-02): `tools/aof.py` reads and
      writes AOF. Written back unchanged, every object comes out byte for
      byte (the compiler's and the assembler's chunk orders differ: each
      file's own is kept; `aof.py --check`, in the `check` target).
      Changes: `rename_area`, `rename_symbol`, `redirect` (relocations to a
      symbol sent to another, an import added), `drop_area` (its symbols
      become imports; relocations by area index renumbered), `prune`
      (imports nothing refers to left out). Apple's `DumpAOF` reads the
      changed objects.
- [x] **R5 The jump table for our own objects.** Done (2026-10-02), not as
      planned: Apple's code calls through the slot in its own file too
      (see "The jump table"), so `romasm.py` sends every relocation to a
      global symbol with a slot to `VEC_Name` (an absolute symbol in
      `abs.a`), defined in the object or not. Tested with
      `SetOrientation`, called by `FSetOrientation` in the same file.
- [ ] **R6 C++ in place.** Started (2026-10-02). `src/` holds the source
      tree (folders as the port's; a file may cover part of an original
      file, its header comment says which ROM range). CMake compiles each
      `src/**/*.cc` (`*.cp`) with the standard options into
      `build/src/`, and `romasm.py --object` puts each object in place: its
      one code area goes where its functions are in Apple's table (the
      generated source leaves those bytes and labels out), renamed to sort
      between the generated pieces (`ROM$$RO$$09`, `ROM$$RO$$09$$01` the
      object, `ROM$$RO$$09$$02`: the linker orders areas by name); its
      vtables (Common areas) are dropped, the generated source has the
      ROM's; references to slot functions go to the slot (R5). It stops if
      something outside needs a label inside (a branch into the middle, a
      PC-relative load across the edge) or the range crosses a file
      boundary. `romlink.py` links `.o` files in `files.txt` as they are.
      The check: the whole image identical to Apple's. Changing a source
      changes the image (tested: `return 77` instead of 76 in the probe:
      one word, at 0x188D38).
      With `-zo` an object has an area per function (`C$$c_File`,
      `C$$c_File_1`, ...): they go one after the other, renamed in order
      (`ROM$$RO$$09$$01$$000`, `$$001`, ...); `probe.py` renames them so
      too (by name, `_10` would sort before `_2`).
      **`src/OS/SystemNatives.cc`** (2026-10-02): the file from
      `FGetSerialNumber` (0x20171C) to `FBatteryStatus` (ends 0x203DE8,
      the flash driver follows): NewtonScript natives for the system
      (serial number, batteries, power, backlight, Gestalt, contrast,
      orientation, tablet bypass, power and heap statistics; newton-re
      calls it `SystemNatives`). Done (2026-10-02): 37 of its 41
      functions, 7,144 bytes of code, and the file's data (16 bytes at
      0x0C104C48: `gLastBatteryLevel` 100, `gLastWakeupTime`, a static 4
      at +12). Three are written but `#if 0`, each with a note on what
      still differs: `FGetHeapStats` (the order of two loads; a search
      over 20 forms found none), `SetBatteryType` (r12/lr swapped in a
      call's setup; a search over 96 forms found none),
      `BatteryStatusHelper` (the ROM gives each `if` statement new stack
      for its temporary; ours reuses).
      **Out-of-line copies of inline functions**: the compiler makes them
      as Common areas `C$$i$<name>` (`RefVar`'s constructor and
      destructor, for the vector constructor of a `RefVar` array); the
      linker sorts them after `C$$dtorvec`, so the ROM has them there,
      unnamed (the "code after C$$dtorvec$$Limit"), and two can have the
      same bytes. `aof.py`'s `inline_copies` reads off where the ROM's code
      refers to them (the ROM word where our relocation is: literal words
      so far); `romasm.py` labels that address `name@0xADDR` (the
      function itself is elsewhere, with a jump-table slot) and the object
      refers to the label; such a reference is never sent to the slot.
      `probe.py` the same with absolute symbols. `probe.py` lays an
      object's areas out in ROM order, with filler for functions left out
      (a source's functions out of ROM order showed as "lands at": a
      search counting only differing words missed that; count every line
      that is not "identical").
      **Finding the source form** (`tools/variants.py <source>
      <variants>`: tries forms, keeps what lowers the differences): when
      only registers or the order of
      instructions differ, the choice is the whole function's (a change in
      one case moves registers in another), so trying the plausible forms
      together pays (`FGestalt`: two forms of a variable times four ways
      to write `SetArraySlot` (Apple's `SetArraySlot(ARG...)`, the
      `RefVar` method, `...RefArg`, `...Ref`) twice). The types of Apple's
      constants decide code: `i < kMaxROMExtensions` (a `const ULong`) is
      an unsigned compare of a signed `long i`, `i < kMaxPatchCount` (a
      `#define`) a signed one.
      **`src/Stores/T28F016_SA_SVDriver.cc`** (2026-10-02): the flash
      driver for Intel's 28F016SA/SV and Sharp's chips, 0x203DE8..0x204698
      (the fax line codec follows). 21 of its 24 functions, 1,576 bytes;
      `InitializeDriverData`, `Write` and `ReportWriteEraseStatus` are
      written but `#if 0` (registers). Its ClassInfo (0x384820) is
      ProtocolGen's glue: generated. Reconstructed headers
      `Stores/FlashDriver.h` (`SFlashChipInformation`), `Stores/
      FlashRange.h` (`TMemoryAllocator`, `TFlashRange`: offsets as the
      ROM's code uses them, from newton-re's findings, names ours),
      `Stores/T28F016_SA_SVDriver.h` (the class, its 32-byte state).
      Commands go to every byte lane: (command x 0x01010101) & lanes,
      which the compiler makes as `BIC` of the complement. A virtual call
      is `LDR pc, [obj]` for the first entry, `LDR r, [obj]; ADD pc, r,
      #4n` for the others (the vtable is branches): the vtable pointer is
      at +0. Lessons: a `switch`'s cases in ascending order of value
      (the compiler's tests follow); `else if (a && b) ; else if (...)
      return` chains where the ROM's code branches instead of using
      conditional instructions; two variables initialised from the same
      expression (`readArray = mask << shift; lanes = mask << shift`) where
      the ROM computes it once and copies it twice.
      **`src/Communications/Fax/T4FaxLine.cc`** (2026-10-02): the fax
      tool's T.4 (modified Huffman) line codec, 0x204698..0x205180:
      `TT4FaxLine` (the decoder of received pages, a ring buffer read a bit
      at a time) and `EncodeT4`, `T4AddRTC`, `writeCodeWord`, `outputRun`
      (the encoder); and its tables, 0x377C0C..0x378410 (TADSPConnection
      follows the code). 14 of its 17 functions, 1,936 bytes, and the
      tables, 2,052 bytes; `GetNextBit` (one word: the order of a compare's
      operands), `AppendTo` (registers), `EncodeT4` (three words,
      registers) are written but `#if 0`. Reconstructed header
      `Communications/Fax/T4FaxLine.h` (layout from the ROM's code and
      newton-re's findings, names ours). Bugs: BUGS.md B4, B5.
      **`src/Communications/AppleTalk/ADSPConnection.cc`** (2026-10-02):
      `TADSPConnection`, an ADSP connection (open state machine, probe,
      retry, send and forward-reset timers, control packets, data, acks),
      0x205180..0x206494 (telephony option constructors follow). 31 of its
      35 functions, 3,736 bytes; `Match`, `CheckSendData`,
      `MatchFilterAddress`, `MatchAddress` are written but `#if 0`
      (registers). No published header and nothing in newton-re: every
      type reconstructed from the code, in `Communications/AppleTalk/
      AppleTalk.h` (`TAddress`, `TWriteElement`, `TWriteChain`,
      `TMessageTimer`, `TMemoryObject`, `TPacketMessage`, `TATAsyncMsg`,
      `TAppWorld`, `TAppleTalkWorld`, `WriteSocket`) and
      `ADSPConnection.h` (`ADSPHeader`, `ADSPOpenConnInfo`, `State`, the
      events, the buffers, the class: 428 bytes, no vtable). Lessons:
      **the ARM610 has no halfword loads**: a `UShort` field is `LDR` and a
      shift; a halfword the ROM *writes* with a word read-modify-write
      (`LDR`, `LSL #16`/`LSR #16`, `ORR`, `STR`) is a 16-bit bitfield (a
      `UShort` is written as two `STRB`). Bitfields are allocated from the
      most significant bit; consecutive constant assignments to bitfields
      of one word are merged into one read-modify-write (9 flags set to 0:
      `LSL #9; LSR #9`; two set: `ORR #&90000000`); a field the ROM reads
      with `ASR #24` is a signed bitfield (`int descriptor : 8`). An inline
      method shows as stores through a pointer register kept from the
      constructor call (`TAddress::Clear`). Locals for values used twice
      (`ULong sendSeq = fSendBuffer->fSendSeq;` in `PrepHeader`), a
      local pointer and a local loaded before stores
      (`ExecuteState`), a declaration apart from its assignment (`Read`),
      `if (x) a = K; else a = 0;` (a conditional `ORR`) against `a = x ? K
      : 0`, `if ((fSendWindow >>= 1) == 0)`: each made a function
      identical. A local object with a virtual destructor
      (`TWriteElement`) is destroyed through its vtable (`LDR pc, [sp]`);
      a class whose destructor calls a method inline (`~TWriteChain() {
      Destroy(); }`) shows the call at the end of the scope.
      **`src/Communications/TAPIOptions.cc`** (2026-10-02): the 12
      telephony options' constructors (`TCMOTAPIHold` .. `TCMOTAPIService`,
      0x206494..0x20684C), all identical; each `TOption(kOptionType)`, then
      `SetLabel('hold')`, `SetLength(sizeof(...) - sizeof(TOption))`, its
      fields (header `Communications/TAPIOptions.h`).
      **`src/Testing/AgentReporter.cc`** (2026-10-02): `TAgentReporter`
      (0x20684C..0x206BF0), all 5 identical; the port's `CAgentReporter`
      (Matt's) as the start, Apple's names (`TTestReporter`, `TDate` in
      `src/Dates.h`, `ConvertFromUnicode` in `src/Utilities/Unicode.h`).
      **String literals**: in the function's own area (`-zo`), at the
      literal pools; MPW's compilers swap `\n` and `\r` (`\n` is 13), so
      the ROM's carriage returns are `\n` in source. The destructor
      calls the base destructor twice: the body calls it explicitly
      (BUGS.md B6). A local copy of a parameter (`char * name =
      inTestName;`) changed which of two registers each got.
      `aof.py` keeps each symbol's string offset (the compiler can write
      the same name twice in the string table).
      **`src/Recognition/Array.cc`** (2026-10-02): `TArray`, the
      recognizers' dynamic array in a Handle, and its iterator
      (0x208E98..0x209654): 25 of 28 identical; `Add`, `Save`, `GetNext`
      written but `#if 0` (1 to 3 words). Headers `Recognition/
      RecObject.h` (`TRecObject`, `TArrayIterator`, `TArray`, `TDArray`),
      `RecGlue.h` (the Handle glue at 0x11B858: `MakeHandle`,
      `ResizeHandle`, ...), `Msg.h` (`TMsg`). The port's `CArray` was the
      start; Apple's arrays keep a Handle and a reference count.
      **Vtables decide virtual calls**: a virtual function is called by its
      index, so a class's virtual functions must be declared in the order
      of the ROM's vtable: `tools/vtable.py build/rom TArray` finds the
      vtable the constructor stores and names its entries (the base
      class's first; `TRecObject`: `Dispose`, `Dump`, `SizeInBytes`,
      `CopyInto`, no virtual destructor). Calls through a slot: Apple's
      `GetArraySlot(ARG, long)` inline (not `GetArraySlotRef` directly)
      copies the index once more; a `goto` past a test the compiler
      would otherwise not skip (`IArray`).
      **Constant data** (`static const`, `const` tables) goes into an area
      of its own, `C$$cd_<file>` (read-only, Code attributes, objects
      aligned to 4 and padded with zeros), in the order of the
      definitions; the ROM has the files' areas together (here after the
      sound gains, before `EXP_TABL`). Placed like a function: at its first
      named table's address minus its offset. Tables whose layout the ROM
      shows are written from its bytes (generated once); `static` ones have
      no name in Apple's table. C++ `const` globals have internal linkage:
      declared `extern` in the header, so they keep Apple's names.
      **A class's vtable** (Common area `C$$__VTABLE__<class>`, a local
      symbol): the ROM's have no names, so `aof.py`'s `inline_copies` reads
      its address off the ROM word where the code loads it (the
      constructor), as for inline copies.
      **Declarations decide registers**: where only registers differ, the
      order of declarations (and declaring a function's locals at its top,
      C style) often changes them (`DoMHDecodeLine`: `int bytes, code, run,
      next;` at the top made it identical; `EncodeT4`'s main loop came
      right by reordering its first seven declarations). Also: a `while`
      with the decrement in the body (`i--` before the `if`) where the ROM
      decrements before the test at the bottom; a value computed into a
      variable before a call (`code = table[...]; writeCodeWord(...,
      code, ...)`) where the ROM computes it before pushing the stacked
      arguments; `outBytes = outBytes + bytes` and `outBytes += bytes`
      differ in registers.
      **`probe.py --each`** compares one function at a time, the object's
      other functions dropped (their names then imports at their ROM
      addresses), so a function of the wrong size does not move the ones
      after it; `--function PREFIX` only those, `--option` adds a compiler
      option (to try: `-O`, `-Otime`, `-Ospace` changed nothing here).
      `variants.py` uses `--each`.
      **Data**: Norcroft addresses a file's own globals from its data
      area's base (`LDR r0, =gLastBatteryLevel; LDR r2, [r0, #12]` for the
      static at +12), so a function that uses them is only identical with
      the globals defined in the file. `romasm.py` puts the object's data
      area where its first global is in the RW data (`rw.a` is cut into
      pieces like the read-only part); `probe.py` links it there
      (`-RW-base`). The data comes in file order, as the code does (the
      stroke file's globals before, `screenWidth` after: another file's).
      A global with a constructor, even an empty one (`TTime`), makes a
      static constructor (`C$$ctor`, `C$$ctorvec`) and a pointer to it at
      the end of the data area; the ROM's block is 16 bytes, so here the
      original had none: `gLastWakeupTime` is an `Int64`. `FGetHeapStats` (0x202FF4) is written but `#if 0`:
      identical but for 3 words, the order of two loads (see there).
      Each `-zo` area goes where its own function is, so a source file
      may leave a function out (it stays generated in between).
      **How the compiler lays out the stack** (found on the way, from
      `-S` listings): a declaration starts a segment, and the segment's
      stack (its locals, and the temporaries of the statements up to the
      next declaration: two RefVars per `SetFrameSlot(f, SYM(x),
      MAKEINT(n))`) is allocated there, a declaration with an initialiser
      before it is evaluated; later declarations get lower addresses;
      zero-initialised locals are stored in declaration order. A block
      (`{ }`) gets its own allocation and gives it back at its end (a
      following block reuses it). So the `SUB sp, sp, #n` in the ROM's
      code shows where the original declared things. An `if`, `while` or
      `do` statement gets the stack for its temporaries itself and gives
      it back at its end; a statement directly in a block shares the
      block's (so a call kept to the end of the block is a block
      statement with a `goto` around it: `ExtendedGestalt`). A value that
      the ROM uses unmasked was an `int`/`long`, not a `Boolean` (which
      gets `AND #255`). Implicit conversions (`MAKEINT(n)`, `TRUEREF` as a
      RefArg) and explicit `RefVar(x)` temporaries come out in different
      stack orders. Other lessons:
      `RefVar x(NILREF)` (not `RefVar x;`) puts NILREF in a register for
      both; a value used in a call's arguments is computed before them
      only if it is in a variable; `goto` to the end, not `return`, gives
      a branch around the epilogue; Apple's `includes/` has more than it
      seems (`OS600/VirtualMemory.h` has `SGlobalsThatLiveAcrossReboot`
      in full: search it first). `probe.py --includes includes --includes src --words
      src/OS/SystemNatives.cc` compares function by function (every word
      that differs). `tools/rsheaders.py` writes `src/Frames/RSSymbols.h`
      and `src/Frames/ROMResources.h` from Apple's table: every RS constant
      (`extern Ref * RSSYMname;`, 1,768 symbols, 1,099 other objects) and
      `RA(name)`, `SYMA(name)` (the constant as a RefArg; names in lower
      case, as in the ROM). `src/OS/RDM.h`,
      `src/MemoryManager/MemMgr.h` (the port's "Memory Manager", without
      the space).
      Before it: `src/Graphics/Screen.cc`, `FGetOrientation`,
      `FSetOrientation`, `SetOrientation` (0x202AE4..0x202C6C, 392
      bytes). What it took: `QD_Gray` defined (`PixelMap` has a
      `grayTable`, 28 bytes: now a standard option), and
      `TGestaltSystemInfo` has `fManufactureDate` at its end in the ROM (60
      bytes; the published header lacks it: added).
      **Headers** (Matt: the published ones cover driver development
      only, so most types and globals are missing): what is missing is
      reconstructed in `src/`, at the port's header path (`src/Graphics/
      Screen.h`, `src/Frames/NewtGlobals.h`, `src/Recognition/Tablet.h`),
      with Apple's names (`TInterpreter`, not the port's `CInterpreter`;
      globals as in Apple's table: `gGrafPort`, `screenWidth`), layouts as
      the ROM uses them (`NewtGlobals.graf` at +0C), never a name that
      Apple's `includes/` has. Sources compile with `-I includes -I src`;
      no declarations in the `.cc` files.
      Next: more files. Find the original files' extents (function order,
      literal pools and strings, static data), start from the port's C++
      and newton-re's `src/` (both cite ROM addresses), turned into 32-bit
      code with Apple's headers; missing headers reconstructed in `src/`.
      Track how many bytes come from source (2026-10-02, after the fax
      codec: 12,708 bytes of code and constant data, 16 of RW data, from
      3 sources; after the ADSP connection: 16,444 bytes from 4 sources;
      after the telephony options and the agent reporter: 18,328 bytes from
      6 sources; after TArray: 19,920 bytes from 7; the whole image
      identical). Next: `TCommServer` (0x209654), then `TController`;
      `TArbiter` (0x206BF0..0x208E98, recognition, 183 virtual calls) is
      left for when the recognition headers are further along. Zero-initialised data (`C$$zidata`) and vtables from
      source: not yet.
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
touched, text files read into memory when opened (2026-10-02).
Open (2026-10-02): `ARMCpp` crashes ("pc out of bounds: 0x73206572", text
as a pc) after it has written the object, when its summary line `###
"file": n warnings (+ m suppressed), ...` names a long path (an absolute
Unix path, about 160 characters in all; the same file given as a relative
path is fine). The CMake build now compiles with relative paths.
