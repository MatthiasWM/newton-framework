

# A New Task

We are going back to debugging, but this time we will debug an application
running on Einstein or even a real MP2x00 connected via the serial port, using
the NTK Inspector?Toolbox protocol.

## Tools

NTK comes with a .pkg file that can be installed on the device:
'~/Azureus/unna2/apple/development/NTK/macntk/NTK 1.6.4b3/NTK 1.6.4b3/Toolkit.pkg'

NTK itself has an Inspector window that mainly runs NewtoScript code on the
remote machine to - erm - inspect data structures, set breakpoints, etc. .

## Protocol

Toolbox.pkg provides the debugger protocol. Some of it is documented in
"Newton Docking Protocol.txt", some was reverse engineered in
"~/dev/DyneE5/DyneTK/fltk/Flio_Inspector.cxx".

I may be able to find other resources if needed.

## Goal

Source code level debugging app on remote devices

## STeps

### 1. Protocol Analysis

Collect information about the Inspector/Toolbox protocol and document your
findings in an .md text file, so we can use that as a base to write the
DAP extension.

### 2. Evaluate Remote tools

As you know already, '/Users/matt/Azureus/unna2/apple/development/NTK/macntk/NTK 1.6.4b3/NTK 1.6.4b3/Newton Debug Tools 2.2/NS Debug Tools.pkg' adds more debugginf features
to a stock Newton.

Other tools in that directory are DebugHashToName.pkg, Exception Printer.pkg
HeapShow.pkg Snarf.pkg, and vFlags.pkg which may help us on the way.

### 3. DAP Extension and protocol implementation

Expand "newtc" in DAP mode, so it can connect to a device, upload packages, and
debug them with the symbolic debugger right on the device. This shall work
for existing packages where we create the source code by decompiling, and for
source code written on the host, compiled into a package, and uploaded to the
MP.

---

# The plan (Claude, 2026-10-04)

Branch `dap_to_toolbox`. The previous plan is in `Matt/CLAUDE-bak.md`
(conventions, where things are, ROM reading tips); finished work goes to
`Matt/HISTORY.md` as before. Same rules: one small step at a time, every
step ends with something Matt can run and a test that keeps it working;
Apple's API and implementation first; no debugger logic in TypeScript.

## What we already have (found while planning)

- **The device side is in the ROM, and ported here.** `NTK/NTK.cc` has
  `PNTKInTranslator`/`PNTKOutTranslator` (ROM 0x129EF4 ff.), `NTKListener`,
  `NTKDownload`, `NTKSend`, `NTKAlive`, `NTKStackTrace`; the commands are in
  `NTK/NTKProtocol.h`. Toolkit.pkg on the Newton mostly opens the
  connection and installs these translators. So the device's half of the
  protocol can be *read*, not guessed.
- **Packets** (same framing as the dock): `'newt' 'ntp ' <cmd> <length>
  <data>`. Newton -> host: `cnnt` connect, `text` output, `rslt` result,
  `eext`/`bext` enter/exit break loop, `eerr`/`estr`/`eref` exceptions,
  `fobj` an object (NSOF; what `NTKSend(obj)` sends), `dpkg`. Host ->
  Newton: `okln`, `lscb` (NSOF code block, compiled on the host, run by the
  REP), `pkg ` / `pkgX` load / delete a package, `stou` timeout, `term`.
  `fobj`/`code`/`term` go both ways.
