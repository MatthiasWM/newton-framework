

# Matt's NewtonScript debugger

## the goal

The `newtc` command line application gets a REPL debugger for NewtonScript,
based as much as possible on what is already inside NewtonScrip (`BreakLoop`)
and what the "NS Debug Tools.pkg" provides (Next, StepIn, StepOut, ...).

The REPL mode will be accessible via command line ( `-dbg` ) by expanding
the BreakLoop interface to also understand `gdb` style shortcut like `n`, `s`,
and `r`. And via DAP Debug Protocol.

## REPL

Much of REPL already comes with NewtonScript, and additional functions are in
"NS Debug Tools.pkg" that came with NTK. WE can decompile the debug tools (there
is some ARM32 code in there) and lift the features and make them part of newtc.

In a first iteration, we only look at ByteCode commands. We must keep in mind
that we want source level debugging later.

### Preserving and activating what we have

NewtonScript comes with a few basics that are extremely helpful here. The
original ROM has support for breakpoints and jumps into an interactive BreakLoop
when a breakpoint is hit. This is probably the first thing that must be reactivated
and tested. The Interpreter in ROM has a "slow interpreter loop" that manages this
that was not lifted from the ROM into this library. I have access to the disassembly,
so if needed, we can decompile it or at least look at how they did it.

More intricate debugging functions (StepIn, StepOut, etc.) came in a sepaarte
package "NS Debug Tools.pkg" that came with NTK. Note that Apple never fully
finished and tested this. "This has not yet undergone extensive official
testing, but seems to work well.". We should stick at least with the APple API,
or even with theeir implementation where it makes sense. Again, ARM32 machine
code is involved.

## Command line mode

For testing and debugging "newtc", a terminal version of REPL mode is available
via `-dbg`. In this mode, we break directly into then NewtonScript `BreakLoop`
where we can enter NewtonScript commands. This already mostly works. Adding
single letter shortcuts maked this mode usable (vs. typing StepIn(), etc. ).
If this works reliable on ByteCode level, we add support for the DAP protocol.

## DAP protocol

I want to implement a debugger interface for the "newtc" executable using the
DAP protocol with Visual Studio Code. The DAP is handled using cppdap:
https://github.com/google/cppdap . DAP mode is enabled by passing the `-dap`
flag.

DAP is to be implemented on top of a REPL debugger interface.

ByteCode debugging should map directly. How do we handle the display of the
ByteCode diassembly though?

## Source Level Debugging

### Internal Debug Information

To allow source level debugging, we must map the pair sourefile/linenumber to
the newtonscript function address/ByteCode offset (PC) - and back. We need
to find an internal representation to do this mapping and write a function
for converting in either direction.

### Generating and storing the debug map

We need to generate this map as external debug data files that can be loaded
by `newtc` when debug mode is enetered. We have three cases:

- Compiling a source file into NewtonScript directly: the compiler must
  generate the map directly, no inbetween file is needed (compile and debug)

- Compiling into a NSOF or pkg file: the compiler must write the debug
  information file alongside the NSOF/pkg file. Mapping functions could be done
  by adding an index or UID to every function that the compiler generates

- Decompiling packages: in this case, newtc load a pkg file without debug
  information and decompiles it. Here, the decompiler must geenrate the map.
  Every function has a natural index that should be repeatable after loading
  the package.

- Decompile ROM: decompiling the ROM, again, the decompiler generates the map.
  Since the ROM is stored as a single binary blob, we should be able to look
  up functions inside the blob by storing the binary offset in the map file.

To sum up, every separately loaded chunk (ROM, NSOF, pkg, compiled code) gets
its own map file. All required map files are loaded when in debug mode. An
internal map is generated. Unmapped functions will fall back to ByteCode
debugging.


---

# Working notes (maintained by Claude — update as we go)

## Findings (2026-09-25)

- **The break loop already works.** `newtc -s 'Print("before"); BreakLoop(); Print("after");'`
  with commands piped on stdin enters the loop, evaluates NewtonScript, and
  `ExitBreakLoop()` resumes. `FBreakLoop`/`FExitBreakLoop` live in `REP.cc`.
  But `StackTrace()` prints `*** Skipping bad stack frame` for every frame and
  warns "Inaccurate stack trace. Use SetDebugMode(true)".
- **The ROM breakpoint machinery is ported but dormant.** `gFramesBreakPoints`,
  `gFramesBreakPointsEnabled`, `SetBreakPoints()`, `EnableBreakPoints()` and
  `CInterpreter::handleBreakPoints()` exist in `Frames/Interpreter.cc`;
  `handleBreakPoints()` has no call site (the ROM called it from the "slow"
  interpreter loop `SlowRun`, which was not ported).
- **NS Debug Tools.pkg decompiles cleanly** with `newtc -pkg ... -decompile`
  (about 4000 lines). Almost everything is NewtonScript. `Step`, `StepIn`,
  `StepOut`, `RunUntil`, `Where`, `InstallBreakPoint`, and the rest work like this: they
  inspect the stack through a debug API object, decode the current
  instruction (`MakeDisassembler`), install a *temporary* breakpoint at the
  next PC (or the branch target, or PC 0 of the callee), then call `ExitBreakLoop()`.
  `FindBreakLoop()` locates the paused frame by searching the stack for
  `functions.BreakLoop`, so it doesn't need a separate "paused location" snapshot.
