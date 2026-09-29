<!-- Archive: finished work moved out of Matt/CLAUDE.md (2026-09-27).
     Current notes and the plan are in Matt/CLAUDE.md. -->

# History: newtc's debugger, and how we got here

What was done, in order, with the reasons and the details: the original
brief, the first findings, phases 0 to 9 of the debugger (REPL, DAP, source
level, debug maps), phase 10.1 (the FLTK event loop), the bugs fixed, and
the early decisions. Matt/CLAUDE.md has the current state and plan; step
numbers there (e.g. "5.3", "B8") refer to the entries here.

---


## The original brief (Matt, 2026-09-25)



### Matt's NewtonScript debugger

### the goal

The `newtc` command line application gets a REPL debugger for NewtonScript,
based as much as possible on what is already inside NewtonScrip (`BreakLoop`)
and what the "NS Debug Tools.pkg" provides (Next, StepIn, StepOut, ...).

The REPL mode will be accessible via command line ( `-dbg` ) by expanding
the BreakLoop interface to also understand `gdb` style shortcut like `n`, `s`,
and `r`. And via DAP Debug Protocol.

### REPL

Much of REPL already comes with NewtonScript, and additional functions are in
"NS Debug Tools.pkg" that came with NTK. WE can decompile the debug tools (there
is some ARM32 code in there) and lift the features and make them part of newtc.

In a first iteration, we only look at ByteCode commands. We must keep in mind
that we want source level debugging later.

#### Preserving and activating what we have

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

### Command line mode

For testing and debugging "newtc", a terminal version of REPL mode is available
via `-dbg`. In this mode, we break directly into then NewtonScript `BreakLoop`
where we can enter NewtonScript commands. This already mostly works. Adding
single letter shortcuts maked this mode usable (vs. typing StepIn(), etc. ).
If this works reliable on ByteCode level, we add support for the DAP protocol.

### DAP protocol

I want to implement a debugger interface for the "newtc" executable using the
DAP protocol with Visual Studio Code. The DAP is handled using cppdap:
https://github.com/google/cppdap . DAP mode is enabled by passing the `-dap`
flag.

DAP is to be implemented on top of a REPL debugger interface.

ByteCode debugging should map directly. How do we handle the display of the
ByteCode diassembly though?

### Source Level Debugging

#### Internal Debug Information

To allow source level debugging, we must map the pair sourefile/linenumber to
the newtonscript function address/ByteCode offset (PC) - and back. We need
to find an internal representation to do this mapping and write a function
for converting in either direction.

#### Generating and storing the debug map

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


## The debugger, step by step

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
- [x] 8.1 Stack frames with the real `.ns` file and line: for a function
      with a line table, `LineOfPC(fn, pc)` gives `source` {name (the file
      name), path} and `line`; the newest frame looks up its PC (next
      instruction; after an exception PC - 1, the one that threw), callers
      PC - 1 (their PC is after the call). Only absolute paths count
      (`IsAbsolutePath`): code without a file (the tools' own
      "NSDebugTools.ns", `Compile()`: "<unknown>") keeps its bytecode
      listing, so a stack can mix source and listing frames. -dap compiles
      the program with -g, so its frames show the .ns file.
      Tests: the DAP transcripts show the source files; `dap_listing` now
      compiles its functions with Compile() to keep testing listings;
      VSNewt "Shows the source line, or a bytecode listing" (both in one
      session).
- [x] 8.2 Breakpoints by file:line. `setBreakpoints` with a source path
      replaces that file's breakpoints (`DAP.lineBreakpoints`: {id, path,
      line, verified, actualLine, installed}). Each is resolved with
      `CodeForLine` and installed with the tools' `InstallBreakPoint(fn,
      pc)` in every function with code on that line (reported verified, at
      the line used). The program is compiled and run one top-level
      statement at a time and VS Code sends breakpoints before anything is
      compiled, so an unresolved breakpoint stays pending ("No code for
      this line yet: set when the code is compiled"). New compiler hook
      `gCompiledStatementHook` (CompilerSupport.cc; ParseFile calls it after
      compiling a statement, before running it): in -dap mode
      `DAP:NewCode()` installs the pending breakpoints that have code now
      and sends a `breakpoint` event (reason "changed", verified, line).
      Breakpoints set while the program runs arrive through the poll.
      Tests: `dap_line_breakpoints` (pending -> verified at compile, a line
      without code moving to the next statement, replacing the set while
      stopped, one still pending when set); `dap_session` now sets line 99
      (no code; stays pending); VSNewt "Stops at a breakpoint set in the
      source file" (a SourceBreakpoint set through the VS Code API).
- [x] 8.3 Line stepping in C++. New interpreter hook `gDebuggerStep(fn,
      pc, frameIndex)` (Interpreter.h, not in ROM): while set, the slow loop
      calls it before every instruction (next to the breakpoint check and
      the pause poll; frameIndex on the CNSDebugAPI scale, 0 = oldest);
      true stops there (reason kBreakLoopStep). `Matt/LineTables.cc`:
      `StartLineStep(kind, fn, pc, depth)` / `CancelLineStep()`, NS
      function `StartLineStep('over | '|in| | 'out, fn, pc, depth)`.
      Rules (`StepCheck`): left the start frame (return, exception) -> stop
      in the caller where it continues (if it has a line table); the same
      depth but another function -> the next top-level statement, at its
      start; in the start function -> a statement start of another line, or
      one jumped back to (a loop body on its own line stops once per
      iteration, and the loop line too; a loop on one line runs as a
      whole); deeper -> only for 'in, at the first statement of a function
      with a line table (recursion and calls run through for 'over). 'out
      stops only on leaving the frame (or at the next top-level statement).
      Code without a line table is run through. Resuming: the break loop
      and the request handler run in deeper frames first; checks start when
      the program is back in the start frame, skipping the start
      instruction if it gets checked again. Any stop cancels the step
      (`PDAPOutTranslator::enterBreakLoop`, e.g. a breakpoint in a called
      function), and the end of the program.
      DAP: `next`/`stepIn`/`stepOut` step by line when the stopped function
      has a line table and the request doesn't say granularity
      "instruction"; otherwise Apple's Step/StepIn/StepOut as before. The
      adapter's own DAP.ns is now loaded without line tables and variable
      names (handleArgDap clears dbgKeepLineNumbers/dbgKeepVarNames while
      loading it), or stepping would stop in it.
      Tests: `dap_line_step` (step in/out, next over a call, recursion, a
      multi-line and a one-line loop, into the next top-level statement,
      off the end); `dap_step` now asks for granularity "instruction".
- [x] 8.4 VS Code's Disassembly view (bytecode next to the source;
      switching between source and instruction level). Capabilities
      `supportsDisassembleRequest`, `supportsSteppingGranularity` (VS Code
      then sends granularity "instruction" while the Disassembly view has
      the focus: Apple's instruction steps), `supportsInstructionBreakpoints`.
      Addresses: VS Code sees one address space, NewtonScript one bytecode
      per function; each listing gets a 64 KB window (bytecode is smaller,
      branch targets are 16 bits): address = listing ref * 0x10000 + pc
      (`AddressOf`, `ListingAt`, `HexString`, `ParseHex`). Listings are now
      made for every interpreted frame and keyed by `instructions` (closures
      share them); they keep each instruction's text (`texts`).
      - Stack frames: `instructionPointerReference` (the next instruction;
        callers and exceptions: the instruction before the PC).
      - `disassemble`: instructionCount instructions from instructionOffset
        instructions after the one at memoryReference (+ offset bytes):
        address, instructionBytes ("27 00 08"), instruction, symbol (at pc
        0), and location + line from the line table; outside the function
        "invalid" instructions (DAP wants exactly instructionCount).
      - `setInstructionBreakpoints`: replaces the ones set before; an
        address that doesn't start an instruction is not verified.
      VS Code's view asks for -50..+50 and 0..50 instructions around the
      IP; in the test window it logs an internal "Cannot read properties of
      undefined (reading 'element')" (also without the invalid padding; the
      requests and answers are right): check in a real window.
      Test harness: `run_dbg_tests.py` now flags keys of newtc's messages
      that start with a capital (the symbol spelling trap bit again: a
      method `InstructionBytes` respelled `instructionBytes`).
      Tests: `dap_disassemble` (frames' addresses, disassemble around the
      IP, before the start and past the end, an unknown address,
      instruction breakpoints valid and not, an instruction step); VSNewt
      "Opens the Disassembly view" (VS Code's own requests).

### Phase 9: Code without source
- [x] 9.1 Debug maps for decompiled packages. `newtc -pkg P -odecompile
      x.ns` writes the decompiled source to x.ns and its debug map to
      x.nsdbg; `newtc -pkg P -nsdbg x.nsdbg ...` loads it (stderr:
      `newtc: debug map "x.nsdbg": N of M functions found`), and the
      package's functions then have line tables into x.ns: stack frames,
      line breakpoints, line stepping, the Disassembly view, all as for
      code compiled with -g. The package's objects are not changed.
      - Map format (JSON): `{"format": "nsdbg", "version": 1, "source":
        <absolute path of x.ns>, "functions": [{"path": ["part", 0, "data",
        "InstallScript"], "hash": "bef6a6c0b0183fd8", "lines": [pc, line,
        ...]}]}`. path: slot names and array indexes from the package
        (PathToObject: depth-first from the root, magic pointers not
        followed); hash: FNV-1a 64 of the instructions (InstructionsHash).
      - Writing: the printer (Matt/Printer) counts the lines it writes.
        `Decompiler::MarkStatement(node)` asks it to note the line where the
        statement's first token comes out (`MarkNextItem`, resolved in
        `DoStartItem`, so a pending newline is counted first), at the
        statement's first pc (`Node::FirstPC()`, through the new
        `VisitChildren()`, which mirrors `PrintChildren()` in every node
        type). Marked: top-level statements of a function, compound
        statements, loop bodies, if/else branches. The decompiled text is
        unchanged: byte-identical for 300 corpus packages before and after.
      - Loading (`LoadDebugMap`): collects the package's functions (walk
        without allocating), matches each map entry by hash (several
        functions with the same bytecode: by path) and registers the table
        by `instructions` (`RegisterLineTable`; `TableOf` falls back to the
        registry, one-entry cache). All line-table users go through TableOf
        (LineOfPC, CodeForLine, StepCheck); DAP.ns's LineStep now asks
        `LineOfPC(fn, 0)` instead of looking for a `lineTable` slot.
      - Checked on the corpus (`Test/nsdbg_check.py --batch ... --limit
        300`): 8118 of 8119 functions found again (one miss in mb_v27);
        35477 of 35522 mapped lines are statement lines of the same function
        when x.ns is recompiled with -g (an independent line table; the
        bytecode differs between NTK's compiler and ours, the statements
        don't). The rest are functions the decompiler prints twice (the same
        text in two places): the map uses the last one.
      Fixed on the way: `SafelyPrintString` (Frames/ObjectPrinter.cc) didn't
      reset its chunk index after writing a chunk, so a string longer than
      240 characters printed the buffer again and again and overflowed it
      (garbage, then a crash). Test `print_long_string`.
      Tests: `Test/dbg/test_nsdbg.py` (hello package: -odecompile, -nsdbg,
      then DAP: a breakpoint on a line of the decompiled InstallScript, the
      stop there, `next` to the next line), `Test/nsdbg_check.py`.
