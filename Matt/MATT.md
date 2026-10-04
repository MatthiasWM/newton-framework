

Releasing VSNewt into the world:

- Run Matt/tools/build_vsnewt_newtc.sh in newton-framework to put a fresh
  newtc into VSNewt.
- If the built-in function list changed, run python3 scripts/make_grammar.py
  in VSNewt.
- Raise version in package.json and add a CHANGELOG entry.
- Run npm run package, then tag and create a GitHub release with the
  new VSIX, as this time.

----


# 1. Rebuild
cmake --build build/VSCode --target newtc -j4

# 2. Quick regression check (12-package sample) — catches obvious breaks fast,
#    before spending time on the full corpus sweep
mkdir -p /tmp/regress_check
i=0; while read -r pkg; do
  [ -z "$pkg" ] && continue
  case "$pkg" in \#*) continue;; esac
  i=$((i+1))
  ./build/VSCode/newtc -pkg "$pkg" -decompile > "/tmp/regress_check/$i.out" 2>&1 </dev/null
done < /tmp/regress_list.txt
for i in $(seq 1 12); do
  diff -q "/tmp/regress_foreach_combined_fix/$i.out" "/tmp/regress_check/$i.out" > /dev/null 2>&1 || echo "DIFFERS: file $i"
done

# 3. Preserve the current manifest as "before", then run the full corpus sweep
#    (this overwrites Test/corpus_results/latest_manifest.json and
#    latest_summary.txt, and also writes a fresh timestamped copy under
#    Test/corpus_results/<timestamp>/)
cp Test/corpus_results/latest_manifest.json /tmp/manifest_before_printdependents_fix.json
python3 Test/run_corpus.py --jobs 16 --timeout 20

# 4. Compare old vs new — this is the actual regression check
python3 Test/run_corpus.py --compare /tmp/manifest_before_printdependents_fix.json Test/corpus_results/latest_manifest.json

# 5. (optional but recommended) Tier 2 self-consistency spot-check
python3 Test/round_trip.py --batch Test/corpus_results/latest_manifest.json --limit 200 --jobs 12


### REPL style Debugger ###

You have two source situations, and both point the same way: for packages you extract/decompile from the original ROM, you can't (and shouldn't) touch the package binary — it needs to stay byte-identical to the original for compatibility testing. For packages you compile yourself with newtc, you could embed a debug segment, but keeping the format identical either way means one code path in the debugger instead of two, and it keeps release builds trivially strippable (just don't emit the side-car). So: Foo.pkg + Foo.nsdbg next to it, same base name.

Two robustness details worth building in from day one since these files will drift apart over time as you re-decompile things:

Store a checksum of the package's code segment inside the .nsdbg file, and have the debugger refuse (or warn loudly) if it doesn't match the loaded package — cheap insurance against silently misattributing lines after a re-decompile.
Key by that checksum rather than (or in addition to) filename if you ever expect packages to get renamed/moved independently of their debug info — basically the debuginfod/build-id idea, though for a personal tool, filename pairing is probably enough to start.

Format: don't reach for DWARF or JS-style source maps — you need much less than either. Both are designed for interop with a broad tooling ecosystem you don't need to interoperate with; a minimal custom binary table will be smaller and simpler to write/read than shoehorning your data into either.

The standard trick (same idea as DWARF's line program, Python's co_linetable) is a delta-encoded, PC-ordered table, because within a statement many consecutive bytecodes share the same line:

entry := pc_delta: uleb128, line_delta: zigzag-leb128, file_delta: uleb128 (0 = same file)

Store the sequence sorted by PC, plus a small trailing string table for filenames referenced by index. To resolve PC → (file, line) at debug time, decode once into a flat sorted array in memory and binary-search it — at package sizes up to 4MB you're talking a few thousand instructions at most, so the whole table is tens of KB uncompressed and decode is instant. No need for anything fancier (no column info, no is-statement/basic-block flags like DWARF carries — you don't need those for a source-line stepping debugger).

The nice part: you already have the infrastructure for this from the compiler-diagnostics discussion — your codegen already knows the current source position at every emit call. Just append a table entry whenever that position differs from the last one you recorded; it falls out of the existing pipeline almost for free and is naturally run-length-friendly (most instructions produce a zero line-delta).