- **The ARM code is small and already has C++ equivalents here:**

  | NSDT native (ARM `BinCFunction`) | C++ in this repo |
  |---|---|
  | `NSDInstallBreakPoints(bps)` | `SetBreakPoints()` (Interpreter.cc) |
  | `NSDEnableBreakPoints(bool)` | `EnableBreakPoints()` (Interpreter.cc) |
  | `NSDMakeNSDebugAPI()` + `NSDSelfFuncs` methods `AccurateStack, NumStackFrames, Function, ProgramCounter, SetProgramCounter, Receiver, Implementor, GetVar, SetVar, FindVar, SetFindVar, NumTemps, TempValue, SetTempValue` | `CNSDebugAPI` methods (Frames/DebugAPI.cc) |
  | `NSDFindSlotName`, `NSDRefToHexString` | `FindSlotName` exists; hex string is trivial |

  NSDT saves the ROM `BreakLoop` as `NSDOriginalBreakLoop` and installs its
  own NewtonScript `BreakLoop`, which prints the location and calls the
  optional hooks `NSDBreakLoopEntry`/`NSDBreakLoopExit`.
- **Apple already shipped shortcuts:** `NSDShortCuts/myFunctions` (plain
  NewtonScript source next to the pkg) defines `s` (Step, one bytecode
  instruction and steps over calls), `si` (StepIn), `so` (StepOut), `r`/`cont`/`e`
  (continue), `w` (Where), `qs`/`st` (stack traces), `stop`/`stopat`/
  `clearbp`/`listbps`, `gl`/`sl` (get/set local), `dis`/`dishere`, and more.
  Apple's users type them as calls, e.g. `s()`.
- **First attempt (branch `try_to_debug`, notes in that branch's
  `Matt/CLAUDE.md`, "Runtime debugging" onward).** Worth keeping:
  - Checking breakpoints in `run1()` behind `gFramesBreakPointsEnabled`.
    After a hit, re-derive `instrBase`/`instrPtr`/`literalSlot`/`localSlot`,
    because NewtonScript run inside the break loop can compact the heap.
  - Compiler line numbers must be captured at parser *shift* time
    (`shiftLineNumber`). Reading `lineNo()` in a reduce action is off by one
    statement (LALR lookahead). Codegen runs after the full parse, so
    `emit()` can't read the lexer position.
  - How a C++ function becomes NS-callable: a
    `{class: kPlainCFunctionClass, function:, numargs:}` frame placed in
    `gFunctionFrame` (see `newtc.cc` `init()`). `Frames/Funcs.cc` is dead code.
  - A native function's own `VMState` is pushed before it runs, so the
    caller is `vm - 1`.

  Deliberately **not** carried over: the custom `checkStep()` stepping engine
  and the `gPaused*` snapshot globals. This time we follow Apple's design
  (temporary breakpoints plus `FindBreakLoop`) as closely as possible.

## Plan

Rules: one small step at a time. Every step ends with something Matt can
run and see, and an automated test that keeps it working. Decompiler
regression checks (MATT.md) still apply when shared code is touched.

### Phase 0: Groundwork
- [x] 0.1 Debugger test harness: `Test/dbg/cases/<name>.ns` + `<name>.in`
      (stdin commands for the break loop) + `<name>.expected`. Run
      `Test/dbg/run_dbg_tests.py` (all), `... <name>` (filter), `-v` (show
      output), `--update <name>` (accept new output). Runs `newtc -script`,
      10 s timeout, normalizes heap refs to `#<ref>` and shows a lone `\r` as
      `<CR>`. Every later step adds a case.
      Found while building it:
      - `-run <file>` crashed after every script (it ran the result a second
        time). Now `-run` takes no argument and will run (open) whatever
        `-pkg`, `-nsof`, or `-script` loaded, e.g. `newtc -nsof app.nsof -run`
        (VSNewt already calls it that way). Not implemented yet. `-r` was
        removed; `-s` does the same. Compiling NewtonScript always runs it,
        so `-script`/`-s` compile *and* run.
      - At end of stdin the break loop spins forever (step 0.3).
      - The REPL printed each result as `#<ref>  <value>\r`, so in a terminal
        the next line overwrote it. Fixed (see Conventions).
- [x] 0.2 `StackTrace()` works in a break loop. Three porting bugs, each
      checked against the ROM:
      - `REPStackTrace` (DebugAPI.cc) called `FindSlotName(impl, func)` even
        when `impl` is nil (any global function). The iterator threw for every
        frame → "Skipping bad stack frame". ROM skips it for nil.
      - `PrintWellKnownObject` (ObjectPrinter.cc) had the `IsAggregate` test
        inverted, so functions were never looked up by name. The fallback
        format is `(#%lX)` like ROM (was `%p`).
      - `SearchForObjectName` missed ROM's third search, `builtinFunctions`
        (the port keeps built-ins apart from the RAM `functions` frame), so
        `BreakLoop`/`StackTrace` had no names.
      The last two are also fixed in `ROMData/32bitObjectPrinter.cc` (MessagePad
      target; syntax-checked only). Tests: `stacktrace`, `stacktrace_method`.
      About the "Inaccurate stack trace" warning: ROM
      `TInterpreter::SetFastLoopFlag()` uses the fast loop only when
      `gAccurateStackTrace` (`SetDebugMode(true)`), tracing,
      `gFramesBreakPointsEnabled`, and profiling are all off; otherwise it uses
      `SlowRun`. In our port nothing reads `gAccurateStackTrace`. Our single loop
      saves the caller's `vm->pc` on every call/send, so every frame below the
      top is exact. Only the innermost interpreted frame is stale when an
      exception is thrown inside an instruction (e.g. `breakOnThrows`).
      See 1.3.
