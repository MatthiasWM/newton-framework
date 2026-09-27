# newtc: NewtonScript in VS Code, and a Newton on the desktop

Working notes for Matt and Claude: the goal, how we work, where things are,
the plan, the open bugs, and reference material. Keep it current and short:
finished steps move to **Matt/HISTORY.md** (the original brief, phases 0 to
10.1 in detail, fixed bugs, early decisions). Step and bug numbers ("5.3",
"B8") refer to the entries there.

## Goal

One self-contained VS Code extension (VSNewt) with everything to edit,
compile, run, and debug NewtonScript programs, and eventually packages with
their GUI. newtc does all the work (compiler, runtime, debugger, and next
the Newton views on FLTK); the extension only starts it.

Done so far (details in HISTORY.md):
- Debugger: ROM breakpoints and Apple's NS Debug Tools (`-dbg`); DAP for VS
  Code (`-dap`, `-dap-server`, `-dap-log`): breakpoints and stepping in the
  source, variables, watch/evaluate, exceptions, pause, bytecode listings,
  the Disassembly view. Line tables (`-g`), debug maps for decompiled
  packages (`-odecompile`, `-nsdbg`; launch "program" can be a `.pkg`).
- VSNewt 0.1.0 released on GitHub (macOS arm64): highlighting, run and
  debug, compile commands.
- Stubs say so when called (`-stubs log|throw|quiet|report`).
- The FLTK event loop (10.1, branch Add_fltk, `NEWTC_USES_FLTK`).

## How we work

- One small step at a time. Every step ends with something Matt can run and
  see, and a test that keeps it working. Commit and push only when Matt says
  so.
- Stick with Apple's API, and with their implementation where it makes
  sense. The ROM is the reference: `newtonos.s` for C++ (see Reference) and
  the ROM's own NewtonScript, which newtc can decompile (see "Reading the
  ROM's NewtonScript").
- No debugger logic in TypeScript (VSNewt stays thin), no cppdap, no
  external dependency except the optional FLTK (fetched and linked
  statically).
- Every bug found goes into the bug list below until it is fixed; fixed
  ones are checked off and moved to HISTORY.md, never deleted.
- Everything newtc writes uses LF; input accepts CR, LF, and CRLF.

## Where things are

- `newtc.cc`: command line, the native functions it registers, `-dap`,
  `installPackage()`.
