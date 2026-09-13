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
- `Builder` — the DSL: `Required`/`Optional`/`Statements`/`Repeat`/
  `JumpPair`/`Guard`/`Name`/`Priority`, ending in `.Build(callback)`.
- `Register(spec)` / `TryResolve(anchor)` — a `Tag`-indexed registry; a new
  idiom is a **new file-scope static registration**, zero edits to the
  anchor bytecode's own class.

`Matt/ASTControlFlowPatterns.cc` is where idioms get registered. `loop...end`
is ported there (`BCBranch::ResolveLoop()` is deleted); everything else
still runs through the old `ResolveXxx()` methods in `ASTControlFlow.cc`.

### Order of remaining work (not done yet)
1. Port `or` (second data point).
2. Port `while/do`, `repeat/until`.
3. Port the three `if/then/else` shapes (bare-if, if/else-statement,
   if/else-expression) as three registrations sharing one construction
   helper — this is also where `and` (currently only recovered as print-time
   sugar in `CFIfThen::Print()`, never matched as its own construct) should
   eventually slot in as a fourth registration.
4. Port `for...to...by...do` — first idiom needing the *tail* of a captured
   statement run.
5. Port `foreach...do` — first idiom needing the *head* of a run.
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
to the next idiom** — don't batch multiple idiom ports between checks.

`CodeBlock` currently still exists as a *mid-resolution* list node built by
`compressAST()` (a still-active separate pass) — it has **not** yet been
demoted to print-time-only. `Node::UnlinkChain(Node *last)` (added this
session, next to `UnlinkRange`) is the primitive that will let matchers pull
a `Statements()`-captured run out of the list as a walkable chain without
needing a `CodeBlock` wrapper; `CFLoop::Print()` already uses a
`PrintBodyChain()` helper (in `ASTControlFlowHelper.cc`) that handles either
a single node or a real N-node chain, as a template for how the other
`ControlBlock`-derived `Print()` methods should be updated once their
matchers stop depending on `compressAST()`.

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

## Namespace footgun

Free functions declared inside `namespace ast { ... }` in a header, but
defined in a `.cc` file that only does `using namespace ast;` (rather than
wrapping the definition in `namespace ast { ... }` or qualifying it as
`ast::FuncName(...)`), silently define a **different, global-namespace**
function — compiles fine, fails at link time with an "undefined symbol"
that looks unrelated. Bit us once with `JumpPairMatches`; qualify or wrap
free-function *definitions* explicitly, don't rely on `using namespace` for
that.