- [x] 9.1b Debugging a package from VS Code. Launch "program" can be a
      package (.pkg): the launch request loads it (new native
      `DAPLoadPackage(path, mapPath)`; it becomes ref0) with its debug map,
      the launch attribute "debugMap", else `<program without .pkg>.nsdbg`
      if there is one; the Debug Console says how many of the map's
      functions were found (a warning if none: a map of another build). A
      missing "debugMap" file fails the launch. Breakpoints sent before the
      launch are set once the package is there (`DAP:NewCode()`). At
      configurationDone the package is installed (`installPackage()`, also
      used by `-run` now), from the ROM (package installer and
      `InstallFormPart`, NewtonScript in ROM at 0x5579C0): any part frame's
      `DoNotInstall()` returning non-nil cancels; for a "form" part, its
      devInstallScript (NTK's name for the developer's InstallScript), else
      its InstallScript, is sent to a new frame {_proto: partFrame, app,
      InstallScript, RemoveScript} with that frame as argument (package
      frames are read-only), then set to nil; an "auto" part gets
      partFrame:?InstallScript(partFrame). Not like the ROM: no
      EnsureInternal copies (the package is in memory; copies would also
      lose the map, which finds functions by their instructions object);
      the form isn't opened (BuildContext(theForm) into the root view) and
      there is no "already installed" check (both need the root view).
      A .ns program runs as before. VSNewt: launch attributes "debugMap",
      "args" (newtc arguments before -dap, e.g. `["-pkg", "lib.pkg"]`),
      snippet "NewtonScript: Debug a package". newtc now reserves stdout
      for DAP first thing when -dap is on the command line, so what those
      arguments print goes to stderr.
      Fixed on the way: send-if-defined and resend-if-defined (`:?`,
      `inherited:?`) to an undefined method overwrote the stack entry below
      the arguments with nil instead of pushing nil as the result (Interpreter.cc):
      `a:?nothing(a); a` returned nil (the NTK InstallScript wrapper failed
      with "ObjectPtr of non-pointer"). Test `send_if_defined`.
      Tests: `Test/dbg/test_nsdbg.py` (the package as the program: map next
      to it, "debugMap", breakpoints before the launch, a missing map, no
      map), VSNewt "Debugs a package in its decompiled source", "Passes
      \"args\" to newtc".
- [ ] 9.2 Later: ROM code (with Einstein), `-run` opening the form
      (needs the root view: GetRoot, BuildContext).


### Phase 10: GUI with FLTK (branch Add_fltk), as of 10.1
Decisions (Matt, 2026-09-27): FLTK is the host layer (windows, drawing,
events), optional: CMake option `NEWTC_USES_FLTK` (off by default) fetches
FLTK master from GitHub (FetchContent) and links it statically; without it
nothing is fetched and everything builds as before. newtc stays single
threaded: FLTK only runs while no NewtonScript runs.
- [x] 10.1 The event loop (Matt/EventLoop.{h,cc}). After the program (a
      -script, -run, or the -dap program) returns, `RunEventLoop()` waits
      for events (`Fl::wait()`) while it has a window open; it returns when
      the last one closes or the DAP client is gone. Rules:
      - Host events run NewtonScript only through `SendEventMessage(rcvr,
        msg, args)`, and only while no NewtonScript runs (the interpreter's
        control stack is where the loop started): scripts never overlap.
        While a script runs or is stopped in a break loop nobody calls
        Fl::wait(), so no events come; one that does (a nested loop, e.g.
        an FLTK dialog opened by a native) is dropped. Later, a modal Newton
        dialog (ModalDialog()) will open a nested loop whose events may run
        scripts inside the one that opened it, as in NewtonOS.
      - An exception in a callback is caught there (newton_try) and
        reported like one in the program (-dap: counts for the exit code);
        it must not unwind through FLTK. With breakOnThrows the debugger
        stops at the throw first, as usual.
      - DAP requests are one more event source: `Fl::add_fd()` on the DAP
        input (stdin or the -dap-server socket), handled with
        `DAPHandleIdleRequests()`; requests that came while the program ran
        are handled when the loop starts. The closed input ends the loop.
      - Pause while idle stays pending: `DAPEnterScript()` then makes the
        interpreter poll before the next instruction (new
        `DebuggerPollNow()`, Interpreter.h), so it stops at the first
        instruction of the next callback. Handy to step through whatever a
        click does.
      - Stepping out of a callback cancels the step (`DAPLeaveScript()`):
        the next callback is something else, the program just runs.
      - Stopped in a break loop, the windows don't repaint (macOS keeps
        their contents; the beach ball shows over them): like any native
        app stopped in a debugger.
      - Refs kept by host objects (widgets) must be `RefStruct` members:
        the GC updates them when objects move (a plain Ref goes stale), and
        they live as long as the widget. Not `RefVar`: its handle belongs to
        the stack position it was made at, and when a NewtonScript exception
        unwinds past it (longjmp, no destructors) `ClearRefHandles()` frees
        it (seen: a button's message turned into garbage after an exception
        in a callback). A RefStruct's handle has stack position 0 and is only
        freed by its destructor. (AddGCRoot is for a few C++ globals.) The
        test window's MessageButton is the pattern.
      A window for testing until there are Newton views
      (Matt/TestWindow.{h,cc}, FLTK only): `TestWindow(title, receiver,
      message)` (a button that sends receiver:message()),
      `TestWindowClick()` (clicks it from the event loop, as a user would:
      tests and the Debug Console make events with it), `TestWindowClose()`.
      Test harness: cases named `fltk_*` are skipped when newtc has no FLTK.
      Tests: `fltk_events` (-script: events after the program returned, one
      at a time, an exception in a callback, the window ends newtc),
      `fltk_dap_breakpoint`, `fltk_dap_pause` (pause while idle, next, step
      out doesn't stop in the next callback), `fltk_dap_exception`,
      `fltk_dap_eof`.
- [ ] 10.2 Measure what the ROM's views need: GetRoot and BuildContext with
      a drawing backend that does nothing; `-stubs report` while the Hello
      form opens lists the native functions to implement.
- [ ] 10.3 The Newton screen: an FLTK window, drawing with FLTK.
- [ ] 10.4 Pen and keyboard events into the view system.


## Phase 10: a Newton GUI on FLTK (2026-09-27 to 2026-09-29)

Moved here from CLAUDE.md on 2026-09-29, when Phase 11 (NewtPlay) began;
the design notes stayed there ("The FLTK layer"), and so did what is left
of 10.7e and 10.7f ("Backlog").

### Goals

- **Short term**: the built-in Hello app (`newtc -hello`) gets a button;
  clicking it shows a modal alert; the app reacts to its window being
  closed. That covers most of the first FLTK work: the root view, opening an
  app, a button and its script, a modal dialog (with its nested event loop),
  and closing.
- **Long term**: Battleship runs (nBattleship 1.4: short enough, interesting
  enough to demo, and one of the few Newton apps whose source was
  published). In the corpus: `unna2/development/source/nBattleship1.4/`
  (`nBattleship 1.4 Code/Battleship.pkg`, and the source).

### How a Newton installs and opens a package (research, 2026-09-27)

The ROM's NewtonScript for this is in newtc (481 ROM NewtonScript functions,
in `@4098`); decompiled with `newtc -script x.ns -decompile` where x.ns is
`GetGlobalFn('RegisterNewPackage);`:

