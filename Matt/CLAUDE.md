# newtc: NewtonScript in VS Code, and a Newton on the desktop

Working notes for Matt and Claude: the goal, how we work, where things are,
the plan, the open bugs, and reference material. Keep it current and short:
finished steps move to **Matt/HISTORY.md** (the original brief, phases 0 to
10 in detail, fixed bugs, early decisions). Step and bug numbers ("5.3",
"B8") refer to the entries there.

## Goal

One self-contained VS Code extension (VSNewt) with everything to edit,
compile, run, and debug NewtonScript programs and packages with their GUI,
and NewtPlay, a Mac app that runs Newton packages for anyone. newtc does
all the work (compiler, runtime, debugger, the Newton views on FLTK); the
extension and NewtPlay only start it.

Done so far (details in HISTORY.md):
- Debugger: ROM breakpoints and Apple's NS Debug Tools (`-dbg`); DAP for VS
  Code (`-dap`, `-dap-server`, `-dap-log`): breakpoints and stepping in the
  source, variables, watch/evaluate, exceptions, pause, bytecode listings,
  the Disassembly view. Line tables (`-g`), debug maps for decompiled
  packages (`-odecompile`, `-nsdbg`; launch "program" can be a `.pkg`).
- VSNewt 0.1.0 and 0.2.0 released on GitHub (macOS arm64): highlighting,
  run and debug, compile commands; 0.2.0 with Newton apps in windows.