Our package or streaming object can be compiled from multiple files. So we need
map the file and line number to a function and pc when storing debug data. We need
to map the function and pc to a memory address, so we can set breakpoints.

During debugging, we also need to map the memory address to a file and line
number, so we can display the source code.

PCs are offsets to the start of binary data, so all we need to store is the start
and size of a bytecode block. We can then use the pc to index into the
binary data and find the correct line.

../../newtonresearch/newton-toolkit/NTX/Context/Engineering/platform.Dump.ns

See: "/Users/matt/dev/Newton/NTK 1.6.4b3/NTK 1.6.4b3/Newton Debug Tools 2.2/NS Debug Tools.pkg"

FindBreakLoop();
      loc1 := self:CurrentStack();
      loc2 := loc1:temporary(loc1:FindBreakLoop() - 1 - arg0, arg1);


GloballyEnableBreakpoints() -> use "slow" interpreter
BreakLoop()
BreakOnThrows()
STackTrace()
Where()
GetCurrentPC()
GetCurrentFunction()
point := InstallBreakPoint(Debug("mySlider").changedSlider,0);
RemoveBreakPoint();
RemoveAllBreakPoints();
EnableBreakPoint();
SetBreakPointLabel();
GloballyEnableBreakPoints();
NSDBreakLoopEntry(), NSDBreakLoopExit -> callback for conditional breakpoints

StackTrace, GetCurrentFunction,
GetCurrentPC, SetCurrentPC, Where, GetAllTempVars,
GetTempVar, SetTempVar, GetAllNamedVars, GetNamedVar,
SetNamedVar, GetCurrentReceiver, and GetCurrentImplementor
GetPathToSlot and GetPathWhereSet
Disasm

Step, StepIn, StepOut, RunUntil

The stepping functions create temporary break points, which are removed as
soon as they're used.

gFramesBreakPoints

SetBreakPoints(Ref bps)
{ programCounter: [
    {
      programCounter: 1000;
      instructions: ; -> ref to binary data block
      disabled: true/NIL;
      temporary: true/NIL;
    }
  ]
}
CInterpreter::handleBreakPoints() is never called


Types of binary/source links:
We always need a map that converts a function addrss and a PC into a
source filename and a line number - and back.

ROM Code (compile time assembler) stays in order, so we can decompile
any part of the code, generate source files, and use the offset of the
function to the start of the ROM blob to identify the function. The works
even if the ROM lob was relocated when the app was loaded.

Decompiled package: if we only decompile a package and never recompile it,
the order in which the functions are found when walking the root object(s)
should be the same when decompiling and when loading a package into RAM.
We store the index of the function, and the package loader restores the
address of the function or the lookup map.

Compiled package and NSOF: the function lookup must be written at compile
time. Offsets don't work, and a running index will probably not work either
because frames may cahnge the order of element when growing. We can ask the
compiler to add an index or a unique ID to every function it generates and
store them inside the function frame. Again the package loader shold probably
be where we restore the adress lookup when we find the additional information.
Question: where in the function frame can we store this in a compatible way?
See: "debuggerInfo" written by NTK in debug mode.

So we need a function and PC lookup file for every file taht we read and for
the ROM blob. One binary can be composed of multiple source files, and we
have three ways to encode the function address as described above.

We need function address plus PC lookup to file index and line number
We need file index plus line number to function address and PC.

So every file first lists the source code files it references, so we can index them.
Then we need an entry for every known function, its encoded address in memeory
(and the type of encoding as above), and a PC to line lookup (we assume that
functions can not spread across files). We always have more PCs than line numbers,
so we should only store a PC/line pair when the line number changes.

How to store this at runtime in memeory is still to be determined. Since we
defiend infinite RAM, we can store this uncompressed in array. OTOH, access
time is alo not an issue, so we could just as well keep the compressed data
and parse it on request.



At some point we want to decompile the ROM itself, produce one or more
source code files, and save debug information, so we can single-step
through the ROM source code.

A: we can rewrite ResMaker to write the NewtonScript data as a NSOF or PKG and
disassemble that in debug mode, and the compile it again. Problem is, we lose
all machine code labels.