1. `RegisterNewPackage(pkgRef, name, soup)`: `GetPkgRefInfo(pkgRef)`; gives
   up for `IsSirNotAppearingInThisROM`; every part frame's `DoNotInstall()`
   (in a try: an error or non-nil means "not installed", with a Notify); a
   package of the same name already there: not installed; adds an entry to
   the "Packages" union soup (`AtomicAction`); then
   `SafeActivatePackageQT(pkgRef, ...)` (refuses a name already in use,
   else calls the native `ActivatePackage(pkgRef)`); `XmitPackageOp`.
2. `ActivatePackage` (C++, the package manager) calls each part's handler;
   for 'form and 'auto parts `InstallPart(...)` in `Packages/Parts.cc` builds
   an install info frame (`canonicalFramePartInstallInfo`: partType,
   partFrame, packageId, packageName, partIndex, size, packageType,
   deviceKind, deviceNumber, packageStyle) and calls the ROM's NewtonScript
   `InstallPart(info)`; its result is kept for removing the part.
3. `InstallPart(info)`: `InstallFormPart(info)` or `InstallAutoPart(info)` in
   a try (an error: Notify "An error occurred activating the package ...",
   the other parts still install); then the Extras drawer learns about the
   part (`GetRoot().ExtrasDrawer:HandleNewHighROMPart(info)` for built-in
   packages, `|HandleNew1.XPart|` for 1.x ones).