- **The wire is MNP** (LR/LA/LT/LD frames, CRC-16), for a real MP2x00 on a
  serial port and for Einstein alike. Einstein offers its serial port as
  named pipes (`~/Library/Application Support/Einstein Emulator/
  ExtrSerPortSend|Recv`) or as a TCP client (`TSerialPortDriverTcpClient`,
  Einstein connects to us). MNP implementations to learn from: DyneTK
  `Flio_MNP4_Protocol.cxx` (Matt's), NTX `MNPSerialEndpoint.m`, unixnpi
  `newtmnp.c`.
- **Host-side references:** DyneTK `Flio_Inspector.cxx` (Matt's,
  reverse engineered), NTX (`~/dev/newton-toolkit`, Simon Bell:
  `NTX/Comms/Protocol/NTKProtocol.h`, `Session.mm`), NTK 1.6.4 itself
  (`~/dev/NTK.ghidra`).
- **newtc already has** the ROM compiler (bytecode exactly as the Newton
  runs it), NSOF in and out (`Frames/RefIO`), packages (`PackageWriter`),
  line tables in functions (`lineTable` slot, travels into packages and is
  ignored by the Newton), `.nsdbg` maps for decompiled packages (functions
  found by an FNV hash of `instructions`), a NewtonScript DAP layer
  (`Debugger/DAP.ns`), and the NS Debug Tools.

## Design (confirmed by Matt, 2026-10-05)

```
VS Code --DAP--> newtc -dap (host)                 Newton / Einstein
                 DAP.ns + DAPRemote.ns              ROM REP + NTK translators
                 compiler, line tables, .nsdbg      Toolkit.pkg (connection)
                 NTK packets / NSOF   --MNP-->      NS Debug Tools.pkg
                 transport: tty | pipe | TCP        DAP agent .pkg (ours)
```

- newtc stays the debug adapter: it keeps the sources, line tables, and
  maps, and compiles everything it sends. The device runs the program and
  Apple's NS Debug Tools.
- **A small "DAP agent" package** (NewtonScript, compiled by newtc from
  `Matt/Debugger/`, uploaded automatically) answers each request in one
  round trip: stack, scopes, variables, evaluate, breakpoints. Serial is
  slow (38400 bps); round trips must be few.
  Refined by steps 1 and 2 (Toolkit Protocol.md 6, 7.3): requests that
  return data go as `code` (the result comes back as NSOF, also while
  stopped), each wrapped in `try`; REP-like actions (`Step()`,
  `ExitBreakLoop()`) as `lscb`; the stop itself is pushed by the agent's
  `NSDBreakLoopEntry` hook with `NTKSend` (`fobj`). The agent builds on
  NSDT's functions (`InstallBreakPoint`, `GetAllNamedVars(level)`, ...),
  which work remotely (7.1). On Einstein the agent installs a `Write`
  that accepts any object until Einstein's NS Runtime is fixed.
- **Host layering in C++:** transport -> MNP -> NTK packets -> NSOF, then
  NS natives (`NTKConnect`, `NTKSend`, `NTKReceive`, `NTKLoadPackage`, ...)
  so the remote logic is NewtonScript like the rest of DAP: a new
  `DAPRemote.ns` with `{_parent: DAPRequests}` overriding the requests,
  so local `-dap` and its tests stay untouched.
- **newtc stays running (Matt, 2026-10-05).** Real debugging is many
  edit-compile-debug rounds; the Newton link must survive them. So the
  remote debugger is a background server: `newtc -dap-server <port>
  -ntk <target>` keeps the link to the Newton open and serves one VS Code
  session after the other (VS Code connects with `"debugServer"`), instead
  of exiting after the first session. The link lives in its own I/O
  thread (transport + MNP: acks, retransmits, keep-alives), independent of
  NewtonScript and of DAP sessions; the main thread gets whole Toolkit
  packets through a pipe it can poll like the DAP fd.
- **Our own connection tool (Matt, 2026-10-05)**, maybe instead of
  Toolkit.pkg: our agent package can open the link itself
  (`NTKListener` with an options frame: 57600 bps), stay connected, and
  reconnect.
- **Functions across the link** are identified like the .nsdbg map does:
  by the hash of their `instructions`, cached on the host per device ref
  (NSDT's `NSDRefToHexString`). One mechanism for decompiled packages and
  for packages newtc compiled with `-g`.
- **Line stepping** can't use the C++ `gDebuggerStep` on a Newton. Plan:
  the host sends the agent a step plan (the statement start PCs of the
  function, depth, kind) and the agent's break loop hook keeps stepping
  with NSDT on the device until the rules of `StepCheck` say stop: no
  round trip per instruction.
- **Testing without hardware:** newtc can play the Newton itself
  (`-ntk-device <port>`: its own interpreter, NTK translators, NSDT, agent;
  raw packets over TCP, optionally MNP). Then remote sessions run in
  `run_dbg_tests.py` like the local ones; Einstein and the real MP2x00 are
  checked by hand at the milestones.

## Steps

### 1. Protocol analysis -> `Matt/Toolkit Protocol.md`
- [x] 1.1 Packets from the device side: read `NTK/NTK.cc` against the ROM
      (`newtonos.s`, PNTK*Translator, NTKInit, FNTKListener, the nub):
      every command, its payload, who sends it when, error codes, timeouts,
      padding/alignment.
- [x] 1.2 Toolkit.pkg: decompile it (`newtc -pkg Toolkit.pkg -decompile`),
      note what it adds on top of the ROM (connect UI, ADSP vs serial,
      package download, auto-connect).
- [x] 1.3 The host side: DyneTK, NTX `ToolkitProtocolController.mm`,
      NewtonInspector (Jake Borden), NTK's strings (Ghidra not needed):
      the connect handshake, evaluating (`lscb`/`code`), what NTK sends,
      download/delete package, break loop handling. Open points (padding,
      `code` in a break loop, the 1 s pause) go to the capture in 1.5.
- [x] 1.4 MNP: the subset we need (LR negotiation, LT/LA windows, LD,
      CRC, escapes), with sources compared; the dock's
      "Newton Docking Protocol.txt" for what is shared.
- [x] 1.5 Captures with Einstein: our own desktop side,
      `Test/ntk/ntk_probe.py` (MNP + Toolkit packets in Python, full byte
      log, commands from a script or a named pipe `--fifo`), sessions in
      `Test/ntk/captures/`. Connect, `lscb`, `code`, exceptions, break
      loop, `fstk`, package upload/delete all confirmed (.md section 6).
      Found: NS Debug Tools break `BreakLoop()` from `lscb` (step 2), and
      Einstein's environment sends `Print`/`Write` to Einstein's log.

### 2. Evaluate the remote tools -> section in the same .md
- [x] 2.1 NS Debug Tools on Einstein (ROM 2.1), driven by the probe:
      breakpoint, `code` inspection while stopped (PC, temporaries, named
      variables from newtc's `-g`), Step, StepOut, all work (.md 7.1).
      Einstein's NS Runtime replaces `Write`/`Print` with a string-only
      logger, which breaks NSDT's BreakLoop: Matt to fix in Einstein
      (workaround per session in 7.1).
- [x] 2.2 Decompile and summarize DebugHashToName, Exception Printer,
      HeapShow, Snarf, vFlags: what each gives a debugger (exception text
      for `stopped`, names for hashed symbols, view flag names for
      variables, heap info), which we use, which we skip.
- [x] 2.3 What the agent package needs from them, and what the ROM alone
      can't do (e.g. stack access without NSDT's natives).

### 3. newtc: remote DAP (details refined after 1 and 2)
- [x] 3.0 Agree on the design above with Matt (2026-10-05; plus: newtc
      keeps running in the background, our own connection tool).
- [x] 3.1 Transports (`Matt/NTKTransport.{h,cc}`): TCP server on
      127.0.0.1:3679 for Einstein's TCP client (accepts again after a
      disconnect), serial tty (termios, 57600); `newtc -ntk-dump <target>`
      just prints what arrives, in hex. Targets `tcp`, `tcp:<port>`,
      `serial:<device>[@<bps>]` (38400 unless given). Non-blocking, no
      thread: the user polls `fd()` and calls `handleReadable()` (events
      kConnected, kData, kDisconnected). Test `Test/ntk/test_transport.py`
      (Einstein over TCP twice, a pty as the serial port, bad targets).
- [x] 3.2 MNP in C++ (`Matt/MNP.{h,cc}`) in the link's I/O thread:
      connect, send, receive, ack, retransmit, keep-alive. Framing
      (`MNPEncodeFrame`, `MNPDecoder`, byte by byte: Einstein sends one
      byte per TCP packet), `MNPLink` (the thread: answers every LR,
      one LT in flight, resent after 1 s, up to 10 times, LA keep-alive
      after 3 s, LD both ways; events kConnected, kLinkUp, kData,
      kLinkDown through `notifyFd()`/`nextEvent()`; `send()` never
      blocks). `newtc -ntk-mnp <target>`: events and data on stdout,
      frames on stderr, hex lines from stdin are sent. Test
      `Test/ntk/test_mnp.py` (a fake Newton: handshake, resent LR and LT,
      bad CRC, DLE bytes, 256-byte LTs, retransmission, keep-alive, LD,
      relink, reconnect, end).
- [x] 3.3 NTK packets + NSOF (`Matt/NTKInspector.{h,cc}`):
      `NTKPacketReader` (the length rules from the captures: `code` by
      its size word, `eerr` +4, `estr`/`eref` +8), NSOF in memory
      (`NTKFlatten`/`NTKUnflatten`), `NTKInspector` (answers `cnnt`
      with `okln`; `evaluate()` = `lscb`, `call()` = `code`,
      `loadPackage()`, `deletePackage()`, `terminate()`; a listener gets
      text (MacRoman -> UTF-8, CR -> LF), results, objects, exceptions as
      C++ `Exception`s, break loop entry/exit). `newtc -ntk <target>`: a
      terminal Inspector; each stdin line is compiled here and sent with
      `lscb` (`=` first: `code`, the result is printed); lines typed
      early wait for the Newton; at the end of stdin a last `code` call
      as a sync, then `term`. Exceptions print like the local REPL.
      `NEWTC_NTK_TRACE=1` shows the MNP frames. Test
      `Test/ntk/test_inspector.py` (a fake Newton with captured replies).
      Waiting uses `select()`, never `poll()`: on macOS poll() doesn't
      support devices (serial ports) and misses a FIFO's end.
- [x] 3.4 `-ntk-device`: newtc as the Newton side, for automated tests.
  - [x] 3.4a `Matt/NTKDevice.{h,cc}`, `newtc -ntk-device <target>`: a
        TCP client transport (`tcp-client:<port>`, like Einstein),
        `MNPLink` in the Newton's role (`setRole(kNewton)`: Einstein's LR,
        resent every second, given up after four; the desktop's LR
        confirmed with LA 0), the nub as in the ROM (`cnnt`, `okln` with
        length 0, Toolkit.pkg's `'dante` fobj; `lscb` -> a frame for the
        REP and `rslt 0`; `code` -> `rslt 0`, then the result with the
        request's length, an exception ends the connection; `pkgX`,
        `stou`, `term`), translators `PNTKDeviceIn/OutTranslator` (text
        in 255-byte packets at CR, exceptions as `estr`/`eref`/`eerr`,
        `eext`/`bext`), newtc's REP idling (`REPIdle`) until `term`.
        Test `Test/ntk/test_device.py` (newtc -ntk against newtc
        -ntk-device, no Einstein).
  - [x] 3.4b Break loops and debugging on the device: `fstk` for
        StackTrace (`StackFrameInfoFrame`s as in the ROM's
        NTKStackTrace: newest first, `CodeBlock` "functions.<name>" or
        nil, pc -1 for natives); `newtc -dbg -ntk-device` is a Newton with
        NS Debug Tools (loaded, breakpoints enabled), plain `-ntk-device` a
        stock one. Test: the session of Toolkit Protocol.md 7.1
        (breakpoint, `code` inspection while stopped, Step, `fstk`,
        ExitBreakLoop) gives the same answers as Einstein, down to NSDT's
        BreakLoop at pc 409 in `fstk`.
  - [x] 3.4c Packages on the device: `pkg ` installs (from memory,
        `installPackage(package, false, &parts)`: DoNotInstall, the ROM's
        InstallPart, not opened, as on a Newton; the same name again:
        -10402), `pkgX` removes (the ROM's RemovePart with each part's
        install info and remove frame). `installPackage()` got the two
        optional arguments (open the app; the parts). The terminal
        Inspector: `:pkg <file>`, `:pkgx <name>`.
- [x] 3.5 Packages: upload (`pkg `), delete (`pkgX`), replace on rebuild;
      upload NSDT and the agent when missing.
  - [x] 3.5a The bridge for NewtonScript (`Matt/NTKRemote.{h,cc}`):
        `NTKOpen(target)`, `NTKWaitConnected(s)`, `NTKIsConnected()`,
        `NTKCall(fn)` (waits for the result, `ntkTimeout` seconds,
        throws `evt.ex.msg` when the connection is gone),
        `NTKEvaluate(fn)`, `NTKInstallPackage(binary)` and
        `NTKDeletePackage(name)` (wait for the Newton's `rslt`),
        `NTKPoll(s)`, `NTKSetHandler(frame)` (Connected, Disconnected,
        Text, Exception(name, data), BreakLoop(entered), Object, Result),
        `NTKClose()`; NTK's `LoadDataFile(fileName, class)`.
        `NTKInspector` now tells which command a `rslt` answers.
        Test `Test/ntk/test_remote.py` (a `-script` drives
        `-ntk-device`).
  - [x] 3.5b Deploying (`Matt/Debugger/Remote.ns`, the global
        `NTKRemote` from `NTKLibrary()`): `InstallPackage(binary)`
        replaces by name (`pkgX` + `pkg `), `EnsureDebugTools()` uploads
        `debugToolsPackage` (env `NEWTC_NSDT_PACKAGE`, else NTK 1.6.4's
        folder) when `NSDOriginalBreakLoop` is missing, `EnsureAgent()`
        installs `Matt/Debugger/Agent.ns` (version 1: `|DAPAgent:Version|`,
        on Einstein a `Write` that takes any object) unless that version
        is there. Natives `NTKMakePackage(frame)`, `NTKPackageName(binary)`;
        `writePackageToMemory()` in PackageWriter. Test
        `Test/ntk/test_deploy.py`.
- [x] 3.6 DAP launch on a target (VSNewt attributes `target`,
      `port`, `baud`; no logic in TS): output events, `eext` -> `stopped`,
      continue, exceptions.
      Done so far (`Matt/Debugger/DAPRemote.ns`, launch attribute
      `"target"`): wait for the Newton (requests still served), deploy
      with breakOnThrows off (agent, NS Debug Tools), then the exception
      filter; a `.ns` program is compiled here as one top-level block in a
      try and run with `code` (NTKCompileFile: top-level semantics,
      exceptions as data, no REP echo); a `.pkg` is installed. Output,
      exceptions, `stopped` (BreakLoop, exceptions), `continue`,
      `evaluate` while stopped (breakOnThrows off meanwhile), a placeholder
      stack frame; breakpoints/stepping answered as later steps.
      `code` replies are matched by break loop depth (a call sent while
      stopped is answered before the call that stopped). Agent v2 sends
      Einstein's Write output to the desktop. newtc no longer runs a
      BinCFunction (ARM code): it throws. Test
      `Test/ntk/test_dap_remote.py`. Works on Einstein (dap_client:
      connect, agent v2, output, BreakLoop stop, evaluate, continue,
      exception stop, end). `continue` sends ExitBreakLoop() with `code`
      (no REP echo). VSNewt: `"target"` in package.json (schema +
      snippet "Run on Einstein", uncommitted in VSNewt); demo
      `Test/ntk/remote_demo.ns`. Tried in VS Code with Einstein by
      Matt (2026-10-05): output, stop, Debug Console, continue,
      exception stop, end.
- [x] 3.7 Stack trace, scopes, variables, evaluate through the agent.
  - [x] 3.7a The stack. Agent v4 (needs NS Debug Tools): an
        NSDBreakLoopEntry hook that enters the break loop itself (no NSDT
        location text), `|DAPAgent:Stack|(exceptionStop)` -> [{name, pc,
        file, line}] newest first (names from `functions` or the
        implementor's slot, lines from the function's `lineTable`,
        computed on the Newton). The deploy enables breakpoints
        (accurate stacks). DAP `stackTrace` from it (cached per stop):
        source + line for files, "subtle" natives, `<program>`. Every
        call the adapter makes runs in a try on the Newton
        (`RemoteCall`): an exception in a `code` block ends the
        connection. Remote calls are compiled from strings (a func()
        in a method would take the method's frame along). Agent v3:
        Write fix only on an old Einstein (no `|Einstein:OrigWrite|`).
        NTKClose waits for the Newton's LD (no "connection lost").
        Einstein dropped the link ~30 s into a stop (LD reason 5, MNP
        inactivity): our LAs carried credit 1; with credit 8 (DyneTK's,
        Matt's hint) it stays up (traced with timestamps,
        NEWTC_NTK_TRACE). newtc also sends `stou` after 15 s without a
        Toolkit packet (`NTKInspector::tick()`), to be safe.
        Tried in VS Code with Einstein by Matt: Call Stack Inner, Outer,
        <program> with lines, continue, exception stop, clean end.
  - [x] 3.7b Scopes and variables (handles on the Newton). Agent v5:
        `|DAPAgent:Scopes|(level)` -> {args, locals, receiver, temps}
        (NSDT's GetAllNamedVars: the first GetFunctionArgCount are the
        arguments; GetAllTempVars without 'bottom), and
        `|DAPAgent:Expand|(handle, start, count)` -> [[name, value]].
        Values travel described, not whole: simple ones as {value} (a
        string over 500 characters cut, with its length), the others as
        {kind, cls, count, preview (5 entries), handle}; the handle is an
        index into a table on the Newton, emptied by each Stack call (a new
        stop). DAP `scopes` (Arguments, Locals with self, Stack) and
        `variables` (expand on request, arrays paged with start/count;
        local values from evaluate as before) in DAPRemote.ns. The
        program's frame hides NTKCompileFile's wrapper (its local and its
        try's two stack values). Test: test_dap_remote.py (Inner's a, b, a
        frame f with a nested array and a function, paging).
  - [x] 3.7c Evaluate in a stopped frame. Agent v6:
        `|DAPAgent:Evaluate|(level, fn, names, fresh)`. As DAP.ns's
        EvaluateInFrame: the desktop compiles the expression as
        `func(<the frame's variables>)` returning [result, the variables
        afterwards] (names from the agent's Scopes, noted per stop); the
        agent calls it with GetNamedVar's values and the frame's receiver
        as self (a writable receiver: a temporary slot and Perform, so
        assignments to its slots stay; a read-only one: a frame with it as
        _proto), writes changed variables back with SetNamedVar, and
        describes the result (previews and expansion as for variables; no
        more `[#0x...]`). Without a frame (or not stopped): globally, the
        handle table emptied when not stopped. breakOnThrows off meanwhile;
        a syntax error is found here. Test: test_dap_remote.py (Inner's
        variables, an assignment Inner returns, <program>'s globals, a
        method's self and slot assignment). Not tested yet: a read-only
        receiver, Apple's NSDT GetNamedVar/SetNamedVar (Einstein).
- [x] 3.8 Breakpoints (.ns programs; packages in 3.10). NSDT breakpoints
      are {instructions, programCounter}: they need the Newton's copy of
      the function. So the program's block finds them: NTKCompileFile
      puts the wrapper on line 0 (`CCompiler::setLineNo`; the file keeps
      its line numbers) with a call of the agent's
      `|DAPAgent:Begin|('|DAPAgent:program|, '|DAPAgent:breakpoints|)`;
      before sending, DAPRemote replaces these literals with the block
      itself and [[file, [[path, pc], ...]], ...]: CodeForLine(file, line)
      gives the functions and PCs here, the new native `PathTo(root,
      target)` where each is in the block (LineTables.cc); the agent (v7)
      follows the paths in its copy and calls InstallBreakPoint (closures
      share the template's instructions, so they stop too).
      `|DAPAgent:SetBreakpoints|([file, places])` replaces a file's
      breakpoints while stopped (changed while running: sent before
      `continue`). Set before the program is compiled: pending, then a
      "breakpoint" event (verified, the line used). The agent's stop hook
      sends {|DAPAgent:Stop|: 'breakpoint} (fobj, before `eext`) when it
      stops at one of its breakpoints: reason "breakpoint". newtc's
      `-ntk-device` got real `NTKAlive`/`NTKSend` (stubs before, so the
      agent's messages never left it). Test: test_dap_remote.py (lines 3
      and 6 before compiling, top level and in a function, line 12 added
      while stopped, cleared).
- [x] 3.9 Stepping. Agent v8: `|DAPAgent:Step|(kind, instruction)`, kind
      'over, 'into, 'out ('in is a reserved word). NSDT steps one
      instruction (Step, StepIn, StepOut set a temporary breakpoint and
      leave the break loop); the agent's stop hook applies the rules of
      the local line step (LineTables.cc StepCheck: a new statement or back
      at a statement start in the same frame, the caller once it returns,
      a called function's first statement for 'into; code without lines
      is left with StepOut) and, until then, takes the next NSDT step
      itself, with ExitBreakLoop a no-op meanwhile (the hook returns nil:
      no break loop), so the program runs on: no round trip per
      instruction. The depth: two GetCurrentFunction probes (is there a
      level depth - 1, a level depth?). A leftover temporary breakpoint at
      a stop: something else stopped it (breakpoint, exception,
      BreakLoop()): the step ends, the temporary goes. Stops send
      {|DAPAgent:Stop|: 'step} (reason "step"); a step that can't go on
      (e.g. out of the program, called by native code) sends
      {|DAPAgent:Step|: message}: console "...; continuing.", it
      continues (as DAP.ns). Without a line table, or granularity
      "instruction": one instruction. DAP next/stepIn/stepOut in
      DAPRemote.ns (`RemoteStep`, sent like continue). Test:
      test_dap_remote.py (next over top-level statements and lines,
      stepIn into Inner (onto a breakpoint) and into a method, next over
      BreakLoop(), stepOut of a method and of the program). Found B34 on
      the way.
- [x] 3.10 Packages: compiled from source with -g, or decompiled, uploaded
      unchanged and debugged in their source. Launch `"program": "x.pkg"`
      (`newtc -g -script x.ns -opkg x.pkg`; or `-odecompile` and the
      `.nsdbg`, `"debugMap"`).
      - Desktop: DAPLoadPackage now also registers the package's own line
        tables (`RememberLineTables`), so CodeForLine finds its
        functions; a breakpoint's place is [path in the part's data, pc,
        [package name, part index]] (`PathTo` per part).
      - Agent v9: finds the installed package's parts with the ROM's
        `GetPkgRefInfo(GetPkgRef(name, store)).parts` (checked in the ROM:
        GetPkgInfoFromVAddr sets `parts`; GetPackages has none), the same
        objects the package runs; `|DAPAgent:Reset|` at session start.
        Agent v10, decompiled packages: their line tables stay here;
        `|DAPAgent:Known|([[path, [name, part]], ...])` (from the new
        native `MappedFunctions(root)`) tells the agent which functions
        have one; its Stack gives such a frame `known` and `at`, and the
        desktop finds the line (LineOfPC).
      - Install without blocking (`NTKInstallPackageAsync`,
        `NTKPackageResult`, serving requests meanwhile): with "All
        Exceptions", an exception in the InstallScript stops there. After
        the install the Newton is idle: evaluate works, and an evaluate
        that runs into a breakpoint stops (RemoteEvaluate serves requests
        until its answer comes; the response waits until the user
        continues). Remote calls run with breakOnThrows off
        (`vars.breakOnThrows`: the global may not exist).
      - `NTKLeaveBreakLoop(fn)` for continue and steps: calls sent after it
        run one level up; before, a reply could go to the wrong call when
        the Newton's `bext` was late (seen with an evaluate right after
        continue).
      - newtc's `-ntk-device`: `GetPkgRef`/`GetPkgRefInfo` for its
        packages, keeping the installed package frame (`packageRef()`
        makes new objects each time).
      - Test: test_dap_remote.py (a -g package: breakpoint, evaluate that
        stops there, variables, step, continue, a second evaluate
        meanwhile; a decompiled package: breakpoint in the decompiled
        source, the stack's line from the map; an InstallScript that
        throws stops there; the hello package: evaluate works when idle).
        Found B35 on the way.
      - Matt's first try on Einstein (hello2, 2026-10-06), fixed: an old
        hello2.nsdbg next to the package was loaded (a package's own line
        tables now win over a map found by name) and registered the
        functions a second time, so each breakpoint was set twice ("WARNING:
        Breakpoint already exists!"; `RememberFunctionOnce`); the app
        didn't open (the ROM's install doesn't: now opened like a program,
        RunBlock, so its scripts stop at breakpoints); the breakpoints
        stayed after the session and froze Einstein when the app ran
        without newtc: the session end (`EndSession`) resets the agent's
        breakpoints and lets a stopped program go on, agent v11 doesn't stop
        at its breakpoints or steps while no desktop is connected (removes
        them), and its RemoveScript resets them (an upgrade). newtc
        `-ntk-device` no longer runs the event loop of an opened app after
        the session.
      Not yet: breakpoints in an InstallScript (it runs during the
      install, before the breakpoints are set), and code the ROM copies
      when installing (a form part's InstallScript).
- [ ] 3.11 The real MP2x00 on the serial port (57600, timeouts,
      reconnects).
- [ ] 3.12 Later: attach to a package that is already installed/running.
- [ ] 3.13 The background server: `-dap-server` with `-ntk` serves session
      after session over one Newton link.
- [ ] 3.14 Our own connection tool in the agent package (instead of
      Toolkit.pkg; 57600 bps; reconnect).
      Note (Matt, 2026-10-06): an app that reconfigures the serial port
      (EinsteinPrefs: `call Einstein.Platform with
      ('GetSerialPortDriverNames, ...)` and friends) can't be debugged
      over that serial port; stepping over that line hung. On Einstein, a
      transport that isn't the emulated serial port would avoid it
      (something to look at here).

Status 2026-10-06: 3.11 waits for a working MP2x00 (Matt's three machines
need repairs: two recaps, one ePaper display conversion). Next without
hardware: 3.12 attach, 3.13 background server, or the wishes 3.15 to 3.17.

### Wishes from trying it out (Matt, 2026-10-06; no priority)
- [ ] 3.15 Breakpoints in an InstallScript, and a "Break in InstallScript"
      entry under "All Exceptions" (DAP exceptionBreakpointFilters). The
      script runs during the install, before newtc can find it on the
      Newton. Plan: when there are InstallScript breakpoints (or the entry
      is checked), upload the package with each part's InstallScript
      wrapped: the wrapper hands the function it is about to call to the
      agent (which sets the breakpoints in it, or a temporary one at pc 0)
      and then calls it. The original function stays as it is (lines,
      PCs); it also covers the copy the ROM makes of a form part's
      InstallScript. Only then is the uploaded package not byte for byte
      the local one.
- [ ] 3.16 Progress while uploading a package: "Uploading hello2.pkg (18
      KB) ....... done" (Debug Console output without a newline continues
      the line; MNPLink knows the bytes the Newton acknowledged). First
      measure where the 5 s between "connected" and "Installed" go (agent
      and NSDT checks, pkgX, the upload, the install itself).
- [ ] 3.17 Don't upload an unchanged package again. Decide by the .pkg's
      bytes, not by source dates: after an upload, newtc tells the agent
      the package's name and a hash of its bytes, with the size and
      timestamp the Newton reports for it (GetPkgRefInfo); kept in RAM (a
      reboot: one more upload). Skip the upload only if the package is
      still installed, its size and timestamp match, and the hash matches
      the local file. Exceptions: its InstallScript doesn't run again (some
      apps reset their state there): a launch option `"reinstall": true`;
      always upload when debugging the InstallScript (3.15).

## Answers (Matt, 2026-10-04)
- Einstein runs the 717006 ROM (2.1). Its serial port defaults to a TCP
  client to 127.0.0.1:3679 (newtc listens there). If that is buggy, Matt
  fixes Einstein first.
- MP2x00: USB-C serial adapter, aiming for 57600 bps reliably (the
  adapter's source exists, timing can be adjusted).
- `-ntk-device` (newtc plays the Newton) for automated tests: yes.
- "launch" first; "attach" is nice to have, later.
- Einstein: /Applications/Einstein.app, or build from `~/dev/Einstein.git`.
  The serial port selector is `~/dev/Einstein/EinsteinPrefs` (buggy; one
  of the first apps to debug with this tool).
- DyneTK's protocol was sniffed from a live connection long ago; its
  details are not documented anywhere else.

## Known bugs (to fix)

Every bug found goes here until it is fixed; then check it off and move it
to HISTORY.md ("Bugs fixed"). The ones still open from before (B3, B15,
...) are in `Matt/CLAUDE-bak.md`.

- [x] B31 `newtc -nsof x.nsof` can't read the NSOF of a bare nil (`02 0A`,
  a `code` reply from the Newton): "Can't read NSOF". Found 2026-10-04
  (Test/ntk/captures/capture3/obj006_code.nsof). Fixed 2026-10-05:
  handleArgNsof took a nil result for a failed read (the reader throws
  on a bad stream).
- [x] B35 The compiler's walker for closed-over variables (`WalkNodes`,
  TOKENassign) skipped array element assignments (`a[i] := value`):
  a local used only there (in the array, the index, or the value) inside
  a closure wasn't closed over and was "Undefined variable" when the
  closure ran. Like the `:` receiver fixed before (`closure_send`).
  Found 2026-10-05 in the agent (`cache[1] := call packageParts with
  (...)`); fixed in Frames/Compiler/Compiler.cc. Test `closure_aset`.
