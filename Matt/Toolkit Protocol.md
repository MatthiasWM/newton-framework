# The NTK Toolkit (Inspector) protocol

How NTK's Inspector talks to a Newton, as the base for remote debugging in
`newtc -dap` (plan: Matt/CLAUDE.md). Sources: the ROM (`newtonos.s`, ROM
717006), its port in `NTK/NTK.cc`, DyneTK `Flio_Inspector.cxx`, NTX
`~/dev/newton-toolkit/NTX/Comms`. "ROM" below means verified in the
disassembly; "to confirm" means it still needs a capture (step 1.5).

## Contents
1. Layers
2. Device side: the NTK nub (ROM)  <- step 1.1
3. Toolkit.pkg                      <- step 1.2
4. Desktop side                     <- step 1.3
5. MNP                              <- step 1.4
6. Captured sessions                <- step 1.5
7. Remote tools                     <- step 2

---

## 1. Layers

```
NewtonScript REP (break loop, gREPin/gREPout)
PNTKInTranslator / PNTKOutTranslator    the REP's I/O while NTK is connected
TNTKNub (gNTKNub)                       commands, packets
CTaskSafeRingBuffer x2 (512 bytes)      between the REP task and the 'ntk ' task
CNTKTask / CNTKEndpointClient           an endpoint: serial MNP, ADSP, ...
wire                                    MNP over serial (also Einstein)
```

## 2. Device side: the NTK nub (ROM)

### 2.1 Starting and stopping

NewtonScript entry points (ROM natives; the Toolkit app calls them):