4. `InstallFormPart(info)`: `root := GetRoot()`; `app :=
   EnsureInternal(partFrame.app)`; if `root.(app)` exists: Notify "An
   application by the name ... is already installed", done. Else `context
   := {_proto: partFrame, app, InstallScript: EnsureInternal(devInstallScript
   or InstallScript), removeScript: EnsureInternal(devRemoveScript or
   removeScript)}`; `context:InstallScript(context)`; `root.(app) :=
   BuildContext(partFrame.theForm)` (the app's base view: its view frame,
   not open, no native view yet); `cardSocket` for card packages;
   `context.InstallScript := nil`; returns the context (the remove cookie).
   `InstallAutoPart(info)`: `partFrame:?InstallScript(partFrame)`.
5. Removing: `RemoveFormPart(info, context)`: the removeScript, then `if
   root.(app).viewCObject then root.(app):Close()`, then
   `RemoveSlot(root, app)`.
6. Opening: installing opens nothing. The Extras drawer shows the icon; a
   tap opens the app: `GetRoot().(appSymbol):Open()`. Open makes the native
   view (in NewtonOS a CView; the view frame's `viewCObject` slot refers to
   it) and runs the scripts: viewSetupFormScript, viewSetupChildrenScript,
   the children, viewSetupDoneScript, then shows it. `Close()` runs
   viewQuitScript and removes `viewCObject`. (To confirm in CView,
   `Views/View.cc`, when we get there.)

For newtc (Matt, 2026-09-27): there is no Extras drawer, so no icon to tap,
and newtc runs a single app. So `-run` (and `-dap` with a package as the
program) installs and activates the package and then opens its app right
away: `GetRoot().(appSymbol):Open()`. The NewtonOS environment (the button
bar, the Extras drawer, the built-in apps) may come in a later iteration,
not for a while.

Findings that shape the design:
- **`viewCObject`**: the ROM's NewtonScript uses this slot 251 times, often
  to decide whether a view is open (e.g. RemoveFormPart above). So the link
  from a view frame to our native side *is* `viewCObject` (else the ROM
  code would think no view is ever open). Decided (Matt, 2026-09-27): no
  `__ncWidget` slot.
- **EnsureInternal** (InstallFormPart) copies the install script, and a
  copied function no longer matches its debug map (maps find functions by
  their instructions object): breakpoints in a decompiled InstallScript
  would stop working. Options: newtc's EnsureInternal treats a loaded
  package as internal (it stays in memory until newtc exits); or debug maps
  also match copies (by the hash of their instructions). Decided (Matt,
  2026-09-27): packages stay read-only as on a Newton, so EnsureInternal
  copies; line tables follow the copies (done, 10.2b). Since 10.3
  `installPackage()` uses the ROM's own InstallPart.
- **The C++ view system isn't in newtc**: CView/CRootView (`Views/`) and the
  package manager are only compiled for MessagePad. `GetRoot`,
  `BuildContext` and the view natives are stubs. The link classes (below)
  take CView's place; CView's code tells what they must do.

### Where things stood (2026-09-28)

A Newton package as a macOS app of its own (2026-09-28, for Adam Tow,
nBattleship's author): Matt/tools/build_app.sh <app.pkg> [name] [version]
builds build/App/<name>.app (target newtc_app, CMake -DNEWTC_APP_PKG=...:
newtc with the package compiled in, Matt/EmbeddedApp.h), universal (arm64
and x86_64, both tested with a whole game), macOS 13, signed ad hoc, and a
zip of it. Double-clicked it runs the package with its store in
~/Library/Application Support/<name>/; with arguments it is newtc.
Its icon: the package's own (cmake/print_app_icon.ns, make_app_icon.py:
Newton pixels on a green rounded square). Signing for other Macs:
SIGN_IDENTITY="Developer ID Application: Matthias Melcher (BK4ST6N599)"
and NOTARY_PROFILE=<notarytool profile> for build_app.sh (hardened
runtime: a whole game runs with it). Windows are 1.5 times FLTK's scale
(ScaleScreens, Links.cc). XOR text goes through an image (DrawInverted):
CoreText ignores the blend mode (protoTitle's title had vanished).

VSNewt 0.2.0 is released (2026-09-28, github.com/MatthiasWM/VSNewt,
tag v0.2.0): newtc with FLTK, built from 9ac9d0d by
Matt/tools/build_vsnewt_newtc.sh (Release, FLTK on); VSNewt's tests with
NEWTC set (11) and a whole Battleship game passed with that binary.

Last commit 6f83ae5 (10.7e, most of it). Next: the rest of 10.7e (see its
open items), then 10.7f and 10.7g. Builds: `build/VSCode` (FLTK, the
default for the tests) and `build/Release` (no FLTK; pass the runner an
absolute `--newtc`). All suites: `python3 Test/dbg/run_dbg_tests.py`
(96 with FLTK; 75 + 21 skipped without), then `test_nsdbg.py`,
`test_dap_extras.py`, `test_terminal.py`, `test_store.py`.
Working on Battleship (the package: see 10.7):
- `newtc -pkg Battleship.pkg -odecompile bs.ns`: its source, to read.
- A `-script` after `-pkg ... -run` runs once the app is open: walk
  `GetRoot().|Battleship:ATOW|:ChildViewFrames()` and print text,
  viewJustify, `:GlobalBox()`; `TestSnapshot(view, "x.png")` saves a
  picture to compare with Matt's Screenshot1-5.jpg (repo root, not in
  git). Top-level `local`s don't carry over between statements: wrap the
  script in a func. An uncaught exception in a script leaves the window
  open (newtc then waits): run with a timeout.
- A ROM function's literals (`fn.literals`) tell which natives it calls:
  that is how the DrawXBitmap caller was found.
- `-stubs throw` or `-stubs report` for the stubs a run calls.

### Steps

- [x] 10.1 The event loop (details in HISTORY.md; rules under Reference).
- [x] 10.1b `installPackage()` follows the ROM (RegisterNewPackage,
      InstallPart): DoNotInstall in a try, per-part error handling with the
      ROM's messages; install script as InstallFormPart does it, without
      EnsureInternal (replaced by the ROM's InstallPart in 10.3).
- [x] 10.2 The root view (Host/Root.{h,cc}; not FLTK-specific, so -dap
      without FLTK has one too): `GetRoot()` returns the root view's frame,
      the same one every time (a GC root), writable; `root:Notify(level,
      title, message)` prints "title: message" (the Debug Console with
      -dap; an FLTK alert later). The ROM's NewtonScript works with it:
      InstallFormPart refuses an app that is already there through
      Notify. installPackage() reports its errors through it, too. The
      GetRoot stub is gone. Test harness: `<name>.after` holds arguments
      after `-script <name>.ns` (e.g. `-run`). Tests: `root_view`,
      `install_parts`.
      Needed later (then, not before): the Extras drawer
      (`GetRoot().ExtrasDrawer`): GetAppName asks it when an app's base
      view has neither appName nor title, and InstallPart tells it about
      built-in and 1.x packages.
- [x] 10.2b Debugging after EnsureInternal. Package objects stay read-only
      as on a Newton, so EnsureInternal copies their code (the ROM's
      InstallFormPart copies the install script); line tables follow the
      copies. New hook `gClonedFunctionHook(original, copy)` (Objects.h, not
      in ROM), called by DeepClone, TotalClone, and EnsureInternal when they
      have copied a function with its slots. LineTables (`FunctionCopied`):
      if the original's code has a line table and the copy's code is new,
      the copy gets it (its own lineTable slot is copied along; a debug
      map's table is registered for the copy) and CodeForLine finds the
      copy; then `gCodeCopiedHook()`. With -dap it asks for a poll before
      the next instruction (DebuggerPollNow), and DAPPoll calls
      `DAP:CodeCopied()`, which installs the line breakpoints again, in the
      copies too, before they run. (Shallow Clone shares the code: nothing
      to do.)
      Fixed on the way (both porting slips, checked against the ROM):
      `DeepClone1` wrote the cloned slots into the original instead of the
      copy, so DeepClone changed its argument (e.g. Stores.cc deep-clones
      the ROM's canonicalCardInfo); `TotalClone1` added the original, not
      the copy, to its list of clones, so an object reached twice (or a
      cycle) led back to the original.
      Tests: `clone_copies`, `dap_copied_code` (a breakpoint in a function
      stops in its TotalClone, next goes on by line in the copy),
      `test_nsdbg.py` (the ROM's own InstallFormPart installs the decompiled
      Hello package: the breakpoint in hello.ns stops in the copied
      InstallScript).
- [x] 10.3 `BuildContext(template)` (Host/Views.{h,cc}; the stub is gone):
      the view frame for a template, as CView::buildContext and the ROM
      (FBuildContext builds even a hidden view): a clone of the ROM's view
      frame (magic pointer 29: `_parent`, `_proto`, `viewCObject`; 31 adds
      `realData` for data views), or the template's own `_cacheContext`;
      `_proto` the template, `_parent` the root view; a stationery
      (`viewStationery`, e.g. 'para, or ink) gets its form from `stdForms`
      (view classes with bit 0x10000 keep the template as realData). No
      native view: `viewCObject` stays nil until the view is opened.
      `installPackage()` now hands each 'form and 'auto part to the ROM's
      own `InstallPart`, with the install info Packages/Parts.cc makes (a
      clone of canonicalFramePartInstallInfo; no store: ids and types 0, no
      packageStyle, so the Extras drawer isn't asked). So the ROM installs
      the form part: devInstallScript before InstallScript, EnsureInternal
      copies (debuggable since 10.2b), root.(app) := BuildContext(theForm),
      the "already installed" check, errors reported through Notify (the
      ROM catches an exception in a part and shows only its message; with
      -dap the debugger stops at the throw first).
      Tests: `install_hello` (-hello -run: GetRoot().|hello:SIG| is the base
      view, not open), `install_parts`, `test_nsdbg.py` (breakpoints in the
      decompiled Hello package, installed by the ROM).
- [x] 10.4 Open and close (first part). Research: `@180` is protoFloatNGo
      (a protoFloater with a protoClosebox child, `@166`); the Hello text is
      protoStaticText (`@218`). The view methods are in the ROM's root view
      template `@287` (viewClass 75): ROM NewtonScript (Open, Toggle, ...)
      and natives (`_Open` is FOpenX, `Close` is FCloseX, Hide, Show, Dirty,
      SyncView, ...); a view finds them through its _parent chain, so the
      root view's frame now has `_proto: @287` (the built-in apps' templates
      come along as slots; nothing is built). The ROM's Open checks the
      orientation (displayParams) and calls `self:_Open()`; FOpenX calls
      RealOpenX, which has the parent add the child (CView::init); FCloseX
      queues a close command (CView::dispose later; newtc closes at once).
      Host/FLTK/ (namespace nfl, only with NEWTC_USES_FLTK): `nfl::Link` (a
      view's link: the context as a RefStruct, parent and children, global
      bounds, the widget; viewCObject = AddressToRef(link) as in the ROM,
      widget user_data() = link); `WindowLink` for a child of the root view
      (widget `nfl::FloatNGo`, an Fl_Double_Window; closing it sends the
      view Close() through SendEventMessage), `ParagraphLink` for
      clParagraphView (an Fl_Box with the text), `ViewLink` for everything
      else (an Fl_Group for the children). Open follows CView::init:
      viewSetupFormScript, bounds (viewBounds in the parent's bounds by
      viewJustify; for a root child the app area; parent justification
      only), declareSelf, the widget, viewSetupChildrenScript, the children
      (viewChildren then stepChildren, each BuildContext'ed and opened),
      viewSetupDoneScript; a window is shown, viewShowScript. Close follows
      CView::dispose: viewQuitScript, viewCObject nil, the children,
      viewPostQuitScript on 'postQuit; each link deletes its own widget
      directly (groups and windows don't delete children, see GroupLink).
      View scripts are found through _proto only (CView::runScript). An
      error while opening closes what was built and passes the exception
      on. FOpenX and FCloseX live in Host/ViewMethods.cc (since 10.4b);
      without FLTK they are stubs. `installPackage()` opens the form part's app right after
      installing it (`GetRoot().(app):Open()`), for -run and -dap. The
      window sits at (100, 100) plus its position on the Newton display.
      `DoProtoMessage`, `DoProtoMessageIfDefined` are now declared in
      Frames/Funcs.h. Test helper `TestCloseWindow(view)` (Matt/TestWindow):
      presses the view window's close button from the event loop.
      Stubs met: GlobalBox (protoFloater's viewSetupFormScript uses it to
      keep a floater on the screen), ChildViewFrames. The debugger names
      the ROM's view methods after @287's debug slot ("646.Open"): a better
      name would help.
      Tests: `fltk_views` (the script order, open and close),
      `fltk_hello_window` (-hello -run opens a window; its close button
      closes the view and ends newtc), `fltk_dap_view` (a breakpoint in a
      viewSetupFormScript), `install_hello` (now the same with and without
      FLTK), `test_nsdbg.py` and VSNewt close the app's window at the end.
- [x] 10.4b The view natives and justification. Host/ViewMethods.{h,cc}
      holds the view methods that are natives (FOpenX and FCloseX moved
      there from Host/Views.cc): GlobalBox, LocalBox, ChildViewFrames, Hide,
      Show, Dirty call nfl:: functions in Links.cc; without FLTK they are
      stubs (their NTKStubs.cc stubs are gone). As the ROM: a closed view
      throws "nil view" (FailGetView), except Close and Dirty (nil).
      GlobalBox and LocalBox while viewSetupFormScript runs justify the
      viewBounds as they are then (CommonBox, the setup-form flag). Hide and
      Show run viewHideScript and viewShowScript only when the state
      changes; the ROM queues both as commands, newtc does them at once.
      Justification (`Justify()` in Links.cc) follows the ROM's
      TView::JustifyBounds: parent, previous sibling (vjSibling*: the
      sibling's bounds replace the parent's for that direction), and ratios
      (percent of the sibling's or parent's size). The port only declares
      CView::justifyBounds; the code follows the ROM.
      The Hello app no longer meets a stub. Test `fltk_view_methods`.
- [x] 10.4c Buttons and the close box. Pen: a pen down (FL_PUSH) on a
      view with vClickable sends viewClickScript(unit) (through _proto
      only; the unit is nil for now); Link::HandlePen, from the widgets'
      handle(). TrackHilite(unit) (FTrackHiliteX) waits in a nested
      Fl::wait() loop while the pen is down, as the ROM waits in its own
      loop, hilites the view while the pen is inside, and returns true if
      it came up inside; Hilite(on) (FHiliteX) does nothing on a closed
      view, as the ROM's (the close box's buttonClickScript closes the view
      before its viewClickScript unhilites it). The ROM's scripts do the
      rest: protoTextButton (@226) and protoPictureButton (@198) call
      TrackHilite, buttonClickScript, Hilite(nil); protoClosebox (@166, via
      protoLargeClosebox @163) closes `base`.
      Host/FLTK/Widgets.{h,cc}: nfl::TextView (clTextView 98: text, font,
      the box of its viewFormat: a rounded black frame is Matt's FL_UP_BOX,
      hilited FL_DOWN_BOX with a white label) and nfl::PictureView
      (clPictureView 76: its icon, a Newton bitmap drawn as an Fl_Bitmap;
      hilited inverted). viewFont: Helvetica (Espy, Geneva) or Times (New
      York), bold and italic, the Newton size; ParagraphLink uses it too.
      Test helper `TestTap(view, outside)` (Matt/TestWindow): taps one
      after the other through Fl::handle(). Tests `fltk_buttons` (tap,
      tap and slide out, TrackHilite both ways, the close box closes the
      app), `fltk_dap_button` (a breakpoint in a buttonClickScript after
      the nested loop).
      Not yet: buttonPressedScript, the click sound, units (strokes), a
      default (Enter) button. Close is immediate (the ROM posts a command).
      Frames: NewtonOS draws a view's frame outside its viewBounds. A
      window is bigger than its view by `FrameOutset(viewFormat)` all
      around: kDraggerBorderWidth (8) for a dragger frame (vfDragger 13,
      protoFloatNGo), else pen + inset; children sit relative to the
      content (Link::WidgetX/WidgetY). Checked against Matt's screenshot of
      hello.pkg on a Newton: window 168x112, the close box 13x13, its top
      left 22x23 from the window's bottom right. The close box is the ROM's
      13x13 bitmap (blocky on Retina; all graphics need a 2x look).
      Test helper `TestSnapshot(view, path)`: the view's window as a PNG
      (fl_capture_window, fl_write_png; screen resolution). It waits until
      the window is on the screen, so don't close the window in the same
      breath.
- [x] 10.5 The Hello app with a button and a modal alert. `-hello` is
      now Einstein's Hello: a protoFloatNGo with a "Say Hello" button whose
      buttonClickScript calls ModalConfirm("Hello World of
      NewtonScript.\n\nHow exciting to see you!", ["OK"]); its
      viewQuitScript prints "Goodbye" (install_hello switches that off: its
      output must be the same without FLTK). ModalConfirm is the ROM's: it
      builds the alert @544 (a clPictureView, its message, a row of
      protoTextButtons placed by sibling justification) and calls
      ModalDialog. Natives: ModalDialog (FModalDialog: opens the view as a
      modal window, set_modal(), and waits in RunModalEventLoop until it is
      closed), SetupIdle (FSetupIdleX: viewIdleScript on an Fl timeout,
      again after the milliseconds it returns; the alert's gyre uses it),
      SetBounds, OffsetRect, StrFontWidth (Host/Graphics.{h,cc}; with
      FLTK's font metrics, for StdButtonWidth). Paragraph text: CR (a
      Newton "\n") is a new line. Every view's widget is its bounds plus
      its frame around them (FrameOutset: pen + inset; NewtonOS draws
      frames outside viewBounds), and the text or picture goes inside the
      frame (ViewWidget::FrameInset). Checked against Matt's measurement of
      an alert's OK button on a Newton: 25x17 = StdButtonWidth - 1 + 2x2
      pen, 13 + 2x2 (with Espy's "OK" 16 pixels wide; Helvetica's 14 gives
      23x17 here). A window that isn't a dragger gets the box of its
      viewFormat. The ROM's alert picture (@13) is a PICT v1: one 208x154
      1-bit PackBitsRect (the whole alert).
      Event loop (Matt/EventLoop): RunModalEventLoop() lets events run
      their scripts on top of the waiting one (as NewtonOS); the DAP
      requests stay an event source. DAPLeaveScript(inOutermost): the end
      of an event script inside a modal loop doesn't cancel the waiting
      script's line step (found while testing: "next" over ModalConfirm ran
      on). TestTap(view or text, outside): taps queue up and the next one
      starts once the pen of the one before is up (inside a modal loop
      too); a string taps the button with that text.
      Tests: `fltk_modal` (ModalConfirm with ["OK"] and 'yesNo, SetBounds,
      OffsetRect, StrFontWidth), `fltk_hello_alert` (-hello -run: Say
      Hello, OK, the close box), `fltk_dap_modal` (a breakpoint after the
      alert), `fltk_dap_modal_step` (next over ModalConfirm); test_nsdbg.py
      and VSNewt count the package's functions instead of expecting 2.
      Not yet: the alert's look (its icon @13 is a QuickDraw PICT; 10.5b
      draws it), DoDrawing (the gyre), SetKeyView and keys (the
      default button, Return), Close as a posted command.
      Note: NewtonScript's escapes are \n (CR), \t, \\, \", \u; "\r" is
      an "r" (Apple's compiler; newt/0, which Einstein's samples were
      written for, also knows \r).
- [x] 10.5b The ROM's graphics as PNGs, compiled in.
      `Host/FLTK/Images/extract_rom_images.py path/to/newtc` has newtc dump
      every magic pointer that is a picture (a PICT: 4, all version 1 with
      one PackBitsRect: @13 the alert frame, @74 its gyre, @320 the light
      bulb, @321 the world map) or a bitmap frame (144 icons, 1 bit) and
      writes rom_NNNN[_name].png (NNNN the magic pointer index, the name
      from Frames/MagicPointers.h): PICTs 8-bit gray, opaque; icons gray and
      alpha (transparent where no bit is set; the 'mask slot isn't used).
      27 KB in all. cmake/EmbedImages.cmake compiles them into a table
      (build/.../generated/RomImageData.cc), Host/FLTK/RomImages.h finds one
      by magic pointer index (RomImage(13); inverted for hiliting). A
      picture view whose icon is one of the ROM's (a magic pointer, e.g.
      protoClosebox's @334) draws its PNG; a window-like one draws it
      behind its children (the alert: its wavy frame). Every image is drawn
      at the ROM object's size (a PICT's frame, an icon's bounds;
      Fl_Image::scale()), so a PNG can be replaced by one of any resolution
      and depth (tried: the alert frame at 416x308, 8-bit gray);
      extract_rom_images.py keeps existing PNGs (--force: all again).
      Found and fixed on the way: B17 (the interpreter's stacks were 4 KB).
      Not yet: DoDrawing('CopyBits, [@74, 0, 0, 2]) (the gyre: @74 XORed
      over the alert every 300 ms, 12 times). Icons in alerts: ModalConfirm's
      has none; Notify's alerts do (the alert's messageHIndent makes room):
      when Notify shows alerts, find their pictures in the ROM and draw them
      with RomImage().
- [x] 10.6 What Battleship needs (nBattleship 1.4, corpus
      `unna2/development/source/nBattleship1.4/nBattleship 1.4 Code/`).
      Two ways to count. Static: `Test/stub_census.py newtc pkg [--depth N]`
      reads the package's code (and the ROM NewtonScript and protos it
      reaches) and asks newtc what every global function and view method
      is: stub, native, ROM NewtonScript, undefined (`StubName(fn)`: each
      NS_STUB now records its function, Utilities/Unimplemented.h). Of 991
      stubs, Battleship reaches 18 directly, 23 at depth 3 (Hello: 13, all
      through ROM code: dragging, keys). Dynamic: running it (with a fake
      System soup, below) until the next wall. Fixed on the way, since
      they stopped it: allocateContext/stepAllocateContext and
      preAllocatedContext (TView::Constructor, TView::AddView: children's
      view frames made before viewSetupFormScript; protoLabelPicker),
      Parent (FParentX), GetViewFlags (FGetFlags; Visible() uses it),
      SetValue (was a printf in ObjectSystem.cc; now sets the slot and the
      open view shows text and bounds), FontHeight, Random (B18), randomx.
      Now its setup dialog opens, Play starts a game and shows the map.
      Tests `fltk_view_values`, `random_numbers`, `stub_name`.
- [x] 10.7 Battleship, in this order (each a step of its own; it plays to
  the end, 2026-09-28; what is left of 10.7e and 10.7f is in CLAUDE.md,
  "Backlog"):
  - [x] 10.7a Soups. The hybrid: Apple's soup code (soups, entries,
        B-tree indexes, cursors, union soups, package stores; ported) stays;
        only the internal store's bottom changes. `Stores/HostStore.{h,cc}`
        is a CStore in memory (a map from PSSId to bytes; root id 39 as
        CFlashStore) instead of the emulated flash chip (CFlashStore,
        blocks, drivers: no longer used by newtc; `NEWTC_HOST_STORE` in
        PSSManager.cc). Transactions as CFlashStore: a main one
        (lockStore/unlockStore/abort) and objects in transactions of their
        own (startTransactionAgainst, newWithinTransaction, separatelyAbort,
        addToCurrentTransaction), both by keeping objects' contents before
        the change. `-store <file>` keeps it in a file (read at the start;
        the committed state written to a temporary file and renamed, at
        each commit and at exit); without it, a new empty store each run
        (as before: tests). Fixed on the way: B19 (StoreGetSoup's kludge;
        the endless loop is gone), B20 (EntryChange threw once a cursor
        existed), B21 (an uninitialized flag in CUnionSoupIndex), B22
        (validTest kept the entries it should drop). The ROM's start now
        sets printLength to 16 (it reads the System soup): newtc resets it.
        Battleship opens on real soups and keeps its settings with -store.
        Tests `soups`, `Test/dbg/test_store.py`. Not done: import/export as
        JSON (no windfall: Matt/JSON.cc covers what DAP needs, not all
        NewtonScript objects). Careful when testing: `[c:Entry()]` is an
        array of class 'c; and `=` on strings compares identity.
  - [x] 10.7b Pen strokes. A unit is a host stroke (Host/FLTK/Pen.h,
        nfl::Stroke; AddressToRef, as the ROM's UnitFromRef): its points on
        the Newton display from pen down to up, added by the pen events on
        the view's widget (Link::HandlePen; screen positions, so they hold
        while a window moves); the last 8 strokes stay valid, an older unit
        is "nil unit". Natives (Host/Pen.{h,cc}): GetPoint (selectors 0-8),
        GetPointsArray (y, x: Battleship takes points[0] as the row; the
        port wrote x, y: B23), GetPointsArrayXY, StrokeBounds, StrokeDone,
        InkOff/InkOn (no ink drawn yet), GetUnit*Time. Asking about a stroke
        that goes on lets FLTK handle waiting events (a script may poll
        StrokeDone). Drag(unit, bounds) (FDragX): the view follows the pen
        in a nested loop, within bounds, and ends where the pen left it
        (Link::MoveBy: bounds and widgets of the view and its children; a
        window moves on the desktop). protoFloatNGo drags by its dragger
        (top center, in the frame; the ROM's DragWindow; nfl::FloatNGo
        passes pen events to its link now); RelBounds. Fixed: a group
        widget didn't get FL_RELEASE (FLTK clears Fl::pushed() first). The
        picker diamond U+FC01 shows as U+25C6 (DisplayText). Test helpers:
        TestDrag(view, dx, dy), TestPen(view, x, y, dx, dy); TestSnapshot
        waits for the taps before it; synthetic pen positions are screen
        positions. Test `fltk_pen`. Battleship: ships drag and snap into
        the grid. Not yet: ink, gestures (viewGestureScript: a quick tap
        on a ship turns it by 90 degrees, gestureKind 49; Matt: FLTK has
        Fl::event_is_click() for a tap), recognition.
  - [x] 10.7c Shapes and drawing. Host/Shapes.{h,cc}: shapes in the
        ROM's formats (16-bit big-endian; checked against the ROM; its
        canonical frames): MakeRect, MakeOval, MakeRoundRect, MakeWedge,
        MakeLine (y1, x1, y2, x2; the port swapped them: B24), MakePolygon,
        MakeText, MakeShape (an icon, a PICT, a polygon), MakePict (here
        [style, shapes]; a PICT on a Newton), OffsetShape, ShapeBounds,
        IsPrimShape, PointsToArray, ArrayToPoints; also without FLTK.
        Host/Pict.{h,cc}: PICTs of bitmaps (version 1, BitsRect,
        PackBitsRect) to a 1-bit bitmap: picture shapes, and picture views
        whose icon is a PICT (Battleship's ships). Host/FLTK/Drawing.{h,cc}:
        DrawShape, DoDrawing, viewDrawScript; styles (penSize, penPattern,
        fillPattern: grays, or an 8x8 pattern's gray; transferMode copy and
        or; transform [dx, dy]; font; justification; style frames in lists).
        A view's canvas: an Fl_Image_Surface (high res; Matt: the right
        tool) with what scripts drew and one with a mask of which pixels;
        the widget draws itself and its children, then the canvas on top
        (as on a Newton, over the children); a Newton-side change drops it
        (Link::DropCanvas). (A canvas made by drawing the widget into it,
        Fl_Widget_Surface::draw, came out shifted; the mask is better
        anyway.) A viewDrawScript runs before the children, also while a
        script runs (a redraw in a nested loop). Fixed: FloatNGo drew its
        frame at its screen position (draw_box() uses x(), y()). Test
        helpers TestLater(fn), TestPixel(view, x, y); SendEventCall (a
        function as an event). Tests `shapes`, `fltk_drawing`. Battleship:
        both maps with their grids, the ships as pictures, the shots.
        Not yet: transferMode xor, bic and the others (FLTK has no raster
        ops; Matt: left out on purpose; pixels by hand if an app needs
        them), CopyBits (the alert's gyre), transform scaling, regions,
        ink, a real PICT from MakePict, DrawXBitmap (a stub Battleship
        calls; it was the status bar's clock: done in 10.7e).
        After Matt's screenshots of nBattleship 2.5 on a Newton
        (Screenshot1-5.jpg, not in git): groups draw their viewFormat
        (DrawViewFormat: fill, and the frame outside the bounds, pen wide;
        the maps' 565: black, gray frame), so the game view covers the
        title; picture views draw their icon in their viewTransferMode
        (copy, the default: the 0 bits white; the deploy map's grid on its
        black fill); a dragged view is drawn over its siblings while it
        moves, then back in its place. Grays are solid here; a Newton
        screen shows them as patterns (a 50% dither: the dotted frames,
        the gray shots).
  - [x] 10.7d Timers. Host/Timers.{h,cc}: AddDelayedCall, AddDelayedSend,
        AddDeferredCall, AddDeferredSend, and the 1.x AddDelayedAction,
        AddDeferredAction (the ROM's procrastinated calls are NewtonScript
        on AddDelayedCall). Each runs as an event (SendEventCall,
        SendEventMessage); if a script runs when it is due, it waits for
        it (not dropped). With FLTK: Fl timeouts; without: RunEventLoop
        sleeps until the next is due (also works without FLTK). A program
        without windows runs on while calls wait; one that had windows ends
        when they close (an app's timers may repeat). Calls the ROM
        schedules while newtc starts are dropped (EnableTimers() after
        init): they belong to the built-in apps (the first sets up the
        owner from the Names soup, which newtc doesn't have). FLTK: without
        a window, Fl::wait() returns at once; Fl::wait(time) runs the due
        timeouts first, then waits: so the loop waits until the next call
        at most. Ticks are 60 a second (B25). Test `timers` (both builds).
        Battleship: a whole turn, the computer answering after its delay.
  - [x] 10.7e Pickers and the settings' details.
        Done: DoPopup (Host/FLTK/Popup.{h,cc}): the items (strings;
        frames with item, mark, pickable nil; 'pickSeparator a divider) as
        an Fl_Menu_Item array at the given place (a bounds frame, or left,
        top in the view), Helvetica bold 12; the pick goes to the context
        after the script (AddDeferredSend): pickActionScript(index in the
        items, separators counted) or pickCancelledScript.
        protoLabelPicker opens its labelCommands with the current one
        checked (U+FC0B shows as a check mark). Paragraphs are TextViews
        (the pen reaches them); a viewClickScript that returns nil passes
        the pen to the parent (the entryLine's to the picker). FontAscent,
        FontDescent, FontLeading (nfl::FontMetric); GetView. TestPick(n)
        answers the next menu (nil: cancel; a queue). Test `fltk_popup`.
        Matt asked why Battleship's window had a plain frame: its
        viewFormat 0x50103F1 is a matte frame (vfMatte, pen 3, inset 1,
        round 5), which got the button box; matte frames are floaters now
        (FLOATER_BOX and the dragger's outset, as vfDragger).
        The Comm picker showed over Difficulty (vjSiblingFullV: both in
        one place, the app hides one): Hide/Show of a view whose widget
        can't hold children (a paragraph: its children's widgets are in
        the window) now hides and shows them too.
        protoCheckbox: its UpdateBitmap sets the icon slot and calls
        Dirty(): a picture view reads its icon again on Dirty (and on
        SetValue 'icon); its clTextView child has no text or viewFont of
        its own and finds the checkbox's (NewtonScript's lookup goes up
        the _parent chain: TextLink uses GetVariable). The dashed square
        is the ROM's offBitmap, as on a Newton.
        DrawXBitmap(bounds, strip, index, mode), a global function that
        draws on self (4 arguments; the port's stub had 3): the index-th
        bounds-sized cell of a strip, mode as transferMode (nil: copy). The
        status bar's clock draws its face and hands with it (smallClocks:
        24 cells of 17 by 17). DrawShape and DrawXBitmap share DrawOnView
        (onto the widget in a viewDrawScript, else canvas and mask).
        Test `fltk_view_details`.
        Matt: Newton frames and boxes are box types of their own (nfl::UP_BOX,
        FL_UP_BOX stays FLTK's); DoPopup's place under the view; menu
        styles closer to a Newton's; check marks through FLTK; an item's
        icon (a bitmap frame, GetPictAsBits) through an Fl_Multi_Label; a
        floater's link follows its window (FloatNGo::resize,
        Link::UpdatePosition). Helpers: ToNewtonBitmap (a bitmap frame,
        its bits, or a PICT) and ToFlImage (Widgets.h), used by pictures,
        drawing and menus. TestMenuSnapshot(path): the next menu shows for
        real, saved as a PNG, then cancelled.
        Frames (Matt's design): the link keeps the viewFormat
        (Link::ViewFormat(), read at open, updated by SetValue: a new frame
        width re-lays out the widget and its children, Link::Layout());
        every view widget (Group, TextView, PictureView, FloatNGo) calls
        DrawViewFormat(x, y, w, h, viewFormat, hilited) first in draw();
        their box is VIEW_BOX, a box type that draws nothing (not
        FL_NO_BOX, which FLTK treats specially). BoxForFormat is gone.
        Geometry as the ROM's CView::outerBounds (Matt's drawing): the
        widget is the bounds, inset + pen around them (only with a pen; the
        pen only with a frame color: FrameOutset), and the shadow at the
        right and the bottom (FrameShadow, Link::Shadow()). The frame is at
        the widget's edge, the inset between it and the bounds; fill under
        it; hilite inverts the fill (black without one). No extra pixels:
        Screenshot1's Play button (48 by 13, pen 2) is 52 by 17 on a
        Newton. The ROM's +3 above and below is only for the default
        button (its keyboard indicator; not yet). Test `fltk_view_format`.
        Fonts (Host/FLTK/Fonts.{h,cc}): the four families as FLTK fonts
        (FONT_SYSTEM ... FONT_HANDWRITING, four faces each): System (Espy
        Sans) and Simple (Geneva): Geneva, bold and italic Verdana (Geneva
        10 and Verdana Bold 10 measure as Espy Sans' bitmaps: "Turn Speed"
        55 and 64, Newton 55 and 63); Fancy (New York, which macOS keeps
        for its UI): Times New Roman; Handwriting (Casual): Apple Casual.
        Elsewhere FLTK's Helvetica and Times. FontFromSpec knows 'espy,
        'newYork, 'geneva, 'handwriting and integer specs. The link keeps
        viewFont (through the parents, as the ROM's getVar) and
        viewJustify, and gives them to the widget: labelfont, labelsize,
        align (paragraphs: top, wrapped); SetValue changes them (viewFont
        also for the children that inherit it). vjFullH/V: left, top.
        /Library/Fonts/Espy Sans on Matt's Mac is the Newton's bitmap font
        (NFNT 9 to 16, plain and bold): the exact pixels, some day.
        The Newton's Casual 10 is Apple Casual 14 in size (Screenshot1,
        ink: "Medium" 45, "Yes" 19, "Normal" 41; ours now 43, 19, 41):
        Handwriting is drawn and measured 1.4 times its size
        (kSizeScale); FontHeight, FontAscent, FontDescent answer for the
        size asked. System and Simple bold (Verdana Bold) 1.1 times: Espy
        Sans Bold 9 (bitmaps) has capitals 7 high and "Difficulty" 50 wide,
        Verdana Bold 10 7.3 and 51 (9: 6.6 and 46), so Play sits in its
        button as on a Newton; "Turn Speed" gets wider (64, Newton 56).
        A picture view without a viewJustify centers its icon (ROM's
        CPictureView; protoInfoButton).
        Drawing order as the ROM's CView::draw: fill (preDraw), content,
        viewDrawScript, children, then the frame (postDraw):
        DrawViewFormat(..., kViewFill / kViewFrame); FLOATER_FRAME is
        FLOATER_BOX without its fill. protoTitle reaches 3 pixels into the
        app's frame (bounds top -3); the frame now covers it, as on a
        Newton.
        Hilite as the ROM's CView::hilite: a view with a viewHiliteScript
        runs it (true / nil); if it returns non-nil it did the hiliting
        (protoLabelPicker: an XOR round rect over its label only, from -2
        to indent; the second XOR undoes it), else the widget is drawn
        inverted. The picker's bounds are 150 wide
        on a Newton too (its value, entryLine, is inside them); before,
        the default inversion blackened all of it.
        FLTK draws a changed widget with the window clipped to it: our
        window cleared that area and redrew only the changed widgets, so
        views overlapping it (a picker's value over its label) were
        erased. FloatNGo and Group now draw all their children (the clip
        keeps it cheap), as the ROM redraws every view in a dirty area.
        XOR (transferMode 2): Quartz's kCGBlendModeDifference with white
        inverts (kCGBlendModeXOR is Porter-Duff, on alpha: no use); the
        canvas has an invert layer (Link::fInvert, toggled by XOR shapes)
        drawn last in that mode (DrawOverlay); in a viewDrawScript straight
        onto the widget. macOS only: BlendInvert() has an #error for the
        other platforms until FLTK has a blend mode (Matt opens an issue).
        Every view widget is a group now (ViewWidget: Fl_Group): a view's
        children are in its widget, drawn after what it shows, then its
        frame, what scripts drew, and its hilite (the widgets outside in
        the window, ShowOutsideChildren, are gone). The default hilite is
        the ROM's inversion of the bounds grown by the inset, on top of all
        (DrawHilite, difference blend mode): protoCheckbox's label (a child)
        white on black; buttons as before; DrawViewFormat gets no hilite
        from the widgets any more. fl_rounded_rectf() fills a pixel less at
        the left and top than fl_rectf() (FLTK: vertices on x .. x+w-1);
        DrawHilite makes up for it.
        A change of a view drops its own canvas, but in the views it is in
        only clears its area (DropCanvas): a picker's pick sets its value,
        then unhilites; its XOR on the label stays until then. Open: the popup menus' font (Matt's Helvetica Bold 9).
        Test `fltk_fonts`.
        Open: MoveBehind; the floater's dragger (a bump at the top center;
        Matt's box); the handwriting font of the pickers' values (Matt's);
        the status bar's clock overlaps the info button a little.
  - [x] 10.7f The rest: keys (SetKeyView, SendKeyMessage, MatchKeyMessage,
        RestoreKeyView), AddUndoAction (protoCheckbox's ToggleCheck calls
        it), TableLookup, SyncView.
        Done: taps (Links.cc, Recognize): a stroke no viewClickScript takes
        (all return nil) goes to recognition when the pen is up (already,
        if a script waited for it: Drag); a tap (within 4 pixels, up within
        30 ticks) is viewGestureScript(unit, aeTap 49) of the view under
        it if it allows gestures (vGesturesAllowed), else of the views it
        is in, up while they return nil. Battleship turns a ship so (its
        click script drags, then returns nil). A picture view reads its
        icon again on any SetValue and on Dirty of it or a view it is in
        (Reread; a Newton reads the slot when it draws: Battleship sets
        the ship's icon slot, then its parent's Dirty()). Test
        `fltk_gestures`. Other gestures (scrub, caret, lines), words and
        shapes: not yet.
        The store file (Stores/HostStore.cc): a CRC-32 (version 2; 1 is
        read), written whole to .tmp, fsync'd, renamed over the file; the
        first save of a run keeps the old file as .bak (.bak before as
        .bak2); a file that isn't whole is kept as .bad-<time> and the
        backup used. Test test_store.py (9). See B26.
        A text view draws in its viewTransferMode: XOR (2) white in the
        difference blend mode (Battleship's messages: white on black).
        A whole game runs to its end with the Release build (VSNewt's
        newtc, now with FLTK: Matt/tools/build_vsnewt_newtc.sh), no stub
        called (-stubs report: 0 of 945). (FontAscent and GetView: 10.7e;
        RelBounds and InkOff: 10.7b.)
  - [x] 10.7g Hidden stubs: port functions that only print or return nil
        without NS_STUB (SetValue was one; `-stubs report` can't see them).
        Test/hidden_stubs.py reads every native newtc is built from (a C
        function Ref F...(RefArg rcvr, ...); files from
        build/VSCode/compile_commands.json) and lists those whose body only
        returns a constant without its arguments, prints without them, or
        says it isn't done; checked ones are in its REVIEWED list. Found
        and done (2026-09-29): FDeinstallPackage an NS_STUB;
        FRefreshViews draws now (Fl::flush, Host/ViewMethods.cc);
        FindLocaleBundleByName was empty (SetLocale("United Kingdom") did
        nothing): the ROM's FindLocale; which uncovered B27 and B28 (number
        strings). FBatteryCount (one battery) and FSoundCheck (nothing in
        the ROM either) are right. Now 0 of 501 natives; all 515 of
        NS/plainC.txt are there. The script sees only the obvious ones: a
        native that does part of its job still needs -stubs report and use.
        Test `numbers_locales`.
- Later: move EventLoop/TestWindow into `Host/FLTK/`; the FLTK layer as a
  library; Windows and Linux.

## Bugs fixed

Moved here from the bug list in Matt/CLAUDE.md when fixed.


Interpreter and runtime
- [x] B1 **Recursion is broken in NOS 1 code** (`-nos1`, or a `//! -nos1`
  script; no longer the default). A recursive `Fib(n)` returns `n-1`; even
  `if n < 2 then return n` gives wrong values. Looks like the NOS 1 argFrame
  (locals) is shared instead of copied per call. NOS 2 code is correct. It
  still matters: a 2.x ROM also runs NOS 1 packages.
  Fixed (2026-09-26): not the argFrame (callCodeBlock clones it), the
  stack: ROM `TInterpreter::CallCodeBlock` pops the arguments while it
  moves them into the argFrame and sets stackFrame to 3 below where they
  were, so return leaves the result there. The port copied them without
  popping and used the old top, so return put the result above the
  arguments: every NOS 1 call left its arguments on the caller's stack
  (`100 + Id(5)` failed, recursion gave wrong values). Tests `nos1_calls`,
  `debugapi_temps_nos1`.