- [x] B34 newtc -dap crashed (SEGV in `UnsafeSymbolEqual`, from
  `ICache::lookup`) on a DAP request after a garbage collection. The
  lookup caches are weak: `ICache::update` set a dead slot symbol to
  INVALIDPTRREF but kept the entry valid when its receiver lived on (the
  request's command symbol, from `Intern`, dies after the request; the
  handler frame lives), and the next lookup of a symbol with the same
  hash read INVALIDPTRREF as a symbol. A dead implementor likewise turned
  an entry into "doesn't exist". Fixed 2026-10-05 (Frames/Lookup.cc): an
  entry whose receiver, slot, or implementor died is dropped. Found by
  the stepping test (a GC at the right moment).
- [ ] B33 Undefined global function: newtc's interpreter throws
  `{errorCode: -48808, value: 'Foo}`, the ROM `{errorCode: -48808, symbol:
  'Foo}` (Einstein, Toolkit Protocol.md 6.1). Check the other errors'
  slot names against the ROM too. Found 2026-10-05 (test_remote.py
  accepts both for now).
- [ ] B32 `newtc -nsof` with a missing file or one that isn't NSOF ends in
  "Unhandled exception evt.ex.pipe -- warm reboot!" (or evt.ex.fr.store)
  with exit code 0, instead of an error message and exit code 1. (Before
  B31's fix too.) Found 2026-10-05.
