# Matt's NewtonScript Decompiler — working notes

This file is scoped to `Matt/`: a NewtonScript bytecode decompiler built on
top of the `newton-framework` NewtonOS reimplementation. It turns compiled
NewtonScript functions (from `.pkg` packages, NSOF streams, or the object
system) back into readable NewtonScript source.

## The actual goal (read this first)

- The `/Users/matt/Azureus/unna2` directory is estimated to hold **~95% of
  every NewtonScript package ever publicly released**. The real objective of
  this whole effort is: decompile all of them correctly. Once that's done,
  this code will likely see only occasional maintenance — this is a push to
  a finish line, not the start of an open-ended project.
- **Precision over speed.** Decompile time is a non-issue (this runs
  occasionally, offline, per package). Never trade correctness for
  performance in this codebase.
- All real-world packages were built with the **original Newton Toolkit
  (NTK)** running on Mac OS System 7 or Windows 98 — NTK's compiler
  performs optimizations. This project's own from-scratch compiler
  (`Frames/Compiler/*`, used by `newtc -s`/`-script`) does **not** optimize.
  That means hand-written test scripts compiled through `-s`/`-script` are
  useful for exercising a specific bytecode shape in isolation, but they
  won't necessarily reproduce the exact optimized bytecode idioms that NTK
  emits in real packages — the real corpus is the only fully authoritative
  test set.
- **The real acceptance test**: Matt has BasiliskII running a
  remote-controllable copy of the original NTK. The plan is to feed
  decompiled NewtonScript back into the *real* NTK and diff the
  regenerated `.pkg` against the original package — a full roundtrip. That
  (not just "the decompiler's own output looks plausible") is the ground
  truth for correctness.
- **Future payoff**: once the ROM itself can be decompiled, the remaining
  missing NewtonOS GUI calls in this framework can be implemented, which
  would let this project actually *run* almost all of these packages, not
  just decompile them.

## Directory map

Core decompiler (~3,000 lines, the focus of this work):

| File | Role |
|---|---|
| `AST.h` / `AST.cc` | `ast::Node` base class, doubly-linked-list plumbing, `JumpPairMatches()`, `HandleBreakTargets()` |
| `ASTAdmin.h` / `.cc` | `Bytecode`, `Consume1/2/N` — generic stack-consuming base classes ("pop N values off the simulated stack") |
| `ASTDataFlow.h` / `.cc` | One leaf node class per "plain" bytecode: push/pop/arith/path/array/frame ops. Mechanical, not the pain point. |
| `ASTControlFlow.h` / `.cc` | One node class per branch/loop/exception/call bytecode; every `Resolve(Pass::ControlFlow)` just calls `pattern::TryResolve(this)` now — no hand-written matchers left |
| `ASTControlFlowHelper.h` / `.cc` | Synthetic "resolved" nodes (`CFLoop`, `CFWhile`, `CFIfThen`, `CFForLoop`, `CFTry`, `JumpTarget`, …) built once a pattern matches |
| `ASTPattern.h` / `.cc` | Declarative pattern-combinator engine (see below) |
| `ASTControlFlowPatterns.cc` | Where every control-flow idiom is registered as a `pattern::Spec` |
| `Decompiler.h` / `.cc` | Driver: decodes raw bytecode into the initial node list (`generateAST`), runs the fixpoint resolve loop (`solve`, now just DataFlow + ControlFlow, no Compression phase), prints source (`printSource`). Owns all AST nodes (`nodePool_`/`MakeNode<T>`). |
| `Printer.h`/`.cc`, `ObjectPrinter.h`/`.cc` | Generic text-layout + Newton-object/source pretty-printer, reused by the decompiler for output |

Unrelated tools that happen to live in `Matt/` (not touched by this work):
`BookWriter.*` (package→HTML/PDF), `PackageWriter.*` (Ref→package binary),
`PDFGen/` (vendored C PDF lib).

## Branch

This work lives on `AST_pattern_matching` (branched off `restructure`). Matt
reviews and commits each stage himself as it lands: `455fd20` (arena,
`JumpPairMatches`, the pattern engine, `loop`), `8b8cecf` (`while/do`,
`repeat/until`, and `or`), `3b1157e` (`if/then/else`, `for`), `e9eccc7`
(`foreach...do`), `a8826ad` (`try...onException...do`), `7ab0e4d`
(`foreach...collect`). Stage 8 (removing `compressAST()`/`CodeBlock` and the
handful of real bugs that removal surfaced — see below) is uncommitted as of
this writing, since it landed most recently.

## How to build and test

```
cmake --build build/VSCode --target newtc -j4
```

`newtc` CLI (see `newtc.cc` for the full list):
- `-pkg <file>` / `-nsof <file>` / `-script <file>` / `-s "<source>"` — load or compile something
- `-decompile` — print the loaded object with every function decompiled
- `-debug ast` — trace the resolve loop's progress (confirms which pass/pattern actually fired)
- `-debug bc` — dump raw bytecode before decompiling
- `-pkglist <file>` — apply the following commands to every package listed in `<file>` (one path per line, `#`-prefixed lines are comments/notes)
- `-trap <path.to.func>` — break into the debugger when decompiling a specific function