- [x] B2 **NOS 1 CodeBlock frames in `CNSDebugAPI::stackStart()`**: its
  `stackFrame` is the stack top at call time (args stay on the stack), yet
  stackStart adds 3 like for NOS 2 functions. Verify against the ROM and a
  test (temps of/above a NOS 1 frame); may be related to B1.
  Fixed with B1 (2026-09-26): with the ROM's stackFrame, +3 is right for
  NOS 1 frames too (FunctionStackSize is 0 for them: "Newton 1.x does not
  stack args"). Test `debugapi_temps_nos1` (a stopped NOS 1 frame has
  exactly its two pushed temps).
- [x] B4 **Sorted array set operations**: `GenOrderedSetOp`
  (Frames/SortedArrays.cc, behind `BDifference`, `BIntersect`, `BMerge`)
  divides a `Ref*` difference by `sizeof(Ref)`, as `LSearch` did (lines
  ~956-968): copies too few elements, truncates the result.
  Fixed (2026-09-26), with B5: the loop copied and advanced in one step
  (`*r4++ = *r6++`), and only when the element was to be copied, so an
  element to skip was never passed (BDifference hung on the first equal
  pair); the duplicate loop of array 2 compared against an unset value and
  used the operation code as its end flag. Rewritten from the operation
  bits (advance / copy / over duplicates, per array), whose tables
  (GOSOP_Merge, GOSOP_Intersection, GOSOP_Difference) and the uniqueOnly
  handling match the ROM. Note (ROM behaviour): without uniqueOnly,
  BIntersect keeps the matching elements of both arrays ([2, 2, 4, 4]).
  Test `sorted_arrays`.
