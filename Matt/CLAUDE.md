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
| `ASTControlFlow.h` / `.cc` | One node class per branch/loop/exception/call bytecode, plus the hand-written pattern-matching `ResolveXxx()` methods (being migrated out, see below) |
| `ASTControlFlowHelper.h` / `.cc` | Synthetic "resolved" nodes (`CFLoop`, `CFWhile`, `CFIfThen`, `CFForLoop`, `CFTry`, `CodeBlock`, `JumpTarget`, …) built once a pattern matches |
| `ASTPattern.h` / `.cc` | **New**: declarative pattern-combinator engine (see below) |
| `ASTControlFlowPatterns.cc` | **New**: where control-flow idioms get registered as `pattern::Spec`s |
| `ASTMacros.h` | Old `REQUIRED_NODE`/`OPTIONAL_NODE`/`REQUIRED_COND`/`OPTIONAL_COND` macros, still used by not-yet-ported matchers |
| `Decompiler.h` / `.cc` | Driver: decodes raw bytecode into the initial node list (`generateAST`), runs the fixpoint resolve loop (`solve`), prints source (`printSource`). Owns all AST nodes (`nodePool_`/`MakeNode<T>`). |
| `Printer.h`/`.cc`, `ObjectPrinter.h`/`.cc` | Generic text-layout + Newton-object/source pretty-printer, reused by the decompiler for output |

Unrelated tools that happen to live in `Matt/` (not touched by this work):
`BookWriter.*` (package→HTML/PDF), `PackageWriter.*` (Ref→package binary),
`PDFGen/` (vendored C PDF lib).

## Branch

This work lives on `AST_pattern_matching` (branched off `restructure`). Matt
reviews and commits each stage himself as it lands:
`455fd20 "Initial commit for new pattern matching AST"` (arena,
`JumpPairMatches`, the pattern engine, `loop`), then
`8b8cecf "Adding patterns for While-Do and Repeat-Until"` (which, despite
the name, also included `or`). Whatever's ported since the latest commit
(currently: `if`/`then`/`else`, all three shapes) is uncommitted until he
does the same.

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
  `Repeat`/`JumpPair`/`Guard`/`Name`/`Priority`, ending in `.Build(callback)`.
  `NonEmpty(slot)` rejects the match unless a prior `Statements(slot)`
  captured at least one node — needed for idioms (like `if...then`) that,
  unlike `loop`/`while`/`repeat`, never default to a `nil` body.
- `Register(spec)` / `TryResolve(anchor)` — a `Tag`-indexed registry; a new
  idiom is a **new file-scope static registration**, zero edits to the
  anchor bytecode's own class.

`Matt/ASTControlFlowPatterns.cc` is where idioms get registered. Ported so
far: `loop...end`, `while...do...end`, `repeat...until...end`, `a or b`,
all three `if...then...[else...]` shapes (bare, statement/statement,
expr/expr), and `for...to...by...do`. `BCBranch::ResolveLoop()`,
`BCBranchIfTrue::ResolveWhileDo()`, `BCBranchIfTrue::ResolveOr()`,
`BCBranchIfFalse::ResolveRepeatUntil()`, `BCBranchIfFalse::ResolveIfTheElse()`,
and the entire body of `BCBranchLoop::Resolve()` (it had no separate
`ResolveXxx()`, the matcher lived inline) are all **deleted/replaced** in
`ASTControlFlow.cc`/`.h` — `BCBranch`/`BCBranchIfTrue`/`BCBranchIfFalse`/
`BCBranchLoop` have no hand-written matching logic left at all, only
`pattern::TryResolve(this)`. Only `foreach`/`try` are untouched.

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

### Order of remaining work
1. ~~Port `or`~~ — done.
2. ~~Port `while/do`, `repeat/until`~~ — done.
3. ~~Port the three `if/then/else` shapes~~ — done. `and` (currently only
   recovered as print-time sugar in `CFIfThen::Print()`, never matched as
   its own construct) is deliberately deferred to step 7, per the original
   plan — it needs the same shape as the if/else-expr spec above (reusing
   `MakeIfThen`), just with `elseBody` forced to a literal `nil` check.
4. ~~Port `for...to...by...do`~~ — done (still `CodeBlock`-dependent, see above).
5. Port `foreach...do` — first idiom needing the *head* of a run (and,
   per the note above, likely ALSO stuck reaching into a `CodeBlock` for
   its slot/value setup statements until Stage 8).
6. Port `try...onException...do` — exercises `Repeat()` for real.
7. Only once all of the above are green: implement `foreach...collect` and
   `and` as fresh registrations — this is the acceptance test for the whole
   redesign (should now be tractable ~40-line registrations, not 130-line
   near-duplicates).
8. Delete `ASTMacros.h`, `Decompiler::compressAST()` and the Compression
   phase of `Decompiler::solve()`, `CodeBlock`'s list-splicing methods
   (`add`/`moveToBody`/`pop_back`/`pop_front`/`UnlinkIfEmpty`), and every
   now-unreferenced `ResolveXxx()` declaration.

**After every single port, re-run the regression recipe above before moving
to the next idiom** — don't batch multiple idiom ports between checks. Also
write at least one direct hand-compiled test per idiom (`-script`, see
below) covering: the plain case, a multi-statement body, and — for anything
with a body that reaches `HandleBreakTargets(..., findPushNil=true)` (i.e.
everything except `loop`, which passes `false`) — a `break <value>` inside
the body. All three mattered in practice: the multi-statement case is what
originally exposed the `PrintBodyChain` gap below.

`CodeBlock` currently still exists as a *mid-resolution* list node built by
`compressAST()` (a still-active separate pass) — it has **not** yet been
demoted to print-time-only. `Node::UnlinkChain(Node *last)` (added next to
`UnlinkRange`) is the primitive that lets matchers pull a
`Statements()`-captured run out of the list as a walkable chain without
needing a `CodeBlock` wrapper. **Every `ControlBlock`-derived `Print()`
that prints a `body_` must go through the shared `PrintBodyChain(dec,
body_, flags=0)` helper in `ASTControlFlowHelper.cc`, not
`body_->PrintOnNewLine(flags)` directly** — `PrintOnNewLine()` only knows
how to print a *single* node (or a real `CodeBlock*`, via its overridden
`IsMultiStatement()`); a raw multi-node chain from `UnlinkChain()` would
silently print only its first statement and drop the rest, since plain
`Node::IsMultiStatement()` is `false` by default. `PrintBodyChain()`
detects the single-vs-chain case itself and, for a real chain, honors
`kPrintSuppressBeginEnd`/`kPrintSuppressList` exactly like `CodeBlock::Print()`
does (`CFRepeat` needs `kPrintSuppressBeginEnd` since `repeat`/`until` are
already the delimiters; `CFLoop`/`CFWhile` don't). This was originally added
for `CFLoop`, then found to be *also* missing on `CFWhile`/`CFRepeat` when
those were ported — check every `ControlBlock` subclass's `Print()` when
porting a matcher that feeds it a `Statements()`-derived body, not just the
one you're actively working on.

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