- [x] 0.3 End of input inside a break loop quits newtc (message on stderr,
      exit code 1) instead of spinning. New virtual `PInTranslator::
      inputEnded()` (default `false`, so the Hammer/Null translators are unchanged);
      `PStdioInTranslator` returns true at `feof(stdin)`; `BreakLoop()`
      (REP.cc) checks it. Same as gdb/pdb at end of input, and right for
      DAP later (EOF = client gone). Tests: `breakloop_eof`, `breakloop_eof_nested`.

### Phase 1: Reactivate ROM breakpoints (C++ only)
- [x] 1.1 Breakpoints are checked in the interpreter loop, the ROM way.
      ROM: `TInterpreter::AlternatingLoops()` switches between `FastRun()` and
      `SlowRun()` depending on the `isFast` flag (offset x60), which
      `SetFastLoopFlag()` sets when `gAccurateStackTrace`, tracing,
      `gFramesBreakPointsEnabled`, and profiling are all off. `EnableBreakPoints()` calls
      it. `SlowRun` calls `HandleBreakPoints()` before every instruction, with
      `instructionOffset` pointing at that instruction, and re-reads the
      instruction pointer each time. Both loops check `isFast` only after a
      call or return and return false to switch loops, true when done.
      The port's `handleBreakPoints()` matches ROM `HandleBreakPoints`.
      Ours (Interpreter.cc): `run1<kSlow>()` is a template, so both loops come
      from one source; `alternatingLoops()` and `setFastLoopFlag()` as in ROM
      (only breakpoints decide for now); `isFast` restored in Interpreter.h;
      `EnableBreakPoints()` updates every interpreter. The slow loop re-derives
      `instrBase`/`instrPtr`/`literalSlot`/`localSlot` after
      `handleBreakPoints()` (GC, changed PC). A PC changed in the break loop
      comes back via `call()` → `vm->pc` → `return` → `instructionOffset`.
      Cost, Release build, 10M-iteration loop + Fib(29), breakpoints off:
      baseline 0.80 s; one loop with a per-instruction flag test 0.90 s (+11%);
      alternating loops 0.81 s (noise). With the slow loop forced on
      (temporarily), all tests and the benchmark gave identical results.
      Not testable from NewtonScript until 1.2.
- [x] 1.2 `NSDInstallBreakPoints(bps)` and `NSDEnableBreakPoints(flag)` are
      NewtonScript functions (`FNSD...` in DebugAPI.cc, registered in newtc.cc
      `init()` via the new `defGlobalCFunction()` helper). Disassembling the
      package's ARM code (part "NSDCPatch1", offsets 0x9C and 0xFC) showed thin
      wrappers: they call (confirmed through the jump table, see below) ROM
      `TInterpreter::SetBreakPoints` (returns the
      previous frame) and `TInterpreter::EnableBreakPoints(flag <> nil)`
      (returns the previous setting as true/nil). Tests: `breakpoint_temporary`
      (pc 0, fires once, removes itself and the empty list),
      `breakpoint_persistent` (every call; `disabled`; switched off),
      `breakpoint_gc` (GC in the break loop while stopped mid-statement; a
      temporary print confirmed the bytecode really moved, and the result is
      still right). `Disasm` is not registered: NSDT brings its own (Phase 3).
      To find PCs for tests: end the script with `functions.Foo;` and run
      `newtc -script x.ns -debug bc -decompile`.
- [x] 1.3 PCs are exact while paused, also when an instruction throws.
      ROM: `SlowRun` keeps `instructionOffset` pointing at the next
      instruction (`+= (opcode & 7) == 7 ? 3 : 1`) before executing it. On an
      exception, `HandleException` enters the break loop via `DoBlock` →
      `call()`, which stores `instructionOffset` as the frame's `vm->pc`.
      Ours: the slow loop does the same; `setFastLoopFlag()` also looks at
      `gAccurateStackTrace`; `SetDebugMode()` updates the flag at once (not in
      ROM: there it only takes effect the next time `SetFastLoopFlag()` runs).
      The "Inaccurate stack trace" warning stays ROM-like: it only looks at
      SetDebugMode, even when breakpoints make the PCs exact.
      Test: `pc_exception` (fast loop: stale PC 9; debug mode or breakpoints:
      PC 25, right after the failing get-path).
      Fixed on the way: `ForgetDeveloperNotified()` (called by `ExitHandler`
      after a handled exception) left `gDeveloperNotified` pointing at freed
      memory when it removed the first item → the next exception with
      `breakOnThrows` read freed memory (crash). Now walks the links like ROM.
      Found with AddressSanitizer, which is now on in all Debug builds (see
      Conventions).