- [x] B5 **`BMerge([...], [...], '|<|, nil, nil)` hangs.** (2026-09-26: it
  returned `[]` by now; fixed with B4.)
- [x] B6 **`LSearch(["x","y"], "y", 0, '|str=|, nil)` returns nil** (the general
  test path, `CGeneralizedTestFnVar`). Fixed (2026-09-26): the fast path for
  a symbol test without a key only knew `'=` and returned "not found" for
  any other symbol; now only `'=` takes it. Test `sorted_arrays`.
- [x] B16 **A closure over a `for` loop variable is a syntax error**:
  `for i := 0 to 2 do begin local g := func() i; ... end` gives -48601
  "syntax error" (with a copy, `local k := i; func() k`, it works). Check
  whether the ROM/NTK compiler refuses this on purpose (the loop keeps
  hidden locals i|limit, i|incr) or whether it is a porting bug.
  Resolved (2026-09-26): on purpose. The ROM compiler has the same check
  and message ("can't close over a for-loop index variable"): the loop
  increments its variable with IncrVar, which only works on stack locals.
  What was wrong: the message never showed. The REPL's error report now
  prints the compiler's message after the error text (REP.cc
  PrintErrorDetail; not in the ROM, which printed only "syntax error"),
  also for parse errors ("syntax error -- read ..., but wanted ..."); this
  one no longer carries the parser's stale state, and its apostrophe is
  ASCII (the typographic one cut the message off). Test `for_closure`.