- `Matt/`: newtc's additions. `DAP.{h,cc}` (the protocol in C++: framing,
  translators, polling) and `Debugger/DAP.ns` (the requests, NewtonScript);
  `Debugger/NSDebugTools.ns`, `NSDShortCuts.ns` (Apple's tools, embedded);
  `JSON`, `LineTables` (line tables, stepping, debug maps), `EventLoop` and
  `TestWindow` (FLTK), `EmbeddedScript`; `ObjectPrinter`, `Decompiler`,
  `AST*` (the decompiler); `tools/` (`rom_jumptable.py`,
  `build_vsnewt_newtc.sh`).
- `Utilities/Unimplemented.{h,cc}`: `NS_STUB` and `-stubs`.
- `Host/`: newtc as a Newton on the desktop: `Root` (the root view, not
  FLTK-specific); `Host/FLTK/` (planned) for the FLTK layer, namespace
  `nfl`.
- `Frames/Interpreter.{h,cc}`: the debugger hooks (not in ROM):
  `gBreakLoopReason`, `gDebuggerPoll`, `DebuggerPollNow()`,
  `gDebuggerStep`.
- `Views/` (CView, CRootView, ...) and `Packages/` (package manager): the
  ported NewtonOS code. newtc compiles neither (only MessagePad does, which
  we don't pursue); they are the reference for what views and packages do.
  Old Mac files: CR line endings, MacRoman.
- Tests: `Test/dbg/` (`run_dbg_tests.py` with `cases/`: `.ns` plus `.in`,
  `.dap`, `.args`, `.after`, `.expected`; `dap_client.py`; `test_dap_extras.py`,
  `test_terminal.py`, `test_nsdbg.py`), `Test/nsdbg_check.py`,
  `Test/lines_invariant.py`, `Test/run_corpus.py` (the package corpus,
  `Test/corpus_results/latest_manifest.json`).
- VSNewt: `/Users/matt/dev/VSNewt.git/vsnewt` (github MatthiasWM/VSNewt).
  Release: `Matt/tools/build_vsnewt_newtc.sh`, `scripts/make_grammar.py` if
  the built-in functions changed, raise the version, CHANGELOG,
  `npm run package`, GitHub release with the VSIX.

## Plan: Phase 10, a Newton GUI on FLTK (branch Add_fltk)

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

### Design

Decided or leaning (Matt, 2026-09-27):
- **Link classes**: a hierarchy of link classes connects each Newton view to
  an FLTK widget; one per view class (clView, clParagraphView,
  clPictureView, clEditView, ...), and one for the root view. The view
  frame refers to its link in its `viewCObject` slot (decided, see above),
  the widget refers to it with `user_data()`.
- **Ownership**: leaning towards the view (its link) owning the widget,
  rather than FLTK parents owning their children with the link only
  passing messages. To watch: an `Fl_Group` deletes its children when it is
  deleted, so a link must take its widget out of the parent (or be told)
  before either is deleted.
- **Refs in C++**: links keep the objects they need as `RefStruct` members
  (never `RefVar`, see 10.1 in HISTORY.md); `TestWindow`'s `MessageButton`
  is the pattern.
- **The link in the view frame**: a CObject binary
  (`AllocateCObjectBinary` with a destructor, like `NSDMakeNSDebugAPI`),
  so that the garbage collector can tell us when a view frame is gone.
- **Look and feel**: the NewtonOS look (graphics for frames and other
  elements, the Newton fonts) is implemented here, in `Host/FLTK/`. FLTK
  itself gets inline fonts (fonts compiled into the program; Matt's PR,
  https://github.com/fltk/fltk/pull/1617), which the Newton fonts will use.
- **The root view** (decided, 2026-09-27): only what programs need, when
  they need it. NewtonOS's root view has much more before any package is
  opened (the Extras drawer, notifications, memory set aside so it can
  still show an out-of-memory alert, ...). No window of its own: no
  Newton screen; each window-like Newton view (an app's base view, a
  floating view, ...) gets an FLTK window of its own.
- **Scripts from events**: only through `SendEventMessage()`, one at a time,
  exceptions caught there (the 10.1 rules, summarized under Reference).
- **Telling the user** (agreed, 2026-09-27): what NewtonOS shows in a
  notification goes through `GetRoot():Notify` (the ROM's own code does it,
  e.g. for install errors), and so will an exception that escapes a
  callback (the ROM's behaviour; to change in SendEventMessage when we get
  there). Notify decides how to show it: printed while developing (-dap,
  terminal; a modal alert would get in the way, and the debugger already
  stopped at the throw), a Newton-style FLTK alert later for someone just
  running a package (an option, or when no debugger is attached). Our texts
  can say more than the ROM's bare error numbers ("Undefined variable:
  'foo", the number in small print).
  Modal dialogs open a nested event loop whose events may run scripts inside
  the one that opened the dialog, as in NewtonOS.

Directory and namespace (decided, Matt, 2026-09-27):
- **`Host/FLTK/`** for everything that ties NewtonOS to FLTK: the links
  (`Host/FLTK/Links/`), root and app windows, alerts, and later the event
  loop and the test window (now in `Matt/`). "Host" as in the host
  platform: next to the NewtonOS components (`Views/`, `Graphics/`, ...),
  not mixed into them, and not named `FLTK/` (FLTK's own sources are in the
  build tree, `build/*/_deps/fltk-src`). CMake adds `Host/FLTK/*.cc` only with
  `NEWTC_USES_FLTK` (in the `if` block), so these files need no `#if`.
  Parts that don't depend on FLTK (the root view frame, `BuildContext`,
  `GetRoot`) go elsewhere, so that `-dap` without FLTK can still install
  packages. Later the folder can become a library target of its own
  without moving files.
- **Namespace `nfl`** ("Newton on FLTK") for the new classes:
  `nfl::Link`, `nfl::ViewLink`, `nfl::RootLink`, ... The framework has no
  namespaces and many general names; FLTK owns `Fl_`/`fl_`; a short
  namespace keeps us out of both and marks the layer. Native functions stay
  `extern "C"` at global scope (`FGetRoot`, ...): the ROM's function table
  finds them by their C names. Include FLTK's headers before the
  framework's (the framework `#define`s `OVERRIDE`, `INVISIBLE`, ...).

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
- [ ] 10.4 Open and close: the first link classes (root, clView, the app's
      base view as an FLTK window); `-run` and `-dap` open the app right
      after installing it (`GetRoot().(app):Open()`; no Extras drawer, one
      app at a time); the view scripts run in NewtonOS's
      order; closing the window closes the view (viewQuitScript).
- [ ] 10.5 The Hello app with a button (protoTextButton,
      buttonClickScript), a modal alert, and a reaction to closing; tests
      (TestWindow's approach: clicks from the event loop).
- [ ] 10.6 `-stubs report` on Hello and Battleship: the list of natives to
      implement next.
- [ ] 10.7 Battleship.
- Later: move EventLoop/TestWindow into `Host/FLTK/`; the FLTK layer as a
  library; Windows and Linux.

### Debugger tasks

- [ ] D1 ROM functions as source, on demand. When the debugger meets a ROM
      NewtonScript function (a stack frame, a step into it, the Disassembly
      view), newtc decompiles just that function (fast) into a virtual
      source (a DAP sourceReference, like the bytecode listings of 6.1),
      with its line table from the decompiler (as for debug maps, 9.1),
      registered for the session (RegisterLineTable). VS Code then shows
      the ROM's code as readable NewtonScript, with breakpoints and stepping
      by line, instead of bytecode. Decided (Matt, 2026-09-27): one function
      at a time, when it is entered, instead of decompiling the whole ROM
      into one huge (about 32 MB) source file.

### Open items from earlier phases

- 3.3 Verify the Apple API one group per step (Where, QuickStackTrace,
  breakpoints, Step, StepIn, StepOut, RunUntil, named and temp variables,
  Disasm).
- 4.1, 4.2 (low priority): gdb-style bare-word commands in the terminal
  break loop; less noise in the REPL output.
- 9.2 ROM code (with Einstein).
- LSP mode (`-lsp`) for VSNewt: diagnostics, completion.

## Known bugs (to fix)

Every bug found goes here until it is fixed; then check it off and move it
to HISTORY.md ("Bugs fixed"), never delete it.

Interpreter and runtime
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
  First pass done (2026-09-26): the eleven stubs the ROM doesn't know are
  deleted (nothing referenced them). `Floor` and `Ceiling` follow the ROM
  (FFloor/FCeiling): an integer if the result fits (the ROM: 30 bits; here
  kRefValueBits), else a real; the port's Floor always made a real and
  Ceiling an integer only from 1 up. Test `floor_ceiling`. `Min`/`Max`
  work. Still open: the tethered-listener spelling, and the other stubs one
  by one as the GUI work needs them.
  Strategy (2026-09-26): every stub now says so when called (see
  Conventions, "Stubs"); `-stubs report` shows which ones real programs
  call, to choose what to implement next.
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
  2026-09-26: `"" & 1.5 & " " & 1152921504606846975 & " " & $a & " " & 'sym`
  gives the right string now; check the other callers before closing.
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
- [ ] B10 **Round trip**: `Test/round_trip.py` reports `GEN2_FAILED` for 29 of
  the first 30 manifest packages, with the binary from before 1.1 too.
- [ ] B12 **ASCII only characters**: make sure that the decompiler outputs only
  ASCII characters and that characters that were originally non-ASCII UTF-16
  are output a escaped sequences - eventually we have to decide if NewtonScript
  shall go all UTF-8.

## Reference

### The event loop (10.1)

- After the program returns, `RunEventLoop()` (Matt/EventLoop.h) waits for
  FLTK events while a window is open (-script, -run, -dap); it ends when the
  last window closes or the DAP client is gone.
- Host events run NewtonScript only through `SendEventMessage()`: only while
  no NewtonScript runs (the interpreter's control stack is where the loop
  started), one at a time; exceptions are caught and reported there.
- DAP requests come in through `Fl::add_fd`; a pause while idle stops at
  the first instruction of the next callback (`DebuggerPollNow`); stepping
  out of a callback cancels the step.
- Stopped in a break loop, windows don't repaint (as with any native app in
  a debugger).

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


### Reading the ROM's NewtonScript

newtc has 481 of the ROM's NewtonScript functions (the built-in function
frame, magic pointer `@4098`). To read one, decompile it:
`printf "GetGlobalFn('InstallFormPart);\n" > x.ns; newtc -stubs quiet
-script x.ns -decompile`. Its C++ side is in `newtonos.s` (below).

### The ROM source

**ROM source** is in `./newtonos.s` (132 MB, git-ignored, ARM
   disassembly with labels). Apple's class names are `Txxx`; this port renamed
   them to `Cxxx` (`TInterpreter` → `CInterpreter`, `TNSDebugAPI` →
   `CNSDebugAPI`, `TDictionary` → `CDictionary`). Useful labels:
   `SlowRun__12TInterpreterFl` (line ~941591),
   `HandleBreakPoints__12TInterpreterFv` (~903964),
   `SetBreakPoints__12TInterpreterFRC6RefVar`, `EnableBreakPoints__12TInterpreterFUc`,
   `TNSDebugAPI::*` (~901908), `FBreakLoop` (~871677). Search with `grep -n`;
   never read the whole file.

### Embedding a .ns file in newtc

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


### How to read the ARM code in NS Debug Tools.pkg

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


### Conventions

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

- **Stubs** (built-in functions not implemented yet) are written
  `NS_STUB(FName, RefArg rcvr, ...)` (Utilities/Unimplemented.h): the same
  function (the ROM's built-in table in ROMData/*/RefData.s finds it by its
  C name), registered at startup. A call logs once per stub
  (`newtc: Fsin is not implemented yet (stub in Maths.cc:654), returns
  nil.`, on stderr; in -dap mode in the Debug Console) and returns nil.
  `-stubs throw` (or `NEWTC_STUBS=throw`) throws a NewtonScript exception
  instead (evt.ex.msg), so a test or a debug session fails at the call;
  `-stubs quiet` says nothing; `-stubs report` lists at the end which stubs
  were called and how often (e.g. `throw,report`). Stubs called while newtc
  starts are counted but not logged. `NS_STUB_NIL_OK` is for stubs whose nil
  is fine for now (never logged or thrown; e.g. GetRoot until there is a
  root view); `CXX_STUB()` marks a C++ function without a NewtonScript name
  (logged only). 1043 stubs were converted mechanically (Stubs.cc,
  NTKStubs.cc, Maths.cc, Power.cc, Dictionaries.cc, Packages.cc,
  StoreWrapper.cc, DrawImage.cc, ObjectSystem.cc); newtc links 1004 of them.
  The dbg test harness shows stub places and the total as `<file:line>`
  and `<total>`. Tests: `stubs_log`, `stubs_throw`, `stubs_report`,
  `dap_stubs`.

- **Debug builds use AddressSanitizer** (CMakeLists.txt, all targets, via
  `CMAKE_<LANG>_FLAGS_DEBUG`; UBSan was already on for newtc). A memory bug
  aborts with a report showing where the memory was allocated, freed, and
  misused. Release builds have neither.