`Test/pkglist.txt` (3300+ entries) is a real regression corpus, mostly
pointing into `/Users/matt/Azureus/unna2`. **Caveat**: running the full list
via `-pkglist` was unreliable in this environment (a `while read` loop
combined with `newtc` invocations seemed to stall or steal stdin in ways
that weren't fully root-caused — always redirect the inner command's stdin
from `/dev/null` if scripting this, and don't be surprised if it needs
per-file timeouts). A large, **known**, pre-existing set of failing/crashing
packages is already catalogued in a scratch-log comment block near the top
of `Decompiler.cc` (categories like "unresolved nodes", "crashes", "byte
code differs", "order of slots changed", etc., each with the exact `.pkg`
path) — check that list before assuming a new failure is a regression.

**Practical regression-testing recipe used successfully this session**
(cheap, reliable, no dependency on the flaky `-pkglist` loop): pick a
handful of real packages that are known to decompile cleanly, capture their
`-decompile` output, `git stash` the change under test, rebuild, recapture,
`diff`. A one-line-shifted line number inside a pre-existing assertion
message (from edits adding/removing lines earlier in a file) is expected
noise, not a regression — check the actual diff content, not just whether
`diff` reports any difference at all.

The 12-package curated sample used throughout this file's port-by-port
verification happened to include two packages that crashed on a pre-existing
`assert(i < (int)locals_.size())` in `Decompiler::decompile()`
(`SolitoDeluxe2.5/sdx25.pkg`, `Motile/motile.pkg`) — every regression check
above diffed clean *except* a line-number shift inside that one assertion
message, which is why it kept coming up. That assert is now replaced with a
diagnostic (`fprintf` to stderr: file, `ObjectPrinter::RefPath()`, slot
index, tag name, `locals_.size()` vs. `numArgs_`/`numLocals_`, `argFrame`'s
real length, and the raw bit-packed `numArgs` field) followed by `break`
instead of aborting — so a mismatch no longer takes down the whole
decompile, just skips naming the extra slot(s). Both of those two packages
turned out to hit the **exact same shape**: a zero-arg `func()` named
`viewSetupFormScript` where the raw `numArgs` field decodes to
`numArgs_=0, numLocals_=0`, but `argFrame` has one extra named slot
(`prefs`) the bit-packed count doesn't account for — with both packages now
decompiling that function's *body* correctly regardless (worth understanding
*how*, since it clearly isn't reading the name from `locals_[3]`). Root
cause not yet investigated — likely an NTK `kPlainFuncClass` encoding edge
case specific to this zero-arg/zero-local shape, not a general local-count
bug, since normal functions with real args/locals work fine throughout the
whole corpus. `viewSetupFormScript` appearing verbatim in two unrelated
packages suggests shared boilerplate (a proto/library template both apps
were built from), which might make it easier to track down where it
originates.

## The pattern-finder redesign (in progress)

### The problem
`ASTControlFlow.cc`'s control-flow idiom recognition (`if/else`, `while`,
`repeat`, `for`, `foreach`, `try`, `loop`) is a set of hand-written
`do { ...; break; } while(0)` blocks per idiom: pointer-chase `prev`/`next`,
`dynamic_cast` each neighbor to an exact expected type, bail on mismatch.
Two concrete symptoms proved this had hit its ceiling:
- `BCNewIter::ResolveForeachSlotValueCollect()` (`foreach...collect`) is
  abandoned mid-implementation — duplicates ~90% of its working sibling,
  then hits an unconditional `break;` with `// FIXME: the code is not
  complete!` and commented-out construction code.
- Matt's own design note, `AST.cc:10-40` ("Nov 21 2025"): the current
  approach of bundling statement runs into a physical `CodeBlock` node via a
  separate compression pass, which later matchers then reach into, "does
  not work" and "we *must* remove this" — replaced by a declarative,
  registered regex-like pattern system.

Also: the branch/jump-target consistency check
(`jt->Origin()==x->pc() && jt->pc()==x->b()`) was hand-copied in nearly
every matcher.

