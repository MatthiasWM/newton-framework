

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
- [x] 3.1 Clean up the decompiled NSDT source into `Matt/Debugger/NSDebugTools.ns`
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
        `DisasmRange` written from its bytecode (decompiler bug B8).
        NOS 2 variables show as `[ n ]` (no DebuggerInfo).
        Fixed on the way: `ExtractByte` was signed (ROM: unsigned, so opcodes
        >= 0x80 decoded negative); `ExtractWord`, `ExtractLong`, `ExtractXLong`,
        `ExtractUniChar`, `StuffWord`, `StuffLong`, `StuffUniChar` accessed all
        data natively (little-endian, unaligned). Byte order depends on the
        binary: the package/NSOF readers convert strings and reals to host
        order (hasByteSwapping), everything else (instructions, bitmaps,
        sounds, ...) stays big-endian. Now: host order for strings and reals,
        big-endian (byte by byte) for the rest (Utilities/DataStuffing.cc;
        test `datastuffing`). Matt: these functions are a bad idea in the
        port in general (swapped vs. unswapped data, 4-byte Refs become 8-byte
        Refs); use with care. `Display` was a stub (ROM: `PrintObject` without
        newline).
        Test: `nsdt_disasm`.
  - [x] 3.1f Stepping: `Step()` (decodes the instruction at the PC and sets
        a temporary breakpoint where execution goes next: after it, at the
        branch target (decided from the stack for branch-if-true/false and
        branch-if-loop-not-done), or in the caller on return; then
        `ExitBreakLoop()`), `StepIn()` (callee from call/invoke/send/resend and
        the stack; breakpoint at its pc 0; falls back to `Step()`; refuses
        natives and BreakLoop), `StepOut()`, `RunUntil(fn, pc)`,
        `SetCurrentPC(pc)`; helper `FindInterpretedFunctionBelow`. Stepping
        needs breakpoints enabled. newtc: `Step`'s `stopPoint` is a local (the
        package assigned a global). Known limitation (Apple's design): a
        temporary breakpoint fires in any activation of the function, so
        stepping over a recursive call stops in the inner call.
        Tests: `nsdt_step`, `nsdt_stepin`.
  - [x] 3.1g `Where()`, `QuickStackTrace()`, and the tools' `BreakLoop`: on
        every stop it prints `function(arguments), pc: instruction [what it
        is about to do]` (`SimpleDecompile` shows the call/send/operator with
        the actual values from the stack), and calls the optional hooks
        `NSDBreakLoopEntry(fn, pc, params)` (nil = don't stop: conditional
        breakpoints) and `NSDBreakLoopExit(didBreakLoopHappen)`. Installed
        once: `StackTraceOld` := ROM `StackTrace`, `StackTrace` :=
        `QuickStackTrace`, `NSDOriginalBreakLoop` := ROM `BreakLoop`,
        `BreakLoop` := the tools' (a maker in kNSDTools; in the package it was
        defined in the install script). All 27 functions of the package are in.
        newtc changes: `Where` sets the stack before checking accuracy (the
        package used it unset); `NSDBreakLoopExit` gets whether the break loop
        ran (the package always passed true; the bytecode confirms it);
        `breakOnThrows` is defined if missing.
        Fixed on the way: `@4098` (magic pointer table 1, entry 2) resolved to
        the RAM `functions` frame; the ROM resolves it to the built-in
        functions (`RSbuiltinfunctions`), which `SearchForObjectName` needs
        (Frames/RefMemory.cc). The .ns embedding now uses `unsigned char`
        (bytes >= 0x80 didn't compile).
        NewtonScript gotcha: `to` is reserved (`for ... to`), not a parameter name.
        Tests: `nsdt_breakloop`; all other `-dbg` tests now also show the
        location line at each stop.
- [x] 3.2 **Milestone: `newtc -dbg` is a Newton with Apple's debug tools.**
      It runs `NSDebugTools.ns`, then `Matt/Debugger/NSDShortCuts.ns` (Apple's
      `NSDShortCuts/myFunctions` verbatim, LF line endings, plus the install
      loop its InstallScripts did: every slot of `kFunctionsToInstall` becomes a
      global function), then what the "Enable breakpoints" checkbox of the NS
      Debug Tools about box did: `NSDEnableBreakPoints(true)` and
      `SetupMyDebug(true)` (prints its settings; breakOnThrows on, printDepth 3,
      printLength 50, stackTracePrintDepth -1). Shortcuts: `s()` Step, `si()`,
      `so()`, `r()`/`cont()`/`e()` continue, `w()` Where, `qs()`/`st()`,
      `stop(sym|fn, pc)`, `stopat(pc)`, `listbps()`, `clearbp(i)`,
      `clearallbps()`, `contto(pc)`, `dis(fn, from, to)`, `dishere()`,
      `args()`, `gl(name)`/`sl(name, v)`, ... (see the file). `stop` takes a
      symbol like `'|functions.Work|`: it is compiled to find the function and
      is the label `listbps()` shows.
      With breakOnThrows on, deliberate exceptions stop in a break loop too.
      Fixed on the way: a native function called as a method (send) got no
      stack frame (`stackFrame` nil, in the ROM too), so its arguments showed
      as garbage (and the caller's temps were wrong). `send`/`unsafeDoSend` now
      call `setNativeStackFrame()` like `call()` does (Interpreter.cc).
      Tests: `nsdt_session` (a whole session with shortcuts); the other `-dbg`
      tests changed accordingly (settings lines, exact PCs, breakOnThrows).
- [ ] 3.3 Verify the Apple API one group per step: `Where`/`QuickStackTrace`;
      `GetCurrentFunction`/`GetCurrentPC`; `InstallBreakPoint`/
      `RemoveBreakPoint`/`GetAllBreakPoints`; `Step`; `StepIn`; `StepOut`;
      `RunUntil`; `Get/SetNamedVar`, `Get/SetTempVar`; `Disasm`.

### Phase 4: Comfortable command-line REPL
Abbreviated (Matt): once DAP works, the terminal REPL matters less. 4.3 is
enough for testing; 4.1 and 4.2 are low priority.
- [x] 4.3 Line editing and history in the break loop: arrow keys, Ctrl-A/E,
      ..., up/down recall earlier lines, prompt `(newtc) ` (`(newtc 2) ` in
      a nested break loop), history kept in `~/.newtc_history`. Uses libedit's
      readline() if CMake finds it (macOS always has it; Linux if installed);
      otherwise newtc builds without it and reads plain stdin as before. No
      new dependency. Only when stdin and stdout are a terminal, so pipes
      (tests, VSNewt) are unchanged. Ctrl-D ends input like end of file.
      (REP.cc `PStdioInTranslator::produceFrameFromTerminal`; CMake
      `HAVE_LIBEDIT`.) Test: `Test/dbg/test_terminal.py` (runs newtc in a
      pseudo-terminal: types a command, recalls it with the up arrow, checks
      the history file in a temporary HOME; exit 2 if built without libedit).
      Checked that a build without libedit (configure with
      `-DEDITLINE_LIBRARY=EDITLINE_LIBRARY-NOTFOUND`) compiles and passes the
      tests.
- [ ] 4.1 (low priority) Break-loop input filter: a line that is a bare
      command word (`c`, `n`, `s`, `finish`, `bt`, `b ...`, `info b`, ...)
      becomes a call; anything else is evaluated as NewtonScript as before.
- [ ] 4.2 (low priority) Cleaner REPL output: the `#2 nil` result lines after
      every command. (The location on each stop is done: the tools' BreakLoop.)

### Phase 5: DAP, bytecode level (revised 2026-09-25)
Decisions (Matt): no TypeScript, no cppdap. newtc has its own small DAP
layer: C++ converts JSON <-> NewtonScript objects and moves the messages;
the DAP handlers are NewtonScript (embedded like NSDebugTools.ns) and use
the NS Debug Tools directly (CurrentStack, Step, StepIn, ...). DAP plugs in
as a pair of REP translators (like PHammerIn/OutTranslator): the break loop
is unchanged. No threads: the in-translator reads messages synchronously.
- [x] 5.1 JSON <-> NewtonScript objects in C++ (`Matt/JSON.{h,cc}`, reusable
      for LSP later) plus NS functions `JSONParse(str)`, `JSONStringify(obj)`
      (registered in newtc.cc `init()`, so always available, not only with
      `-dbg`). C++: `ParseJSON(text, length)`, `ToJSON(obj)`.
      Objects <-> frames (keys <-> symbols, slot order kept; keys must be
      printable ASCII), arrays <-> arrays, strings (UTF-8 <-> UTF-16, \u
      escapes incl. surrogate pairs; output is pure ASCII with \uXXXX),
      numbers: integer if it fits in `kRefValueBits` (62 bits on a 64-bit
      host, -2^61 .. 2^61-1, the range the compiler accepts for literals;
      30 on a Newton), else real; reals written in the
      shortest form that reads back exactly (NaN/Inf -> null); true <-> true,
      false/null -> nil, nil -> false; symbols and characters -> strings;
      anything else throws. Errors throw `|evt.ex.msg|` with a message and
      the offset, e.g. "JSON: expected ',' or ']' at offset 5". Nesting is
      limited to 256 levels (a cyclic frame throws instead of overflowing).
      A real that is a whole number is written without a fraction (1000.0 ->
      `1000`) and reads back as an integer; JSON doesn't tell them apart.
      NewtonScript gotchas: `try` and `self` are reserved (so no function
      `Try`, no slot `f.self`); `.ns` sources are read as MacRoman, so tests
      write non-ASCII characters as `\u00E9\u`. Test: `json`.
- [x] 5.2 `newtc -dap`: message framing, the translator pair, the handshake,
      running the program, output. Pieces:
      - C++ `Matt/DAP.{h,cc}`: `Content-Length` framing (CRLF; a plain LF
        is accepted). NS natives `DAPReceive()` (next message as a frame, nil
        at end of input), `DAPSend(frame)` (adds `"seq"` as the first member,
        so NS frames never need to be modified), `DAPExit(code)`.
        `DAPStartIO()` keeps the real stdout for DAP only (dup) and points
        file descriptor 1 at stderr, so any stray printf/cout can't corrupt
        the protocol. `PDAPOutTranslator`: everything printed (Print, Write,
        results, stack traces) becomes an `output` event, one per line or
        at flush(); exceptions (`exceptionNotify`) get category `stderr` and
        are counted for the exit code. It builds the event JSON as a C++
        string, because it runs while the object printer walks NS objects
        (a GC there could move them). `PDAPInTranslator`: in a break loop,
        `produceFrame()` waits for a message and calls `DAP:Dispatch(msg)`,
        returning nil (nothing to evaluate); end of input ends the break
        loop like for stdio.
      - NS `Matt/Debugger/DAP.ns` (embedded as `gDAPScript`): global `DAP`
        (state `launchArgs`, `configured`; `SendResponse`,
        `SendErrorResponse`, `SendEvent`, `Dispatch`, `WaitForLaunch`,
        `Finish`) and `DAPRequests` (`_parent: DAP`; one method per command:
        `initialize` (capabilities, then the `initialized` event), `launch`
        (needs `program`), `setBreakpoints` (all unverified until Phase 9),
        `configurationDone`, `threads` (one thread), `disconnect` (quits)).
        An unknown command or a handler that throws gets `success: false`
        with a message.
      - newtc.cc `handleArgDap()`: DAPStartIO, the `-dbg` setup (its
        messages go to stderr), `breakOnThrows := nil` until 5.3 can report
        exception stops, DAP translators, DAP.ns, `DAP:WaitForLaunch()`,
        run the program like `-script` (a missing file is reported by name),
        `DAP:Finish(exitCode)` (`exited` with 1 if an exception was
        reported, `terminated`, then requests until `disconnect` or end of
        input).
      Tests: `Test/dbg/dap_client.py` (a DAP client; also usable by hand:
      `dap_client.py script.dap PROGRAM=/path/x.ns`) and `.dap` cases in the
      harness (a `.dap` script instead of `.in`: requests as JSON lines,
      `wait <event>`, `eof`; the expected output is the message
      transcript): `dap_session`, `dap_errors`, `dap_nofile`, `dap_eof`.
      NewtonScript gotchas: `ClassOf(func() nil)` is `'_function` in newtc
      (use `IsFunction`); strings compare with `StrEqual`, not `=`.
- [x] 5.2b VS Code starts `newtc -dap` (VSNewt, /Users/matt/dev/VSNewt.git/vsnewt,
      github MatthiasWM/VSNewt). package.json: language `newtonscript`
      (.ns/.newt/.newtonscript, language-configuration.json: comments,
      brackets), `breakpoints` for it, debugger type `newtonscript` (launch
      attributes `program`, optional `newtc`), setting `vsnewt.newtcPath`
      (e.g. build/VSCode/newtc; also used by the compile commands),
      activation `onDebugResolve:newtonscript`. extension.ts: a
      DebugAdapterDescriptorFactory (`DebugAdapterExecutable(newtc, ['-dap'])`)
      and a DebugConfigurationProvider (F5 without launch.json runs the active
      .ns file). No debugger logic in TypeScript. Try it: open vsnewt in VS
      Code, run "Run Extension (samples)", open samples/hello.ns, F5.
      Test: `NEWTC=<newtc> npm test` in vsnewt starts a real debug session in
      a downloaded VS Code and checks output and exit code.
- [x] 5.3 Stopping: `stopped` events, `stackTrace`, `continue`, exceptions.
      - Why it stopped is recorded in C++: `gBreakLoopReason` (Interpreter.h,
        not in ROM) is set right before the interpreter calls BreakLoop:
        `kBreakLoopBreakPoint` / `kBreakLoopStep` (only temporary breakpoints
        hit) in `handleBreakPoints()`, `kBreakLoopException` in
        `handleException()` (breakOnThrows); nothing = the program called
        BreakLoop(). `PDAPOutTranslator::enterBreakLoop()` turns it into
        `DAP:Stopped(reason, text)`: reason "breakpoint", "step",
        "exception" (text: the exception, captured from the exceptionNotify()
        that comes just before), or "pause" (description "Paused in
        BreakLoop()"). An exception that stops is not counted for the exit
        code (the program may still catch it).
      - `stackTrace`: from the tools' stack object (`kNSDTools:CurrentStack()`,
        frames from `FindBreakLoop() - 1` down to 0), newest first; id =
        stack index + 1; name `Inner, pc 2` (Name for globals, Object.Name
        for methods, `<program>` for the program; native frames get
        presentationHint "subtle"); line/column 0 until Phase 6 gives them a
        source; `startFrame`/`levels` paging, `totalFrames`.
      - `continue` = `ExitBreakLoop()` (then the response). The break loop
        blocks in `PDAPInTranslator::produceFrame()` while waiting.
      - Exceptions: capability `exceptionBreakpointFilters` "all" ("All
        Exceptions", default on); `setExceptionBreakpoints` sets
        `DAP.breakOnExceptions`. breakOnThrows follows it while the program
        runs (`DAP.running`), is off before and after it, and off while a
        request is handled (a mistake in the debugger must not stop).
        newtc change in the tools' BreakLoop: a breakOnThrows set in the
        break loop is kept (the package restored the value from before, so
        neither the user nor VS Code could turn it off while stopped).
      Symbol spelling bit us: a DAP.ns method `StackFrames` turned the key
      `stackFrames` into `StackFrames` (renamed to `CollectStackFrames`).
      Fixed on the way: `SubStr` (and `Abs`, `Ceiling`, `Floor`, `Signum`)
      were stubs returning nil: the built-in function table uses the ROM
      names (`FSubstr`, `FAbs`, ...), which were the stubs in Stubs.cc, while
      the real code used other capitals (`FSubStr`, `Fabs`, ...). Renamed to
      the ROM names, stubs removed. `StrMunger` (behind `SubStr`, `StrMunger`)
      clamped the count to the string length instead of what is left after
      the start (ROM: `length - start`), so `SubStr(s, 10, nil)` copied past
      the end.
      Tests: `dap_breakloop`, `dap_breakpoint`, `dap_exception`,
      `dap_exception_off`, `nsdt_breakonthrows`; VSNewt sample
      `samples/stopping.ns`.
- [x] 5.4 `scopes`/`variables`.
      - Variable names: `-g` sets the global `dbgKeepVarNames`, and the
        (ROM) compiler then writes NTK's `DebuggerInfo` into NOS 2 functions
        (class `'dbg1`: [skip, inherited names..., one entry per arg/local:
        its name, or the index of its name when a closure keeps it in the
        argFrame]), like NTK's "Compile for debugging". `-dbg` and `-dap`
        do `-g` after loading the tools, so the program has names; the
        disassembler, GetNamedVar, and the tools' location line use them.
        Fixed on the way: the port's `makeCodeBlock` collected the
        argFrame's values instead of its names (ROM: the iterator's tag).
      - `scopes(frameId)`: Arguments, Locals (with `self` first if the
        receiver is a frame), Stack (values the function pushed and hasn't
        used yet, "top" first). Names from DebuggerInfo, or the argFrame
        in NOS 1 code, else arg0/loc0 (numArgs: locals << 16 | args).
        Compiler-made locals (`item|iter`, `i|limit`, `i|incr`) are shown.
      - `variables(ref)`: a scope's variables, a frame's slots, or an
        array's elements `[0]`, ... (`indexedVariables`, `start`/`count`).
        Frames (functions too) and arrays are expandable. References are
        indexes into `DAP.handles`, reset at every stop.
      - Values: `ValueText()` prints like the REPL on one line (printDepth
        0, printLength 10, prettyPrint nil: `{a: 1, b: {#...}}`), functions
        as `<function, 1 arg>`, other binaries as `<class, length n>`; type
        = ClassOf. New native `DAPPrintObject(obj)`: what Print would print,
        as a string (PDAPOutTranslator::printToString, capture mode).
        Fixed on the way: with `prettyPrint` nil the printer printed frames
        and arrays as `{}`/`[]` (the port put the whole slot loop under the
        prettyPrint test; ROM: only the multi-line decision).
      NewtonScript gotchas: `'self` is a syntax error (reserved), write
      `'|self|`; a send inside an array literal needs parentheses.
      Tests: `dap_variables`; the `-dbg` tests now show names
      (`functions.Add('a=10, 'b=1), 0: GetVar a`), `nsdt_temps` finds `t`
      by name, `nsdt_breakloop` handles NSDBreakLoopEntry's params in both
      forms ([name, value, ...] with names, [value, ...] without).
- [x] 5.5 Stepping, `evaluate`, `pause`.
      - `next`/`stepIn`/`stepOut` call Apple's `Step`/`StepIn`/`StepOut`
        (one bytecode instruction; stops with reason "step"). StepIn into a
        native function or BreakLoop steps over it. When they can't step
        (returning to C++ at the end of a top-level statement), newtc says
        so in the Debug Console and continues.
      - `evaluate`: with a frameId, `DAP:EvaluateInFrame()` compiles
        `func(<the frame's args and locals>) begin local |dap result| :=
        begin <expression> end; [|dap result|, <args and locals>] end` and
        calls it with the frame's receiver as self (new native
        `DAPCallWithSelf(fn, receiver, args)` = DoScript), so the expression
        sees variables by name and self's slots; changed variables are
        written back (SetVar, or SetFindVar for argFrame variables). Without
        frameId it is a global expression. Variables now have
        `evaluateName` (`info.tags`, `items[1]`, `self`) for Add to Watch
        and Copy as Expression. Error responses have readable texts: new
        native `DAPErrorText(code)` (the REPL's error strings), e.g.
        "Undefined variable: 'noSuchVariable".
      - `pause`: new interpreter hook `gDebuggerPoll` (Interpreter.h, not in
        ROM): in the slow loop, every 1000 instructions, next to the
        breakpoint check. In -dap mode (`DAPSetPolling(true)` while the
        program runs) `DAPPoll()` handles the requests that are waiting
        (select() on stdin, so newtc now reads stdin into its own buffer),
        which also makes `setBreakpoints` work while the program runs;
        `pause` calls the native `DAPPause()`, and the interpreter then
        enters the break loop there (reason `kBreakLoopPause`). Not while
        stopped (the break loop reads requests itself) or re-entered.
        Windows: PeekNamedPipe instead of select() (untested).
      - A program's own BreakLoop() comes from C++ as reason "breakloop"
        and goes out as DAP "pause" with description "Paused in
        BreakLoop()".
      Fixed on the way: `CurrentException().data` for "type.ref"
      exceptions was the address of the C++ RefStruct as an integer
      (`translateException` did `(Ref) x->data`); now the data frame
      (`{errorCode: -48807, value: 'x}`). Test `exception_data`.
      NewtonScript: `and` and `or` have the same precedence, evaluated left
      to right (confirmed by Matt from Apple's table; newtc's parser has
      PRECEDENCELogOperator for both), so parenthesize mixes. Apple's
      precedence, highest first, all left to right: `.`; `:` `:?`; `{ }`;
      unary `-`; `<<` `>>`; `*` `/` `div` `mod`; `+` `-`; `&` `&&`;
      `exists`; `<` `<=` `>` `>=` `=` `<>`; `not`; `and` `or`; `:=`.
      "\n" in a string is a CR; DAP.ns uses `kLF` for output.
      Test client: `mask /regex/replacement/` for values that vary (where a
      pause stops). Tests: `dap_step`, `dap_evaluate`, `dap_pause`.
- [x] 5.6 `-dap-server <port>` and `-dap-log <file>` (for working on newtc).
      - `-dap-server <port>`: like -dap, but newtc listens on 127.0.0.1:port,
        serves one client over TCP, and exits after the session; stdin and
        stdout stay free (the tools' messages go to the terminal), so newtc
        can run in a debugger. POSIX sockets; not on Windows yet.
      - `-dap-log <file>` (before -dap/-dap-server): every message both ways,
        one line each, in the test transcript format ("-> " from the client,
        "<- " to it), flushed at once.
      - VSNewt: `"debugServer": <port>` in a launch configuration makes the
        extension connect (DebugAdapterServer) instead of starting newtc;
        `"log": <file>` adds -dap-log. Snippet "NewtonScript: Connect to
        newtc -dap-server". newtc's (untracked) .vscode/launch.json has
        "newtc: -dap-server 4711" (lldb, with -dap-log /tmp/newtc-dap.log).
      Tests: `Test/dbg/test_dap_extras.py` (the log equals the client's
      transcript; the same session over TCP gives the same transcript;
      stdout stays free); VSNewt `npm test` runs sessions with "log" and
      "debugServer". The client (dap_client.py) talks over a pipe or a
      socket (ProcessConnection, SocketConnection).
Symbol spelling: NewtonScript symbols are case-insensitive and keep the
spelling they were first created with, while DAP keys are case-sensitive
camelCase. Checked: none of ~60 DAP keys clashes with an existing symbol;
the DAP handlers are loaded before any user code, so their keys are created
first with the right spelling.

### Phase 6: Bytecode display in VS Code
- [x] 6.1 Bytecode listings: every NewtonScript function on the stack gets
      a *virtual source* (DAP `sourceReference`), its disassembly with one
      instruction per line (`   25: GetVar               t`), made by the
      tools' disassembler (`PrintInstruction`; new native
      `DAPCaptureOutput(fn)` returns what a function prints). `DAP.listings`
      keeps one per function for the session: {fn, name (`Work.nsbc`,
      `program.nsbc`), ref, pcs (the PC of each line), text, breakpoints}.
      - Stack frames point into them: `source` {name, path (= name, only
        for display; VS Code builds the document URI from it and fails
        without), sourceReference}, `line`, `column` 1. The newest frame is
        at the instruction that runs next (after an exception: the one that
        threw; `DAP.stopReason`), callers at their call instruction (their
        PC is after it). The name no longer shows ", pc N".
      - `source` request: the listing text, mimeType
        `text/x-newtonscript-bytecode`.
      - `setBreakpoints` with a sourceReference: the tools'
        `InstallBreakPoint(fn, pc)` for each line, replacing those set from
        that listing before (`RemoveBreakPoint`); a line without an
        instruction is not verified. Breakpoints in .ns files stay
        unverified until Phase 9.
      - VSNewt: language `newtonscript-bytecode` (`.nsbc`, that mimetype),
        a small TextMate grammar (syntaxes/newtonscript-bytecode.tmLanguage.json:
        PC, opcode, strings, symbols, numbers), breakpoints allowed in it.
        VS Code opens the listing at every stop and highlights the line;
        clicking in the gutter sets bytecode breakpoints (they last for the
        session). VS Code registers its debug: document provider when the
        debug view comes up; the VSNewt test opens it first.
      Fixed on the way: the compiler's `WalkNodes` skipped the receiver of a
      message send (`:` node; the ROM walks it after the arguments), so a
      local used only as a receiver inside a closure (`func() d:P(1)`) was
      not closed over and was "Undefined variable". Test `closure_send`.
      NewtonScript: a program's global function named like a shortcut
      (`Stop`, `stop`: symbols are case-insensitive) replaces the shortcut
      and takes its spelling.
      Tests: `dap_listing` (listing text, frame lines, breakpoints set,
      replaced, cleared, a line past the end); the DAP transcripts now show
      sources; VSNewt "Shows a bytecode listing for a stopped function"
      (opens the listing as a VS Code document: language, current line).
      Optional, not done: DAP `disassemble` + instruction breakpoints for
      VS Code's Disassembly view.

### Debugging newtc while it serves DAP
1. In the newtc window, start "newtc: -dap-server 4711" (lldb). newtc waits
   for a client ("newtc: waiting for a DAP client on port 4711").
2. In the VSNewt test window (Extension Development Host), run a
   NewtonScript configuration with `"debugServer": 4711` (snippet
   "NewtonScript: Connect to newtc -dap-server"). Breakpoints in newtc's
   C++ stop in the newtc window; one session, then newtc exits.
3. The traffic: `-dap-log <file>` (the configuration above writes
   /tmp/newtc-dap.log), or `"log": "<file>"` in a VSNewt configuration.
4. Most adapter work needs no VS Code at all: the .dap cases and
   `Test/dbg/dap_client.py script.dap PROGRAM=...`.
Not done (not needed so far): CodeLLDB attach with "waitFor" plus an
environment variable that makes newtc wait for the debugger.
VSNewt stays a thin shell (its existing TypeScript; no debugger logic in
it): `contributes.debuggers` (type `newtonscript`) with a
DebugAdapterExecutable `newtc -dap`, `languages` for .ns, and
`breakpoints: [{language: "newtonscript"}]`; the setting
`vsnewt.newtcPath` for a development build of newtc.

### Phase 7: Line tables in the compiler (revised 2026-09-25)
Decisions (Matt): code newtc compiles carries its line table *in the
function object*, like NTK's DebuggerInfo (it travels into NSOF and
packages; no side-car pairing or checksums; a Newton ignores the slot). A
side-car `.nsdbg` is only for code whose binary must not change
(decompiled packages, ROM; Phase 9). Lookups have one path: the function's
own table, else one registered for its `instructions`. Line stepping runs
in the interpreter (C++), not in NewtonScript.
- [x] 7.1 Line numbers in the parser. The lexer records where each token
      starts (`theToken.location`; it reads one character ahead, so
      `lineNumber` after a token can already be the next line). The yacc
      driver (Compiler.cc `parser()`) keeps a line stack next to the value
      stack (`lStack`/`yylsp`): a shift pushes the token's line, a reduce
      pushes the line of the rule's first token (`yyline`). With -g
      (`CCompiler::fKeepLines`, global `dbgKeepLineNumbers`), `withLine()`
      wraps statements in `[TOKENline, line, statement]` (TOKENline = 920,
      not a lexer token): top-level statements (rules 3, 5), statement
      sequences (110, 111), if branches (77, 78), loop bodies (84-88, 93),
      repeat's until (94), function bodies (61, 62, 95-97), onexception
      handlers (101). Without -g the tree is exactly as before.
      `WalkNodes` descends into the wrapper; the top-level "=" warning
      looks through it.
- [x] 7.2 Code generation: `walkForCode` case TOKENline calls
      `CFunctionState::noteLine(line)` and generates the statement;
      `noteLine` appends (pc, line) when the line changes (or replaces the
      last entry if no code came since). Loops note their own line again
      before the control code at the bottom. `makeCodeBlock` stores
      `lineTable: [lineTable: file, pc, line, ...]` in the function; the
      file is one string per compile (absolute path via realpath, else the
      stream's name, e.g. "NSDebugTools.ns"). -g sets dbgKeepLineNumbers
      (with dbgKeepVarNames); -dbg and -dap do -g. Closures share the
      template's instructions and carry its table.
      **The code doesn't change**: `Test/lines_invariant.py` decompiles
      corpus packages, compiles the source without and with line tables,
      and compares the decompiled results (ignoring the shared file name
      constant and Ref_N numbering): 300 packages, 0 different (9 skipped:
      already failing without line tables); every function compiled from
      the source has a table (the few without are ROM functions reached
      through magic pointers).
- [x] 7.3 Lookups, `Matt/LineTables.{h,cc}`: the compiler reports every
      function it makes with a line table (new hook
      `gCompiledFunctionHook` in CompilerSupport.cc, so the compiler
      doesn't depend on newtc); `InstallLineTables()` keeps them in one
      list (GC root). `LineOfPC(fn, pc)` -> [file, line] (binary search;
      a prologue before the first entry, like copying closed-over args,
      belongs to the first line). `CodeForLine(file, line)` ->
      {line, code: [[fn, pc], ...]}: the functions with code on that line
      (lowest pc each); a line without code moves to the next line with
      code in a function spanning it, else (between top-level statements)
      the next line with code in the file; paths compared after realpath.
      Both are NewtonScript functions, too.
      Fixed on the way: `StrPos` didn't find a match at the very end of the
      string (the port looped while `start + len < strLen`; ROM `<=`), and
      now treats a negative start as 0 like the ROM.
      Tests: `line_tables` (tables of loops, a multi-line if, a closure;
      LineOfPC; CodeForLine with moves), `Test/lines_invariant.py`.

### Phase 8: DAP, source level
- [ ] 8.1 Stack frames with the real `.ns` file and line; the bytecode
      listing stays the fallback for functions without a line table.
- [ ] 8.2 Breakpoints by file:line. The program is compiled and run one
      top-level statement at a time, and VS Code sends breakpoints before
      anything is compiled: unresolved breakpoints stay pending and are
      resolved whenever new code is compiled, before it runs (then a
      `breakpoint` event: verified, snapped to the statement's line).
- [ ] 8.3 Line stepping in C++: a step mode in the slow loop (next to the
      pause poll): over = a new line in the same or an outer frame, in = any
      new line, out = frame depth drops; recursion- and exception-safe.
      DAP granularity "instruction" keeps Apple's Step.
- [ ] 8.4 (optional) DAP `disassemble` + instructionPointerReference, so VS
      Code's Disassembly view shows bytecode next to the source (VS Code
      switches between source and instruction level; Matt).

### Phase 9: Code without source
- [ ] 9.1 The decompiler writes a `.nsdbg` next to its output: for each
      function, keyed by its object path in the package
      (`part.0.data.theForm.viewClickScript`, ObjectPrinter::RefPath) plus a
      hash of its instructions, the line table into the decompiled source.
      Loading a package with its .nsdbg registers the tables.
- [ ] 9.2 Later: ROM code (with Einstein), `-run` for form packages.

### Later: the rest of the VS Code extension
- `-lsp` mode in newtc (diagnostics, completion, ...), TextMate grammar.

## Known bugs (to fix)

Every bug found while working on the debugger goes here until it is fixed:
check it off (with the commit) when fixed, don't delete it. Bugs fixed right
away are described in the step where they were found (0.2, 1.3, 2.1, 3.1c-e).

Interpreter and runtime
- [ ] B1 **Recursion is broken in NOS 1 code** (`-nos1`, or a `//! -nos1`
  script; no longer the default). A recursive `Fib(n)` returns `n-1`; even
  `if n < 2 then return n` gives wrong values. Looks like the NOS 1 argFrame
  (locals) is shared instead of copied per call. NOS 2 code is correct. It
  still matters: a 2.x ROM also runs NOS 1 packages.
- [ ] B2 **NOS 1 CodeBlock frames in `CNSDebugAPI::stackStart()`**: its
  `stackFrame` is the stack top at call time (args stay on the stack), yet
  stackStart adds 3 like for NOS 2 functions. Verify against the ROM and a
  test (temps of/above a NOS 1 frame); may be related to B1.
- [ ] B3 **Stubs**: `Stubs.cc` has many built-ins that just return nil.
  Replaced so far because the debugger needs them: `GetGlobals`, `ArrayPos`,
  `Display`; in 5.3 `Abs`, `Ceiling`, `Floor`, `Signum`, `SubStr` (the real
  code existed with other capitals, see 5.3). Go through the list and
  implement the ones a script can reasonably call (compare with the ROM).
  Still to check from the capitals scan: `FSetupTetheredListener` is a stub
  while `FSetUpTetheredListener` exists (NTK); `Fmin`/`Fmax` are stubs and
  `FMin`/`FMax` real, and the ROM has both spellings (which one is 'Min?).
  Stubs whose name the ROM doesn't know are unused (`Farray`, `Fdebug`,
  `FhasVariable`, `Fisa`, `FmodalState`, `FntkDownload`, `FntkListener`,
  `ForigPhrase`, `Freal`, `Fstats`, `FGetSortID`): delete them. Also check
  against the ROM: `Floor` returns a real, `Ceiling` an integer (>= 1).
- [ ] B4 **Sorted array set operations**: `GenOrderedSetOp`
  (Frames/SortedArrays.cc, behind `BDifference`, `BIntersect`, `BMerge`)
  divides a `Ref*` difference by `sizeof(Ref)`, as `LSearch` did (lines
  ~956-968): copies too few elements, truncates the result.
- [ ] B5 **`BMerge([...], [...], '|<|, nil, nil)` hangs.**
- [ ] B6 **`LSearch(["x","y"], "y", 0, '|str=|, nil)` returns nil** (the general
  test path, `CGeneralizedTestFnVar`).

- [ ] B11 **Undefined behaviour when the store is created**: the first run
  with a new HOME (no store in `~/Library` yet) reports
  `Stores/FlashStore.cc:1896:35: runtime error: reference binding to null
  pointer of type 'CStoreObjRef'` (UBSan). Seen with a temporary HOME in
  `Test/dbg/test_terminal.py`.

- [ ] B15 **`SPrintObject` is half ported** (Frames/Strings.cc,
  `MakeStringObject`): reals, nil, true, frames, arrays give `""`, a 62-bit
  integer is cut to 32 bits (`1152921504606846975` -> `"-1"`,
  `IntegerString(RINT(obj))`), and the function never copies into the
  result string in some branches. In the ROM it is the `&` conversion
  (strings, numbers, symbols, characters), not the printer. DAP uses its
  own `DAPPrintObject`.

- [ ] B16 **A closure over a `for` loop variable is a syntax error**:
  `for i := 0 to 2 do begin local g := func() i; ... end` gives -48601
  "syntax error" (with a copy, `local k := i; func() k`, it works). Check
  whether the ROM/NTK compiler refuses this on purpose (the loop keeps
  hidden locals i|limit, i|incr) or whether it is a porting bug.

- [ ] B13 **The REPL prints strings unescaped**: `Print("a\"b\\c")` shows
  `"a"b\c"` (`SafelyPrintString`, Frames/ObjectPrinter.cc, marked "not
  complete yet"): `"`, `\` and control characters (CR, LF, tab) are not
  escaped, so the output is no valid NewtonScript. Matters for DAP variable
  values (5.4). Check what ROM `SafelyPrintString` does.

- [ ] B14 **Integers overflow silently**: integers have 62 bits on a 64-bit
  host (`kRefValueBits`), but arithmetic wraps without notice:
  `1152921504606846975 * 4` gives `-4` (e.g. Interpreter.cc
  `MAKEINT(RINT(a) + RINT(b))`). Check what the ROM does (throw, or convert
  to a real?). Also: the compiler rejects the literal `-2305843009213693952`
  (it negates the out-of-range 2^61), like C does; `-2305843009213693951 - 1`
  works. And a 62-bit integer can't go into a package or NSOF file for a
  real Newton (30 bits): check what the writers do with one.

Decompiler
- [ ] B7 **Output depends on memory layout.** With AddressSanitizer on (Debug
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
  a different order when the allocator changes. Totals with ASan: 1909 CLEAN,
  371 UNRESOLVED, 69 CRASHED.
- [ ] B8 **Drops statements** (NS Debug Tools.pkg, `Ref_270` = `DisasmRange`):
  in `if A then X else if B then Y else begin if C then Z; <more statements>
  end`, the decompiled source ends after `if C then Z`; the bytecode (pc 52-96:
  a second `if` and the call to `Disassemble`) is missing. See
  `newtc -pkg ... -debug bc -decompile`, search for "DisasmRange".
- [ ] B9 **Loses parentheses** (NS Debug Tools.pkg, `StepIn`): an `if` without
  `else` whose condition is `A or B` is printed as `A or B and X`, which means
  `A or (B and X)`. The bytecode (pc 160-207) evaluates `A or B` first, then
  branches.
- [ ] B10 **Round trip**: `Test/round_trip.py` reports `GEN2_FAILED` for 29 of
  the first 30 manifest packages, with the binary from before 1.1 too.
- [ ] B12 **ASCII only characters**: make sure that the decompiler outputs only
  ASCII characters and that characters that were originally non-ASCII UTF-16
  are output a escaped sequences - eventually we have to decide if NewtonScript
  shall go all UTF-8.

Open work (not bugs)
- `-g` (VSNewt's "Compile ... for debugging" commands pass it): compile with
  debug information and write the debug map (Phase 8). newtc accepts and
  ignores it until then.
- The newtc in VSNewt's bin/darwin-arm64 is old (no -dap): copy a current
  build there before packaging a VSIX.
- `-run` (run a loaded 'form package) is not implemented.
- newtc's compiler writes no `DebuggerInfo` (NTK's variable names), so NOS 2
  locals have no names in the debugger (Phases 7/8).

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