- Phase 11: NewtPlay 0.1, a Mac app that runs packages (on the VSNewt
  v0.2.0 release, signed and notarized; universal, macOS 13): double
  click, drop, Open With (.nspkg its own, .newtonpkg, .pkg), a splash
  window with the history, a store per package version, Make a Shortcut
  and Make an App (next to the package, signed ad hoc on the user's Mac).
- Stubs say so when called (`-stubs log|throw|quiet|report`); hidden stubs
  found (`Test/hidden_stubs.py`).
- Phase 10 (branch Add_fltk, `NEWTC_USES_FLTK`): Newton views on FLTK, the
  pen, drawing, fonts, popup menus, timers, soups in a store file;
  nBattleship 1.4 plays to its end; 2.5 too (offscreen bitmaps:
  MakeBitmap, DrawIntoBitmap, ViewIntoBitmap; 1 bit deep so far). The
  display is portrait (2026-09-30): GetOrientation() 0, displayParams
  the ROM's GetRawDisplayParams(0) (320 by 480, app area 320 by 434).

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
- `Host/`: newtc as a Newton on the desktop, not FLTK-specific: `Root`
  (the root view), `ViewMethods` and `Graphics` (the natives, stubs
  without FLTK), `Shapes` (shapes in the ROM's formats), `Pict` (PICT
  bitmaps), `Pen` (stroke natives), `Timers` (delayed and deferred calls).
- `Host/FLTK/`: the FLTK layer, namespace `nfl` (design: "The FLTK layer"
  below). `Links` (a Link per open
  view: the link slot is `viewCObject`; Build/Dispose, pen, hide/show,
  bounds, fonts), `Widgets` (ViewWidget, TextView, PictureView,
  DrawViewFormat), `FloatNGo` (a window), `Boxtypes` (Matt's boxes:
  FLOATER_BOX, buttons), `Drawing` (DrawShape, DrawXBitmap, canvases, XOR,
  viewDrawScript), `Pen` (strokes), `Popup` (DoPopup), `Fonts` (the Newton
  font families on Mac fonts), `RomImages` and
  `Images/` (the ROM's pictures as PNGs, embedded by
  cmake/EmbedImages.cmake).
- `Stores/HostStore.{h,cc}`: the store in a file (`-store`), under Apple's
  soup code (CRC, safe writes, backups).
- NewtPlay: `Matt/NewtPlay.{h,cc}` (how it starts, the splash window,
  the history, the menu bar, CheckStarted), `Matt/NewtPlayMake.cc`
  (shortcuts and apps), `cmake/NewtPlayInfo.plist.in`,
  `cmake/MakeIcns.cmake`, `Resources/NewtPlay.png` (Matt's newt),
  `Matt/tools/build_newtplay.sh [version]` (signing:
  `SIGN_IDENTITY="Developer ID Application: Matthias Melcher
  (BK4ST6N599)"`, `NOTARY_PROFILE=notary`), `Matt/tools/nspkg.sh`
  (packages copied as .nspkg). Test `Test/dbg/test_newtplay.py`.
- `Frames/Interpreter.{h,cc}`: the debugger hooks (not in ROM):
  `gBreakLoopReason`, `gDebuggerPoll`, `DebuggerPollNow()`,
  `gDebuggerStep`.
- `Views/` (CView, CRootView, ...) and `Packages/` (package manager): the
  ported NewtonOS code. newtc compiles neither (only MessagePad does, which
  we don't pursue); they are the reference for what views and packages do.
  Source files are ASCII with LF line endings (the CR-only files were
  converted, 2026-09-27; the last MacRoman characters too, 2026-09-27: 132
  files). In comments, plain ASCII (' " * ... (c)); a character with a
  meaning keeps it as an escape: NewtonScript "\u201C\u" and $\u2019 (its
  strings are UTF-16), C/C++ '\xNN'. Curly quotes are gone too (they
  stood in for \" in messages): straight quotes, escaped in strings. What
  is still UTF-8 is harmless: a few symbols in comments (degrees, dashes),
  the .md files, and test outputs that show Newton strings.
- Tests: `Test/dbg/` (`run_dbg_tests.py` with `cases/`: `.ns` plus `.in`,
  `.dap`, `.args`, `.after`, `.expected`; cases named `fltk_*` need
  FLTK and are skipped without; `dap_client.py`; `test_dap_extras.py`,
  `test_terminal.py`, `test_nsdbg.py`, `test_store.py`),
  `Test/stub_census.py`, `Test/hidden_stubs.py`, `Test/nsdbg_check.py`,
  `Test/lines_invariant.py`, `Test/run_corpus.py` (the package corpus,
  `Test/corpus_results/latest_manifest.json`).
- VSNewt: `/Users/matt/dev/VSNewt.git/vsnewt` (github MatthiasWM/VSNewt).
  Release: `Matt/tools/build_vsnewt_newtc.sh`, `scripts/make_grammar.py` if
  the built-in functions changed, raise the version, CHANGELOG,
  `npm run package`, GitHub release with the VSIX.

## Resume here (2026-09-30)

Phases 10 and 11 are done: newtc runs Newton apps in windows (FLTK),
nBattleship 1.4 and 2.5 play to their end, and NewtPlay 0.1 is released
(VSNewt v0.2.0 release, `NewtPlay-0.1-macOS.zip`, from ed41f10; the
nBattleship zip is down). Next: Phase 12, to be decided with Matt (below).

Builds: `build/VSCode` (Debug, FLTK: the default for the tests),
`build/Release` (no FLTK; give the runner an absolute `--newtc`),
`build/VSNewt` (Release, FLTK: `Matt/tools/build_vsnewt_newtc.sh`),
`build/NewtPlay` (`Matt/tools/build_newtplay.sh`). All suites:
`python3 Test/dbg/run_dbg_tests.py` (102 with FLTK; 77 + 25 skipped
without), then `test_nsdbg.py` (14), `test_dap_extras.py` (7),
`test_terminal.py` (5), `test_store.py` (9), `test_newtplay.py` (20,
needs the NewtPlay target built; opens NewtPlay windows); VSNewt: `NEWTC=<newtc> npm
test` (11), `npm run test:grammar`.

Working on a package:
- `newtc -pkg app.pkg -odecompile app.ns`: its source, to read.
- A `-script` after `-pkg ... -run` runs once the app is open: walk
  `GetRoot().|App:SIG|:ChildViewFrames()` and print text, viewJustify,
  `:GlobalBox()`; `TestSnapshot(view, "x.png")` saves a picture;
  `TestTap`, `TestPen`, `TestPick` play it (see Matt/TestWindow.h).
  Top-level `local`s don't carry over between statements, and a global
  `func Name()` is a function, not a variable: use DefGlobalVar('name,
  func ...) for callbacks. An uncaught exception leaves the window open
  (newtc then waits): run with a timeout.
- A ROM function's literals (`fn.literals`) tell which natives it calls;
  `newtc -s "@190.viewClickScript" -decompile` shows a ROM function.
- `-stubs throw` or `-stubs report` for the stubs a run calls;
  `Test/stub_census.py` for a package's needs without running it.

## Phase 12 (to be decided)

1. **The next apps**: the stub census over the package corpus
   (`Test/run_corpus.py`, `Test/stub_census.py`), and 3 to 5 apps chosen by
   which natives unlock the most.
2. **Text input**: edit views and `protoInputLine` with the Mac keyboard,
   key views, the caret (handwriting recognition is far off).
3. **Fonts**: embedded fonts (Matt's FLTK work), maybe Espy Sans' own
   bitmaps (NFNT, `/Library/Fonts/Espy Sans` on Matt's Mac) for
   pixel-exact text.
4. **Platforms**: the FLTK layer as a library, Linux and Windows builds
   (VSNewt and NewtPlay there).
5. **Debugger D1** (below), and the bugs below (B14, B15, B7).

## Backlog (left from Phase 10)

- 10.7e: MoveBehind; the floater's dragger nub (ROM picture @691, Matt's
  box); the status bar's clock overlaps the info button a little; the
  popup menus' font (Matt's Helvetica Bold 9; NewtonFont(tsSystem,
  kBoldFace) would be the Newton's).
- 10.7f: keys (SetKeyView, SendKeyMessage, MatchKeyMessage,
  RestoreKeyView), AddUndoAction (protoCheckbox's ToggleCheck calls it),
  TableLookup, SyncView; gestures other than taps (scrub, caret, lines),
  words and shapes.
- FLTK (Matt's issues): a blend mode for XOR on all platforms
  (`BlendInvert()` has an `#error` off macOS); `fl_rounded_rectf()` fills
  a pixel less at the left and top than `fl_rectf()` (DrawHilite makes up
  for it).
- Move EventLoop/TestWindow into `Host/FLTK/`; the FLTK layer as a library.

## The FLTK layer (Phase 10, 2026-09-27 to 2026-09-29; the steps in HISTORY.md)

Newton views are FLTK widgets: a link per open view (Links.h) makes its
widget, every view widget is a group (its children's widgets inside), and
draws as the ROM's CView::draw: fill, content, viewDrawScript, children,
frame (DrawViewFormat, from the viewFormat the link keeps), what scripts
drew (a canvas per view, with an invert layer for XOR), the hilite (an
inversion, or the view's viewHiliteScript). Frames follow the ROM's
outerBounds; fonts the Newton's families on Mac fonts (Fonts.h), sized to
match; windows are 1.5 times FLTK's scale. The pen is the mouse (Pen.h),
taps are recognized (viewGestureScript); popup menus, timers, soups (a
store file) work.

### Design (decided with Matt, 2026-09-27)

Decided or leaning (Matt, 2026-09-27):
- **Link classes**: a hierarchy of link classes connects each Newton view to
  an FLTK widget; one per view class (clView, clParagraphView,
  clPictureView, clEditView, ...), and one for the root view. The view
  frame refers to its link in its `viewCObject` slot (decided, see above),
  the widget refers to it with `user_data()`.
- **Ownership** (decided, 2026-09-27): each link owns its widget and
  deletes it when the view closes, children before their parent. A widget
  that holds the children's widgets (a GroupLink's: nfl::Group,
  nfl::FloatNGo) must not delete them as an Fl_Group does: it overrides
  delete_child() to only take the child out (GroupLink::RemoveChild), and
  calls clear() in its own destructor (in ~Fl_Group the object is an
  Fl_Group again, and clear() would call Fl_Group::delete_child()).
  Widgets are deleted right away, also from their own callback (FLTK 1.4:
  Fl_Widget_Tracker); Fl::delete_widget() is legacy, not used.
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

## Debugger tasks

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

## Open items from earlier phases

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
- [ ] B15 **`SPrintObject` is half ported** (Frames/Strings.cc,
  `MakeStringObject`): nil, true, frames, arrays give `""` (reals: fixed
  with B28, 2026-09-29), a 62-bit
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


Views and drawing
- [ ] B30 **A frame 2 wide is 2/3 of a pixel off** (found 2026-09-30 with
  2.5's maps): at 1.5 times on a Retina screen (3 screen pixels a Newton
  pixel), DrawViewFormat's frame (fl_rect, line width 2) covers 7 screen
  pixels from 2 left of the widget's edge: it reaches 2 screen pixels
  into the view at the right and the bottom (2.5's gray map frame). In
  an image surface it came out a pixel further in (ViewIntoBitmap reads
  pixels at their top left, which matches the screen). Check pens 1 to 4
  at 1, 1.5 and 2 times (Matt's wide-line work in FLTK).
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