### Phase 2: The NS Debug Tools native layer
- [x] 2.1 `NSDMakeNSDebugAPI` + `NSDSelfFuncs` backed by `CNSDebugAPI`, one
      method group at a time. Test each from a break loop.
      How the ARM code (NSDCPatch2, `Ref_334`) works: 33 ROM call stubs at
      the start (all resolved by name with rom_jumptable.py: `TNSDebugAPI`
      methods, `AllocateCObjectBinary`, `GetFrameSlotRef`, `BinaryData`, ...).
      `NSDMakeNSDebugAPI()` = `AllocateCObjectBinary(NewNSDebugAPI(
      GetGInterpreter()), <b DeleteNSDebugAPI>, 0, 0)`. Every method does
      `api = BinaryData(GetFrameSlotRef(self, 'nsDebugAPI))` and calls the
      `TNSDebugAPI` method, converting with RINT/MAKEINT and nil/true.
      NS Debug Tools builds `{_proto: NSDSelfFuncs, nsDebugAPI: ...}`
      (`CurrentStack()`). Frame index 0 is the oldest frame.
  - [x] stack/function/PC: `NSDMakeNSDebugAPI`, `AccurateStack`,
        `NumStackFrames`, `Function(i)`, `ProgramCounter(i)` (-1 for native
        frames), `SetProgramCounter(i, pc)` (returns nil). Port's
        `CNSDebugAPI` checked against ROM `TNSDebugAPI` (StackFrameAt throws for
        a bad index, PC, SetPC, Function). Registered in newtc.cc
        (`installNSDebugToolsNatives()`; `NSDSelfFuncs` is a global var).
        Not in ROM: the methods check that `nsDebugAPI` is a CObject binary.
        Test: `debugapi_stack` (finds Inner's frame, changes its PC to skip a
        statement, deletes the object with GC()).
        Fixed on the way (found by ASan): the REPL's printer hex-dumped
        `Length()` bytes of a CObject binary's data, reading past the C object;
        now prints `<CObject, length 32>` like ROM's `<%s, length %d>`.
  - [x] receiver/implementor: `Receiver(i)`, `Implementor(i)` return the
        frame's `rcvr`/`impl` (ROM offsets 0x10/0x0C of the stack frame; port
        matches). Test: `debugapi_receiver` (method inherited through
        `_proto`: receiver is the object, implementor the proto; a slot
        changed through `Receiver()` is seen by the running method).
  - [x] vars: `GetVar(i, n)`/`SetVar(i, n, v)` by index (args first, then
        locals; NOS 2 on the stack, NOS 1 in the argFrame, natives their args)
        and `FindVar(i, sym)`/`SetFindVar(i, sym, v)` by name (lexical lookup
        in the frame's argFrame, so only NOS 1 code and closures have names).
        Port checked against ROM `TNSDebugAPI`; fixed: `setFindVar` ignored a
        failed `SetVariableOrGlobal()`, ROM throws "Undefined variable".
        Tests: `debugapi_vars` (NOS 2), `debugapi_vars_nos1`.
        NewtonScript gotcha: `[api:GetVar(i, 0), ...]` is an array of class
        `'api` holding a call to a *global* `GetVar`; write
        `[(api:GetVar(i, 0)), ...]`.
  - [x] temps: `NumTemps(i)`, `TempValue(i, n)`, `SetTempValue(i, n, v)`: the
        values a frame pushed beyond its args and locals (half-evaluated
        expressions; NSDT's `Step` reads the top one to predict branches).
        Temp 0 is the deepest. Port checked against ROM (`StackStart`,
        `NumTemps`, `TempValue`, `FunctionStackSize`).
        Deliberate difference from ROM: `stackStart()` adds 3 only for
        NewtonScript frames. Frame layout: a NOS 2 function's `stackFrame` is 3
        below its first argument; a native function's is its first argument.
        The ROM adds 3 for every frame, so when the next frame is native the
        caller got 3 phantom temps above the stack top. NSDT never hit it (its
        own BreakLoop is NewtonScript); we do while BreakLoop is native, and
        whenever a native calls back into NewtonScript. (NOS 1 CodeBlock frames
        keep ROM's +3; their stackFrame is the stack top at call time.)
        Tests: `debugapi_temps` (stopped mid-expression by a breakpoint; next
        frame native), `debugapi_temps_call` (next frame a NewtonScript
        function).
- [x] 2.2 `NSDFindSlotName(context, obj)`: tag of the first *own* slot whose
      value EQs obj, else nil (the ARM code uses `NewIterator()`, i.e. no
      `_proto` chain, unlike the ROM's `FindSlotName()` that StackTrace uses).
      `NSDRefToHexString(obj)`: `sprintf("#%lX", ref)` as a string. Test:
      `natives_misc`. Part 0 installs `MakeDisassembler`, `DisasmRange`,
      `Disasm`: all NewtonScript (`Ref_27`), no natives.
      **All natives of NS Debug Tools.pkg now exist in newtc**
      (`installNSDebugToolsNatives()` in newtc.cc).

### Phase 3: NS Debug Tools NewtonScript on top
- [ ] 3.1 Clean up the decompiled NSDT source into `Matt/Debugger/NSDebugTools.ns`
      (plus Apple's `myFunctions`) and embed it into newtc at build time.
      Package layout (decompiled): part 0 = disassembler (`MakeDisassembler`,
      `Disasm`, `DisasmRange`; `Ref_41` is the disassembler proto); parts 1/2 =
      ARM natives (done in Phase 2); part 3 = library frame `Ref_427` whose
      "makers" (`GetTempVar: func() func(level, n) ...`) return the 27 global
      functions as closures with `self` = the library frame (NTK ran them at
      build time: `Ref_410[i].argFrame._parent` is the library), plus the
      install script (27 functions from `Ref_381`/`Ref_410`; `StackTraceOld` :=
      `StackTrace`; `StackTrace` := `QuickStackTrace`; `NSDOriginalBreakLoop`
      := `BreakLoop`; a NewtonScript `BreakLoop` replacement); part 4 = the
      about-box app (skip). `Ref_261` in part 0 is NewtonScript compiled to ARM
      by NTK's native compiler (literals `'DebuggerInfo, 'vars,
      'becausePeterSaidSo`): `Disasm` refuses functions without debug info.
      The decompiled `DisasmRange` never disassembles: check its bytecode
      (decompiler bug?).
  - [x] 3.1a The embedding mechanism, `-dbg`, and a first slice.
        `cmake/EmbedNewtonScript.cmake` + `embed_newtonscript()` in
        CMakeLists.txt (see "Embedding a .ns file" below); `Matt/EmbeddedScript.{h,cc}`
        (`RunEmbeddedScript()`); `newtc -dbg` runs `gNSDebugToolsScript`.
        NSDebugTools.ns so far: `kNSDSetTrace` (Ref_414), `kNSDParam`
        (Ref_573), `kNSDStackProto` (Ref_531), `kNSDTools` with
        `CurrentStack`, `MakeNSDebugAPI`, `EnableBreakPoints`,
        `StackIsAccurate`, and the globals `GetCurrentFunction(level)`,
        `GetCurrentPC(level)`. Test harness: optional `<name>.args` (extra newtc
        arguments). Test: `nsdt_current`. -48800 is "Not in a break loop".
  - [x] 3.1b Stack and variable functions: `GetCurrentReceiver`,
        `GetCurrentImplementor`, `GetTempVar`, `SetTempVar`, `GetAllTempVars`
        (top first, ends with `'bottom`), `GetNamedVar`, `SetNamedVar`; helper
        `kNSDHasDebugInfo` (Ref_462). Named variables use NTK's `DebuggerInfo`
        (class `'dbg1`: `info[0]` = entries to skip, then the variable names in
        index order) and fall back to `FindVar` (lexical, NOS 1 code and
        closures only). newtc writes no DebuggerInfo, so for NOS 2 code
        `GetNamedVar` fails like on a Newton with code not compiled for
        debugging. Idea for Phases 7/8: have the compiler write DebuggerInfo.
        Tests: `nsdt_self`, `nsdt_temps`, `nsdt_named`.
  - [x] 3.1c `GetPathToSlot(frame, slot)` (path through `_proto`, then
        `_parent`, e.g. `_parent._proto._proto.count`; helpers `ProtoSlotPath`,
        `ParentSlotPath`), `GetPathWhereSet(frame, slot)` (where `slot :=`
        stores: cut after the last `_parent`, else the frame itself),
        `GetAllNamedVars(level)` (named vars as a frame; then probes literal
        symbols with FindVar and warns about undeclared ones; its DebuggerInfo
        branch needs `MakeDisassembler`, coming with the disassembler). FindVar
        only looks lexically (ROM passes lookup flag 1), so globals are never
        reported as undeclared, on a Newton either.
        Fixed on the way: `GetGlobals()` was a stub returning nil (ROM:
        `gVarFrame`); `LSearch` divided a `Ref*` difference by `sizeof(Ref)`,
        so indexes below 8 came out as 0.
        Tests: `nsdt_paths`, `nsdt_allvars`.
  - [x] 3.1d Breakpoint functions: `InstallBreakPoint(fn, pc)` (returns the
        breakpoint frame; a duplicate is added with a warning),
        `RemoveBreakPoint(bp)`, `RemoveAllBreakPoints()`, `GetAllBreakPoints()`,
        `EnableBreakPoint(bp, flag)`, `SetBreakPointLabel(bp, label)`,
        `GetBreakPointLabel(bp)`, `GloballyEnableBreakPoints(flag)`; library
        helpers `InstallBreakPoints`, `InstallTempBreakPoint` (for stepping);
        constants `kNSDIsInterpreted` (Ref_581), `kNSDIsOpen` (Ref_664).
        newtc: `EnableBreakPoint(bp, enableMode)` follows the NTK documentation
        (non-nil enables, nil disables; returns true if it was enabled). The
        package had it backwards (`disabled := enableMode <> nil`, returning the
        previous `disabled`).
        newtc: `GloballyEnableBreakPoints` skips updating the NS Debug Tools app
        (`GetRoot()` is nil without a GUI).
        Fixed on the way: `ArrayPos` was a stub returning nil (now in Arrays.cc,
        as in ROM: nil start = 0); `ArrayPosition` treated every test-function
        result as true (`if (DoBlock(...))`, but nil is not 0); `SetRemove` always
        returned the array (`ISNIL()` on a C++ bool) instead of nil when nothing
        was removed. Test: `nsdt_breakpoints`.
  - [x] 3.1e The disassembler (package part 0): `MakeDisassembler(fn)`,
        `Disasm(fn)`, `DisasmRange(fn, start, stop)`; `kNSDDisassemblerProto`
        (Ref_41: `InstrOpcode`/`InstrParameter`/`InstrLength` for Step,
        `PrintInstruction`, `GetArgName`, ...); `kNSDCanDisassemble` (Ref_261,
        NewtonScript compiled to ARM by NTK: `HasPath(fn, 'DebuggerInfo) or
        vars.becausePeterSaidSo`, decoded from its code via the jump table:
        FrameHasPath, GetVariable, GetFramePath). newtc: `-dbg` sets
        `becausePeterSaidSo` (our code has no DebuggerInfo); `Disassemble`
        catches `|evt.ex|` (the package had `|ex.evt|`, which never matches).
        `DisasmRange` written from its bytecode (decompiler bug, see below).
        NOS 2 variables show as `[ n ]` (no DebuggerInfo).
        Fixed on the way: `ExtractByte` was signed (ROM: unsigned, so opcodes
        >= 0x80 decoded negative); `ExtractWord`, `ExtractLong`, `ExtractXLong`,
        `ExtractUniChar`, `StuffWord`, `StuffLong`, `StuffUniChar` accessed the
        data natively (little-endian, unaligned); Newton data is big-endian,
        now byte by byte as in ROM (Utilities/DataStuffing.cc); `Display` was a
        stub (ROM: `PrintObject` without newline).
        Test: `nsdt_disasm`.
  - [ ] 3.1f ... the remaining 7: `Where`, `QuickStackTrace`, `Step`, `StepIn`,
        `StepOut`, `RunUntil`, `SetCurrentPC`; the replacement BreakLoop;
        StackTraceOld/StackTrace.
- [ ] 3.2 `-dbg` flag (exists since 3.1a, loads NSDebugTools.ns): also
      enable breakpoints, set `breakOnThrows`, install Apple's `myFunctions`
      shortcuts.
- [ ] 3.3 Verify the Apple API one group per step: `Where`/`QuickStackTrace`;
      `GetCurrentFunction`/`GetCurrentPC`; `InstallBreakPoint`/
      `RemoveBreakPoint`/`GetAllBreakPoints`; `Step`; `StepIn`; `StepOut`;
      `RunUntil`; `Get/SetNamedVar`, `Get/SetTempVar`; `Disasm`.

### Phase 4: Comfortable command-line REPL
- [ ] 4.1 Break-loop input filter: a line that is a bare command word
      (`c`, `n`, `s`, `finish`, `bt`, `b ...`, `info b`, ...) becomes a call;
      anything else is evaluated as NewtonScript as before.
- [ ] 4.2 On stop, show function, PC, and the current instruction (`dishere` style).
- [ ] 4.3 (optional) Line editing/history.

### Phase 5: Debugger engine interface (C++)
- [ ] 5.1 Put a C++ interface around the REPL-level operations: commands
      (continue, step kinds, set/clear breakpoints, stack, scopes/variables,
      evaluate) and events (stopped + reason, output, exited). The REPL of
      Phase 4 uses it too, so the Phase 0 tests cover it.
- [ ] 5.2 The break loop waits on a command source that can be stdin *or* a
      queue fed from another thread (needed because cppdap is threaded while
      the interpreter must stay on one thread).

### Phase 6: DAP, bytecode level
- [ ] 6.1 Add cppdap (CMake), `-dap` flag, stdio transport; initialize/
      launch/configurationDone/disconnect; output events; run to completion.
- [ ] 6.2 Stop on `BreakLoop()`/breakpoint/exception → `stopped`,
      `threads`, `stackTrace`.
- [ ] 6.3 `scopes`/`variables` (args, locals, self, value-stack temps).
- [ ] 6.4 `continue`/`next`/`stepIn`/`stepOut`.
- [ ] 6.5 Showing bytecode. Proposal: each unmapped function gets a
      *virtual source* (DAP `sourceReference`) holding its disassembly, one
      instruction per line. Line breakpoints and stepping then map 1:1 to
      PCs and reuse the source-level machinery. Optional extra: DAP
      `disassemble` + instruction breakpoints for VS Code's Disassembly view.
- [ ] 6.6 `evaluate` (VS Code debug console) → same evaluation as the break loop.

### Phase 7: Source level, internal map
- [ ] 7.1 In-memory representation: per chunk {files[], functions[{key,
      file, pc→line table}]}; runtime index `instructions`-Ref → entry, and
      (file, line) → [(function, pc)]. Both lookup functions, tested with
      hand-built maps.
- [ ] 7.2 Function keys per chunk kind: live compiled Ref; compiler-
      assigned ID (NSOF/pkg); walk-order index (decompiled pkg); blob offset
      (ROM).
- [ ] 7.3 Line-level stepping (`n`/`s` repeat bytecode steps until the
      line changes, depth-aware so recursion doesn't stop early).

### Phase 8: Generating and storing maps
- [ ] 8.1 Compiler generates the map in memory (compile-and-debug). Test:
      breakpoint by `file:line`, then `n`/`s`.
- [ ] 8.2 `.nsdbg` side-car format (delta-encoded, PC-sorted, file table,
      checksum; see MATT.md) with a round-trip test.
- [ ] 8.3 Compiler writes `.nsdbg` next to `-opkg`/`-onsof`; the loader
      restores the mapping.
- [ ] 8.4 Decompiler writes source + `.nsdbg` for packages.
- [ ] 8.5 ROM: decompiled source + offset-keyed map.

### Phase 9: DAP, source level
- [ ] 9.1 `setBreakpoints` by file/line, source-mapped stack frames,
      fall back to Phase 6.5 virtual sources for unmapped functions.

### Later: the rest of the VS Code extension
- `-lsp` mode in newtc (diagnostics, completion, ...), TextMate grammar.

## Bugs found on the way (not fixed yet)

- **Recursion is broken in NOS 1 code** (`-nos1`, or a `//! -nos1` script;
  no longer the default). A recursive `Fib(n)` returns `n-1`; even
  `if n < 2 then return n` gives wrong values. Looks like the NOS 1 argFrame
  (locals) is shared instead of copied per call. NOS 2 code is correct. It
  still matters: a 2.x ROM also runs NOS 1 packages.
- **Decompiler output depends on memory layout.** With AddressSanitizer on (Debug
  builds since 2026-09-25) the corpus sweep has 13 packages that decompile fine
  without ASan (Debug or Release) but fail with it: 11 recurse without end (stack
  overflow; with a bigger stack they run out of NewtonScript memory instead),
  `Tymnet-MCI_1.1.pkg` hits `assert(IsSymbol(ref))` in `PrintTag`
  (Matt/ObjectPrinter.cc:125), and `mobilem1.pkg` throws
  `evt.ex.fr.type;type.ref.frame`. ASan reports no memory error. Ruled out:
  ASan's malloc fill, its fake stack, uninitialized locals
  (`-ftrivial-auto-var-init=zero` changes nothing). Also broken at commit
  b003eaf, so not caused by the debugger work. Lead:
  `std::map<Ref, Node> map` in Matt/ObjectPrinter.h:69 is ordered by object
  address, so the printer (and its cycle handling, "Fix 6") visits objects in
  a different order when the allocator changes. Other packages: 1909 CLEAN,
  371 UNRESOLVED, 69 CRASHED with ASan vs. the last manifest's 1906/382/61
  (that manifest is older; 13 packages got better since).
- **Decompiler drops statements** (found in NS Debug Tools.pkg, `Ref_270` =
  `DisasmRange`): in `if A then X else if B then Y else begin if C then Z;
  <more statements> end`, the decompiled source ends after `if C then Z`;
  the bytecode (pc 52-96: a second `if` and the call to `Disassemble`) is
  missing. See `newtc -pkg ... -debug bc -decompile`, search for "DisasmRange".
- **Sorted array set operations**: `GenOrderedSetOp` (Frames/SortedArrays.cc,
  behind `BDifference`, `BIntersect`, `BMerge`) has the same `Ref*` difference
  divided by `sizeof(Ref)` as `LSearch` had (lines ~956-968: copies too few
  elements, truncates the result). `BMerge([...], [...], '|<|, nil, nil)` hangs.
  Also `LSearch(["x","y"], "y", 0, '|str=|, nil)` returns nil (general test
  path). Not fixed yet; nothing in the debugger uses them.
- **Round trip**: `Test/round_trip.py` reports `GEN2_FAILED` for 29 of the first
  30 manifest packages, with the binary from before 1.1 too (pre-existing).

## Embedding a .ns file in newtc

To build a NewtonScript file into newtc (no runtime dependency, and the .ns
file stays the one to edit):
1. In CMakeLists.txt: `embed_newtonscript(newtc <path/file.ns> <cName>)`.
   At build time `cmake/EmbedNewtonScript.cmake` writes
   `<build>/embedded/<cName>.cc` defining `const EmbeddedScript <cName>`
   (file name + text as a byte array); it is regenerated when the .ns changes.
2. In C++: `extern const EmbeddedScript <cName>;` and
   `RunEmbeddedScript(<cName>)` (Matt/EmbeddedScript.h). It compiles and runs
   the top-level statements one by one like `-script`; an exception stops it
   and is reported as `File "file.ns"; Line n` (n may be a line or two after
   the actual error, as for -script).
Top-level statements are compiled separately: use global constants/vars (e.g.
`DefineGlobalConstant`) to share things between them, not locals.

## How to read the ARM code in NS Debug Tools.pkg

The natives are `BinCFunction`s: `{class: 'BinCFunction, code: <binary>,
numargs:, offset:}`. The code binary is in the decompiled package
(`newtc -pkg ".../NS Debug Tools.pkg" -decompile`, e.g. `Ref_299` for part
NSDCPatch1, `Ref_334` for NSDCPatch2) as `MakeBinaryFromHex("...")`. The
words are big-endian. To disassemble: extract the hex into a file, swap each
4-byte word to little-endian, wrap it in `p.s` as
`.text / .arm / _start: / .incbin "p.bin"`, then run
`xcrun clang -target armv4t-none-eabi -c p.s -o p.o` and
`xcrun llvm-objdump -d --triple=armv4t-none-eabi p.o`.

**ROM calls go through the public jump table** (the MMU maps it, so ROM bugs
can be patched later and packages have fixed entry points across ROM
versions). `ldr pc, [pc, #-4]` followed by `0x018xxxxx` is such a call.
Verified chain:
1. entry i is at virtual `0x01800000 + 4*i`; the ROM stores it at
   `gROMPublicJumpTable` (0x13000..0x15E0C) + 4*i. It is a `b` to a
   `VEC_<name>` address (`.equ VEC_...` in newtonos.s).
2. the MMU maps that VEC_ address to ROM
   `(((a>>5) & 0xffffff80) | (a & 0x7f)) - 0xCE000` (Matt's formula,
   verified): a patch table entry, a `b` to the real function.
`Matt/tools/rom_jumptable.py <index or 0x018xxxxx address> ...` follows the
chain and prints the names (reads newtonos.s, under a second). The original
ROM calls through these tables almost everywhere; newtonos.s shows those
calls already resolved by name (`bl VEC_Name`).
Symbol lists (git-ignored, repo root): `symbols.txt` (address, name) and
`Symbols_demangled_by_name.txt`.
NSDCPatch1 uses entries 1978 `GetGInterpreter()`, 2045
`TInterpreter::SetBreakPoints`, 2096 `TInterpreter::EnableBreakPoints`, and
2339 `PublicFiller_1` (an unused slot: the code throws if a function's entry
is the same as that filler entry, i.e. the ROM is too old).

## Conventions

- **Line endings**: CR (`\r`) is a leftover from classic Mac OS. Input must
  treat CR, LF, and CRLF the same wherever it shows up, because existing
  packages and sources still contain CR. Everything newtc *writes* for general
  use should use LF (Unix/current macOS).
  Done for the REPL (REP.cc): `PStdioOutTranslator::write()` turns CR and
  CRLF into LF for everything written to stdout (`Write`, `Print`, results,
  stack traces); `PStdioInTranslator::produceFrame()` ends a break loop line
  at LF, CR, or CRLF. Test: `line_endings`. `ObjectPrinter` printing char
  0x0D as `$\n` is correct NewtonScript and stays.

- **NewtonOS 2.x is the default target.** newtc compiles for NOS 2
  (`compilerCompatibility` 1; 2.1 uses the same code format). NOS 1 code is
  generated only on request (`-nos1`, or `//! -nos1` as the decompiler writes it
  for NOS 1 packages, so round trips still work).
- **Why the original code looks the way it does**: NewtonOS was built for
  low memory (`_proto` inheritance) and low battery use (deep sleep whenever
  nothing happens); interpreter calls are short GUI-style callbacks to user
  actions (plus timers for games). This explains many implementation choices.
  It is *not* a goal for us: memory and battery hardly matter today, so don't
  over-optimize; prefer clarity.

- **Debug builds use AddressSanitizer** (CMakeLists.txt, all targets, via
  `CMAKE_<LANG>_FLAGS_DEBUG`; UBSan was already on for newtc). A memory bug
  aborts with a report showing where the memory was allocated, freed, and
  misused. Release builds have neither.

## Decisions (2026-09-25)

1. **ROM source** is in `./newtonos.s` (132 MB, git-ignored, ARM
   disassembly with labels). Apple's class names are `Txxx`; this port renamed
   them to `Cxxx` (`TInterpreter` → `CInterpreter`, `TNSDebugAPI` →
   `CNSDebugAPI`, `TDictionary` → `CDictionary`). Useful labels:
   `SlowRun__12TInterpreterFl` (line ~941591),
   `HandleBreakPoints__12TInterpreterFv` (~903964),
   `SetBreakPoints__12TInterpreterFRC6RefVar`, `EnableBreakPoints__12TInterpreterFUc`,
   `TNSDebugAPI::*` (~901908), `FBreakLoop` (~871677). Search with `grep -n`;
   never read the whole file.
2. **NS Debug Tools as a `.ns` file**: check in the decompiled NewtonScript
   as a readable source file (real names and comments). It gets **embedded
   into the newtc binary at build time** (CMake generates a C++ string from
   it), so newtc has no runtime dependency on external files. The same applies to
   Apple's `myFunctions` shortcuts.
3. **Shortcuts**: bare-word commands in the break loop follow gdb (`c`, `n`,
   `s`, `finish`, `bt`, `b`, `info b`, ...). Apple's functions stay callable
   as `s()`, `si()`, and so on. `bt` depends on the `StackTrace()` fix (0.2).
4. **VS Code**: one extension, `/Users/matt/dev/VSNewt.git/vsnewt`
   (TypeScript; it already runs `newtc` for compile/run commands, with binaries
   in `bin/<platform>/newtc`). The long-term goal is one extension with the best
   NewtonScript support we can build: compiler, debugger, syntax highlighting,
   completion. Proposal: the extension stays thin and `newtc` does the work, in
   two modes: `-dap` (Debug Adapter Protocol, via
   `contributes.debuggers` + a `DebugAdapterExecutable`) and later `-lsp`
   (Language Server Protocol: diagnostics, completion, go-to-definition,
   all reusing the compiler). A TextMate grammar handles highlighting.