| Function | ROM | What it does |
|---|---|---|
| `NTKListener(start, connectionKind, machineName, inTranslator, outTranslator)` | 0x12C318 | start: create the nub, `StartListener`; else stop it. Returns an error code (-1: nub already exists / doesn't exist). |
| `SetUpTetheredListener(start, kind, machine)` | | the same with the default translators |
| `NTKDownload(kind, machine, in, out)` / `pkgDownload(kind, machine)` | 0x12C3DC | one-off package download (Toolkit's "Download Package"), nub torn down after |
| `NTKSend(obj)` | 0x12C45C | send `obj` to the desktop as `fobj` (NSOF). -1 if not connected. |
| `NTKAlive()` | 0x12C498 | true while the nub exists |

`connectionKind` (from `CreateNub`, the option code is `#if 0` in the
port): 0 plain serial (Ez options), 1/2 ADSP (AppleTalk; `machineName` a
string = the Mac to look up), 3 MNP serial; a frame = endpoint options
(`EzConvertOptions`). If `machineName` is non-nil but not a string, the
nub uses `PSerialIn/OutTranslator` instead: a **plain text REPL** (lines of
NewtonScript source, parsed on the Newton with `ParseString`; output as
text, LF after CR). Not what NTK uses, but a fallback worth remembering.

`StartListener` (ROM 0x12ADBC): saves `gREPin`/`gREPout`, installs the
NTK translators, `ResetREPIdler()`, then sends `cnnt` (length 0) and
waits for `okln`. **`okln` must have length 0** (ROM checks it; anything
else is `kDockErrBadHeader` -28016). The port doesn't check the length.

`StopListener`: sends `term` (length 0) if connected. `NTKShutdown(err)`
(any I/O error) stops and deletes the nub; an error other than 0, -1 and
-36006 is shown as an alert on the Newton.

### 2.2 Packets

Every packet starts with a 16-byte header, all words big-endian, **no
padding** (unlike the dock protocol, which pads to 4 bytes):

```
'newt' 'ntp ' <command, 4 chars> <length, u32>  <data>
```

Some packets put other data where the length goes (noted below).
A packet from the desktop whose first 8 bytes aren't `newt` `ntp ` is
`kDockErrBadHeader` (-28016).

Timeouts: 30 s per read/write by default, retried every 50 ms; `stou`
changes it. The input buffer is 512 bytes.

### 2.3 Desktop -> Newton

The nub reads commands in `PNTKInTranslator::Idle()` (ROM 0x129FC8): the
REP calls it when NewtonScript is idle **or waiting in a break loop**. If
no frame is pending and input is waiting, `TNTKNub::DoCommand()` (ROM
0x12B2D4) reads one command. Its result: < 0 shuts the connection down,
> 0 means "a frame is available" (the REP then calls `ProduceFrame`).

| Cmd | Data | Newton does | Reply |
|---|---|---|---|
| `okln` | none, length 0 | only as the answer to `cnnt` | - |
| `lscb` | NSOF of a compiled code block (length = its size, but the reader doesn't use it) | sets "frame available"; the REP then calls `ProduceFrame`, which reads the NSOF from the stream, replies `rslt 0`, and the REP **evaluates it like typed input**: in the break loop if one is active, results printed as `text` | `rslt` 0, then `text` output |
| `code` | u32 (read and ignored), then NSOF of a code block | `HandleCodeBlock` (ROM 0x12B428): reads the NSOF (`ProduceFrame`, which replies `rslt 0`), `InterpretBlock(code, gREPContext)`, sends the result back | `rslt` 0, then `code` with **length = the request's length** (not the result's), then u32 NSOF size, then NSOF of the result |
| `pkg ` | the package (length = its size) | `LoadPackage`: `NewPackage(pipe, GetDefaultStore(), nil, 0)` reads the package from the stream | `rslt` err |
| `pkgX` | package name, UTF-16 with terminator (length = bytes) | finds the package by name (`TPMIterator`, `Ustrcmp`) and calls `RemovePackage(id)`; no error if not found | `rslt` 0 |
| `stou` | u32 seconds | sets the in and out timeouts | `rslt` 0 |
| `term` | none | marks disconnected, returns -1, the nub shuts down (no alert) | none |
| other | | | `rslt` -28016 |

Notes:
- **`code` is the remote procedure call we want**: a structured result
  (NSOF), no text parsing. But an exception inside it returns its error
  to `Idle()`, and a negative one **shuts the connection down**. So the
  code blocks we send must catch everything themselves (`try ... onexception
  |evt.ex| do ...`) and return the error as data.
- `lscb` is what NTK's Inspector window uses (typed NewtonScript,
  compiled on the Mac). It runs in the REP's break loop context, so it is
  how breakpoints, `ExitBreakLoop()`, and NS Debug Tools commands are
  driven while stopped. To confirm in 1.5: whether `code` also works
  while in a break loop (it should: the same Idle path).
- Code blocks are compiled on the desktop. newtc's compiler is the ROM's,
  so its NOS 2 bytecode is exactly what the Newton runs. Strings and
  symbols are NSOF (UTF-16 strings, ASCII symbols).

### 2.4 Newton -> desktop

| Cmd | Length field | Data | Sent when |
|---|---|---|---|
| `cnnt` | 0 | none | connecting (`StartListener`) |
| `rslt` | 4 | i32 error (0 = OK) | after each command; after reading a code block |
| `text` | n (1..255) | n bytes of text, MacRoman, CR line ends | REP output: `Print`, `Write`, results, prompts are empty. Buffered in 255 bytes; flushed at a CR, when full, on `flush()`. A single `vsprintf` over 255 bytes throws `kNSErrStringTooBig`. |
| `eext` | 0 | none | the REP enters a break loop (`EnterBreakLoop(level)`; the level is not sent) |
| `bext` | 0 | none | the REP leaves a break loop |
| `fobj` | NSOF size | NSOF | `NTKSend(obj)` (`SendRef`) |
| `fstk` | NSOF size | NSOF: array of stack frames, newest first | `StackTrace()` / the REP's stack trace (see 2.5) |
| `code` | request's length | u32 NSOF size, NSOF | answer to `code` |
| `estr` | total | u32 nameLen, name (C string, with NUL), u32 msgLen, message (with NUL) | exception with a C string message (`exMsgException` and subclasses) |
| `eref` | total | u32 nameLen, name, u32 NSOF size, NSOF of the exception data (e.g. `{errorCode: -48807, value: 'x}`) | exception whose data is a Ref (`exRefException` and subclasses: all NewtonScript `evt.ex.fr.*` errors) |
| `eerr` | total | u32 nameLen, name, i32 error code | any other exception |
| `teom` | 0 | none | `SendEOM`; no caller found yet |
| `dpkg` | 0 | none | `NTKDownload`: "ready, send the package" (repeated after `pkgX`/`stou`) |
| `term` | 0 | none | disconnecting |

`fobj`, `fstk`, `eref` etc. are written as header + command + (the size
word, which sits where the length goes) + data: the packet layout is the
same, only the producer differs. Note the "total" of the exception
packets: it counts the name and the message/NSOF, not the u32 count
words, so the data after the header is total + 4 (`eerr`) or total + 8
(`estr`, `eref`) bytes (confirmed in 6.1).

The exception name is the C exception symbol, e.g. `evt.ex.fr.intrp`.

### 2.5 `fstk`: the stack trace

`NTKStackTrace(interpreter)` (ROM 0x2D3510) builds an array with one
frame per stack frame (`TNSDebugAPI`, newest first) from
`NTKStackFrameInfo(debugAPI, i)` (ROM 0x2D31D0) and sends it as `fstk`.
Each frame (names to confirm in 1.5, the calls are certain):
- `codeBlock`: the function (`TNSDebugAPI::Function`), or its debug
  name (`FunctionDebugName`) when it is native;
- a name for it: `FindSlotName(implementor, fn)`, else
  `SearchForObjectName(fn)`;
- `programCounter` (`TNSDebugAPI::PC`);
- `receiver`, named through the receiver's `debuggerInfo` / `debug`
  slot or `SearchForObjectName`.
This is NTK's stack window. For us: one packet gives the whole stack with
function refs and PCs, which is what DAP's `stackTrace` needs. In the port
(`NTK/NTK.cc` `NTKStackTrace`) the frame building is `#if 0`.

### 2.6 Typical exchanges

```
Connect:      N: cnnt          D: okln (len 0)
Evaluate:     D: lscb <NSOF>   N: rslt 0, text "5", ...
Call:         D: code <u32><NSOF>  N: rslt 0, code <size><NSOF result>
Break:        N: text "...", eext       (Newton waits in the break loop)
  while stopped: D: lscb/code ...   N: rslt, text/code
  continue:   D: lscb ExitBreakLoop()   N: rslt 0, bext
Exception:    N: eref/estr/eerr (then eext if breakOnThrows)
Package:      D: pkgX <name>   N: rslt 0
              D: pkg  <package>  N: rslt err
Disconnect:   D: term (or N: term)
```

### 2.7 Differences between the port (`NTK/NTK.cc`) and the ROM

To fix when the port is used for `-ntk-device` (step 3.4):
- `DoCommand`: the port sends `rslt` after `code`; the ROM doesn't (the
  `rslt` comes from `ProduceFrame`, the answer is the `code` packet).