- [x] B26 **A store that stops Battleship from starting** (Matt, 2026-09-28:
  six times in his tests, build/Battleship.store had to be deleted before
  the app launched again; it hung). No file kept yet. Likely a soup state
  that sends Apple's index or cursor code (Stores/) into a loop, as B20 to
  B22 were, rather than a damaged file. Since 2026-09-28 the store file
  has a CRC and is written safely, a run keeps the file it found as
  <file>.bak (the run before's as .bak2), and a damaged file is kept as
  <file>.bad-<time> (HostStore.h). Next time: keep the .store (and .bak,
  .bak2), and pause newtc in VS Code while it hangs (the stack shows the
  NewtonScript that loops; lldb for C++).
  Explained 2026-09-29 (NewtPlay, 11.1): nBattleship 2.5 has the same
  package name, Battleship:ATOW, so it shares 1.4's store, and saves
  settings 1.4 can't read (type: 1, a second Comm choice: 1.4's picker has
  one, and its textSetup indexes past it: "Index out of bounds"). Not a
  newtc bug: a Newton going back from 2.5 to 1.4 would stop the same way.
  NewtPlay now says so and offers to start with new data (the store kept
  as .old-<time>).
- [x] B28 **Negative numbers and 0 are "too small" to print** (Locales.cc,
  NumberString: `inNum < DBL_MIN`, the smallest positive double, where
  the ROM means `-DBL_MAX`): `NumberStr(-2.25)` and `NumberStr(0.0)` gave
  garbage, `FormattedNumberStr(-1234.5, "%.2f")` "Number too small".
  Fixed 2026-09-29 (10.7g), with it B15's reals (`SPrintObject(3.5)`).
  Test `numbers_locales`.
