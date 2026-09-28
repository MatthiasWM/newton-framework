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
- `Host/`: newtc as a Newton on the desktop, not FLTK-specific: `Root`
  (the root view), `ViewMethods` and `Graphics` (the natives, stubs
  without FLTK), `Shapes` (shapes in the ROM's formats), `Pict` (PICT
  bitmaps), `Pen` (stroke natives), `Timers` (delayed and deferred calls).
- `Host/FLTK/`: the FLTK layer, namespace `nfl`. `Links` (a Link per open
  view: the link slot is `viewCObject`; Build/Dispose, pen, hide/show,
  bounds, fonts), `Widgets` (ViewWidget, TextView, PictureView,
  DrawViewFormat), `FloatNGo` (a window), `Boxtypes` (Matt's boxes:
  FLOATER_BOX, buttons), `Drawing` (DrawShape, DrawXBitmap, canvases,
  viewDrawScript), `Pen` (strokes), `Popup` (DoPopup), `RomImages` and
  `Images/` (the ROM's pictures as PNGs, embedded by
  cmake/EmbedImages.cmake).
- `Stores/HostStore.{h,cc}`: the store in a file (`-store`), under Apple's
  soup code.
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
  `Test/stub_census.py`, `Test/nsdbg_check.py`,
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

### Resume here (2026-09-28)

A Newton package as a macOS app of its own (2026-09-28, for Adam Tow,
nBattleship's author): Matt/tools/build_app.sh <app.pkg> [name] [version]
builds build/App/<name>.app (target newtc_app, CMake -DNEWTC_APP_PKG=...:
newtc with the package compiled in, Matt/EmbeddedApp.h), universal (arm64
and x86_64, both tested with a whole game), macOS 13, signed ad hoc, and a
zip of it. Double-clicked it runs the package with its store in
~/Library/Application Support/<name>/; with arguments it is newtc.

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
- [ ] 10.7 Battleship, in this order (each a step of its own):
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
  - [ ] 10.7e Pickers and the settings' details.
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
  - [ ] 10.7f The rest: keys (SetKeyView, SendKeyMessage, MatchKeyMessage,
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
  - [ ] 10.7g Hidden stubs: port functions that only print or return nil
        without NS_STUB (SetValue was one; `-stubs report` can't see them).
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

Stores
- [ ] B26 **A store that stops Battleship from starting** (Matt, 2026-09-28:
  six times in his tests, build/Battleship.store had to be deleted before
  the app launched again; it hung). No file kept yet. Likely a soup state
  that sends Apple's index or cursor code (Stores/) into a loop, as B20 to
  B22 were, rather than a damaged file. Since 2026-09-28 the store file
  has a CRC and is written safely, a run keeps the file it found as
  <file>.bak (the run before's as .bak2), and a damaged file is kept as
  <file>.bad-<time> (HostStore.h). Next time: keep the .store (and .bak,
  .bak2), and pause newtc in VS Code while it hangs (the stack shows the
  NewtonScript that loops; lldb for C++).

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

