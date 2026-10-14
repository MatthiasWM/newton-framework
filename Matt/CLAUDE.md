

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
      when installing (a form part's InstallScript): done in 3.15.
- [ ] 3.11 The real MP2x00 on the serial port (57600, timeouts,
      reconnects).
- [x] 3.12 Attach (2026-10-06). DAP `attach` (DAP.ns; remote only:
      "target" required): connect and prepare the Newton (agent, NSDT) as
      for launch, but install and run nothing. "program" (optional) is the
      package installed there, for its line tables or debug map
      ("debugMap"); newtc checks it is installed and warns if its size
      differs ("another build?"), then sets its breakpoints
      (`PackageReady`, shared with RunPackage). Without a program: the
      Debug Console, exceptions, and BreakLoop() stops (no source).
      Disconnect detaches: breakpoints removed, the Newton runs on.
      VSNewt (uncommitted, Matt's repo): "attach" attributes, snippet
      "Attach to Einstein", resolveDebugConfiguration accepts an attach
      without program.
      Fixed on the way: `NTKClose` waited 3 s for an LD it ignored
      (`terminate()` already counted as disconnected), while its TCP server
      accepted the Newton's next connection (now: until the link is
      down); the session end sent ExitBreakLoop once per break loop level
      by our count, which lags right after continue, and an extra one
      threw on the Newton and stopped it in a new break loop (now guarded:
      `ExitBreakLoopBlock`, also for continue). `-ntk-device` serves
      several sessions (env NEWTC_NTK_DEVICE_SESSIONS; its packages stay;
      a moment between sessions, a failed handshake tried again), and its
      GetPkgRefInfo has `size`. Test: test_dap_remote.py, one device for
      four sessions (launch installs; attach: no install, a breakpoint
      stops; attach with another build: a warning; attach without a
      program: evaluate, the earlier breakpoint gone).
- [x] 3.13 The background server (2026-10-13). `newtc -dap-server <port>`
      serves one session after the other (VS Code: `"debugServer": <port>`);
      the target stays in launch.json (`"target"`), so no new option: a
      session with the same target as the open link uses it ("The Newton
      on tcp is still connected."; `NTKOpen` keeps it), no new tap on
      Connect Inspector. Each session starts fresh (DAP.ns and DAPRemote.ns
      loaded again; seq, input, exception count reset: `DAPAcceptClient`).
      `DAPExit` in a session of the server throws `dap.sessionEnd` (not
      under evt.ex: no NewtonScript handler catches it) instead of quitting;
      a client that leaves before launching ends only its session. At the
      end of a session `EndSession` keeps the link (global `dapServer`):
      the agent's breakpoints go, break loops are left, breakOnThrows goes
      off on the Newton. Between sessions newtc keeps the link alive while
      it waits for a client (`NTKIdle` every 0.25 s: the 15 s ping, the
      Newton's packets): `DAPServerIdle` leaves a break loop at once (a stop
      nobody sees would hang the Newton); text, exceptions, and the link's
      end and a new connection are printed in the server's terminal (the
      DAP output translator writes to stderr while no client is there).
      A program run here (no "target") still ends the server: it changes
      newtc itself (test_dap_extras.py unchanged). Test:
      `Test/ntk/test_dap_server.py` (a client that leaves early; launch,
      attach, launch over one link with idle pings between; the global from
      session 1 in session 2; a BreakLoop() that comes after the disconnect
      is left; the server still runs). Not yet: Ctrl-C ends the server
      without 'term' (Toolkit says "connection lost"); VSNewt could start
      the server itself (for newcomers: no terminal step).
- [ ] 3.14 Our own connection tool (instead of Toolkit.pkg; 57600 bps;
      reconnect). Decided with Matt (2026-10-13): a separate package, not
      inside the agent (newtc replaces the agent when its version changes,
      during a session; this one is installed once and never replaced
      meanwhile); after a drop, try again a few times; 57600 bps with 3.11.
  - [x] 3.14a The "newtc" app (`Matt/Debugger/Connect.ns`, package
        "Connect:newtc", a form part, app `|Connect:newtc|` in Extras):
        `newtc -connect-package newtc.pkg` writes it; the user installs it
        once (Einstein: Install Package), no NTK needed. A floater: the
        state, Connect/Disconnect, "Stay connected". The connection is the
        ROM's, as Toolkit makes it (decompiled Toolkit.pkg):
        `ntpTetheredListener(true, 3, nil)` (MNP serial, 38400) from a
        deferred send after the status says "Connecting..." (it blocks
        until newtc answers or the attempt fails), NTK's 'dante hello, a
        power-off handler while connected; Disconnect:
        `ntpTetheredListener(nil, nil, nil)`. With "Stay connected", a
        lost connection (NTKAlive() checked every 2 s by a function copied
        into RAM, harmless once the package is gone) is tried again 3
        times, 5 s apart, then it stops and says why
        (Toolkit's error texts, reworded for newtc). The state lives in the
        app's view frame (stays while closed). A connection made by Toolkit
        is shown and left alone; removing the package ends our connection.
        newtc's "Waiting for a Newton" names both apps. Test:
        `Test/ntk/test_connect_app.py` (FLTK: the app in newtc, the ROM's
        listener stubbed; a failed attempt tried again, NTK's hello, a drop
        tried 3 times then the reason, Disconnect stops the listener).
        Matt's first try on Einstein (2026-10-13): installed, tapping its
        icon soft-locked the Newton. Extras opens an app with the ROM's
        ButtonToggle, which sends Toggle() to the base view: version 1 had
        its own Toggle (the button) and Show, so the tap started a blocking
        connection attempt (and 3 retries) instead of opening the app.
        Version 2: no method names the system sends to views
        (ShowState, ConnectOrDisconnect, ...), and a Connect that fails
        isn't tried again (only a lost connection is); the test opens the
        app with Toggle(). Found B36 on the way. Version 3 (Matt: works;
        the floater was off to the right and cut off, the state didn't
        always fit): centered (vjParentCenterH, 220 wide), the state one
        line taller.
        To check on Einstein: install, connect, a session, a drop (Ctrl-C
        the server, start it again within 15 s), how long a failed attempt
        freezes the Newton.
  - [ ] 3.14b 57600 bps (with 3.11, a real MP2x00): an endpoint options
        frame for `NTKListener(true, options, ...)` (EzConvertOptions, ROM
        0xB1124: slots like openOptions/bindOptions), a speed setting in the
        app.
      Note (Matt, 2026-10-06): an app that reconfigures the serial port
      (EinsteinPrefs: `call Einstein.Platform with
      ('GetSerialPortDriverNames, ...)` and friends) can't be debugged
      over that serial port; stepping over that line hung. On Einstein, a
      transport that isn't the emulated serial port would avoid it
      (something to look at here).

Status 2026-10-13: 3.11 waits for a working MP2x00 (Matt's three machines
need repairs: two recaps, one ePaper display conversion). 3.12, 3.13, and
3.12 to 3.18 done except 3.14b (57600 bps), which waits for hardware with
3.11. Debugging on a Newton needs nothing from NTK any more.

### Wishes from trying it out (Matt, 2026-10-06; no priority)
- [x] 3.15 Breakpoints in an InstallScript, and "Break in InstallScript"
      (2026-10-13). A new exception filter `installScript` (DAP.ns; off
      by default; works with a remote Newton only, a local session ignores
      it). When a breakpoint lies in a part's install function (a form
      part's devInstallScript, else InstallScript; an auto part's
      InstallScript: as the ROM calls them) or the filter is on,
      DAPRemote.ns uploads a copy of the package (`InstallPlan`,
      `WrapInstallScripts`): the install function moved to
      `|DAPAgent:InstallScript|`, a wrapper in its place (compiled from a
      string), and first tells the agent the plan
      (`|DAPAgent:InstallPlan|`: per part the breakpoints as [path in the
      function, pc], the decompiled functions in it with their index in
      knownFunctions, entry). On the Newton the wrapper takes the function
      that will run (a form part: `EnsureInternal` of it, as the ROM's
      InstallFormPart does), calls `|DAPAgent:Installing|(fn, part index)`
      and then the function with the same self and argument. Agent v12
      sets the breakpoints in that very function (also in the copy), makes
      its decompiled functions known (the stack's lines), and for the
      filter a one-time breakpoint at pc 0: stop reason "entry" ("Paused at
      the start of an InstallScript"). From the plan on, the breakpoints'
      paths lead to the moved function (`InstalledPath`; also the Known
      entries after the install). Without either the package goes
      unchanged ("Uploading <name> with its InstallScript wrapped for
      debugging." says when it doesn't). Test:
      `Test/ntk/test_dap_install.py` (an auto part with -g: breakpoint,
      variable, step, continue, installed; a form part with the filter:
      "entry" at its first line in the ROM's copy; the form part
      decompiled: a breakpoint in the decompiled InstallScript, its line in
      the stack; unchanged without). The 26 DAP transcripts in Test/dbg
      got the new filter in their initialize response. Not covered: a
      RemoveScript (the same trick would work when deleting), and 3.17
      must hash the uploaded (wrapped) bytes, not the file's.
- [x] 3.16 Progress while uploading a package (2026-10-13). Every upload
      in a remote session shows on one Debug Console line: "Uploading
      hello2.pkg (3 KB) .......... done in 0.8 s", a dot per tenth the
      Newton acknowledged (new native `NTKUploadProgress()`: [acknowledged,
      total] from MNPLink's pending bytes, while an
      `NTKInstallPackageAsync` upload runs). Also the agent's and NS Debug
      Tools' uploads: Remote.ns `InstallPackage` uploads asynchronously when
      the library has a `reporter` (DAPRemote: UploadStart, UploadTick,
      UploadEnd), else as before. `NEWTC_DAP_TIMES=1` shows what the start
      of a remote session did when ("[4.8 s] agent checked (installed if
      needed)", ... up to "breakpoints set"), to find where the time goes.
      For tests `-ntk-device` can be as slow as a serial line
      (`NEWTC_NTK_DEVICE_BPS=38400`, MNPLink `setThrottle`). Measured there
      at 38400 bps: a fresh Newton spends 4.5 s on the agent (16 KB, about
      the line's 3.8 KB/s), hello2 (3 KB) 0.8 s, everything else together
      about 0.5 s; so the first session after an agent upgrade is the slow
      one (3.17, 3.18 may make the agent smaller). Einstein's numbers: to
      measure (`NEWTC_DAP_TIMES=1 newtc -dap-server 4711`). Test:
      `Test/ntk/test_dap_upload.py`. Fixed on the way: test_deploy.py
      expected agent version 11 (now read from Remote.ns).
- [x] 3.17 Don't upload an unchanged package again (2026-10-13). Decided
      by the bytes: after an upload newtc calls the agent's
      `|DAPAgent:NoteUpload|(name, hash)` (new native `NTKHash(binary)`:
      FNV-1a over the bytes actually uploaded, also a wrapped package's);
      agent v13 keeps it in RAM (a reboot or a new agent: one more upload)
      with the size and timestamp GetPkgRefInfo reports then.
      `|DAPAgent:UploadedHash|(name)` answers the hash only while the
      package is installed with that size and timestamp. RunPackage skips
      delete and upload when it matches the local bytes ("hello2.pkg is
      unchanged on the Newton: not uploaded again ("reinstall": true
      uploads it, and its InstallScript runs)."), then sets the
      breakpoints and opens the app as before. Always uploaded: launch
      `"reinstall": true` (some apps reset their state in the
      InstallScript), and with InstallScript breakpoints or "Break in
      InstallScript" (3.15). `-ntk-device`'s GetPkgRefInfo has a
      `timestamp` now (a new one for each install). Test:
      `Test/ntk/test_dap_reupload.py` (one Newton, seven sessions: uploaded,
      unchanged with its breakpoints working, reinstall, a rebuild, wrapped
      though unchanged, unwrapped again, unchanged). VSNewt (Matt's repo):
      the launch attribute "reinstall" (boolean) for package.json.

### No Apple files on the Newton (Matt, 2026-10-13)
Goal: a newcomer needs only newtc. The newtc app (3.14a) replaced
Toolkit.pkg; NS Debug Tools.pkg is the last file from NTK (stack,
variables, breakpoints, stepping need it; without it: output, exceptions,
evaluate). It can't be replaced by NewtonScript alone: its few
`BinCFunction`s reach the interpreter (Toolkit Protocol.md 7.3).
- [x] 3.18 Our own debug natives, instead of NS Debug Tools (2026-10-13).
  - [x] 3.18a Inventory (2026-10-13): Toolkit Protocol.md 7.4. The agent
        uses 16 NSDT functions (stack, variables, breakpoints, Step/StepIn/
        StepOut, GloballyEnableBreakPoints, the BreakLoop hook); they rest on
        NSDT's NewtonScript (stack object, breakpoint list, instruction
        decoder) and two ARM blobs: NSDInstallBreakPoints and
        NSDEnableBreakPoints (336 bytes), NSDMakeNSDebugAPI and the 13
        methods of NSDSelfFuncs (2600 bytes). The ARM code calls the ROM
        through the public jump table at 0x01800000 (+ offset; physical
        0x13000, the same entries in every ROM build; names from newton-re
        src/armcpu/PublicJumpTable.cpp): AllocateRefHandle 0x0818,
        BinaryData 0x0828, AllocateCObjectBinary 0x27E0,
        TNSDebugAPI::AccurateStack 0x27EC. The whole TNSDebugAPI is there
        (0x27EC-0x283C, with NewNSDebugAPI 0x2838, DeleteNSDebugAPI 0x283C),
        and GetGInterpreter 0x1EE8, TInterpreter::SetBreakPoints 0x1FF4,
        EnableBreakPoints 0x20C0. We need 14 natives (not AccurateStack,
        SetProgramCounter, the printing ones), each one jump-table call
        plus argument conversion.
  - [ ] 3.18b The ARM code. A native is a frame {class: 'BinCFunction,
        code: <binary>, numArgs, offset} in the package; newtc already
        makes the binary with MakeBinaryFromHex (Matt). Our own small
        routines, calling the ROM **only through the fixed public jump
        table** (0x01800000 + offset, 7.4; Matt, 2026-10-13: so ROMs in
        other languages work), never a ROM-build address; not Apple's
        bytes.
    - [x] 3.18b1 (2026-10-13) What NSDT's ARM code
          shows (disassembled with arm-none-eabi-objdump -EB): a native is
          C `Ref F(RefArg rcvr, RefArg a1, ...)`: r0 the receiver, r1...
          the arguments, each a pointer to a RefVar -> RefHandle* -> Ref;
          the Ref back in r0. All 33 ROM calls are stubs `ldr pc, [pc,
          #-4]` + 0x0180xxxx (also in the 336-byte blob, at its end;
          NSDT checks that GetGInterpreter isn't an "unimplemented" entry
          first). Its two literal addresses (0x6F80, 0x5FC0) point into
          its own package (relocated when installed); our packages have
          no relocations, so our code is position independent (adr). The
          method natives read the receiver's slot nsDebugAPI (Intern,
          GetFrameSlotRef) and take BinaryData of that CObject binary as
          the TNSDebugAPI*. Done: `Matt/Debugger/AgentNatives.s` (GNU as
          syntax: arm-none-eabi-as -EB -march=armv4), assembled by
          `Matt/tools/build_agent_natives.sh` into the checked-in
          `Matt/Debugger/AgentNatives.ns` (kAgentNativeCode, kAgentNatives:
          offset and numArgs per native; building newtc needs no ARM
          tools). Five natives first: InstallBreakPoints,
          EnableBreakPoints, MakeNSDebugAPI (its destructor is the jump
          table entry of DeleteNSDebugAPI, 0x0180283C, not code in our
          package, which may be gone when the collector frees it: a new
          agent), NumStackFrames, Function. `Test/ntk/natives_test.ns`
          builds a package (align4) that defines them as
          |NativeTest:<name>|. Assembler: GNU as (Matt: the old ARM6asm
          and ARMLink in the path are outdated). The StrongARM has no
          Thumb (Matt): ARMv4 code only, no bx; `-march=armv4` makes the
          assembler refuse both.
    - [x] 3.18b2 (2026-10-13) The other methods:
          ProgramCounter (PC), Receiver, Implementor, GetVar, SetVar,
          FindVar, SetFindVar, NumTemps, TempValue, SetTempValue: 15
          natives in all, 952 bytes; six macros for their argument kinds
          (an integer frame, a second integer, a symbol, a value; result a
          Ref, an integer, or nil). Fixed on the way: ApiOf read the
          receiver's Ref before Intern, which may allocate and move objects
          (NSDT reads it after). The test package's
          `|NativeTest:SelfTest|()` tries them all on its own frame (and
          asks NSDT the same, if installed); its logic checked in newtc with
          the C++ stand-ins in place of the natives (FindVar needs a
          variable in the argFrame: one a closure uses).
          On Einstein (Matt, 2026-10-13), all as expected, and as NSDT:
          frames 4, frame 2 (NSDT: the same, sameFunction), GetVar of the
          first argument, SetVar 43, FindVar 51, SetFindVar 52,
          EnableBreakPoints [true, true], InstallBreakPoints nil (put back),
          pc 133 (NSDT 586: asked later; the stand-ins gave the same two).
          To look at in 3.18c: NumTemps -14, from NSDT as well, so the ROM's
          answer for a frame that isn't stopped in a break loop (the
          stand-in said 10); check it at a real stop (GetAllTempVars).
  - [x] 3.18c newtc's own debug tools (Matt, 2026-10-13: a separate
        package, NSDT-compatible, not inside the agent).
    - [x] 3.18c1 (2026-10-13) Package "DebugTools:newtc"
          (`Matt/Debugger/DebugToolsPackage.ns`, built in newtc as the global
          NewtcDebugToolsPackage, 37 KB): newtc's port of NSDT's NewtonScript
          (kNSDTools, MakeDisassembler, Disasm, DisasmRange) and our 18 ARM
          natives (3 more: AccurateStack, SetProgramCounter, RefToHexString
          via the ROM's sprintf and MakeString); NSDFindSlotName is
          NewtonScript. Its InstallScript does what NSDT does: the natives
          under NSDT's names, NSDSelfFuncs, the global functions from the
          makers, StackTrace and BreakLoop replaced (the ROM's kept as
          StackTraceOld, NSDOriginalBreakLoop, so the agent's
          NSDBreakLoopEntry hook works as with NSDT), and
          |DebugTools:newtc:Version|; RemoveScript undoes it. NSDebugTools.ns
          is split: the definitions, and NSDebugToolsInstall.ns (what `-dbg`
          runs after them). Remote.ns EnsureDebugTools: newtc's own tools
          (installed, or replaced by a newer version) unless the Newton has
          Apple's NSDT; Apple's only with NEWTC_NSDT_PACKAGE (no more default
          path to NTK's folder). newtc can run the package: `gBinCFunctionHook`
          (Interpreter.cc, not in ROM) gives a BinCFunction with our natives'
          code its C++ stand-in (`NTKAgentNativeStandIn`), so `-ntk-device`
          runs the whole package in tests. Test: `Test/ntk/test_dap_tools.py`
          (a stock Newton gets the tools after the agent; a breakpoint, the
          stack, variables, a step; a second session doesn't upload them
          again). Found B37 on the way.
    - [x] 3.18c2 On Einstein without Apple's NSDT (Matt, 2026-10-13):
          locals all there, the stack fine (so NumTemps at a real stop is
          right; the -14 of 3.18b2 was a frame not stopped in a break
          loop). A Newton now needs nothing from NTK: the newtc app
          (3.14a), the agent, newtc's debug tools.

### 4. Easy packages (Matt, 2026-10-14)

Goal: a VSNewt user writes a simple app without knowing the package frame,
only what differs from a sane default, in NTK's style:

```
newtAppName := "test:NEWTC";
newtAppSymbol := '|test:NEWTC|;
newtAppLabel := "test";
newtAppIcon := MakeIconFromPNG("test_icon.png");
newt.theForm := {_proto: protoFloatNGo, viewBounds: {left: 0, top: 30, right: 180, bottom: 220}};
wTitle := {_proto: protoStaticText, text: "test", ...};
AddStepForm(newt.theForm, wTitle);
StepDeclare(newt.theForm, wTitle, 'wTitle);
```

The platform is built into newtc (decided with Matt, 2026-10-14): the
2.1 constants (2.0 is a subset; 1.x not needed) as a NewtonScript source in
the repo, embedded like DAP.ns, so `protoFloatNGo` just works and always
matches the ROM newtc runs (717006). Apple's platform files stay optional
(4.8).

Found while planning:
- NTK's binary platform files are plain NSOF. Of "Newton 2.1"'s 2,056
  platformConstants, 1,932 are simple values (831 magic pointers, 931
  numbers, 96 strings, 61 characters, ...).
- The other 124 are whole objects: protoendpoint (274 KB as text), the
  mail protos (protoMailTransport, protoMailRouteSlip, ...),
  kMungeBitmapFunc, deprecated 1.x functions, and an embedded package
  installed with SuckPackageFromBinary. NTK copies such a value into every
  package that uses it. We don't need them.
- The ROM names its magic pointers (ROMData/forNTK/MagicPointers.s:
  SYMprotoFloatNGo, ...; fix B44 first), and the C headers hold the view
  flag and justification constants (Views/ViewFlags.h).
- newtc reads Newton bitmaps already (Host/Shapes.cc, BookWriter), so
  writing them is the inverse (4.6).

Matt's list (MATT.md): DefineGlobalConstant, UndefineGlobalConstant,
IsGlobalConstant, GetLayout, SetPartFrameSlot, GetPartFrameSlot, Load,
ReadStreamFile, OpenResFile; -platform, -I includepath, -script a -script b
-compile (b forgets most of a).

To decide with Matt (before the step that needs it):
- D1 Names: separate globals newtAppName, newtAppSymbol, newtAppLabel,
  newtAppIcon, newtAppVersion, newtAppCopyright (and `newt` with theForm,
  InstallScript, RemoveScript, ...), as in the example (`newAppIcon` there
  is newtAppIcon?), or one frame newtApp {name, symbol, ...}. Also define
  NTK's constants kAppName, kAppSymbol, kPackageName from them, so NTK
  sources (and decompiled ones) compile unchanged.
- D2 (for 4.8) ReadStreamFile stays NTK's (it only returns the object);
  installing Apple's platform is UsePlatform(path) (= -platform). Or
  ReadStreamFile of a platform file installs it too (not NTK).
- D3 PNG decoding: stb_image (one public-domain header, every build) or
  FLTK's Fl_PNG_Image (only builds with FLTK).
- D4 Where the package comes from: `newtc -build app.ns -opkg app.pkg`
  (defaults, the script, then the package assembled from newtApp*/newt), or
  -script with an explicit MakeNewtAppPackage() at its end.

Steps (each testable in newtc; a package on Einstein by Matt):
- [x] 4.1 Load errors (2026-10-14). An error in a loaded file is passed on
      to the caller (FLoad -> ParseFile with rethrow; the ROM's ParseFile
      printed it and returned): it keeps its file and line, a `try` around
      Load catches it, otherwise it stops the loading file and -script. A
      missing file throws {errorCode: -43 (the Mac's fnfErr, NTK's world),
      value: "<name> (looked in ...)"} at the Load call: "File not found".
      `-script` stops newtc when an error stopped the script ("-script:
      "x.ns" stopped at the error above.", exit code 255, newtc's usual), so
      `-opkg` after it writes nothing (it said "Not a package" before); so
      does `-s`. A DAP launch runs the program as before (`runScript`, its
      exceptions counted). `-I <folder>`: Load looks next to the calling
      file, then in each -I folder, then in the current directory. Tests:
      cases load_errors (try, a nested Load next to its file, a syntax error
      in a loaded file), load_missing, load_path (-I); four cases end with
      the new error line; plain cases get $DIR for their folder too.
- [x] 4.2 ReadStreamFile(path) (2026-10-14). NTK's: an NSOF file read
      into an object. FReadStreamFile (Frames/RefIO.cc) was compiled
      (newtc builds forNTK) but not registered, and gave nil for any error.
      Now it finds the file as Load does (FindScriptFile, Compiler.cc: next
      to the running file, -I, the current directory; also LoadDataFile),
      throws "File not found" (-43) or "Object stream has unknown format
      version" (-48006, "<path> isn't NSOF"). IsGlobalConstant and
      UndefineGlobalConstant registered; IsGlobalConstant returned the C++
      bool as a Ref (0, or 1, which Print took for a pointer and crashed):
      now nil or true. Reads Apple's "Newton 2.1" (2,056 constants) in about
      0.1 s. Test: case read_stream_file (read_stream_file_data/sample.nsof,
      made with newtc -onsof).
- [ ] 4.3 newtc's built-in 2.1 platform.
  - [x] 4.3a The constants (2026-10-14). `Matt/Platform/Newton21.ns`, one
        line each (`constant protoFloatNGo := @180;`): 831 magic pointers by
        number, 1,103 numbers, strings, characters, symbols by name (inf,
        -inf, nan through DefineGlobalConstant: no literal for them), and a
        comment listing the 122 constants NTK's file has as objects (protos,
        functions, bitmaps; not here). Names from NTK's "Newton 2.1" (Matt,
        2026-10-14: a text list of API names and simple values is fine,
        never the file itself), made by `Matt/tools/make_platform.py
        <platform file>` (newtc -nsof -print, the platformConstants frame),
        the magic pointers checked against the ROM's own names (RSData.s;
        10 have none there); edited by hand from now on. Embedded
        (gPlatformScript) and run at the end of init(): always defined, as
        in NTK. Found on the way: B45 (negative real constants), B46 (the
        compiler leaked 9 RefHandles per function: 2,000 statements took
        3.8 s, newtc's start went from 0.5 to 1.0 s with the platform; now
        0.08 s with it). Test: case platform_constants.
  - [ ] 4.3b The platform's 32 build-time functions (platformFunctions:
        vfShadow(x), fxColumns, PackRGB, ButtonBounds, DefineUnit, ...;
        NTK's "constant functions", evaluated while compiling) and
        platformWeakFunctions (DefConst, XORPixels, MakeExtrasIcons): written
        in NewtonScript, in Newton21.ns or Build.ns (4.4).
- [ ] 4.4 NTK's build functions on the desktop (an embedded Build.ns):
      AddStepForm(parent, child) (child to parent.stepChildren),
      StepDeclare(parent, child, 'sym) (parent.stepAllocateContext, so the
      child is parent.sym at run time), DefConst, GetPartFrameSlot,
      SetPartFrameSlot; later GetLayout (NTK layout files), OpenResFile.
- [ ] 4.5 The newtApp setup (D1, D4): a default package (signature, id,
      version, copyright, one form part with flags, InstallScript and
      RemoveScript that do the usual, theForm a protoApp or protoFloatNGo,
      the app symbol in theForm), the user's script, then the package
      assembled and checked with clear messages (an app symbol is
      'name:SIG', the package name and symbol agree, lengths), NTK's
      kAppName/kAppSymbol/kPackageName defined. Sample in VSNewt.
- [ ] 4.6 Images from PNG files (D3): MakeBitmapFromPNG(path, options),
      MakeIconFromPNG(path, options): 1 bit (threshold or dithered), 4 bit
      gray (16 grays, NOS 2's colorData {cbits, bitDepth}), with and without
      alpha (alpha -> mask); B&W icons (bits + mask, for every Newton) and
      gray icons (colorData too, for 2.x); sizes checked. The bitmap format
      from newtc's readers (Host/Shapes.cc, BookWriter) and hello2's icon.
      Test: PNG -> bitmap -> newtc draws it -> compare; on Einstein.
- [ ] 4.7 VSNewt (Matt's repo): "New Newton App" (app.ns with the newtApp
      setup, icon.png, launch.json); later completions and hover help (the
      names from 4.3).
- [ ] 4.8 Optional: Apple's platform files (was 3.19), for the protos
      newtc doesn't have (mail, the endpoint layer, NTK's exact set).
      `-platform <file>` and UsePlatform(path) (D2): read the platform frame
      (platformConstants, platformFunctions, platformWeakFunctions,
      platformVariables, NameMap, the layout editor's Templates,
      ViewClasses, ScriptSlots, AttributeSlots); its constants replace or
      extend the built-in ones (object values copied into the package as
      NTK does), its functions run at build time. Apple's files: never in
      the repo; the user points newtc to one. The "... Defs" text is a
      listing for people, not used (CR, MacRoman; not compilable).

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

- [x] B46 The compiler leaked every function's CFunctionState (found
  2026-10-14: 2,000 top-level statements took 3.8 s in the debug build).
  Its RefStructs held 9 RefHandles, never given back; every 32 more grow
  the handle table, and each growth is a full GC over all of them, so the
  time per statement grew with the number compiled so far. The original
  deletes the state in endFunction; newtc's didn't (since the first
  commit), likely because the destructor deletes fNext, which is the
  enclosing state, still in use. Fixed (Frames/Compiler/Compiler.cc):
  fNext cleared, then deleted. Handles in use stay at 28 instead of +9 per
  statement; newtc starts in 0.08 s with the platform (1.0 s before).
  NSDebugTools.ns's local `breakPoints` is now `breakpoints`: with the leak
  gone, the symbol 'breakpoints was collected and NSDT's spelling won,
  and DAP answered with "breakPoints" (symbols keep the first spelling).
- [x] B45 `constant k := -0.5` gave garbage (found 2026-10-14 with the
  platform's kNegativeZero): evaluateConstantExpr negated with
  `MAKEINT(0) - value`, which works only for an integer's Ref; a real's
  Ref is a pointer. Now an integer or a real is negated by value. Test:
  case constant_negative_real.
- [x] B44 Magic pointers @42, @273, @274 weren't objects in newtc
  (found 2026-10-14 while walking "Newton 2.1"'s platformConstants:
  ROM_capslockLight is @42). In the ROM these three entries are bare
  symbols ('obsolete, 'espy, 'helvetica). ResMaker wrote them as
  `MPnnnn: Ref MAKEPTR(SYMxxx)` and the table as MAKEPTR(MPnnnn): a pointer
  to a word that holds a Ref, not to an object. So ClassOf(@42) was 0, and
  Length(@42) threw "ObjectPtr of non-pointer" or crashed (ASan: SEGV in
  Length). Fixed 2026-10-14: the table holds the symbol itself
  (MAKEPTR(SYMobsolete), ...), the MPnnnn lines are gone, in all three
  copies of MagicPointers.s (newtc builds ROMData/forNewton/, included by
  its RefData.s; forNTK/ and Frames/ the same). ResMaker now writes the
  entries into a temporary file first and the table from what it found
  (not run here: it needs the ROM image setup; it compiles). Test: case
  magic_pointer_symbols.
- [x] B43 `Load(filename)` (2026-10-14): newtc has it (FLoad -> ParseFile,
  as -script: statement by statement, errors with file and line, the
  debugger's hook sets pending breakpoints in each new statement). But a
  relative name was looked for only in the current directory (VSNewt starts
  newtc in the workspace: "couldn't open file"), and the line tables named
  the file as given. Now a relative name is looked for next to the file
  that calls Load (ParseFile keeps a stack), then in the current directory,
  and the file is compiled under its absolute path, so breakpoints set in a
  loaded file (before it is loaded) stop there: top-level statements and
  its functions. Test: case dap_load (dap_load.nsi). Not on a remote
  Newton: the ROM has a Load (FLoad, 0x002B8290), but the Newton has no
  such file; newtc would have to take loaded files along (compile them into
  the program it sends): a feature for later.
- [x] B42 A remote session failed about half the time with a release
  build (found by VSNewt, 2026-10-13): "Exception: ObjectPtr of
  non-pointer" right after the deploy, the session ended before the
  program ran. The NSOF writer's precedents table (CPrecedentsForWriting,
  Frames/StreamObjects.cc), a radix tree over the bits of the objects'
  addresses, was 32-bit code on 64-bit Refs: `(1 << n)` as an int, for
  n = 31 negative, widened to a Ref tested bits 31 to 63 at once, and the
  bit telling two Refs apart came from their low 32 bits. With objects in
  different regions above 4 GB (as the release build's allocator placed
  them) find() missed objects already written, and the program block,
  which holds itself in its literals (PrepareProgram), was walked without
  end (thousands of prescan frames, then garbage). Fixed: bits tested in
  the Ref's width, the top bit 63. Found with a backtrace at the throw
  (ASan builds never failed: other addresses). Also: NTKCompileFile's
  `failed` is volatile (changed after a longjmp). Test: test_dap_server.py
  on the release build 18 of 18, before about 1 in 2.
- [x] B41 `global x := value` did nothing (found with VSNewt,
  2026-10-13): the compiler emits SetGlobal(value, 'x), newtc had only a
  stub (with the wrong arguments). Now FSetGlobal (Frames/Globals.cc) as
  the ROM's (0x002B72C8): sets the global variable, its symbol made
  internal, returns the symbol. `global` is a top-level statement (the
  ROM's grammar). Test: case global_keyword.
- [x] B39 Error lines were wrong (found by VSNewt, 2026-10-13): a syntax
  error came at the lexer's line after its lookahead (an error on line 2
  followed by blank lines: line 6), a run-time error in -script at the line
  after the top-level statement that ran (the parser had read the next
  statement's first token). Fixed: syntax errors at the offending token's
  line (`CCompiler::errorLine()`: theToken.location while parsing, else the
  statement being generated, -g, else the top-level statement); run-time
  errors at `topLine()`, the first line of the statement that ran
  (ParseFile, -s, embedded scripts, NTKCompileFile). A dropped trailing ';'
  before `end` makes the parser read `end`: that line is reported (the
  message says "read end"). Tests: cases error_line_syntax,
  error_line_runtime; dap_errors, dap_exception, dap_exception_off,
  for_closure now show the right lines.
- [x] B40 A package frame without flags crashed the writer ("Unhandled
  exception evt.ex.fr.type;type.ref.frame -- warm reboot!", found by
  VSNewt): `if (RefVar flagsRef = ...)` took nil for true. Fixed: no package
  flags means none set; a part without flags is a NewtonScript part
  ({type: 'nos}; it was flags 0, a 'protocol part); the frame is checked
  first (a part array of frames with data, a name), and -opkg reports any
  exception as a newtc error. Test: case package_no_flags.
- [x] B38 A package's info (the creator text after the directory) has no
  stored length and no NUL: the reader took everything up to the end of the
  directory, so newtc's own packages read back with "ø ø ø" (newtc padded
  the directory with 0xBF, NTK's padding between objects). The corpus shows
  NTK pads the directory with zeros. Fixed 2026-10-13: the writer pads it
  with zeros, the reader takes the text up to the first NUL (and drops up to
  three trailing 0xBF from older newtc packages). The unused UpdateRef
  (B37) is gone. Corpus unchanged.
- [x] B37 newtc's package reader (NewtonPackage.cc, 64-bit) misread some
  packages: a part's data came back as another object (newtc's debug tools
  package: its data a symbol, so its InstallScript wasn't there). After
  CopyRef had made every Ref an address, UpdateRef "fixed up" the slots
  again through a map keyed by 32-bit package offsets: an address whose low
  32 bits looked like an offset was replaced by another object. Big parts
  are malloc'ed at 0x...00000000, so their addresses' low bits are small
  offsets. Fixed 2026-10-13: no UpdateRef (CopyRef maps everything, also
  cycles: an object is mapped before its slots are copied). Also made the
  Refs of the package frame being built RefVars (allocations may move
  objects). The written package was fine (checked object by object); a
  Newton reads it itself. Test: test_dap_tools.py.
- [x] B36 `GetRoot():ChildViewFrames()` and `GetRoot():GlobalBox()` threw
  "nil view" in newtc (with an app window open, the break loop then waited:
  it looked like a hang). The ROM's ButtonToggle (how Extras opens an app)
  calls them. newtc's root view isn't a Link: each of its children is a
  window. Fixed 2026-10-13 (Host/FLTK/Links.cc): the root's GlobalBox and
  LocalBox are the screen (displayParams scr...), its ChildViewFrames the
  open top-level views in the order they opened (a list kept by Link), Hide,
  Show, SetupIdle do nothing. ButtonToggle opens an app and closes it when it
  is in front. Test: Test/dbg/cases/fltk_root_view; test_connect_app.py now
  opens the app with ButtonToggle.
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
- [x] B33 Undefined global function (and variable): newtc's interpreter
  threw `{errorCode: -48808, value: 'Foo}`, the ROM `{..., symbol: 'Foo}`
  (Einstein, Toolkit Protocol.md 6.1). Fixed 2026-10-13
  (Frames/Exceptions.cc, ThrowExInterpreterWithSymbol, read in newtonos.s).
  The other throw helpers there already match the ROM's slot names (checked
  against newtonos.s). DAP.ns reads 'symbol, else 'value; test_remote.py and
  the case exception_data expect 'symbol.
- [x] B32 `newtc -nsof` with a missing file or one that isn't NSOF ended in
  "Unhandled exception evt.ex.pipe -- warm reboot!". Fixed 2026-10-13:
  "newtc: ERROR: -nsof: can't open ..." or "... isn't NSOF", exit code
  non-zero.