### The fix (staged; see the full plan text below for what's left)
A declarative combinator engine, `Matt/ASTPattern.h/.cc`:
- `pattern::Tag` — a dispatch-only enum (separate from `Node::provides()`,
  which still does double duty as stack-arity *and* an ad hoc type tag for
  historical reasons — `tag()` doesn't touch that). ~12 node classes
  override `tag()`.
- `Cursor` — walks `prev`/`next` from an anchor in a fixed direction.
- `Match` — captured slots: single nodes (`Required`/`Optional`), variable-
  length statement runs (`Statements`, captured **directly off the flat
  list**, no physical `CodeBlock` needed), repeated groups (`Repeat`, for
  N-handler `try` blocks etc.).
- `Builder` — the DSL: `Required`/`Optional`/`Statements`/`NonEmpty`/
  `ZeroOrMore`/`Repeat`/`JumpPair`/`Guard`/`Name`/`Priority`/`Custom`, ending
  in `.Build(callback)`.
  - `NonEmpty(slot)` rejects the match unless a prior `Statements(slot)`
    captured at least one node — needed for idioms (like `if...then`) that,
    unlike `loop`/`while`/`repeat`, never default to a `nil` body.
  - `ZeroOrMore(tag, slot)` is `Statements()`'s sibling for a run keyed on
    an exact `tag()` instead of `IsStatement()` — e.g. a cluster of
    `JumpTarget`s. Unlike `Repeat()` (whose count is known ahead of time
    from the anchor), this greedily takes as many as match and always
    succeeds, even with zero.
  - `Custom(step)` is the escape hatch: embed a raw `Step` (the same
    `bool(Cursor&, Match&)` function type every other combinator compiles
    down to) for logic too irregular to express declaratively — a capture
    whose cursor advancement is conditional on what an *earlier* capture
    found, or a capture that needs to walk the anchor's neighbors in the
    *opposite* direction from the rest of the spec. `foreach...do` and
    `foreach...collect` both need it (see below); reach for it only
    when the named combinators genuinely can't express what's needed.
- `Register(spec)` / `TryResolve(anchor)` — a `Tag`-indexed registry; a new
  idiom is a **new file-scope static registration**, zero edits to the
  anchor bytecode's own class.

`Matt/ASTControlFlowPatterns.cc` is where idioms get registered. Ported so
far: `loop...end`, `while...do...end`, `repeat...until...end`, `a or b`,
all three `if...then...[else...]` shapes (bare, statement/statement,
expr/expr), `for...to...by...do`, `foreach...do`, and
`try...onException...do`. `BCBranch::ResolveLoop()`,
`BCBranchIfTrue::ResolveWhileDo()`, `BCBranchIfTrue::ResolveOr()`,
`BCBranchIfFalse::ResolveRepeatUntil()`, `BCBranchIfFalse::ResolveIfTheElse()`,
the entire ControlFlow-pass bodies of `BCBranchLoop::Resolve()` and
`BCNewHandler::Resolve()` (neither had a separate `ResolveXxx()`, the
matcher lived inline), and `BCNewIter::ResolveForeachSlotValueDo()` are all
**deleted/replaced** in `ASTControlFlow.cc`/`.h`. The only things left
un-ported are `BCNewIter::ResolveForeachSlotValueCollect()`
(`foreach...collect`, still broken/unfinished, deferred to step 7) and `and`
(step 7 also) — **every actual control-flow matcher that currently works is
now on the pattern engine.**

The three if/then/else specs (`BuildIfThenPattern`, `BuildIfThenElsePattern`,
`BuildIfThenElseExprPattern` in `ASTControlFlowPatterns.cc`) share one
`MakeIfThen(...)` construction helper. One deliberate, documented departure
from 1:1 fidelity: the original's expr-shape optionally captured the
`Branch`/`JumpTarget` else-scaffolding (never required it structurally), but
its own class comment states the shape "exists only as if/then/else" — a
value-producing conditional needs both sides to produce a value, so
expr-with-no-else isn't just rare, it's impossible. The expr spec makes
that else-scaffolding `Required()`, not `Optional()`; if that's ever wrong
for some package, the spec fails to match and that function shows up as an
unresolved node — a visible failure, not a silently wrong one. The two
statement-shape specs (bare-if vs if/else) needed no such assumption: they
split cleanly because "next node is a JumpTarget" vs "next node is a
Branch" are exhaustive, mutually exclusive alternatives, provable from how
the bytecode is shaped, not from any belief about what NTK does.

`CFIfThen::Print()` had the same latent `IsMultiStatement()` gap as
`CFWhile`/`CFRepeat` (see below) but couldn't just switch to
`PrintBodyChain()` — it has its own bespoke always-`begin`/`end` policy
(`forceBeginEnd`) and an "else if" chain special-case. Fixed narrowly
instead: added an `IsChainMultiStatement(Node*)` helper (same idea as
`PrintBodyChain`'s single-vs-chain check) for the two `body_`/`elseBody_`
"is this actually multi-statement" tests (including the `a and b` sugar
guard), and turned the two single-node-assuming `body_->Print()` /
`elseBody_->PrintOnNewLine()` calls into loops over the chain. Same
lesson as before: **grep every `IsMultiStatement()` call and every
`body_->Print()`/`PrintOnNewLine()` call across `ASTControlFlowHelper.cc`
whenever a newly-ported matcher can feed a `ControlBlock` a
`Statements()`-derived body — don't assume only the class you're actively
porting is affected.**

`for...to...by...do` (`BuildForLoopPattern` in `ASTControlFlowPatterns.cc`)
is the **one exception** to "no `CodeBlock` needed" among everything ported
so far, and it's not a design compromise — it's a direct, unavoidable
consequence of `compressAST()` still being active. `iter`/`limit`/`incr`
are set by three consecutive `BCSetVar` *statements* right before the
loop's own scaffolding; since `compressAST()` pre-merges any run of 2+
statements before the ControlFlow pass ever runs, those three SetVars (plus
whatever unrelated code precedes them) are *already* fused into one opaque
`CodeBlock` by the time this pattern is tried — there is no flat-list
position where `Required(Tag::SetVar, ...)` could address them
individually today. The spec still requires a `CodeBlock` (`Required(Any,
cond: dynamic_cast<CodeBlock*> && size()>=4)`) and reaches into its tail by
fixed offset inside the callback, exactly like the original — this is
intentional, documented fragility, not an oversight. It should become
`Statements(preamble) + Required(SetVar,iter) + Required(SetVar,limit) +
Required(SetVar,incr)` (no `CodeBlock` at all) once Stage 8 removes
`compressAST()` and those three statements become individually-addressable
list nodes again. **This is worth remembering for `foreach` too** — it will
likely hit the identical constraint for its own multi-statement setup, and
should get the identical documented treatment rather than a fight to avoid
`CodeBlock` prematurely.

Also caught while porting `for`: the original's local-consistency check
`limit = getLimit->b(); if (getLimit->b() != limit) break;` compares a
freshly-assigned value to its own source — it's dead code, always true,
almost certainly meant to check `setLimit->b()` (mirroring the real
iter/incr checks either side of it). Preserved exactly as a no-op in the
port (fixing it would be a behavior change, and it's evidently never
mattered against the real corpus) but flagged in a comment at the port site
— worth a look if `for` loops with a local-index mismatch ever misdecompile.

`foreach...do` (`BuildForeachDoPattern`) hit the `CodeBlock` constraint too,
as predicted above, but in a *weaker* form than `for`: since a `foreach`
loop's per-iteration setup is only 1-2 statements (`SetVar value` and
optionally `SetVar slot`), there's no guaranteed-3-consecutive-statements
invariant forcing a `CodeBlock` to always exist. So the spec has to handle
*both* cases the original did — setValue/setSlot read from a captured
CodeBlock's front elements if compressAST() merged them with the body, or
straight off the flat list (with explicit cursor advancement) if it didn't.
That dependency on an *earlier* Optional capture's outcome doesn't fit
`Statements()`/`Required()`, so it's the first (and, so far, only) user of
the new `Custom()` escape hatch — twice over, actually: once for this
branchy extraction, and once because `BCNewIter` is architecturally
special. It's a `Consume2` (object, deeply) but its `Resolve()` unconditionally
returns `next` on the DataFlow pass and hardcodes `Resolved() → false`
forever, so it never gets its operands auto-wired into `in1_`/`in2_` the
normal way — the matcher has *always* had to walk `prev`/`prev->prev`
directly to find "object"/"deeply", and the port does too, via a
`Custom()` step that (uniquely among every pattern so far) ignores the
spec's own forward `Cursor` and inspects the anchor's backward neighbors
instead.

`try...onException...do` (`BuildTryPattern`, registered twice — once per
statement/expr shape, same reasoning as the if/then/else split, since
`body`'s statement-vs-expr shape must hold uniformly across every handler,
not per-node) turned out to be the **shortest and least eventful** port
despite looking like the most intimidating idiom on paper. Two reasons:
- First real use of `Repeat()`, for exactly what it was designed for: the
  `numEx - 1` middle `onException` clauses, and separately the trailing
  cluster of `exDone` JumpTargets.
- The actual node extraction/unlinking was **never done in
  `BCNewHandler::Resolve()` at all** — `CFTry`'s own constructor (unchanged)
  walks from the anchor to the matched `jtDone` and does that itself,
  re-discovering the same handler/body shape independently. So this port's
  callback is only a few lines (build `CFTry`, `ReplaceWith`, done); almost
  none of the captured slots are ever read back — capturing them still
  exercises the same match-or-reject logic the original relied on, they're
  just structural checkpoints, not construction inputs.

Also notable (not a bug, just a real asymmetry worth knowing): unlike every
other idiom ported so far, this one does **no jump-pair validation at
all** — no check that `brDone` actually targets `jtDone`, or that any
`exDone` branch targets the right trailing `JumpTarget`. The original never
checked those relationships either, trusting structural shape (right
node-type sequence, right count from `b()`) alone. Preserved exactly, since
adding verification the original never had would be a behavior change, not
a port — but it's a legitimate future robustness improvement if `try`
blocks ever misdecompile in a way the other, more rigorously-checked
idioms wouldn't.

`foreach...collect` (`BuildForeachCollectPattern`) is done, replacing the
never-finished `ResolveForeachSlotValueCollect()` (which the original left
mid-rewrite, hitting an unconditional `break` with commented-out,
non-compiling construction code). **Its bytecode shape was re-derived from
scratch** — per the standing rule in step 7 below, the abandoned sketch's
own structure was not trusted; instead: write a hand-compiled
`foreach...collect` script, dump it with `-debug bc`/`-decompile
-debug ast`, and read the *already-resolved* parts of the decompiler's own
output (most of the surrounding data flow resolves generically even with no
control-flow matcher present) to reconstruct the exact shape empirically.
That process is worth repeating for any future idiom this doc doesn't
already cover in detail. Concretely, `collect` differs from `do` in three
ways:
- It pre-sizes a result array (`result := Array(iterator[5], nil)`, where
  `iterator[5]` is the object's total slot count) and a running `index :=
  0` before the loop — a `initBlock` `CodeBlock` with no equivalent in `do`.
- Its per-iteration setup is *never* just the optional single node `do` can
  have — it's *always* `SetVar value := iterator[1]; [SetVar slot :=
  iterator[0];] SetARef(result, index, <body>); Pop`, i.e. always >= 2
  statements, so it's *always* a `CodeBlock` by the time this pattern runs
  (no `Custom()`-driven CodeBlock-or-raw-list fallback needed here, unlike
  `do` — one less irregularity, not more, despite `collect` looking scarier
  on paper). The collect expression itself comes out of that block's
  trailing `Pop`'s `Input()` (a `BCSetARef`) via `.Element()` — and per
  `BCSetARef::Resolve()` (ASTDataFlow.cc), `Element()` is **already
  unlinked from the root list** by ordinary DataFlow resolution, same as
  any Consume1 `in_` — don't call `.Unlink()` on it again, that's a
  double-unlink into a null `prev`.
- Break-handling is structurally different, not just re-derived: `do`'s
  `break` reuses the existing `BCBranch::ResolveBreak()`/`CFBreak`
  machinery (a `Branch` immediately followed by a dead-code `BCPop`);
  `collect`'s compiles to `push value; Branch <landing zone>` with **no**
  following Pop (the pushed value is consumed at the landing zone, not
  discarded), so `ResolveBreak()` never recognizes it. This spec instead
  matches that whole landing-zone shape directly and structurally
  (`ZeroOrMore(JumpTarget)` + optional result-override `SetVar` + cleanup),
  exactly mirroring what the original's incomplete sketch was reaching for.

**Known, deliberate, documented limitation**: `break` used *from inside*
the collect body expression itself (e.g. `foreach x in y collect if cond
then break z else x`) is not supported — recognizing it would need a new
dedicated pattern (matching "Branch whose target is this construct's own
break-landing zone", not the existing Branch-then-Pop shape), which is a
real additional feature, not a structural translation, and was scoped out.
Confirmed safe rather than silently wrong: since the compiler never emits
the dead-Pop marker `ResolveBreak()` looks for, a break-containing body
never resolves into the single clean expression this spec's
`Required(kBodyExpr)`-equivalent step needs, so the *whole* construct
simply fails to match — exactly the same "stays unresolved" outcome as
before this port existed, verified by testing that exact case and diffing
byte-identical against pre-port output.

**This port paid off immediately on real data**: the regression corpus
sample includes `SolitoDeluxe2.5/sdx25.pkg`, whose `viewSetupFormScript`
helper (the same function implicated in the `argFrame`/`numArgs`
diagnostic finding above) contains a real `foreach slot, value in ... collect
{...}` construct, appearing at three call sites in that one package. All
three now resolve correctly (`popup := foreach loc0, loc1 in
GetRoot().|Extras:SoloDx:Tactile|.game collect {item: loc1.userName, sym:
loc0};`) where they previously stayed permanently unresolved — this is
also, incidentally, evidence the same shared-template theory from the
`argFrame` finding is plausible: this exact helper function structure
(`viewSetupFormScript`, plus whatever calls into this `collect`) recurring
verbatim across otherwise-unrelated packages.

**`and` needed no new registration at all** — the plan's step 7 framing
("and... currently only recovered as print-time sugar... never matched as
its own construct") was written before the if/then/else port (step 3)
existed, and turned out to already be moot once it landed: `a and b`
compiles to *exactly* the same bytecode as `if a then b else nil` (the
original's own class comment already said so), `BuildIfThenElseExprPattern`
already builds a `CFIfThen` for that shape, and the **pre-existing,
untouched** print-time sugar in `CFIfThen::Print()` already recognizes a
bare-`nil` `elseBody_` and prints `and` instead of the full if/then/else.
Verified round-tripping correctly as-is in every context tried: `return a
and b`, `x := a and b`, nested (`a and b and c`) — no code change, only the
stale `// TODO: and` checklist comment at the top of `ASTControlFlow.cc`
needed updating. One real, unfixable gap surfaced along the way, not a bug:
`and` used as a bare **statement** whose value is never consumed (e.g. `a
and Print(b);`) decompiles as `if a then Print(b);` (no `and`), because the
compiler elides the "else nil" push entirely when nothing needs the
resulting value — at that point the bytecode for `a and b;` and `if a then
b;` is *identical*, so no amount of pattern-matching (at resolve time or
print time) can recover which one the source actually said. This is the
same category of fundamental, information-theoretic ambiguity as the
`and`/`if-then-else` overlap itself, just one directional case of it.

### Order of remaining work
1. ~~Port `or`~~ — done.
2. ~~Port `while/do`, `repeat/until`~~ — done.
3. ~~Port the three `if/then/else` shapes~~ — done.
4. ~~Port `for...to...by...do`~~ — done (still `CodeBlock`-dependent, see above).
5. ~~Port `foreach...do`~~ — done (also `CodeBlock`-dependent in places, see
   above; introduced `Builder::Custom()`).
6. ~~Port `try...onException...do`~~ — done. Turned out to be the shortest
   port yet (see above) — every real matcher is now on the pattern engine.
7. ~~Implement `foreach...collect` and `and`~~ — done. `foreach...collect`
   needed a genuine new registration (introduced `Builder::ZeroOrMore()`
   and the new `CFForEachSlotValueCollect` node class); `and` turned out to
   need *no* new code at all, just a stale comment fix (see above for both)
   — **every idiom the plan identified is now either on the pattern engine
   or (for `and`) already correctly handled by it indirectly. Nothing left
   before step 8.**
8. ~~Delete `ASTMacros.h`, `Decompiler::compressAST()` and the Compression
   phase of `Decompiler::solve()`, `CodeBlock`'s list-splicing methods, and
   every now-unreferenced `ResolveXxx()` declaration~~ — done. See the full
   write-up below — this was by far the riskiest stage (it removed a
   mechanism several *other* things turned out to depend on silently), and
   is the one stage that genuinely required going back and forth between
   "looks done" and "is actually correct" more than once. **The whole
   redesign plan is now complete**: every idiom lives on the pattern
   engine, and every trace of the old bundle-into-`CodeBlock` architecture
   is gone. Remaining, deliberately-scoped-out work is listed at the end of
   this section, not as a numbered stage — there is no more "next stage."

**After every single port, re-run the regression recipe above before moving
to the next idiom** — don't batch multiple idiom ports between checks. Also
write at least one direct hand-compiled test per idiom (`-script`, see
below) covering: the plain case, a multi-statement body, and — for anything
with a body that reaches `HandleBreakTargets(..., findPushNil=true)` (i.e.
everything except `loop`, which passes `false`) — a `break <value>` inside
the body. All three mattered in practice: the multi-statement case is what
originally exposed the `PrintBodyChain` gap below.

`Node::UnlinkChain(Node *last)` (added next to `UnlinkRange`) is the
primitive that lets matchers pull a `Statements()`-captured run out of the
list as a walkable chain without needing a `CodeBlock` wrapper. **Every
`ControlBlock`-derived `Print()` that prints a `body_` must go through the
shared `PrintBodyChain(dec, body_, flags=0)` helper in
`ASTControlFlowHelper.cc`, not `body_->PrintOnNewLine(flags)` directly** —
`PrintOnNewLine()` only knows how to print a *single* node; a raw multi-node
chain from `UnlinkChain()` would silently print only its first statement and
drop the rest, since plain `Node::IsMultiStatement()` is `false` by default
(nothing overrides it anymore — see Stage 8 below). `PrintBodyChain()`
detects the single-vs-chain case itself and, for a real chain, honors
`kPrintSuppressBeginEnd`/`kPrintSuppressList` (`CFRepeat` needs
`kPrintSuppressBeginEnd` since `repeat`/`until` are already the delimiters;
`CFLoop`/`CFWhile` don't). This was originally added for `CFLoop`, then
found to be *also* missing on `CFWhile`/`CFRepeat` when those were ported —
check every `ControlBlock` subclass's `Print()` when porting a matcher that
feeds it a `Statements()`-derived body, not just the one you're actively
working on. Stage 8 (below) is exactly this lesson recurring at full scale.

### Stage 8: removing `compressAST()`/`CodeBlock` — what actually broke, and why

This stage looked mechanical going in (delete a pass, delete a now-unused
class, remove a handful of `CodeBlock*`-dependent captures the earlier
stages had already flagged as fragile) and **built and passed the 12-package
regression corpus clean on the very first attempt** — for the raw statement-
run captures (`for`, `foreach...do`, `foreach...collect`'s init/setup). But
`compressAST()` turned out to have a second, silent job nothing had flagged:
merging *any* run of "statement(s) followed by exactly one trailing expr"
into one opaque node, not just for control-flow bodies. Two entirely
different classes of code depended on that second job, and both broke the
moment it was gone — a genuine lesson in why "it built and the diff on file
1 looked fine" is not the same as "it's correct":

1. **`if/then/else`-as-expression and `try`-as-expression bodies.**
   `BuildIfThenElseExprPattern`'s `kBody`/`kElseBody` and
   `BuildTryPattern(isProvider=true)`'s body/handler-body captures used a
   single-node `Required(Tag::Any, slot, IsExpr)` — which implicitly relied
   on `compressAST()` having pre-merged a NewtonScript compound expression
   (`begin stmt1; stmt2; value end`, legal anywhere an expression is
   expected) into one node satisfying `IsExpr()`. Confirmed via the 12-
   package regression: a `return if cond then begin a; b end else nil`
   construct that used to resolve stopped resolving entirely (dumped raw
   bytecode instead). **Fix**: two new `Builder` combinators in
   `ASTPattern.h`, direction-aware (forward and backward), mirroring
   `compressAST()`'s exact old merge boundary:
   - `StatementsThenExpr(slot)` — captures "0+ statements, then exactly 1
     required trailing expr" into `m.run(slot)`, same shape as
     `Statements()`'s captured run, extracted the same way
     (`run.front()->UnlinkChain(run.back())`).
   - `OptionalStatementsThenExpr(slot)` — same, but the whole run may be
     entirely absent (e.g. an exception handler with no body at all before
     its `Branch`) — mirrors `Optional()`'s "no match, mark absent" outcome
     rather than rejecting the whole pattern.
   Applied to `BuildIfThenElseExprPattern` (`kBody`/`kElseBody`) and
   `BuildTryPattern(isProvider=true)` (main body via
   `StatementsThenExpr`, each handler body via
   `OptionalStatementsThenExpr`). The statement-shaped `try` variant
   (`isProvider=false`) had the *same* latent bug — its handler bodies used
   a single-node `Optional(Tag::Any, slot, IsStatement)` — fixed the same
   way with plain `Statements()` (multi-statement handler bodies are
   naturally "0 or more", no trailing-expr requirement needed there).
   These captures are structurally safe to generalize this way because
   `if/then/else` and `try` bodies have **real, unambiguous boundaries**
   (the next `Branch`/`JumpTarget`/`PopHandlers`/`ExceptionHandler` in the
   bytecode) — there's no risk of over-reaching into unrelated preceding
   code, unlike case 2 below.

2. **`CFTry`'s own constructor re-derives the same shape independently.**
   Easy to miss: the pattern *spec* validates the body/handler-body shapes
   above, but `CFTry`'s constructor (`ASTControlFlowHelper.cc`) never reads
   those captured slots back — per its own class comment, it re-walks the
   raw node list from `first` (the anchor) to `last` (`jtDone`) itself,
   rediscovering the same boundaries. Fixed in parallel with the pattern
   captures: `ConsumeOptionalBody(Node *&it, bool isProvider, Node **outTail)`
   (local to `ASTControlFlowHelper.cc`) walks the same "0+ statements, then
   optionally 1 expr" shape directly against the still-fully-linked list.
   The subtler half of this fix: the constructor's original blanket cleanup
   sweep (`while (first->next && first->next != last) first->next->Unlink();`)
   unlinks nodes **one at a time**, and plain `Node::Unlink()` nulls out the
   unlinked node's own `prev`/`next` — fine for genuine single-node
   scaffolding (`PopHandlers`, `Branch`, `ExceptionHandler`, trailing
   `JumpTarget`s), but it would silently **truncate a multi-node body chain
   to its head node** if run over one, since each `Unlink()` in the sweep
   severs that node's `next` before printing ever sees it. Fix: detach
   `body_` and each handler's body via `UnlinkChain()` (preserves internal
   links) the moment it's identified, *before* the generic single-node
   sweep runs over whatever's left (which, once bodies are pre-detached, is
   only ever genuine single scaffolding nodes — safe for plain `Unlink()`).

3. **`CFForLoop`'s body was *never* actually multi-statement-safe**, even
   before Stage 8 — `BuildForLoopPattern`'s `kBody` used a single-node
   `Optional(Tag::Any, kBody, IsStatement)` the whole time; it only ever
   worked because `compressAST()` pre-merged a real multi-statement body
   into one `CodeBlock` first. Same fix pattern as case 1: switched to
   `Statements(kBody)` (already exists, no new combinator needed here) and
   `CFForLoop::Print()`'s `body_->PrintOnNewLine()` → `PrintBodyChain(dec,
   body_)`, same as the other `ControlBlock` subclasses. Also fixed the
   equivalent single-node-assuming `Print()` bugs (same class of issue as
   case 1, just on the print side, not the capture side) in
   `CFForEachSlotValueDo::Print()`, `ExceptionHandler::Print()`, and
   `CFTry::Print()` — all switched to `PrintBodyChain()`. `CFIfThen::Print()`
   had two branches gated on `body_->IsMultiStatement()`/
   `elseBody_->IsMultiStatement()` that are now **permanently dead** (that
   virtual is `false` everywhere except the now-deleted `CodeBlock`) —
   removed them; the sibling branch each one gated against (a plain
   `for (Node *it = body_; it; it = it->next)` loop) was already
   chain-safe and is now the only path taken, so this is a pure
   dead-code removal, not a behavior change.
   `CFForEachSlotValueCollect::Print()`'s `body_->Print()` stayed
   untouched — its body is always a single expression (`BCSetARef::Element()`),
   never a chain, by construction.

4. **The one dead end, explicitly not repeated**: a nested `for` loop with
   an arithmetic operand computed by an interleaved statement —
   `loc7 := loc11 + begin loc3 := 7 - (loc8 - 1); loc3 end` in the old
   `compressAST()`-based baseline — regressed to a raw unresolved dump
   (confirmed via the 12-package corpus, `SolitoDeluxe2.5/Games/Pyramid.pkg`
   → `setupCards`). Root cause: `Consume2::Resolve()` (`ASTAdmin.cc`, backs
   `BinaryOperator` and friends) requires strict adjacency
   (`prev->IsExpr() && prev->prev->IsExpr()`) — it never had a
   `compressAST()`-equivalent fallback of its own, it just benefited from
   one running first. **First attempted fix (reverted, do not retry
   without a fundamentally different approach)**: a generic
   `FindOperandBackward()` that let *any* `Consume1`/`Consume2`/`ConsumeN`
   walk backward past an interleaved `IsStatement()` node to find its real
   operand. This is **unsound** and was caught only by the regression
   corpus, not by reasoning about it in advance: it cannot distinguish "a
   statement that computes the exact value this operand's `GetVar` then
   reads" (the one legitimate case) from "there is no pending expression
   value here at all, and the nearest expr further back belongs to
   something completely unrelated" (e.g. a function's own trailing
   implicit-`nil`-return `PushConst` sitting *after* an unrelated `SetVar`
   statement) — list adjacency plus `IsStatement()`/`IsExpr()` alone is not
   enough information to tell those apart. Tried on the 12-package corpus:
   it "fixed" the one real case but silently produced **wrong** (not just
   unresolved) output on 9 of the other 12 files, mostly by having
   `Consume2` reach back through an entire run of prior statements to grab
   some unrelated earlier expression as an operand. Reverted in full
   (`ASTAdmin.h`/`.cc` are back to their pre-Stage-8 state, confirmed via
   `git diff --stat`). **Current status**: this one nested-arithmetic-
   operand shape is left unresolved (dumps raw bytecode, same as any other
   not-yet-handled shape) — consistent with this codebase's core
   discipline of "never wrong, only unresolved." If it recurs often enough
   in the real corpus to be worth fixing, the sound fix is almost
   certainly narrower and more structural than a generic backward skip —
   e.g. recognizing the specific "`SetVar x`, immediately followed later by
   a `GetVar x` that's itself operand N of an outer Consume" shape, or
   giving `pattern::TryResolve` a chance to run *before* generic DataFlow
   operand consumption for exactly this case — not a blanket change to
   `Consume1`/`Consume2`/`ConsumeN`'s operand search.

5. **Everything else genuinely improved, not just "stayed the same".**
   Several of the 12 corpus files came out *better* than the pre-Stage-8
   baseline, not just different — `compressAST()`'s greedy merging had been
   silently over-reaching in ways nobody had previously noticed, folding
   entirely unrelated preceding statements into a nearby `or`'s left
   operand whenever they happened to sit next to each other in the flat
   list (e.g. a dozen unrelated statements swallowed into
   `if begin ...lots of unrelated code...; cond end or cond2 then ...`,
   where the correct reading is those statements standing on their own,
   followed by a plain `if cond or cond2 then ...`). One corpus file's
   "13 unresolved nodes" warning (present on 3 separate functions in the
   pre-Stage-8 baseline) is now gone on 2 of the 3 and unchanged
   (confirmed byte-identical, a pre-existing unrelated limitation) on the
   third.

**Net result, verified via the full recipe** (12-package corpus vs. the
pre-Stage-8 baseline, plus fresh hand-written `-script` tests for
multi-statement if/then/else-as-expression, try-as-expression, try-as-
statement with mixed single/multi/absent handler bodies, multi-statement
`for`/`foreach...do` bodies, and the single/empty-body edge cases): zero
regressions, several corpus files strictly improved, one narrow and now
well-understood limitation left unresolved (case 4 above) rather than
silently wrong.

### What's left (not a numbered stage — deliberately out of scope here)
- The `argFrame`/`numArgs` encoding discrepancy from the diagnostic logging
  described above (`viewSetupFormScript`) — root cause not investigated.
- The nested-arithmetic-operand case from Stage 8 item 4 above.
- The pre-existing "13/12 unresolved nodes" corpus failures already
  catalogued in `Decompiler.cc`'s scratch-log comment block — unrelated to
  this redesign, never claimed to be fixed by it.

## Hard-won C++ gotcha (don't re-discover this)

`Decompiler` holds `std::vector<std::unique_ptr<ast::Node>> nodePool_` as
its node arena, but `ast::Node` is only forward-declared in `Decompiler.h`
(the full AST class hierarchy lives in headers most `.cc` files — e.g.
`ObjectPrinter.cc` — never include). This blows up in a non-obvious way:

- Separating the **destructor** into the `.cc` file (`~Decompiler();`
  declared in the header, defined in `Decompiler.cc`) is necessary but
  **not sufficient**.
- **Any constructor with a body defined inline in the header — even an
  empty `{ }`** — makes the compiler generate exception-unwind logic for
  already-constructed members in case a later member's initializer throws.
  That unwind logic needs `nodePool_`'s destructor instantiated *right
  there*, which needs `ast::Node` complete, which fails in any TU that
  doesn't happen to include the full AST headers.
- Fix: **every constructor**, not just the destructor, must be declared
  only in the header and defined out-of-line in `Decompiler.cc` (where the
  full AST hierarchy is visible). Also: an out-of-line destructor defined
  as `= default` is still "explicitly defaulted" and has its exception
  specification computed at the point of *declaration* (i.e., in the
  header) — use a plain empty body `{ }` instead, which counts as
  "user-provided" and gets the ordinary `noexcept(true)` default without
  inspecting members at all.

This cost significant back-and-forth to isolate via minimal repros; if a
similar "incomplete type" error resurfaces after touching `Decompiler`'s
special member functions, this is almost certainly the same root cause.

## Enum-scoping footgun in ASTControlFlowPatterns.cc

Each `BuildXxxPattern()` needs its own slot-id `enum { kBody, kJt1, ... }`.
Declare it **inside the function**, not at file/anonymous-namespace scope —
a plain (unscoped) `enum` declared at file scope dumps its enumerators
straight into that scope, so a second pattern's `enum { kBody, ... }` a few
lines later is a redefinition error. A local `enum` inside
`BuildXxxPattern()` is still perfectly usable from the nested `.Build([](...)
{ ... })` lambda without capturing it — enumerators are compile-time
constants, not variables, so this isn't a capture-list issue, just a
scoping one.

## Namespace footgun

Free functions declared inside `namespace ast { ... }` in a header, but
defined in a `.cc` file that only does `using namespace ast;` (rather than
wrapping the definition in `namespace ast { ... }` or qualifying it as
`ast::FuncName(...)`), silently define a **different, global-namespace**
function — compiles fine, fails at link time with an "undefined symbol"
that looks unrelated. Bit us once with `JumpPairMatches`; qualify or wrap
free-function *definitions* explicitly, don't rely on `using namespace` for
that.