- [x] B27 **The number pattern is garbled, and freed wrongly**
  (Locales.cc, PositiveNumberProtoStr: the decimal point was copied over
  the "^0", and the pointer kept was past the start of the string, which
  free() got when the locale changed: AddressSanitizer). Found when
  SetLocale("United Kingdom") began to work (FindLocaleBundleByName was
  empty: now the ROM's FindLocale). Fixed 2026-09-29 (10.7g). Test
  `numbers_locales`.
- [x] B25 **Ticks are 50 a second** (ObjectSystem.cc, GetTicks: ms / 20); a
  Newton's are 60 (the ROM's procrastinated calls compute Ticks() + ms *
  60 div 1000). Found and fixed 2026-09-28 (10.7d); test `timers`.
- [x] B24 **MakeLine swaps x and y** (Graphics/Shapes.cc, the port; not
  compiled by newtc): the ROM stores y1, x1, y2, x2 (a Rect's top, left,
  bottom, right); the port put x1 in top. Found 2026-09-28 (10.7c) in the
  ROM's FMakeLine; fixed in the port; newtc's own (Host/Shapes.cc) as the
  ROM.
- [x] B23 **GetPointsArray gives x, y** (Recognition/Unit.cc, the port; not
  compiled by newtc): it wrote h, v, though its comment says v, h and
  GetPointsArrayXY exists for x, y; Battleship's map takes points[0] as the
  row. Found 2026-09-27 (10.7b); fixed in the port, and newtc's own
  (Host/FLTK/Pen.cc) gives y, x.
- [x] B22 **validTest is inverted** (Stores/Cursors.cc, CCursor::validTest):
  an entry was dropped when validTest returned non-nil, the entries it
  should keep. Found 2026-09-27 (10.7a); fixed; test `soups`.
- [x] B21 **An uninitialized flag in CUnionSoupIndex** (Stores/Indexes.cc):
  fIsForwardSearch was never set; UBSan: "load of value 190 ... not a
  valid value for type 'bool'". Found and fixed 2026-09-27 (10.7a).
- [x] B20 **EntryChange throws once a cursor exists** (Stores/Entries.cc,
  EntryChangeCommon): `cursor = GetArraySlot(cursor, i)` instead of
  `cursors` ("ObjectPtr of non-pointer"). Found and fixed 2026-09-27
  (10.7a); test `soups`.
- [x] B19 **Soups are switched off**: `StoreGetSoup` (Stores/StoreWrapper.cc)
  started with `return NILREF;` ("MATT: TODO: KLUDGE: avoid an endless
  loop ... alignment is off occasionally ... 64 bit members"), so
  `GetStores()[0]:GetSoup("System")` was nil although GetSoupNames() listed
  it. Found running Battleship (10.6). Fixed 2026-09-27 (10.7a): the
  kludge is gone, no endless loop any more (Matt's earlier alignment
  fixes); newtc's store is the host store (Stores/HostStore.h).
- [x] B11 **Undefined behaviour when the store is created**: the first run
  with a new HOME (no store in `~/Library` yet) reported
  `Stores/FlashStore.cc:1896:35: runtime error: reference binding to null
  pointer of type 'CStoreObjRef'` (UBSan). Seen with a temporary HOME in
  `Test/dbg/test_terminal.py`. Gone for newtc 2026-09-27 (10.7a): it no
  longer uses the flash store (the MessagePad target still does).
- [x] B18 **Random(low, high) is out of range** (Toolbox/Maths.cc): the port
  computed `lo + rand()/(hi-lo+1)` instead of `%`. Found 2026-09-27 running
  Battleship. Fixed the same day; test `random_numbers`.
- [x] B17 **The interpreter's stacks are 4 KB** (ObjectSystem.cc
  `NewStack`): the port shrank the 64 KB a Newton gives them to 4 KB (512
  values), and nothing checks their bounds (on a Newton they grow in
  virtual memory). Found 2026-09-27 lifting the ROM's images: an array
  literal of 1200 magic pointers, then a native call: AddressSanitizer
  heap-buffer-overflow in `RefStructStack::fill()`. Fixed (2026-09-27):
  the full 64 KB. Test `big_array_literal` (fails without the fix).
- [x] B13 **The REPL prints strings unescaped**: `Print("a\"b\\c")` shows
  `"a"b\c"` (`SafelyPrintString`, Frames/ObjectPrinter.cc, marked "not
  complete yet"): `"`, `\` and control characters (CR, LF, tab) are not
  escaped, so the output is no valid NewtonScript. Matters for DAP variable
  values (5.4). Check what ROM `SafelyPrintString` does.
  Fixed (2026-09-26): the ROM doesn't escape either (it only converts to
  ASCII in chunks of 250), so this is a deliberate difference: `Print` (and
  so the debugger's values) shows a string as NewtonScript source, escaped
  like the decompiler writes strings (`\"`, `\\`, `\n` for CR, `\t`,
  `\uXXXX\u`); new `PrintQuotedString` in Frames/ObjectPrinter.cc. `Write`
  still prints the plain text. Tests `print_escapes`, `json`.

Decompiler
- [x] B8 **Drops statements** (NS Debug Tools.pkg, `Ref_270` = `DisasmRange`):
  in `if A then X else if B then Y else begin if C then Z; <more statements>
  end`, the decompiled source ends after `if C then Z`; the bytecode (pc 52-96:
  a second `if` and the call to `Disassemble`) is missing. See
  `newtc -pkg ... -debug bc -decompile`, search for "DisasmRange".
  Fixed (2026-09-26): the AST was right; CFIfThen::Print printed an else
  branch that starts with an `if` as `else if` and printed only that first
  node. The shortcut is now used only when the `if` is the whole branch.
  300 corpus packages: 816 statement lines are back, none lost (every
  statement of the old output is in the new one), the outputs still compile
  as before (71 of the 73 changed ones, both before and after), and the
  debug maps agree (nsdbg_check, 60 packages: 962/962 functions,
  4449/4449 lines).
- [x] B9 **Loses parentheses** (NS Debug Tools.pkg, `StepIn`): an `if` without
  `else` whose condition is `A or B` is printed as `A or B and X`, which means
  `A or (B and X)`. The bytecode (pc 160-207) evaluates `A or B` first, then
  branches.
  Resolved (2026-09-26): the output was right, `and` and `or` share one
  precedence and go left to right, so `A or B and X` is `(A or B) and X`
  (newtc agrees: `true or nil and nil` is nil). But it reads like C, so
  mixed `and`/`or` are now parenthesized: `(A or B) and X`,
  `(A and B) or C`; chains of one operator stay as they are.
  Fixed on the way: NS Debug Tools.pkg didn't decompile any more (a type
  exception): `IsFunction()` (like the ROM's) only looks at slot 0 of any
  slotted object, so a literals array whose first element is the plain
  function class constant counted as a function. The object printer now
  asks `IsFunctionFrame()` (a frame and IsFunction).

## Early decisions (2026-09-25)

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