- `StartListener` doesn't check that `okln` has length 0.
- `ExceptionNotify`: the `eref` branch is commented out (ROM sends it).
- `NTKStackTrace`: the frame array is `#if 0` (sends nil).
- `readHeader`/`sendCommand` and friends use `size_t` (8 bytes on a 64-bit
  host) where the ROM uses 32-bit words, and the host byte order: on the
  wire everything is big-endian u32.
- `DeletePackage`: the search is `#if 0`, and the name buffer is never
  freed (ROM too).

## 3. Toolkit.pkg

NTK 1.6.4's `Toolkit.pkg` ("Toolkit" 1.3, app symbol `'newtoolspro`, one
form part, 48936 bytes). Decompiled with `newtc -pkg Toolkit.pkg
-odecompile Toolkit.ns` (1823 lines; a few unresolved nodes in
`SetExtrasIcon`, harmless). **It has no protocol code of its own**: the
nub is the ROM's (section 2). What it adds:

**Connecting.** The dialog's "Connect Inspector" calls
`ntpTetheredListener(not ntpState, connectionType, ntpMachineName)` (the
ROM's `SetUpTetheredListener`, default translators); "Download Package"
calls `ntpDownloadPackage(connectionType, ntpMachineName)`. For serial,
`connectionType` is **3 (MNP serial)** and the machine name nil; for
AppleTalk the user picks a host in a network chooser (preference
`connectionType: 'useSerial | 'useAppleTalk`, stored in the system soup
under tag "newtoolspro"). The call runs from a deferred action (it blocks
until the desktop answers `cnnt`). Errors the dialog explains: -10021 no
response (cables), -28011 "Your model of Newton does not work with this
version", -28009/-12314/-16013/-20006 connection problems, -20003
connection closed, -10402/-10409/-10410 package exists / newer / older.

**Right after connecting** it sends a hello:
`NTKSend({interpretation: 'dante, data: {}})` (an `fobj`; "Dante" was
NTK 1.6's code name, to confirm). While connected: the Extras icon shows
"open", a power-off handler keeps the Newton from sleeping
(`powerOffScript` returns nil while `ntpState`), and an idle check runs
every 5 s.

**Things NTK can call** (with `lscb`/`code`), all answering through
`TransmitDataToNTK(results, interpretation)` = `NTKSend({interpretation:,
data:})`:

| Call | Interpretation | What |
|---|---|---|
| `functions.\|ScreenShot:NTK\|()` | `'screenshot` (`'partialScreenShot` pieces when memory is short) | the screen as a bitmap, plus `machineInfo: Gestalt(0x1000003)` |
| `GetRoot().newtoolspro:\|UploadProfilerStats:NTK\|()` | `'profilerResults` | profiler statistics |
| `GetRoot().newtoolspro:\|ConfigProfiler:NTK\|(spec)` | `'profilerConfiguration` | configure the profiler (`{trace, memory, nTasks, scDetailed}`) |
| `GetRoot().newtoolspro:NTK_RPC(fn, args, interp)` | any | **generic RPC**: calls `fn` (with `args` if given) and sends the result |
| `GetRoot().newtoolspro:SetPrintDepth()` | | `printDepth := 1` |

On a ROM without `NTKSend` (1.x) the same data travels as an exception:
`Throw('|evt.ex.fr;type.ref.frame.bitmap|, data)` (also `.stats`,
`.config`) with `breakOnThrows` off, which reaches the desktop as `eref`.

**Native code** (`BinCFunction`s in one ARM blob, `Ref_33`):
`HasProfilerFn`, `EnableProfilingFn` (also installed as the global
`EnableProfiling`), `UploadProfilerStatsFn`, `ConfigProfilerFn` (the
profiler, with its "PCA" window), and `ScreenShotFn`. None of it is
needed for debugging.

**For us:** newtc as the desktop must answer `cnnt` with `okln`, expect
the `'dante` `fobj` right after, and can use `NTK_RPC` or plain `code`
blocks for its own calls. Our own agent package can do the same as
Toolkit.pkg (or we keep using Toolkit.pkg to connect, and upload only the
agent). Screenshots come for free (`|ScreenShot:NTK|`).

## 4. Desktop side

Four desktop implementations, compared with each other and with the ROM:

| | Source | Language | Evaluates with |
|---|---|---|---|
| NTK 1.6.4 | `Newton Toolkit.app` (PowerPC PEF; strings only, Ghidra project in `~/dev/NTK.ghidra` if ever needed) | | `lscb` |
| NTX | `~/dev/newton-toolkit/NTX/ToolkitProtocolController.mm` (Simon Bell, 2012) | ObjC++, the Newton Research framework | `code` |
| NewtonInspector | `~/dev/NewtonInspector` (Jake Borden; MNP in `NewtonConnection.m`) | ObjC, NEWT/0 | `lscb` |
| DyneTK | `~/dev/DyneE5/DyneTK/fltk/Flio_Inspector.cxx` (Matt, 2002-2007, sniffed) | C++, NEWT/0 | `lscb` |

### 4.1 Packet by packet

**Connect.** The Newton sends `cnnt`; the desktop answers `okln` with
length 0 (NTX, NewtonInspector: `newt ntp okln 00000000`), as the ROM
requires. DyneTK's extra bytes `00 01 00 00 00` are the odd one out
(probably a sniffing artifact, e.g. the next MNP frame's bytes): **send
length 0**. Then Toolkit.pkg's `fobj` `{interpretation: 'dante, data:
{}}` arrives (NTX hands it to its UI; NTK's binary has the strings
"dante"/"Dante").

**Evaluate with `lscb`** (NTK, NewtonInspector, DyneTK): `newt ntp lscb
<NSOF size>` + the NSOF of a compiled function (no padding). The Newton
answers `rslt 0`, then whatever the REP prints as `text`.
**Evaluate with `code`** (NTX): `newt ntp code <4 + size>` + u32 0 +
NSOF. The Newton answers `rslt 0`, then `code`, whose length NTX calls
"totally erroneous, just an echo of the kTCode evtLen sent" (confirms
section 2.3), then u32 size + NSOF of the result.

**What NTK sends.** NTK compiles NewtonScript source on the Mac (its
strings: `StackTrace();`, `ExitBreakLoop();`, `BreakOnThrows := true;`,
`trace := nil;`, `printdepth := `, `%s();`). DyneTK's captured bytes are
exactly these, compiled as NOS 1 functions (`class: 'CodeBlock`,
`argFrame` with `_nextArgFrame`/`_parent`/`_implementor`,
`DebuggerInfo`), decoded with `newtc -nsof x.nsof -decompile`:

| DyneTK capture | Function | NTK Inspector button |
|---|---|---|
| dataX0219 | `func() ExitBreakLoop()` | exit break loop |
| dataX0179 | `func() StackTrace()` | stack trace |
| dataX0141 | `func() printDepth := 0` | print depth |
| dataX0068 | `func() trace := nil` | trace off |
| dataX0018 | `func() breakOnThrows := true` | break on throws |

NTK also wraps an app's InstallScript for development
(`if not GetRoot().newtoolspro exists or not ...ntpState then ...Notify
"Beta NTK"...; p:?devInstallScript(p); ...`), which newtc already knows
from Phase 9.1b.

**Output.** `text` is MacRoman with CR line ends, at most 255 bytes per
packet (NTX reads it in 255-byte pieces anyway). `eext`/`bext` mark break
loop entry/exit; NTX counts the depth from them ("Entering break loop,
depth = n"; NTK's strings: "Entering break loop", "Exiting break loop").
`fstk` is an NSOF array (NewtonInspector prints it; NTX doesn't handle it;
NTK has "Stack trace:" and `stackTracePrintDepth`).

**Exceptions** (NTX, matching the ROM): `eerr` = u32 total (in the length
field), u32 nameLen, name with NUL, i32 error; `estr` = ..., u32 msgLen,
message with NUL; `eref` = ..., u32 NSOF size, NSOF (a frame with
`errorCode`, NTX prints it with the Frames error strings, e.g.
"Undefined global function: DefConst / evt.ex.fr.intrp;type.ref.frame /
-48808"). NewtonInspector and DyneTK show `eerr` as text, which is wrong
(it's binary).

**Packages.** `pkg ` + package size, then the package. All three wait
**1 s after the header** before sending the data (no reason found in the
ROM: `LoadPackage` reads straight from the pipe; maybe so the Newton can
set up the store first). NTX sends it in 4 KB chunks for its progress
bar. `pkgX` + the package name as UTF-16BE with a 2-byte NUL, length
including the NUL (NewtonInspector reads the name from the package
header; DyneTK converts ASCII); NewtonInspector deletes before every
install (its "watch package" feature), DyneTK waits 1 s after `pkgX`.

**Disconnect.** `term` with length 0 either way. NewtonInspector gives up
after 3 s if the Newton doesn't close the link.

### 4.2 Disagreements to settle with a capture (1.5; all settled in 6.1)

- **Padding.** NTX pads the data of every command it sends to a multiple
  of 4 bytes (zeroes after `code`'s NSOF and after a package). The ROM
  doesn't skip padding (`ReadHeader`/`ReadData` read exactly what they
  are asked for; NSOF and `NewPackage` read exactly the object), so the
  next header would start with zeroes and fail (-28016, which shuts the
  connection down). NewtonInspector and DyneTK don't pad. Plan: don't
  pad; check that a `code` with an odd-sized NSOF works twice in a row.
- **`code` while in a break loop.** Same Idle path as `lscb`, so it
  should work; and does `ExitBreakLoop()` work from `code`
  (`InterpretBlock` inside Idle) or only from `lscb`?
- **The 1 s pause** after `pkg `: needed, or habit?
- **Keep-alive.** NewtonInspector resends an MNP LA frame every 3 s when
  idle ("Prevents the Toolkit app from disconnecting"): an MNP timer,
  see section 5.

### 4.3 What newtc needs (summary)

- Answer `cnnt` with `okln` (length 0); accept the `'dante` `fobj`.
- Evaluate with `lscb` for REP-like input (break loop commands such as
  `ExitBreakLoop()`, output as text) and with `code` for calls that return
  data (u32 0 before the NSOF; ignore the reply's length; read u32 size +
  NSOF). Every `code` block catches its own exceptions (section 2.3).
- Compile on the host with newtc's compiler (NOS 2 functions are fine:
  the ROM runs them; NTK's NOS 1 `CodeBlock`s are just older).
- Read `text` (MacRoman -> UTF-8, CR -> LF), `rslt`, `eext`/`bext`
  (break loop depth), `fobj`/`fstk` (NSOF), `eerr`/`estr`/`eref`
  (exceptions), `term`.
- Packages: `pkgX` by name, then `pkg ` (1 s pause until 1.5 says
  otherwise), then wait for `rslt`.
- NSOF in and out with `Frames/RefIO` (`FlattenRef`/`UnflattenRef`).

## 5. MNP

The Toolkit packets travel over MNP (Microcom Networking Protocol, class
4: error correction, no compression), the same as the Dock. MNP is
hardly documented; it is part of ITU-T V.42 (annex). Sources compared:

| | Source | Window |
|---|---|---|
| unixnpi | `~/dev/unixnpi/newtmnp.c` (Richard Li et al.) | k = 1, stop-and-wait |
| NewtonInspector | `NewtonConnection.m` (derived from unixnpi) | k = 1 |
| NTX | `NTX/Comms/Endpoints/MNPSerialEndpoint.m` (Newton Research) | k = 1, with timers |
| DyneTK | `Flio_MNP4_Protocol.cxx` (Matt) | k = 8 offered, acks every frame |
| ROM | `TMNP` (0x1174E8 ff., 124 functions), `EzMNP*Options` | the Newton side |

### 5.1 Frames on the wire

```
SYN DLE STX   header   data   DLE ETX   CRC-lo CRC-hi
16  10  02    ...      ...    10  03
```
- Any `10` (DLE) in the header or data is sent twice (`10 10`).
- CRC-16/ARC: polynomial 0xA001 (reflected 0x8005), start 0, over the
  header and data bytes (each DLE counted once) plus the ETX byte; sent
  low byte first. (unixnpi computes it bit by bit, DyneTK and NTX with a
  table; same result.)
- The header starts with its own length (not counting the length byte),
  then the frame type.

| Type | Name | Header | Meaning |
|---|---|---|---|
| 1 | LR | `len 01 02 <params>` | link request (negotiation) |
| 2 | LD | `04 02 01 01 <reason>` | link disconnect (reason 0xFF = user) |
| 4 | LT | `02 04 <seq>` + up to 256 data bytes | link transfer (data) |
| 5 | LA | `03 05 <seq> <credit>` | link acknowledge: last LT received in order, and how many more may come |
| 6 | LN | | link attention (not used) |
| 7 | LNA | | attention acknowledge (not used) |

### 5.2 Connecting

The **Newton starts**: when Toolkit's "Connect Inspector" opens the
endpoint it sends an LR. The desktop answers with its own LR (no
negotiation: it just states what it can do), and the Newton confirms with
an LA. Then both sides send LTs, numbered from 1 (mod 256), each answered
with an LA.

The Newton's LR, as recorded in NTX's comments (probably from a Dock
connection; to confirm for NTK in 1.5):
```
26 01 02
01 06 01 00 00 00 00 FF   constant parameter 2
02 01 02                  framing: octet-oriented
03 01 08                  window k = 8 outstanding LTs
04 02 40 00               max info field N401 = 64
08 01 03                  data phase optimisation: N401 256, short LT/LA headers
09 01 01                  compression (MNP 5 / V.42bis offer?)
0E 04 03 04 00 FA         ?
C5 06 01 04 00 00 E1 00   Apple: speed negotiation to 0xE100 = 57600 bps
```
The desktop's LR (unixnpi, NewtonInspector, NTX; DyneTK the same with
k = 8 and an extra `0E 04 02 04 00 FA`):
```
17 01 02
01 06 01 00 00 00 00 FF
02 01 02
03 01 01                  k = 1
04 02 40 00
08 01 03                  so LTs carry up to 256 bytes, headers as in 5.1
```
Leaving out `09`, `0E`, `C5` declines compression and the speed change.

### 5.3 Data transfer

- **Sending**: split the byte stream into LTs of at most 256 bytes
  (DyneTK uses 250). With k = 1, send one LT and wait for its LA; if none
  comes within about 1 s (NTX's T401), send it again. LT boundaries mean
  nothing to the Toolkit layer: a packet may span several LTs, and
  several packets may share one (each `send_data_block` call in DyneTK
  becomes its own LT, which is fine).
- **Receiving**: an LT whose sequence number is the next one: deliver
  its data, answer `LA seq credit`. The same number again (the Newton
  didn't get our LA): answer the LA again, don't deliver twice. Out of
  order or a CRC error: answer an LA with the last good number, and the
  Newton resends.
- **Credit**: the LA's last byte; unixnpi/NTX/NewtonInspector send 1,
  DyneTK 8.

### 5.4 Keeping the link alive

The Newton aborts an MNP link that is quiet for too long
(`TMNP::InactiveTimeOut`, ROM 0x117824, error -20003, which Toolkit.pkg
shows as "The connection was closed"). All four desktops therefore
**resend the last LA every 3 s** while nothing else is sent (NTX's T403,
NewtonInspector's "keep-alive ping", DyneTK's keep_alive_). The NTK
endpoint also gets an idle timer option of 30 s (`EzMNPConnectOptions`).

### 5.5 Disconnecting

The desktop sends `term` (Toolkit layer), then an LD. The Newton sends an
LD when Toolkit disconnects or the link fails; after an LD nothing more
is sent.

### 5.6 Speed

The NTK connection (`connectionKind` 3) uses `EzMNPSerialOptions` and
`EzMNPConnectOptions` (ROM 0xB0E38): **38400 bps** on the serial port, MNP
data rate 38400 (MNP's timers are based on it), idle timer 30 s, no speed
negotiation option. For 57600 (Matt's adapter) there are two ways:
- `NTKListener(true, <options frame>, ...)`: `connectionKind` may be an
  endpoint options frame (`EzConvertOptions`), so our own package can
  start the listener with serial speed 57600 instead of Toolkit's
  button.
- Apple's speed negotiation (LR parameter `C5`, option
  `TCMOMNPSpeedNegotiation`, default 57600): LRs at the starting speed,
  then both switch. Only if the Newton's LR offers it.
Over Einstein's TCP serial port the speed shouldn't matter (to confirm).

### 5.7 Open points (settled in 6.1: no escape, LR as shown, keep-alive 3 s works)

- DyneTK inserts a byte `01` after every byte equal to a rolling value
  (0x33, then +51 after each use) in data LTs. None of the other three
  does this, V.42 has nothing like it, and unixnpi/NewtonInspector work
  with real Newtons. Probably a misreading of a sniffed session: test by
  sending data with `33` in it and checking that it arrives unchanged.
- The Newton's LR for an NTK connection (k, N401, `C5`?).
- How long the Newton's inactivity timeout really is (3 s keep-alive is
  safe either way).

### 5.8 What newtc needs

Stop-and-wait MNP like NTX/unixnpi: wait for LR, answer with the
desktop LR above, then LT/LA with k = 1, LTs of at most 256 bytes,
retransmit after 1 s (with a limit), LA keep-alive every 3 s, LD on
close. About 300 lines of C++ with no dependencies, tested against a
loopback (newtc's `-ntk-device` speaks the same MNP) and Einstein.

## 6. Captured sessions (Einstein, 2026-10-04)

Einstein (ROM 717006), serial port as TCP client to 127.0.0.1:3679,
Toolkit.pkg "Connect Inspector", our probe on the desktop side:
`Test/ntk/ntk_probe.py` (Python, no dependencies: MNP, Toolkit packets,
full byte log; commands from a script, stdin, or a named pipe). Logs and
the objects received: `Test/ntk/captures/` (capture1/2 with NS Debug
Tools and other debug packages installed, capture3 without).

### 6.1 Confirmed

- **MNP** as in section 5. The Newton's LR for NTK:
  `20 01 02 | 01 06 01 00 00 00 00 FF | 02 01 02 | 03 01 08 | 04 02 40 00 |
  08 01 03 | 09 01 01 | 0E 04 03 04 00 FA` (k = 8, **no `C5`**: no speed
  negotiation, so 38400 stays). Our k = 1 LR is accepted; the Newton
  answers with `LA seq 0`. Keep-alive LAs every 3 s hold the link through
  quiet periods. On `term` the Newton closes the link with
  `LD 07 02 01 01 00 02 01 00`.
- **Bytes are transparent**: `0x33` arrives unchanged (strings `"x3y"`,
  `"3333"`, `'sym3` echoed through `code`). DyneTK's escape is not needed.
- **No padding**: `code` and `lscb` packets of odd sizes back to back work.
  (Whether padding would break things wasn't tested: we don't pad.)
- **Connect**: `cnnt` -> `okln` (length 0) -> `fobj {interpretation:
  'dante, data: {}}`.
- **`lscb`**: `rslt 0`, then the REP prints the result as `text`, in the
  form `#<ref in hex, padded> <value>` plus a CR: `#2        NIL`,
  `#4        1`.
- **`code`**: `rslt 0`, then `code` (length = the request's) + u32 size +
  NSOF of the result: `5`, `"abc"`, `[1, 16]`, a whole function, a
  1563-byte array of package titles. newtc's NOS 2 functions run as they
  are (no need for NOS 1 `CodeBlock`s).
- **A `code` block that catches its own exception** returns it as data:
  `{name: '|evt.ex.fr.intrp;type.ref.frame|, data: {errorCode: -48808,
  symbol: 'NoSuchFunctionProbe}}`.
- **Exceptions from `lscb`** with `breakOnThrows` nil: `eref` with the
  data frame, e.g. `{errorCode: -48808, symbol: 'NoSuchFunctionProbe}`;
  `eerr` for C errors, e.g. `ExitBreakLoop()` outside a break loop:
  `evt.ex.fr.intrp`, -48800 ("Not in a break loop"). **The length field
  of exception packets does not count the u32 count words**: the data is
  length + 4 bytes (`eerr`) or length + 8 bytes (`estr`, `eref`), as the
  ROM code in 2.4 shows.
- **Break loop** (without NS Debug Tools): `BreakLoop()` -> `rslt 0`,
  `eext`; while stopped, `code` works (`6*7` -> 42) and `lscb` too;
  `StackTrace()` -> `fstk`; `ExitBreakLoop()` -> `rslt 0`, `bext`, and
  the REP prints the results of both the inner and the outer evaluation.
- **`fstk`**: an array, newest first, of
  `{class: 'StackFrameInfoFrame, CodeBlock, programCounter, receiver,
  contextFrame}`. Native functions: `CodeBlock: "functions.BreakLoop"`,
  `programCounter: -1`. Our anonymous `lscb` functions: `CodeBlock: nil`,
  `programCounter: 2` (to check with named functions from a package: the
  ROM seems to send only what it can name).
- **Packages**: `pkg ` + size + package, **with or without the 1 s
  pause**: `rslt 0`, and the package is in `GetPackages()`; `pkgX` with
  the UTF-16 name removes it (`rslt 0`, also for a name that doesn't
  exist).

### 6.2 Surprises on this Einstein

- **NS Debug Tools break `BreakLoop()` from `lscb`**: with the debug
  packages installed, `BreakLoop()` threw "Expected a string" (-48402)
  with our code block as the value, however it was compiled (NOS 2,
  `-nos1`, `-g`), and no break loop started. Without them it works. To
  look at in step 2: NSDT's BreakLoop prints the location of the caller
  and seems to need a name for it, which a top-level code block doesn't
  have. We need NSDT for stepping.
- **`Print` and `Write` don't reach the Inspector**: `functions.Write` is
  `func(arg0) |Einstein:Log|(arg0)` (Einstein's log), and
  `functions.Print` is a NewtonScript replacement built on `Write`
  (probably from the "Einstein NS Runtime" package). The ROM's `Print`
  would send `text` (`PrintObject` -> `gREPout->ConsumeFrame`).
  `Display` (still the ROM's native function) does reach the Inspector.
  A debugger must not rely on `Print` for its own data (we use `code`
  results anyway), and may want to show Einstein's log separately.
- Still installed after removing the debug tools: `Snarf`, and ViewFrame
  (`VF+Intercept:JRH`, `VF+Dante:JRH`, ...), which hooks into the system.

### 6.3 Consequences for newtc

- Use `code` for everything that returns data (stack, variables,
  evaluate), each block wrapped in `try ... onexception |evt.ex| do
  {error: CurrentException()}`; use `lscb` for REP-like input whose
  output is text, and for `ExitBreakLoop()`.
- Parse exception packets with the +4/+8 rule; parse `code` replies by
  their size word, not the length field.
- The `fstk` frames give function, PC, receiver per frame; for
  variables we still need NSDT (or our own native code).
- Packages: no pause needed (on Einstein; check on the MP2x00).

## 7. Remote tools

### 7.1 NS Debug Tools on Einstein (step 2.1, capture4)

Fresh Einstein flash (ROM 717006) with only EinsteinPrefs, Toolkit.pkg,
and NS Debug Tools 2.2 ("NS Debug Tools:PIE") added.

**Einstein's `Write` breaks NSDT.** Einstein's built-in "Einstein NS
Runtime" package (`~/dev/Einstein.git/Drivers/NSRuntime`, part of
Einstein.rex) replaces two globals at install: `DefGlobalFn('Write,
kMyWrite)` and `DefGlobalFn('Print, kMyPrint)`. `kMyWrite` is `func(what)
|Einstein:Log|(what)`, and `|Einstein:Log|` accepts only strings; the
original native `Write` is not kept. So on Einstein:
- `Write` of a character, number, function... throws "Expected a string"
  (-48402), and `Print`/`Write` output goes to Einstein's log, never to
  the Inspector (`Display`, still native, does reach it);
- NSDT's `BreakLoop` writes the stop location with `Write(fn)`,
  `Write($()` etc., so it throws before the break loop starts: this was
  the "Expected a string" of captures 1 to 4. (The ROM's `FWrite` prints
  any object with `PrintObject`, like newtc's: no NSDT bug.)
- Workaround for a session (RAM only, gone after a reboot):
  `DefGlobalFn('Write, func(x) |Einstein:Log|(if IsString(x) then x else
  SPrintObject(x)))`. Fix in Einstein: keep the original
  (`functions.Write` before `DefGlobalFn`) and have `kMyWrite` handle
  any object, and call the original too (so the Inspector gets the text).

**With that workaround, NSDT works remotely**, everything driven from the
desktop:

| Sent | Got |
|---|---|
| `code DefGlobalFn('ProbeAddG, ...)` (compiled with `-g`) | the function is defined |
| `code InstallBreakPoint(functions.ProbeAddG, 4)` | `{instructions: <binary>, programCounter: 4}` |
| `code GloballyEnableBreakPoints(true)` | nil (the previous setting) |
| `lscb ProbeAddG(1, 2)` | `rslt 0`, `text` (NSDT's location: the argument values), `eext` |
| `code [GetCurrentPC(0), GetAllTempVars(0), GetAllNamedVars(0), GetCurrentFunction(0) = functions.ProbeAddG]` | `[4, ['bottom], {a: 1, b: 2, c: 3}, true]` |
| `lscb Step()` | `bext`, location text, `eext` |
| `code [GetCurrentPC(0), GetAllNamedVars(0)]` | `[5, {a: 1, b: 2, c: 3}]` |
| `lscb StepOut()` | `bext`, `eext`; now level 0 is the caller (our code block, pc 6) |
| `lscb StackTraceOld()` | `fstk` (the ROM's; NSDT replaced `StackTrace` with its text `QuickStackTrace`) |
| `code RemoveAllBreakPoints()`, `lscb ExitBreakLoop()` | `bext`, the call's result `#18 6` |

Findings:
- **`code` works inside the break loop for NSDT's inspection functions**
  (they find NSDT's `BreakLoop` frame on the same stack): one round trip
  returns PC, temporaries, named variables, the function, as data.
- **newtc's `-g` (NOS 2 `DebuggerInfo`) gives NSDT the variable names** on
  the real ROM.
- NSDT's functions take a stack `level` (0 = the stopped function):
  `GetCurrentPC(level)`, `GetAllTempVars(level)`, `GetAllNamedVars(level)`,
  `GetNamedVar(level, name)`, ... (the wrong arg count gives -48803).
- Stepping is `lscb Step()` / `StepIn()` / `StepOut()`: each leaves the
  break loop (`bext`) and enters it again (`eext`) about 0.1 s later.
- Breakpoints are `{instructions, programCounter}`; the interpreter only
  checks them while `GloballyEnableBreakPoints(true)`.
- Round trips on Einstein: 0.1 to 1 s per `code` request.

### 7.2 The other tools (step 2.2)

From `Newton Debug Tools 2.2/`, decompiled with `newtc -pkg ... -odecompile`
(all decompile; only HeapShow has native code):

| Package | Kind | What it does | For us |
|---|---|---|---|
| DebugHashToName | auto part | Installs `GetDebugName(hash)` / `DebugHashToName(hash)` and `GetDebugHash(name)`: two parallel tables, about 340 integer hashes and their names (`"root"`, `"protoStaticText"`, `"protoInputLine"`, ...). ROM objects carry such a hash in their `debug` slot; NSDT's `GetDebugNameOf` uses `GetDebugName` when it exists, so stack traces show ROM proto names. | Not needed on the device: newtc can read the table from the package file and name ROM objects on the host. |
| Exception Printer | auto part | Replaces `GetRoot().ActionNotify` (the alert for an uncaught exception): the alert then shows the exception name (`lastex`), the error code with a description from its own error table, an error range ("Frames", "Communications", ... 55 ranges), and `lastexmessage`. Restores the original on removal. | Not needed: the desktop gets every exception as `eerr`/`estr`/`eref` and newtc has the error strings (`DAPErrorText`). Its table could fill gaps in newtc's error strings. |
| HeapShow | form app, 11 `BinCFunction`s | A heap monitor (`DrawMemoryMap`, `AllocateBinary`, "Separate Heap", "Reserve", "Check Interval"), settings in user config `HeapShow:preallocate`. | Not now (maybe a memory view later). |
| Snarf | auto part | An In/Out Box transport (`RegTransport('|Snarf:PIE|, ...)`, "Auto Receive", "Batch Receive"): for testing routing. | No. |
| vFlags | form app | Shows the `viewFlags` bits by name (`vClickable`, `vStrokesAllowed`, ..., `vAnythingAllowed`). | Not on the device; the same table on the host can show `viewFlags` as names in the Variables view. |
| NSDShortCuts | auto part + `myFunctions` source | Apple's one-letter shortcuts (`s`, `si`, `so`, `w`, ...). | Already embedded in newtc (`Matt/Debugger/NSDShortCuts.ns`); not needed on the device: the debugger calls NSDT directly. |

### 7.3 What the device needs (step 2.3)

**What the ROM alone gives a desktop debugger:** the NTK nub (`lscb`,
`code`, `fobj`), the break loop (`BreakLoop`, `ExitBreakLoop`, `eext` /
`bext`), exceptions as packets (with `breakOnThrows` they also stop in a
break loop), and `fstk` (function, PC, receiver per frame, no
variables). What it doesn't give from NewtonScript: breakpoints,
single steps, and access to a frame's arguments, locals and temporaries
(the interpreter has them, `TNSDebugAPI` and `TInterpreter::
SetBreakPoints`, but no NewtonScript functions reach them).

**NS Debug Tools adds exactly that**, through its few `BinCFunction`s
(`NSDInstallBreakPoints`, `NSDEnableBreakPoints`, `NSDMakeNSDebugAPI` with
Function, PC, SetPC, GetVar, SetVar, temporaries, ...), plus NewtonScript
on top (Step, StepIn, StepOut, InstallBreakPoint, GetAllNamedVars, ...).
Shown working remotely in 7.1. **So the device needs:**
1. **NS Debug Tools.pkg**, unchanged (Apple's package; we already know
   its source, `Matt/Debugger/NSDebugTools.ns`).
2. **Toolkit.pkg** to open the connection (or our own package calling
   `NTKListener` with an options frame, for 57600 bps: section 5.6).
3. **Our agent package** (NewtonScript, compiled by newtc), small:
   - one `code` call per DAP request, built on NSDT (stack with
     `GetCurrentFunction/PC(level)` per level or `StackTraceOld`'s
     `fstk`, variables with `GetAllNamedVars`/`GetAllTempVars`, evaluate
     in a frame like newtc's local `EvaluateInFrame`), each wrapped in
     `try` so an error comes back as data;
   - an `NSDBreakLoopEntry(fn, pc, params)` hook that sends the stop
     (function, PC, reason) with `NTKSend` the moment a break loop starts,
     so the desktop needn't parse NSDT's location text; it could also
     skip NSDT's location printing (call `NSDOriginalBreakLoop()` itself
     and return nil), which saves time on a 38400 bps line;
   - line stepping on the device: the desktop sends the statement PCs of
     the function (from its line table) and the agent keeps stepping with
     NSDT until a stop rule says stop (newtc's `StepCheck` rules), so
     there is one round trip per line step, not per instruction.
     (Measured on Einstein: an NSDT step plus its `bext`/`eext` takes
     about 0.1 s, a `code` round trip 0.1 to 1 s.)
4. **On Einstein only**: a `Write` that accepts any object (Einstein's NS
   Runtime replaces it with a string-only logger, 7.1). Until Einstein is
   fixed, the agent can install the workaround when it finds
   `functions.Write` not native.
Everything else (names for ROM objects, error texts, `viewFlags` names)
is done on the host.