B: we decompile the ROM in place before the Object System is initialized. The
debug information can not be in place, but must be stored in an external map.
This is good because it does not touch the ROM. We need to make sure that there
are no unreachable ends though, and we decompile the entire thing.

C: we could modify ResMaker to add a __ntDebug field to every function when
it writes out the code. The field is then later patched to hold debug data
internally. Somewhat critical as it changes the ROM at compile time, but not
at run time (addresses don;t move). We gain not needing a live debug file (we
can launch the debugger without needing to decompile every time).


Resolution: external lookup file seems fine. For the ROM, we don;t need absolute
addresses, we jsut need to store the offset to the start of the ROM data. So
we only need to decompile once and generate an external file with no absolute
addresses.

Coverage (DON'T FORGET THE REX!):

GetRoot, GetGlobal, GetStores
Global variables are reachable via RA(gVarFrame) == Magic(1.1)
Global Functions: gFunctionFrame == Magic(1.2)
gConstantsFrame: FDefineGlobalConstant()
gConstFuncFrame: temporary for compiler support
Other Globals?
There is a var in assmbler "symbolTable" and "builtinFunctions_map"
Magic Pointers are reachable via magic lookup table.
InitRExMagicPointerTables(void), gRExImportTables
ResolveMagicPtr(Ref r): gROMMagicPointerTable

Calling native functions from ROM:
NSFn_NewWeakArray {
  class NSNativeFunc
  funcPtr NSNativePtr(C-func-addr)
  numArgs: 1
}

Ref FGetRoot(RefArg rcvr);

TView:Constructor() -> TView::SetupForm()
virtaul call: r1 = vtable,  ld lr, pc ; add     pc, r1, #52

Fonts:

Key Newton Fonts
- Espy Sans (System): The primary UI font built into Newton OS, roughly equivalent to what Geneva or Chicago was to early Mac OS.
 - Simple: A standard, highly legible bitmap font comparable to Geneva.
 - Condensed Gothic: A narrow bitmap font used for high-density information display.
 - Newton Casual: A specialty font styled to mimic clean comic-book handwriting, designed to make handwriting recognition outputs easier for users to read.

Yes, I want to embed the four Newton fonts into my Newton environment simulator. I need a replacement for 	System (Espy sans), Fancy (New York), Simple (Geneva), and HWFont (Casual)

Carthage Sans: https://github.com/csyde/carthage-fonts (probably not!)
https://www.scootergraphics.com/nusans/

https://www.gust.org.pl/projects/e-foundry/tex-gyre/schola
or https://fonts.google.com/specimen/Newsreader?preview.script=Latn

https://github.com/liberationfonts/liberation-fonts
or https://rsms.me/inter/

https://github.com/crozynski/comicneue


Apple's Espy Sans 12 : https://www.google.com/url?sa=i&source=web&rct=j&url=https://github.com/csyde/carthage-fonts

"Nu" font family (Nu Casual / Nu Sans / Nu Serif / Nu Sans Mono) by Marty P. Pfeiffer / Scooter Graphics, copyright 2000

https://fonts.google.com/specimen/Short+Stack?preview.script=Latn

https://www.scootergraphics.com/nupack/index.html


NewtPlay:
- Play -> file chooser -> package
- History -> list of previously run packages
- Make Link
- Make App
- About, Credits, basic help
- Learn NewtonScript
- Give stars for Emulation, App Quality, Relevane, Playability?
- Resources
- Locate Soup in Finder
- Convert to .nspkg...

Starting the PCMCIA card:

I didn't change any code for this; the answer comes from how the ROM's card server brings a card up and how its CIS parser reads it. The card server owns the sockets, so the cleanest place to do this is inside a **card handler**: `RecognizeCard()` gets the `TCardSocket*` with the card already powered. As standalone native code you can get the same objects with `GetSocketInfo()`.

**Call sequence** (MP2x00 ROM addresses)

1. **Get the socket.** `GetSocketInfo(socketNo, &socket, &cardPCMCIA)` (0x000544EC).
2. **Default bus setup.** `socket->SetDefaultConfig()` (0x000554C8). The card server calls this through `InitializePCMCIABus`. It sets:
   - 300 ns attribute, common and I/O speeds;
   - memory interface, write FIFO flushed;
   - Vcc 5 V, pullups 0x1C00;
   - `SetControl(0x0A)` (auto-increment + 32-bit assembly).
3. **Pick the voltage.** Do what `SelectCardCISPower` does:
   - `GetVPCPins() & 0x30` = 0x30 is a 5 V card; `SelectVoltageLevel(kSocketPowerLevelVcc5V)`.
   - 0x00 or 0x20 is a 3.3 V card; `SelectVoltageLevel(kSocketPowerLevelVcc3p3V)`.
   - Then `SelectVoltageLevel(kSocketPowerLevelVpp | level)` so Vpp follows Vcc. Never select 12 V Vpp just to read.
4. **Power on.** Use the global `VccOn(socketNo, 0)` (0x00050A78), not the method of the same name. It counts power users, calls `TCardSocket::VccOn`, sleeps for the Vcc rise time, then calls `EnableBus()`.
5. **Reset.** `socket->PCMCIAReset()` (0x00055618) pulses RESET for 80 ticks. Then wait about 50 ms; the card server waits that long before reading the CIS.
6. **Wait states.**
   - `SetAttributeMemSpeed(300)`, or 600 for 3.3 V cards as the ROM does.
   - `SetCommonMemSpeed(ns)`: use the speed from the card's CIS device tuple, or a safe 600 for unknown ROM cards. The register holds at most 63 wait states, so 600 ns is reachable.
   - Stay on the memory interface (don't call `SelectIOInterface`).
7. **Map the windows.** Attribute memory is at `AttributeMemBaseAddr()` (socket base + 0); common memory is at `CommonMemBaseAddr()` (socket base + 0x08000000). Each window is 64 MB.
   - These virtual addresses only exist in the card domain. Outside it, call `MakeSocketAccessible(base, size)` (0x00055D20), or build your own mapping with `CreateSocketPhys(&phys, offset, size, true)` (0x00055FF4).
   - Wrap all card accesses in an exception handler, as the ROM does. Reading past the end of the card, or a pulled card, gives a bus error.
8. **Read attribute memory.** Only even card addresses hold data. The ROM's CIS reader:
   - clears byte access first: `SetControl(GetControl() & ~kCardByteAccess)`;
   - reads byte *i* with `CardAttrMemReadByte((attrBase + 2*i) ^ 3)` (0x0004ECEC). The `^ 3` corrects for the endian conversion, and the function adds the required delay between reads.
9. **Read common memory.** With the default control (32-bit assembly, endian conversion on), aligned `ldr` reads return the card bytes in order. Check this once against a card whose header you know. For single 16-bit reads at odd half-word addresses, use `Do16BitRead(addr)` (0x00055AA0).
10. **Power off.** Call the global `VccOff(socketNo)`, which balances step 4.

**Card size.** No register reports it. Take it from the CIS device tuple (`TCardPCMCIA::GetCardDevice`). For cards without a usable CIS, read until the data starts repeating: smaller cards usually mirror themselves across the window, which is the same trick the boot code uses to size RAM.

**Caveats**
- After recognition the card server may switch off a card it doesn't recognise (`VccOff`), and it reacts to the lock switch. Run inside a handler or suspend the socket, otherwise it can cut power in the middle of a dump.
- On flash cards, never write to common memory. Some flash chips interpret writes as commands.
- `GetSocketInfo`, the global `VccOn`/`VccOff`, `SetDefaultConfig`, `MakeSocketAccessible` and `CardAttrMemReadByte` are not in the DDK headers, so you'd need glue that jumps to the ROM addresses above. Everything else is declared in `CardSocket.h`.




Yes. The ROM contains a complete low-level debugger stub that works over the serial port. It is separate from the NTK and Hammer NewtonScript translators: it is the target end of what the TBasicSerialPortManager.cpp notes call "Hammer Commands". I only read the ROM; nothing in the repo changed.

**How it starts**
- **At boot:** `InitializeCommHardware` reads `gDebuggerBits`, a word stored inside the ROM at 0x13F4.
  - Bit 0x20 starts `TAsyncDebugLink` on `'extr'` at 57600 baud.
  - Bit 0x40 starts `TGeoPortDebugLink` (`'gpdl'`) at 57600 baud. That one supports async or HDLC framing and detects a host by a beacon.
  - **Your ROM image has 0x40 set.** So it pings for a GeoPort debugger three times on every boot (about 100 ticks each) and gives up if nothing answers. This may be a debug or patched image; a stock ROM may have 0 there.
  - Bit 0x01 means a hardware debugger is attached. `FIQHandler` then passes the reset switch to `DebuggerIntHandler`, and `IsDebuggerPresent()` returns true.
- **On a crash:** `ReportException` calls `XInitSerialDebugging` if a debug link (`gDebugLink`) was set up earlier.
- **From NewtonScript:** `InitSerialDebugging(mode, port)` (`FInitSerialDebugging`):
  - 0 stops the link;
  - 1 is a "pre-init" at 57600 baud;
  - 10 or more is taken as the baud rate, for example `InitSerialDebugging(57600, "extr")`.
  - `FPreInitSerialDebugging`, `FSetDebugMode` and `FMakeWedge` are related.
- `HandleDebugCard`, called from `ROMBoot`, is an empty stub in this ROM.

**Protocol**
- **Framing (async link):** SYN 0x16, DLE 0x10, STX 0x02, data with DLE doubling, DLE ETX 0x03, then CRC-16. This is the same scheme as MNP docking.
- **Commands** (table `gRDPHandlers` at ROM 0x003773E0, numbers in decimal):

| # | Command | # | Command |
|---|---|---|---|
| 0 | Open | 24 | ReadPhysicalMemory |
| 1 | Close | 25 | WritePhysicalMemory |
| 2 | ReadMemory | 26 | ROMBkpt |
| 3 | WriteMemory | 27 | Stop |
| 8 | ReadARM600Registers | 28 | Go |
| 9 | WriteARM600Registers | 29 | XOpen |
| 16 | Execute | 30 | DummyGeoPortResponse |
| 18 | Info | 31 | DownloadBootLoader |
| | | 32 | ImageCommand |

  All other numbers are "not implemented" or "undefined".
- **Support calls:**
  - `RegisterPackageWithDebugger` and `RegisterLoadedCodeWithDebugger` tell the host where packages and code were loaded.
  - `DebugStr`, `SendDebugLine` and `AsmSendDebugLine` send text.
  - `GetPagesForROMBkpts` lets it set breakpoints in ROM by remapping pages.

**Other hidden diagnostic hooks at boot** (not serial)
- `DiagBootStub` checks the start of internal flash and of PCMCIA common memory in sockets 0 and 1 for the signature `"DIS_"`. If found, it jumps into that code and passes `'FROM' 'DIAG' 'STUB'` plus `'FLSH'`, `'PCC1'` or `'PCC2'` to say where it booted from. Your replacement-card programmer could use this to boot diagnostic code from a card.
- A boot routine at 0x19CA4 toggles a 'mdem' serial register (0x0F1F2800, probably DTR) and watches the DCD line to detect a loopback plug. Its result is ignored in this ROM: both branches continue at the same place.

For Einstein, this means a host could connect to the emulated `'extr'` port and use the built-in commands to read and write memory and registers. It would also explain unexpected traffic on that port at boot whenever bit 0x40 is set.


I've read the whole handshake, and it is fully readable from the ROM. No code changed.

**Port.** The GeoPort debug link uses the **`'extr'` serial port** (Voyager serial base 0x0F1C0000), the same port used for docking. On the MP2x00 that is the serial lines of the Interconnect port; on the eMate it is the Mini-DIN-8. `GrabSerialPort` takes the chip from the serial chip registry by location `'extr'`, powers subsystem 1 and installs its own interrupt handler on CPU interrupt 0x00020000.

**Getting into GeoPort mode** (`TGeoPortDebugLink::Install`, ROM 0x000DB6AC):

1. **Beacon on CTS.** The Newton sets the port to async 9600 8N1 (divisor 0x0B, using the driver's formula 3686400 / (baud × 32) − 1). It then polls CTS (status register 0x4400, bit 0x08) with this sequence:
   - CTS deasserted;
   - CTS asserted for **at least 50 µs** (185 ticks);
   - CTS deasserted again **within 50 ms**.

   A shorter or longer pulse resets the detector. At boot, `InitSerialDebugging` gives this about 0.5 s (0x1C1F38 ticks), so the host should keep sending pulses while the Newton starts. CTS is the handshake input (HSKi on a Mac-style Mini-DIN). Which host line drives it depends on the cable; with the usual Newton/Mac cable it is the host's HSKo/DTR.
2. **The Newton answers at 9600 baud.** It sets bit 0 of serial register 0x2800 (probably the TxD driver enable) and sends:
   `A5 5A 00 C8 03 F0 BA`
   The last byte is an 8-bit sum of the bytes before it. The middle four bytes are a parameter from `InitSerialDebugging` (0x00C803F0) whose meaning I don't know.
3. **The host replies with 13 bytes at 9600 baud.**
   - Byte 2 must be **0xAA**.
   - Byte 3 is a flag.
   - Bytes 4–7 are the requested bit rate (big-endian).
   - Bytes 8–9 and 10–11 are the maximum frame sizes in each direction (limited to 1023).
   - Byte 12 is the 8-bit sum of bytes 0–11.

   If CTS goes active again while the reply is coming in, the Newton starts over at step 1.
4. **The Newton confirms.** It picks the fastest rate from its table that doesn't exceed the requested rate: 1843200, 921600, 614400, 460800, 230400, 115200, 57600 or 38400 bit/s (ROM table at 0x371614). It sends `00 00 <rate:4> <size:2> <size:2> <sum>`, switches the serial chip to **HDLC** at that rate, waits about 10 ms and enables its interrupt.

   The HDLC clock is internal: 3.6864 MHz / 2 / (n + 1), with n written to register 0x5400. A requested rate between 0x1C0000 and 0x1C2000 (about 1.84 Mbit/s) sets an extra internal flag; it might mean an external clock, but I couldn't confirm that.
5. **Debug traffic.** From then on, the debug frames (the rdp commands from before) travel as HDLC frames.

A host only needs RS-232/RS-422 levels, a way to pulse its handshake output, and 9600-baud async for the handshake. After that it needs a serial port that can do synchronous HDLC at one of the rates above, which is the original GeoPort/Mac SCC feature. The plain async link (`TAsyncDebugLink`, `gDebuggerBits` 0x20, or `InitSerialDebugging(57600, "extr")` from NewtonScript) avoids HDLC: it uses MNP-style framing over normal async serial, so a PC with a USB serial adapter could talk to it.



Your ROM file is unpatched: `BeaconDetect` still has its original start, `gDebuggerBits` is 0x40 and `gNewtConfig` is 0. Einstein had to patch the beacon probe out, which is strong evidence that **stock MP2x00 ROMs ship with 0x40 set** and probe for the GeoPort debugger on every boot. Your Newton very likely does this as it is.

**Caveats**
- I've only seen this one ROM version. Other dumps (eMate 300, other MP2x00 regional versions, and the Watson ROM that Einstein's patch also targets) may store a different value, possibly at a different address. Their symbol tables would show the address; it is 0x13F4 in this one.
- The window is short: about 0.5 s during `InitializeCommHardware`, and only at boot or restart.

**Testing on real hardware, without opening it or dumping its ROM**
1. Set your RP2350 dongle to 9600 8N1 and keep pulsing the Newton's CTS line: assert for roughly 1–10 ms, deassert, repeat. Each pulse must last at least 50 µs and less than 50 ms.
2. Restart the Newton with the reset button.
3. If the flag is set, the Newton replies with `A5 5A 00 C8 03 F0 BA`. That reply already proves the stub is active.
4. To go further, send the 13-byte reply: byte 2 = 0xAA, then rate, frame sizes and checksum. Then switch to HDLC at the rate the Newton confirms. The RP2350's PIO should be able to handle the HDLC framing and bit-stuffing.

**Einstein's other patch is probably described wrongly.** Its comment says writing 1 to `gDebuggerBits` "seems to disable runtime debugging statistics". According to the ROM, bit 0x01 means "hardware debugger attached":
- `IsDebuggerPresent()` returns true;
- packages and loaded code get registered with the debugger;
- the reset-switch FIQ is passed to `DebuggerIntHandler` instead of resetting.

Should I correct those two comments in TJITGenericROMPatch.cpp?


