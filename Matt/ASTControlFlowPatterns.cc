
/*
 File:    Matt/ASTControlFlowPatterns.cc

 Matt's decompiler Abstract Syntax Tree.
 Control-flow idioms expressed as declarative pattern::Spec registrations,
 built with the combinator engine in Matt/ASTPattern.h.

 This file is where NEW idioms get added: a registration here needs no edit
 to any Bytecode-derived node class (they only need a tag() override, added
 once in ASTControlFlow.h/ASTDataFlow.h/ASTControlFlowHelper.h). See
 Matt/ASTPattern.h and the design note at AST.cc:10ff for the rationale.

 Each BuildXxxPattern() declares its own local `enum { ... }` for slot ids --
 kept function-local (not file-scope) so unscoped enumerators like kBody/kJt1
 don't collide between patterns.

 Written by:  Matt, 2025.
 */

#include "Matt/ASTPattern.h"
#include "Matt/ASTControlFlow.h"
#include "Matt/ASTControlFlowHelper.h"
#include "Matt/ASTDataFlow.h"
#include "Matt/Decompiler.h"

using namespace ast;
using namespace ast::pattern;

namespace {

#pragma mark - loop ... end

/**
 \brief `loop ... end`: an unconditional backward jump, optionally preceded
 by a body (today: at most one node -- a lone statement, or a CodeBlock*
 that Decompiler::compressAST() has already merged from several -- since
 that compression phase still runs before this pattern is tried; ported
 matchers that no longer depend on it may capture longer runs here).
 Ported from the hand-written BCBranch::ResolveLoop() (removed from
 ASTControlFlow.cc) as the first idiom migrated to the pattern engine; see
 the design note at AST.cc:10ff.
 */
Spec BuildLoopPattern() {
  enum { kBody, kJt };
  return Builder(Tag::Branch, kBwd)
    .Guard([](Node *a) { return a->b() <= a->pc(); })  // jump must be backward
    .Statements(kBody)
    .Required(Tag::JumpTarget, kJt, /*mustBeResolved=*/false)
    .JumpPair(Builder::kAnchor, kJt)
    .Name("loop")
    .Priority(10)
    .Build([](Decompiler &dec, Node *anchor, Match &m) -> Node* {
      auto *jt = m.as<JumpTarget>(kJt);

      // Check for a trailing "break targets", same as the old ResolveLoop().
      Node *afterAnchor = anchor->next;
      Node::HandleBreakTargets(jt, afterAnchor, false);

      const std::vector<Node*> &bodyRun = m.run(kBody);
      Node *body = bodyRun.empty()
        ? anchor->NewNil()
        : bodyRun.front()->UnlinkChain(bodyRun.back());

      auto *loop = dec.MakeNode<CFLoop>(dec, anchor->pc(), kProvidesOne, body);
      jt->Unlink();
      anchor->ReplaceWith(loop);
      dec.numASTChanges++;
      return loop;
    });
}

bool registerLoop = [] { Register(BuildLoopPattern()); return true; }();

#pragma mark - while ... do ... end

/**
 \brief `while cond do body end`: same shape as `loop`, but the backward
 branch is conditional (BranchIfTrue jumping back to retest `cond`) and is
 itself preceded by a second, unconditional `Branch` that jumps forward to
 the test -- i.e. the loop tests before its first iteration.
 ```
 Branch jt2            -- branch2: jump to the test, skipping the body once
 JumpTarget jt1:
 <body>                -- optional, at most one node while compressAST() runs
 JumpTarget jt2:
 [<condPrefix>]         -- 0+ statements, see below
 <cond>                -- anchor's Input(), set by BCBranchIfTrue::Resolve()
 BranchIfTrue jt1       -- anchor
 ```
 Ported from the hand-written BCBranchIfTrue::ResolveWhileDo() (removed from
 ASTControlFlow.cc); walks backward from the anchor, mirroring `loop`.

 `<cond>` may itself be preceded by 0+ ordinary statements (e.g. `i :=
 StrPos(...); i` used as the test -- a statement computing a value,
 immediately followed by reading it back as the actual boolean), found via
 corpus-scale testing (Test/run_corpus.py). `BCBranchIfTrue::Resolve()`'s
 own pre-consumption of `<cond>` (before this Builder chain even starts)
 only ever grabs the single trailing node -- deliberately: that
 pre-consumption is shared with BuildOrPattern's `a or b`, whose left
 operand has no reliable boundary marker the way a loop's condition does,
 so walking backward through statements there would (and, when tried,
 did) silently absorb unrelated preceding code. Extending the condition
 backward is therefore done here instead, in a Custom() step, specifically
 because by this point we're already committed to trying `while` and not
 `or` -- walk backward through statements *looking for* `kJt2` (a
 JumpTarget, the loop's own test-label landing, so still a provably
 bounded search, not guesswork); anything found is folded into
 `<cond>` as one CompoundExpr (ASTControlFlowHelper.h) in the callback.
 */
Spec BuildWhilePattern() {
  enum { kJt2, kCondPrefix, kBody, kJt1, kBranch2 };
  return Builder(Tag::BranchIfTrue, kBwd)
    .Guard([](Node *a) { return a->b() <= a->pc(); })  // jump must be backward
    .Custom([](Cursor &c, Match &m) -> bool {
      std::vector<Node*> prefix;
      Node *nd = c.peek();
      while (nd && nd->IsStatement()) {
        prefix.push_back(nd);
        c.advance();
        nd = c.peek();
      }
      auto *jt2 = dynamic_cast<JumpTarget*>(nd);
      if (!jt2) return false;
      c.advance();
      if (c.direction() == kBwd) std::reverse(prefix.begin(), prefix.end());
      m.SetRun(kCondPrefix, std::move(prefix));
      m.SetNode(kJt2, jt2);
      return true;
    })
    .Statements(kBody)
    .Required(Tag::JumpTarget, kJt1, /*mustBeResolved=*/false)
    .JumpPair(Builder::kAnchor, kJt1)
    .Required(Tag::Branch, kBranch2, /*mustBeResolved=*/false)
    .JumpPair(kBranch2, kJt2)
    .Name("while-do")
    .Priority(10)
    .Build([](Decompiler &dec, Node *anchor, Match &m) -> Node* {
      auto *jt1 = m.as<JumpTarget>(kJt1);
      auto *jt2 = m.as<JumpTarget>(kJt2);
      Node *branch2 = m.node(kBranch2);

      // Check for a trailing "push-nil, break targets, consumer".
      Node *afterAnchor = anchor->next;
      int prov = Node::HandleBreakTargets(branch2, afterAnchor, true);

      const std::vector<Node*> &bodyRun = m.run(kBody);
      Node *body = bodyRun.empty()
        ? anchor->NewNil()
        : bodyRun.front()->UnlinkChain(bodyRun.back());

      auto *branchIfTrue = static_cast<BCBranchIfTrue*>(anchor);
      const std::vector<Node*> &condPrefix = m.run(kCondPrefix);
      if (!condPrefix.empty()) {
        // Splice the already-consumed single-node condition (Input(), from
        // BCBranchIfTrue::Resolve()) onto the end of the statement prefix
        // captured above, and wrap the whole chain as one CompoundExpr.
        Node *tail = branchIfTrue->Input();
        Node *prefixHead = condPrefix.front()->UnlinkChain(condPrefix.back());
        condPrefix.back()->next = tail;
        tail->prev = condPrefix.back();
        branchIfTrue->Input(dec.MakeNode<CompoundExpr>(dec, prefixHead->pc(), prefixHead));
      }
      Node *cond = branchIfTrue->Input();
      auto *wd = dec.MakeNode<CFWhile>(dec, anchor->pc(), prov, cond, body);
      branch2->Unlink();
      jt1->Unlink();
      jt2->Unlink();
      anchor->ReplaceWith(wd);
      dec.numASTChanges++;
      return wd;
    });
}

bool registerWhile = [] { Register(BuildWhilePattern()); return true; }();

#pragma mark - a or b

/**
 \brief `a or b`: BranchIfTrue skips straight to pushing `true` when `a` is
 true; otherwise it falls through to evaluate and push `b`, then jumps past
 the `push true`.
 ```
 <a>                  -- already in anchor's Input()
 BranchIfTrue jt2
 <b>                  -- alt
 Branch jt1
 JumpTarget jt2:
 PushConst true
 JumpTarget jt1:
 ```
 Ported from the hand-written BCBranchIfTrue::ResolveOr() (removed from
 ASTControlFlow.cc). The two original single-sided checks
 (`jt1->Origin() != pc()`, `jt2->Origin() != branch->pc()`) are strengthened
 here to full JumpPair() checks: since AddToTargets() creates exactly one
 JumpTarget per branch instruction, keyed by that instruction's own pc() as
 Origin(), `jt->Origin() == x->pc()` already implies `jt->pc() == x->b()` --
 so this is behaviorally identical, just explicit about both halves.
 */
Spec BuildOrPattern() {
  enum { kAlt, kBranch, kJt1, kRetTrue, kJt2 };
  return Builder(Tag::BranchIfTrue, kFwd)
    .Guard([](Node *a) {
      auto *input = static_cast<BCBranchIfTrue*>(a)->Input();
      return input && input->IsExpr();
    })
    .Required(Tag::Any, kAlt, [](Node *n) { return n->IsExpr(); }, /*mustBeResolved=*/true)
    .Required(Tag::Branch, kBranch, /*mustBeResolved=*/false)
    .Required(Tag::JumpTarget, kJt1, /*mustBeResolved=*/false)
    .Required(Tag::PushConst, kRetTrue, [](Node *n) { return n->b() == TRUEREF; }, /*mustBeResolved=*/true)
    .Required(Tag::JumpTarget, kJt2, /*mustBeResolved=*/false)
    .JumpPair(Builder::kAnchor, kJt1)
    .JumpPair(kBranch, kJt2)
    .Name("or")
    .Priority(10)
    .Build([](Decompiler &dec, Node *anchor, Match &m) -> Node* {
      Node *alt = m.node(kAlt);
      Node *branch = m.node(kBranch);
      Node *jt1 = m.node(kJt1);
      Node *retTrue = m.node(kRetTrue);
      Node *jt2 = m.node(kJt2);

      alt->Unlink();
      branch->Unlink();
      jt1->Unlink();
      retTrue->Unlink();
      jt2->Unlink();

      Node *in = static_cast<BCBranchIfTrue*>(anchor)->Input();
      auto *orNode = dec.MakeNode<CFOr>(dec, anchor->pc(), in, alt);
      anchor->ReplaceWith(orNode);
      dec.numASTChanges++;
      return orNode->next;
    });
}

bool registerOr = [] { Register(BuildOrPattern()); return true; }();

#pragma mark - if ... then ... [else ...]

/**
 \brief Shared construction step for all three if/then/else shapes below:
 wire up a CFIfThen from already-captured (but not yet unlinked from the
 JumpTarget/Branch scaffolding) pieces, unlink that scaffolding, and splice
 the new node in. `body`/`elseBody` must already be unlinked from the root
 list by the caller (their extraction differs: a statement run needs
 UnlinkChain(), a single expr node just needs Unlink()); `elseBody == nullptr`
 means "no else clause" (bi2/jt2 are then ignored).
 */
Node *MakeIfThen(Decompiler &dec, Node *anchor, bool returnsAValue,
                  Node *body, JumpTarget *jt1,
                  Node *elseBody, Node *bi2, JumpTarget *jt2) {
  Node *cond = static_cast<BCBranchIfFalse*>(anchor)->Input();
  auto *newNode = dec.MakeNode<CFIfThen>(dec, anchor->pc(), cond, returnsAValue);
  newNode->body_ = body;
  jt1->Unlink();
  if (elseBody) {
    newNode->elseBody_ = elseBody;
    bi2->Unlink();
    jt2->Unlink();
  }
  anchor->ReplaceWith(newNode);
  dec.numASTChanges++;
  return newNode;
}

/**
 \brief `if cond then body end` (bare, no else): a forward BranchIfFalse
 whose target JumpTarget follows immediately after the (non-empty)
 statement body, with no intervening Branch/else scaffolding at all.
 ```
 BranchIfFalse jt1      -- anchor
 <body>                 -- non-empty statement run
 JumpTarget jt1:
 ```
 This spec and BuildIfThenElsePattern() below are mutually exclusive by
 construction, not by priority: after the body run, the very next node is
 either a JumpTarget (this spec) or a Branch (the with-else spec) -- never
 both -- so which one matches is fully determined by the bytecode, not by
 try-order.
 Ported from the hand-written BCBranchIfFalse::ResolveIfTheElse() (removed
 from ASTControlFlow.cc), which handled all three if/then/else shapes in
 one function; see the class comment above BCBranchIfFalse::Resolve() there
 for the original's own description of the three patterns.
 `kBody` is captured via StatementsOrExpr() (ASTPattern.h), not plain
 Statements(), for symmetry with BuildIfThenElsePattern() below, whose
 `kElseBody` genuinely needs it: a real corpus case (`if i >= Length(x)
 then break; end` as the *last* real statement before an implicit `else`)
 compiles its else-branch as a single bare `PushConst nil` -- an
 expression, never IsStatement() -- standing in for the omitted `else`.
 See BCBranch::ResolveBreak()'s class comment (ASTControlFlow.cc) for the
 companion fix a `break`-only "then" body needed there, and
 BuildIfThenElsePattern() below for the actual bug this combinator fixes.
 */
Spec BuildIfThenPattern() {
  enum { kBody, kJt1 };
  return Builder(Tag::BranchIfFalse, kFwd)
    .Guard([](Node *a) { return a->pc() <= a->b(); })  // jump must be forward
    .StatementsOrExpr(kBody)
    .NonEmpty(kBody)
    .Required(Tag::JumpTarget, kJt1, /*mustBeResolved=*/false)
    .JumpPair(Builder::kAnchor, kJt1)
    .Name("if-then")
    .Priority(5)
    .Build([](Decompiler &dec, Node *anchor, Match &m) -> Node* {
      auto *jt1 = m.as<JumpTarget>(kJt1);
      const std::vector<Node*> &bodyRun = m.run(kBody);
      Node *body = bodyRun.front()->UnlinkChain(bodyRun.back());
      return MakeIfThen(dec, anchor, /*returnsAValue=*/false, body, jt1,
                         /*elseBody=*/nullptr, /*bi2=*/nullptr, /*jt2=*/nullptr);
    });
}

bool registerIfThen = [] { Register(BuildIfThenPattern()); return true; }();

/**
 \brief `if cond then body else elseBody end`, both branches statements:
 ```
 BranchIfFalse jt1      -- anchor
 <body>                 -- non-empty statement run, possibly ending in one
                            bare expression (see below)
 Branch jt2
 JumpTarget jt1:
 <elseBody>             -- same shape as <body>
 JumpTarget jt2:
 ```
 See BuildIfThenPattern() above for why this and the bare if-then never
 both match the same bytecode.

 `kBody`/`kElseBody` are captured via StatementsOrExpr() (ASTPattern.h),
 not plain Statements() -- found necessary via corpus-scale testing
 (Test/run_corpus.py) on the real idiom `if i >= Length(x) then break; end`
 used as one statement among several in an enclosing block, with no
 explicit `else`: NTK's compiler still emits full if/then/else scaffolding
 for it (there's no bytecode-level "bare if" special case once other code
 follows), synthesizing an implicit `else` whose entire content is one bare
 `PushConst nil` -- an expression standing in for "nothing," never
 IsStatement() -- which plain `Statements()` could never capture (its
 `NonEmpty()` check would always reject an all-expression run). The `kBody`
 side of the same idiom needed a companion fix, not this one: see
 BCBranch::ResolveBreak()'s class comment (ASTControlFlow.cc) for why a
 `break`-only "then" body couldn't even resolve to a capturable `CFBreak`
 statement in the first place before that fix.
 */
Spec BuildIfThenElsePattern() {
  enum { kBody, kBi2, kJt1, kElseBody, kJt2, kTrailingPop };
  return Builder(Tag::BranchIfFalse, kFwd)
    .Guard([](Node *a) { return a->pc() <= a->b(); })  // jump must be forward
    // Neither kBody nor kElseBody requires NonEmpty(): a genuinely empty
    // branch -- e.g. `if arg0 <> nil then end else <real work> end`, found
    // via corpus-scale testing (Test/run_corpus.py) -- is a real, if
    // unusual, NewtonScript shape (equivalent to `if arg0 = nil then <real
    // work> end`, apparently compiled by testing the *complement* of the
    // written condition rather than negating it, still using the full
    // if/then/else scaffolding with one side simply empty). The callback
    // falls back to NewNil() for an empty run, same as every other
    // idiom's optional/possibly-empty body (loop/while/repeat/for/foreach).
    .StatementsOrExpr(kBody)
    .Required(Tag::Branch, kBi2, /*mustBeResolved=*/false)
    .Required(Tag::JumpTarget, kJt1, /*mustBeResolved=*/false)
    .JumpPair(Builder::kAnchor, kJt1)
    .StatementsOrExpr(kElseBody)
    .Required(Tag::JumpTarget, kJt2, /*mustBeResolved=*/false)
    .JumpPair(kBi2, kJt2)
    // -- A branch whose captured run ends in a bare expression (see
    // StatementsOrExpr()'s class comment) pushes a value nothing else
    // consumes -- only reachable via kElseBody falling straight through
    // (kBody's own equivalent case, if it's ever real, always diverges
    // instead, e.g. via `break`, so it never falls through to needing this).
    // The compiler balances that stray value with an explicit Pop right
    // after the whole construct, which must be claimed here or it prints
    // as its own bogus leftover statement. Required, not Optional, when
    // needed: if the shape says a Pop must be here and it isn't, that's a
    // real mismatch, not a "fine either way" case.
    .Custom([](Cursor &c, Match &m) -> bool {
      const std::vector<Node*> &elseRun = m.run(kElseBody);
      if (elseRun.empty() || !elseRun.back()->IsExpr()) {
        m.SetAbsent(kTrailingPop);
        return true;
      }
      Node *n = c.peek();
      if (!n || n->tag() != Tag::Pop) return false;
      m.SetNode(kTrailingPop, n);
      c.advance();
      return true;
    })
    .Name("if-then-else")
    .Priority(5)
    .Build([](Decompiler &dec, Node *anchor, Match &m) -> Node* {
      Node *bi2 = m.node(kBi2);
      auto *jt1 = m.as<JumpTarget>(kJt1);
      auto *jt2 = m.as<JumpTarget>(kJt2);
      const std::vector<Node*> &bodyRun = m.run(kBody);
      const std::vector<Node*> &elseRun = m.run(kElseBody);
      Node *body = bodyRun.empty() ? anchor->NewNil() : bodyRun.front()->UnlinkChain(bodyRun.back());
      Node *elseBody = elseRun.empty() ? anchor->NewNil() : elseRun.front()->UnlinkChain(elseRun.back());
      if (m.has(kTrailingPop)) m.node(kTrailingPop)->Unlink();
      return MakeIfThen(dec, anchor, /*returnsAValue=*/false, body, jt1, elseBody, bi2, jt2);
    });
}

bool registerIfThenElse = [] { Register(BuildIfThenElsePattern()); return true; }();

/**
 \brief `if cond then body else elseBody end`, both branches expressions --
 the shape that also doubles as `a and b` sugar when elseBody is a bare
 `nil` (recovered at print time in CFIfThen::Print(), not matched here).
 ```
 BranchIfFalse jt1      -- anchor
 <body: [stmt]* expr>   -- StatementsThenExpr(): a NewtonScript compound
 Branch jt2                expression, `begin stmt1; stmt2; value end`,
 JumpTarget jt1:            can legally appear anywhere an expr is expected
 <elseBody: [stmt]* expr>
 JumpTarget jt2:
 ```
 `body`/`elseBody` are captured via StatementsThenExpr() (ASTPattern.h), not
 a single-node `Required(IsExpr)` -- the latter only sees a branch's first
 node and fails to match whenever the branch is a multi-statement compound
 expression, a real shape now that `compressAST()` (which used to physically
 pre-merge such runs into one CodeBlock) is gone.
 Unlike the two statement-shaped specs above, this one requires the else
 clause (bi2/jt1/jt2 are all Required, not Optional) rather than mirroring
 the original's structurally-optional-but-never-actually-absent branch
 capture. That's a deliberate, documented simplification, not an oversight:
 the original's own class comment states this shape "exists only as
 if/then/else" -- a value-producing conditional needs both sides to produce
 a value for the bytecode to make sense in the first place, so an
 expression-bodied if/then with no else is not just empirically rare but
 structurally impossible. If that assumption is ever wrong for some package,
 this spec will simply fail to match (never a wrong answer) and that
 function will show up as an unresolved node -- a visible, detectable
 failure, not a silent one.
 */
Spec BuildIfThenElseExprPattern() {
  enum { kBody, kBi2, kJt1, kElseBody, kJt2 };
  return Builder(Tag::BranchIfFalse, kFwd)
    .Guard([](Node *a) { return a->pc() <= a->b(); })  // jump must be forward
    .StatementsThenExpr(kBody)
    .Required(Tag::Branch, kBi2, /*mustBeResolved=*/false)
    .Required(Tag::JumpTarget, kJt1, /*mustBeResolved=*/false)
    .JumpPair(Builder::kAnchor, kJt1)
    .StatementsThenExpr(kElseBody)
    .Required(Tag::JumpTarget, kJt2, /*mustBeResolved=*/false)
    .JumpPair(kBi2, kJt2)
    .Name("if-then-else-expr")
    // Tried *before* BuildIfThenElsePattern() (priority 5): since
    // StatementsOrExpr() (used there) now also accepts a bare trailing
    // expr per branch, the two specs' matchable shapes genuinely overlap
    // whenever neither branch needs a trailing Pop -- e.g. `cond and expr`
    // (both branches single bare exprs) matches both. Preferring this
    // (expr-shaped) spec whenever it applies is a pure win: same bytecode
    // consumed either way, but only this one preserves CFIfThen::Print()'s
    // "and"/"or" sugar. Safe, not just nicer: whenever a trailing Pop is
    // actually structurally required (a real, different bytecode shape --
    // see BuildIfThenElsePattern()'s kTrailingPop), this spec's own
    // `.Required(Tag::JumpTarget, kJt2)` naturally fails to match a `Pop`
    // sitting there instead, so it correctly falls through to the
    // statement-shaped spec rather than silently mismatching.
    .Priority(4)
    .Build([](Decompiler &dec, Node *anchor, Match &m) -> Node* {
      const std::vector<Node*> &bodyRun = m.run(kBody);
      const std::vector<Node*> &elseRun = m.run(kElseBody);
      Node *body = bodyRun.front()->UnlinkChain(bodyRun.back());
      Node *elseBody = elseRun.front()->UnlinkChain(elseRun.back());
      Node *bi2 = m.node(kBi2);
      auto *jt1 = m.as<JumpTarget>(kJt1);
      auto *jt2 = m.as<JumpTarget>(kJt2);
      return MakeIfThen(dec, anchor, /*returnsAValue=*/true, body, jt1, elseBody, bi2, jt2);
    });
}

bool registerIfThenElseExpr = [] { Register(BuildIfThenElseExprPattern()); return true; }();

#pragma mark - repeat ... until ... end

/**
 \brief `repeat body until cond end`: same shape as `loop`, but the backward
 branch is a BranchIfFalse re-testing `cond` (jumps back to the top while
 `cond` is nil; falls through once `cond` is true).
 ```
 JumpTarget jt1:
 <body>                -- optional, at most one node while compressAST() runs
 <cond>                -- anchor's Input()
 BranchIfFalse jt1      -- anchor
 ```
 Ported from the hand-written BCBranchIfFalse::ResolveRepeatUntil() (removed
 from ASTControlFlow.cc); structurally identical to `loop`'s pattern, just
 anchored on BranchIfFalse with a condition instead of an unconditional
 Branch.
 */
Spec BuildRepeatPattern() {
  enum { kBody, kJt1 };
  return Builder(Tag::BranchIfFalse, kBwd)
    .Guard([](Node *a) { return a->b() <= a->pc(); })  // jump must be backward
    .Statements(kBody)
    .Required(Tag::JumpTarget, kJt1, /*mustBeResolved=*/false)
    .JumpPair(Builder::kAnchor, kJt1)
    .Name("repeat-until")
    .Priority(10)
    .Build([](Decompiler &dec, Node *anchor, Match &m) -> Node* {
      auto *jt1 = m.as<JumpTarget>(kJt1);

      Node *afterAnchor = anchor->next;
      int prov = Node::HandleBreakTargets(jt1, afterAnchor, true);

      const std::vector<Node*> &bodyRun = m.run(kBody);
      Node *body = bodyRun.empty()
        ? anchor->NewNil()
        : bodyRun.front()->UnlinkChain(bodyRun.back());

      Node *cond = static_cast<BCBranchIfFalse*>(anchor)->Input();
      auto *ru = dec.MakeNode<CFRepeat>(dec, anchor->pc(), prov, cond, body);
      jt1->Unlink();
      anchor->ReplaceWith(ru);
      dec.numASTChanges++;
      return ru;
    });
}

bool registerRepeat = [] { Register(BuildRepeatPattern()); return true; }();

#pragma mark - for iter to limit by incr do body end

/**
 \brief `for iter := start to limit by incr do body end`.
 ```
 [[unrelated preceding code]]
 SetVar iter                    -- setIter
 SetVar limit                   -- setLimit
 SetVar incr                    -- setIncr
 GetVar incr                    -- getIncr: pushes incr for BranchLoop's first test,
                                    since brTest below skips straight past incIter
                                    (which would otherwise re-supply it) on iteration 1
 GetVar iter                    -- getIter
 Branch brTest                  -- jump to the test first
 JumpTarget jtAgain:
 <body>                         -- optional, one node
 GetVar incr                    -- re-supplies incr for incIter (and, via incIter
                                    echoing it back, for every later BranchLoop test)
 IncrVar iter                   -- incIter: consumes [iter, incr], produces [iter+incr, incr]
 JumpTarget jtTest:
 GetVar limit                   -- getLimit
 BranchLoop jtAgain              -- anchor: consumes [incr, iter, limit], tests, loops back
 ```
 Ported from the hand-written BCBranchLoop::Resolve() (its whole body used
 to live here, not a separate ResolveXxx() -- BCBranchLoop had no other use
 for Resolve()). `setIter`/`setLimit`/`setIncr` are matched directly here as
 three consecutive Required(SetVar) steps, `getIncr` right after them as a
 fourth -- this became possible only once Decompiler::compressAST() (Stage 8)
 stopped pre-merging them into an opaque CodeBlock; before that, this spec
 had to require a CodeBlock and reach into its tail by fixed offset (see
 git history / Matt/CLAUDE.md for that version and why it was necessary at
 the time). Note the earlier version's own diagram comment had `getIter`
 and `getIncr` swapped -- re-derived here against `-debug ast` output on a
 real `for...by...do` script to get it right before relying on it for this
 redesign; the *code* (which slot each GetVar's value was checked against)
 was always correct, only the prose was wrong.
 */
Spec BuildForLoopPattern() {
  enum {
    kGetLimit, kJtTest, kIncIter, kBody, kJtAgain, kBrTest,
    kGetIter, kGetIncr, kSetIncr, kSetLimit, kSetIter,
  };
  return Builder(Tag::BranchLoop, kBwd)
    .Required(Tag::GetVar, kGetLimit, /*mustBeResolved=*/true)
    .Required(Tag::JumpTarget, kJtTest, /*mustBeResolved=*/false)
    .Required(Tag::IncrVar, kIncIter, /*mustBeResolved=*/false)
    .Statements(kBody)
    .Required(Tag::JumpTarget, kJtAgain, /*mustBeResolved=*/false)
    .Required(Tag::Branch, kBrTest, /*mustBeResolved=*/false)
    .Required(Tag::GetVar, kGetIter, /*mustBeResolved=*/false)
    .Required(Tag::GetVar, kGetIncr, /*mustBeResolved=*/true)
    .Required(Tag::SetVar, kSetIncr, /*mustBeResolved=*/true)
    .Required(Tag::SetVar, kSetLimit, /*mustBeResolved=*/true)
    .Required(Tag::SetVar, kSetIter, /*mustBeResolved=*/true)
    .JumpPair(Builder::kAnchor, kJtAgain)
    .JumpPair(kBrTest, kJtTest)
    .Name("for-to-by-do")
    .Priority(10)
    .Build([](Decompiler &dec, Node *anchor, Match &m) -> Node* {
      auto *getLimit = m.as<BCGetVar>(kGetLimit);
      Node *jtTest = m.node(kJtTest);
      Node *incIter = m.node(kIncIter);
      const std::vector<Node*> &bodyRun = m.run(kBody);
      Node *jtAgain = m.node(kJtAgain);
      Node *brTest = m.node(kBrTest);
      auto *getIter = m.as<BCGetVar>(kGetIter);
      auto *getIncr = m.as<BCGetVar>(kGetIncr);
      auto *setIncr = m.as<BCSetVar>(kSetIncr);
      auto *setLimit = m.as<BCSetVar>(kSetLimit);
      auto *setIter = m.as<BCSetVar>(kSetIter);

      // Check that all three loop locals agree between their SetVar and
      // GetVar occurrences.
      int iter = setIter->b();
      if (getIter->b() != iter) return nullptr;
      // NOTE: faithfully reproduces the original BCBranchLoop::Resolve():
      // this compares getLimit->b() to itself (limit was just assigned FROM
      // getLimit->b(), two lines up), so it can never reject a match. It
      // was almost certainly meant to check setLimit->b() against
      // getLimit->b(), mirroring the iter/incr checks either side of it.
      // Left exactly as-is -- fixing it would be a behavior change, not a
      // faithful port, and nothing in the regression corpus has ever
      // tripped whatever it was meant to catch.
      int limit = getLimit->b();
      if (getLimit->b() != limit) return nullptr;
      int incr = setIncr->b();
      if (getIncr->b() != incr) return nullptr;

      Node *afterAnchor = anchor->next;
      int prov = Node::HandleBreakTargets(brTest, afterAnchor, true);

      getLimit->Unlink();
      jtTest->Unlink();
      incIter->Unlink();
      jtAgain->Unlink();
      brTest->Unlink();
      getIter->Unlink();
      getIncr->Unlink();
      setIncr->Unlink();
      setLimit->Unlink();
      setIter->Unlink();

      // Mark the locals with an alternative use, so they are not declared.
      dec.useLocalAs(incr, Decompiler::Local::Use::iter);
      dec.useLocalAs(limit, Decompiler::Local::Use::iter);
      dec.useLocalAs(iter, Decompiler::Local::Use::iter);

      Node *body = bodyRun.empty() ? anchor->NewNil()
                                    : bodyRun.front()->UnlinkChain(bodyRun.back());
      auto *forLoopNode = dec.MakeNode<CFForLoop>(
        dec, anchor->pc(), prov, setIter, setLimit->Input(), setIncr->Input(), body);
      anchor->ReplaceWith(forLoopNode);
      dec.numASTChanges++;
      return forLoopNode->next;
    });
}

bool registerForLoop = [] { Register(BuildForLoopPattern()); return true; }();

#pragma mark - foreach shared helpers

/**
 \brief Does `setNode` (if non-null) read the iterator's slot at raw
 (tagged-ref) index `rawIndex` -- i.e. is its Input() a `BCARef` of the
 shape `<iterator>[rawIndex]`? `rawIndex` is the raw BCPushConst b() field,
 not a decoded integer (matching how the original matcher compared it: `4`
 for "value" -- decoded index 1 -- and `0` for "slot" -- decoded index 0,
 which happens to be the same either way). Shared by BuildForeachDoPattern
 and BuildForeachCollectPattern, which both extract "value" and optionally
 "slot" this same way.
 */
bool IsIterSlotSetter(BCSetVar *setNode, int rawIndex) {
  if (!setNode) return false;
  auto *aref = dynamic_cast<BCARef*>(setNode->Input());
  if (!aref) return false;
  if (!dynamic_cast<BCGetVar*>(aref->input1())) return false;
  auto *idxConst = dynamic_cast<BCPushConst*>(aref->input2());
  return idxConst && idxConst->b() == rawIndex;
}

/**
 \brief Walk `anchor->prev` (deeplyConst) / `anchor->prev->prev` (setObject)
 directly, independent of the spec's own forward Cursor. Shared by both of
 BCNewIter's idioms (foreach...do, foreach...collect) -- see
 BuildForeachDoPattern's class comment (note 1) for why BCNewIter needs
 this at all.
 */
Step ForeachObjectDeeplyStep(int deeplyConstSlot, int setObjectSlot) {
  return [deeplyConstSlot, setObjectSlot](Cursor&, Match &m) -> bool {
    Node *anchor = m.anchor();
    auto *deeplyConst = dynamic_cast<BCPushConst*>(anchor->prev);
    if (!deeplyConst || !deeplyConst->Resolved()) return false;
    if (deeplyConst->b() != NILREF && deeplyConst->b() != TRUEREF) return false;
    Node *setObject = anchor->prev->prev;
    if (!setObject || !setObject->Resolved() || !setObject->IsExpr()) return false;
    m.SetNode(deeplyConstSlot, deeplyConst);
    m.SetNode(setObjectSlot, setObject);
    return true;
  };
}

/**
 \brief Extract the actual "object" expression from a captured setObject
 node, unlinking it from the list -- shared by both foreach idioms'
 construction callbacks. `setObject` is captured backward from the anchor
 (see ForeachObjectDeeplyStep) as a single resolved expr; before Stage 8
 (compressAST() removal) it could also be a CodeBlock ending in a trailing
 expr (if statements preceded it that got merged in), requiring that
 trailing expr to be split out. That can no longer happen -- nothing merges
 statements into anything anymore, so a captured expr is always already
 exactly one node -- but the split-out step is kept here, not inlined at
 the two call sites, so if that invariant ever turns out to be wrong for
 some real package, there's one place to fix it.
 */
Node *ExtractForeachObject(Node *setObject) {
  setObject->Unlink();
  return setObject;
}

#pragma mark - foreach [slot,] value [deeply] in object do body end

/**
 \brief `foreach [slot,] value [deeply] in object do body end`.
 ```
 [[unrelated preceding code]], <object>       -- setObject (see Custom() step below)
 PushConst deeply
 NewIter                       -- anchor
 SetVar iter
 Branch brStart                -- jump to the "done?" test first
 JumpTarget jtRepeat:
 SetVar value := iterator[4]    -- setValueNode, required
 [SetVar slot  := iterator[0]]  -- setSlotNode, optional
 <body>                         -- whatever statements remain, captured directly
 IterNext iterator
 JumpTarget jtStart:
 IterDone iterator
 BranchIfFalse brRepeat
 [PushConst nil]                 -- pushNil, optional ("clear iterator" is
                                     the node right after it) -- see below
 ```
 Ported from the hand-written BCNewIter::ResolveForeachSlotValueDo() (removed
 from ASTControlFlow.cc). One irregularity this idiom has that no earlier
 port did, handled with Builder::Custom() rather than forced into the named
 combinators: unlike every other anchor ported so far, BCNewIter never runs
 its own Consume2 DataFlow resolution (BCNewIter::Resolved() is hardcoded
 false), so "object"/"deeply" are never wired into Input()/in1_/in2_ -- this
 matcher has always had to walk `prev`/`prev->prev` directly to find them,
 and still does, via ForeachObjectDeeplyStep. Since that step inspects the
 anchor's *backward* neighbors while every other step in this
 (forward-anchored) spec walks *forward*, it deliberately never touches the
 shared Cursor.

 Before Stage 8 (Decompiler::compressAST() removal) this also needed a
 second Custom() step and an Optional(CodeBlock) capture, to handle
 value/slot possibly having been pre-merged with the body into one opaque
 CodeBlock. That's gone now -- nothing merges statements into anything
 anymore, so value/[slot] are always read directly off the flat list, and
 whatever remains after them is just Statements(kBody), same as every
 other idiom's free-form body capture.

 `pushNil` is `Optional()`, not `Required()` -- discovered via corpus-scale
 testing (Test/run_corpus.py), not the 12-package sample: when the loop's
 iterator variable is provably dead afterward, NTK's optimizer omits the
 "clear iterator variable" `SetVar` that normally follows `pushNil`,
 leaving just a bare `PushConst nil; Pop;` pair immediately after
 `brRepeat`. `BCPop::Resolve()` (ASTAdmin.cc) has a DataFlow-time
 optimization that strips exactly that adjacent-`PushConst`-then-`Pop`
 shape *during DataFlow*, before this ControlFlow pattern ever gets a
 chance to claim `pushNil` as a structural marker -- so on this shape,
 `Required()` would never see a raw `PushConst` there at all and the whole
 match would fail, exactly as it did for ~526 of the ~2,200-package first
 corpus sweep (the single largest failure cluster by far -- see
 Matt/CLAUDE.md, "Corpus-scale testing"). When `pushNil` is absent, there's
 also no separate "clear iterator" node to discard: `BCPop`'s optimization
 already erased both, so `brRepeat->next` is already real, unrelated,
 subsequent code -- the callback below only does the `afterPushNil`
 discard/`HandleBreakTargets` dance when `pushNil` was actually captured.
 A hand-compiled `-script` repro of this exact idiom does *not* reproduce
 this shape, since our own compiler always emits the iterator-clearing
 `SetVar` regardless of liveness -- this is a real NTK-optimizer-only
 divergence, only found by testing against the real corpus.
 */
Spec BuildForeachDoPattern() {
  enum {
    kDeeplyConst, kSetObject,
    kSetIter, kBrStart, kJtRepeat,
    kSetValueNode, kSetSlotNode, kBody,
    kIterNext, kJtStart, kIterDone, kBrRepeat, kPushNil,
  };
  return Builder(Tag::NewIter, kFwd)
    // -- Walk backward from the anchor for "object"/"deeply" (see class note).
    .Custom(ForeachObjectDeeplyStep(kDeeplyConst, kSetObject))
    // -- Everything else walks forward from the anchor as usual.
    .Required(Tag::SetVar, kSetIter, /*mustBeResolved=*/false)
    .Required(Tag::Branch, kBrStart, /*mustBeResolved=*/false)
    .Required(Tag::JumpTarget, kJtRepeat, /*mustBeResolved=*/false)
    // -- Read value/[slot] directly off the flat list; whatever's left is body.
    .Custom([](Cursor &c, Match &m) -> bool {
      Node *it = c.peek();
      auto *setValueNode = dynamic_cast<BCSetVar*>(it);
      BCSetVar *setSlotNode = it ? dynamic_cast<BCSetVar*>(it->next) : nullptr;

      // "value" is required: it must read the iterator's value slot (4).
      if (!IsIterSlotSetter(setValueNode, 4)) return false;
      // "slot" is optional: it must read the iterator's tag slot (0).
      if (!IsIterSlotSetter(setSlotNode, 0)) setSlotNode = nullptr;

      c.advance();
      if (setSlotNode) c.advance();

      m.SetNode(kSetValueNode, setValueNode);
      if (setSlotNode) m.SetNode(kSetSlotNode, setSlotNode); else m.SetAbsent(kSetSlotNode);
      return true;
    })
    .Statements(kBody)
    .Required(Tag::IterNext, kIterNext, /*mustBeResolved=*/false)
    .Required(Tag::JumpTarget, kJtStart, /*mustBeResolved=*/false)
    .Required(Tag::IterDone, kIterDone, /*mustBeResolved=*/false)
    .Required(Tag::BranchIfFalse, kBrRepeat, /*mustBeResolved=*/false)
    .Optional(Tag::PushConst, kPushNil, [](Node *n) { return n->b() == NILREF; }, /*mustBeResolved=*/false)
    .JumpPair(kBrStart, kJtStart)
    .JumpPair(kBrRepeat, kJtRepeat)
    .Name("foreach-do")
    .Priority(10)
    .Build([](Decompiler &dec, Node *anchor, Match &m) -> Node* {
      auto *deeplyConst = m.as<BCPushConst>(kDeeplyConst);
      Node *setObject = m.node(kSetObject);
      auto *setIter = m.as<BCSetVar>(kSetIter);
      Node *brStart = m.node(kBrStart);
      auto *jtRepeat = m.as<JumpTarget>(kJtRepeat);
      auto *setValueNode = m.as<BCSetVar>(kSetValueNode);
      auto *setSlotNode = m.has(kSetSlotNode) ? m.as<BCSetVar>(kSetSlotNode) : nullptr;
      const std::vector<Node*> &bodyRun = m.run(kBody);
      auto *iterNext = m.node(kIterNext);
      auto *jtStart = m.as<JumpTarget>(kJtStart);
      Node *iterDone = m.node(kIterDone);
      Node *brRepeat = m.node(kBrRepeat);
      Node *pushNil = m.has(kPushNil) ? m.node(kPushNil) : nullptr;

      int value = setValueNode->b();
      int slot = setSlotNode ? setSlotNode->b() : -1;
      int iter = setIter->b();
      bool deeply = (deeplyConst->b() == TRUEREF);

      // Eval and unlink the jump targets of any break instructions inside
      // the loop, then remove the trailing "clear iterator" cleanup node --
      // both only exist when pushNil itself does (see class comment).
      if (pushNil) {
        Node *afterPushNil = pushNil->next;
        Node::HandleBreakTargets(jtRepeat, afterPushNil, false);
        afterPushNil->Unlink();
        pushNil->Unlink();
      } else {
        Node::HandleBreakTargets(jtRepeat, brRepeat->next, false);
      }

      Node *obj = ExtractForeachObject(setObject);

      deeplyConst->Unlink();
      setIter->Unlink();
      brStart->Unlink();
      jtRepeat->Unlink();
      setValueNode->Unlink();
      if (setSlotNode) setSlotNode->Unlink();
      Node *body = bodyRun.empty() ? nullptr : bodyRun.front()->UnlinkChain(bodyRun.back());
      iterNext->Unlink();
      jtStart->Unlink();
      iterDone->Unlink();
      brRepeat->Unlink();

      if (slot != -1) dec.useLocalAs(slot, Decompiler::Local::Use::iter);
      dec.useLocalAs(value, Decompiler::Local::Use::iter);
      dec.useLocalAs(iter, Decompiler::Local::Use::iter);

      auto *foreachNode = dec.MakeNode<CFForEachSlotValueDo>(
        dec, anchor->pc(), slot, value, deeply, obj, body);
      // pushNil absent proves NTK's compiler pushed no trailing value for
      // this loop at all (there's nothing else that could have consumed a
      // pushed nil away without trace) -- so unlike the normal case (which
      // may still be a value-producing expression, e.g. the last statement
      // of a `begin...end` block), this specific loop is unambiguously
      // used as a bare statement. Without this, CFForEachSlotValueDo's
      // constructor always reports kProvidesOne (IsExpr), which is usually
      // harmless (the top-level "print every node" loop in Decompiler.cc
      // doesn't care about IsStatement() vs IsExpr()) but breaks badly the
      // moment this construct is *nested* inside another pattern's own
      // Statements(kBody) capture (e.g. a foreach nested inside another
      // foreach/if/while/try's body) -- an IsExpr()==true node can never
      // satisfy IsStatement(), so the enclosing capture stops dead right
      // before it, and the *outer* construct fails to match entirely, even
      // though this inner one resolved perfectly well on its own. Found by
      // corpus-scale testing (Test/run_corpus.py) on a package with a
      // `foreach` nested inside another `foreach`'s body.
      if (!pushNil) foreachNode->provides_ = kProvidesNone;
      anchor->ReplaceWith(foreachNode);
      dec.numASTChanges++;
      return foreachNode->next;
    });
}

bool registerForeachDo = [] { Register(BuildForeachDoPattern()); return true; }();

#pragma mark - foreach [slot,] value [deeply] in object collect body end

/**
 \brief `foreach [slot,] value [deeply] in object collect body end`.
 ```
 [[unrelated preceding code]], <object>, PushConst deeply, NewIter  -- anchor (as foreach...do)
 SetVar iter
 SetVar result := Array(iter[5], nil)   -- initResult
 SetVar index := 0                       -- initIndex
 Branch brStart
 JumpTarget jtRepeat:
 SetVar value := iter[1]                 -- setValueNode
 [SetVar slot := iter[0]]                -- setSlotNode, optional
 SetARef(result, index, <body>); Pop     -- collectStmt; <body> is its Element()
 IncrVar index (+1); Pop; Pop
 IterNext iterator
 JumpTarget jtStart:
 IterDone iterator
 BranchIfFalse brRepeat
 Branch skipCleanup                     -- jump past the break-result override
 JumpTarget * (0 or more)               -- landing zone(s) for `break <value>` inside <body>
 [SetVar result := <break's pushed value>]  -- setResult2, only if any break landed above
 Pop; Pop
 JumpTarget jtCleanup:
 GetVar result
 SetVar result := nil                    -- clearResult
 SetVar iter := nil                      -- clearIter
 ```
 Ported from the never-finished BCNewIter::ResolveForeachSlotValueCollect()
 (removed from ASTControlFlow.cc, along with its FIXME'd, commented-out,
 non-compiling construction code) -- re-derived from scratch against real
 compiled output (`-debug bc`/`-decompile` on hand-written `collect` test
 scripts) rather than trusting that abandoned sketch's own structure; see
 Matt/CLAUDE.md for the reasoning. Shares BuildForeachDoPattern's backward
 object/deeply capture (ForeachObjectDeeplyStep) and its
 value/slot-extraction validation (IsIterSlotSetter), but is otherwise a
 genuinely different shape: pre-sizing the result array (`initResult`,
 `initIndex`) and the trailing `SetARef` collector have no equivalent in
 `do`. Every fixed-shape group here (`initResult`+`initIndex`;
 `setValueNode`+`[setSlotNode]`+`collectStmt`; `clearResult`+`clearIter`)
 is matched as individually Required() nodes, not via Statements() -- their
 sizes are always exactly what's shown above, never a free-form run, so
 there's nothing to capture generically.

 **Known limitation, not yet supported: `break` used from *inside* the
 `body` expression** (as opposed to landing on this construct from
 elsewhere). `do`'s `break` is recognized via the existing
 BCBranch::ResolveBreak()/CFBreak machinery, which requires the branch to be
 immediately followed by a BCPop; `collect`'s break compiles differently --
 push the break value, then branch straight to the landing zone above, with
 *no* following Pop (the pushed value is consumed there, not discarded) --
 so ResolveBreak() never recognizes it, and nothing else in this codebase
 does either. This is a real, additional idiom that would need its own
 dedicated pattern (recognizing "Branch whose target is the collect's own
 break-landing zone", not just "Branch then Pop"), out of scope here.
 Confirmed safe to leave unhandled: since the compiler never emits the
 dead-Pop marker ResolveBreak() looks for, a break-containing `body` simply
 never resolves into the single clean expression this spec's `Custom()`
 step needs, so this whole spec fails to match -- exactly like today,
 before this port existed at all. No silent misdecompile; the function
 just stays flagged as unresolved, same as now.
 */
Spec BuildForeachCollectPattern() {
  enum {
    kDeeplyConst, kSetObject,
    kSetIter, kInitResult, kInitIndex, kBrStart, kJtRepeat,
    kSetValueNode, kSetSlotNode, kCollectStmt, kBodyExpr,
    kIncrIndex, kPopIV0, kPopIV1,
    kIterNext, kJtStart, kIterDone, kBrRepeat, kSkipCleanup,
    kBreakTargets, kSetResult2, kPopR0, kPopR1,
    kJtCleanup, kGetResult, kClearResult, kClearIter,
  };
  return Builder(Tag::NewIter, kFwd)
    // -- Walk backward from the anchor for "object"/"deeply" (shared with
    //    foreach...do; see its class comment).
    .Custom(ForeachObjectDeeplyStep(kDeeplyConst, kSetObject))
    // -- Everything else walks forward from the anchor as usual.
    .Required(Tag::SetVar, kSetIter, /*mustBeResolved=*/false)
    .Required(Tag::SetVar, kInitResult, /*mustBeResolved=*/true)
    .Required(Tag::SetVar, kInitIndex, /*mustBeResolved=*/true)
    .Required(Tag::Branch, kBrStart, /*mustBeResolved=*/false)
    .Required(Tag::JumpTarget, kJtRepeat, /*mustBeResolved=*/false)
    // -- Read value/[slot] directly off the flat list, then the collect
    //    expression out of the trailing SetARef(result, index, <body>); Pop.
    .Custom([](Cursor &c, Match &m) -> bool {
      auto *setValueNode = dynamic_cast<BCSetVar*>(c.peek());
      if (!IsIterSlotSetter(setValueNode, 4)) return false;
      c.advance();

      auto *setSlotNode = dynamic_cast<BCSetVar*>(c.peek());
      if (IsIterSlotSetter(setSlotNode, 0)) {
        c.advance();
      } else {
        setSlotNode = nullptr;
      }

      auto *collectStmt = dynamic_cast<BCPop*>(c.peek());
      if (!collectStmt) return false;
      auto *setARef = dynamic_cast<BCSetARef*>(collectStmt->Input());
      if (!setARef) return false;
      Node *bodyExpr = setARef->Element();
      if (!bodyExpr) return false;
      c.advance();

      m.SetNode(kSetValueNode, setValueNode);
      if (setSlotNode) m.SetNode(kSetSlotNode, setSlotNode); else m.SetAbsent(kSetSlotNode);
      m.SetNode(kCollectStmt, collectStmt);
      m.SetNode(kBodyExpr, bodyExpr);
      return true;
    })
    .Required(Tag::IncrVar, kIncrIndex, /*mustBeResolved=*/true)
    .Required(Tag::Pop, kPopIV0, /*mustBeResolved=*/false)
    .Required(Tag::Pop, kPopIV1, /*mustBeResolved=*/false)
    .Required(Tag::IterNext, kIterNext, /*mustBeResolved=*/false)
    .Required(Tag::JumpTarget, kJtStart, /*mustBeResolved=*/false)
    .Required(Tag::IterDone, kIterDone, /*mustBeResolved=*/false)
    .Required(Tag::BranchIfFalse, kBrRepeat, /*mustBeResolved=*/false)
    .Required(Tag::Branch, kSkipCleanup, /*mustBeResolved=*/false)
    .ZeroOrMore(Tag::JumpTarget, kBreakTargets)
    .Optional(Tag::SetVar, kSetResult2, /*mustBeResolved=*/false)
    .Required(Tag::Pop, kPopR0, /*mustBeResolved=*/false)
    .Required(Tag::Pop, kPopR1, /*mustBeResolved=*/false)
    .Required(Tag::JumpTarget, kJtCleanup, /*mustBeResolved=*/false)
    .Required(Tag::GetVar, kGetResult, /*mustBeResolved=*/false)
    .Required(Tag::SetVar, kClearResult, /*mustBeResolved=*/true)
    .Required(Tag::SetVar, kClearIter, /*mustBeResolved=*/true)
    .JumpPair(kBrStart, kJtStart)
    .JumpPair(kBrRepeat, kJtRepeat)
    .JumpPair(kSkipCleanup, kJtCleanup)
    .Name("foreach-collect")
    .Priority(10)
    .Build([](Decompiler &dec, Node *anchor, Match &m) -> Node* {
      auto *deeplyConst = m.as<BCPushConst>(kDeeplyConst);
      Node *setObject = m.node(kSetObject);
      auto *setIter = m.as<BCSetVar>(kSetIter);
      auto *initResult = m.as<BCSetVar>(kInitResult);
      auto *initIndex = m.as<BCSetVar>(kInitIndex);
      Node *brStart = m.node(kBrStart);
      auto *jtRepeat = m.as<JumpTarget>(kJtRepeat);
      auto *setValueNode = m.as<BCSetVar>(kSetValueNode);
      auto *setSlotNode = m.has(kSetSlotNode) ? m.as<BCSetVar>(kSetSlotNode) : nullptr;
      Node *collectStmt = m.node(kCollectStmt);
      Node *bodyExpr = m.node(kBodyExpr);
      Node *incrIndex = m.node(kIncrIndex);
      Node *popIV0 = m.node(kPopIV0);
      Node *popIV1 = m.node(kPopIV1);
      Node *iterNext = m.node(kIterNext);
      auto *jtStart = m.as<JumpTarget>(kJtStart);
      Node *iterDone = m.node(kIterDone);
      Node *brRepeat = m.node(kBrRepeat);
      Node *skipCleanup = m.node(kSkipCleanup);
      const std::vector<Node*> &breakTargets = m.run(kBreakTargets);
      Node *setResult2 = m.has(kSetResult2) ? m.node(kSetResult2) : nullptr;
      Node *popR0 = m.node(kPopR0);
      Node *popR1 = m.node(kPopR1);
      auto *jtCleanup = m.as<JumpTarget>(kJtCleanup);
      Node *getResult = m.node(kGetResult);
      Node *clearResult = m.node(kClearResult);
      Node *clearIter = m.node(kClearIter);

      int resultLocal = initResult->b();
      int indexLocal = initIndex->b();
      int value = setValueNode->b();
      int slot = setSlotNode ? setSlotNode->b() : -1;
      int iter = setIter->b();
      bool deeply = (deeplyConst->b() == TRUEREF);

      Node *obj = ExtractForeachObject(setObject);

      deeplyConst->Unlink();
      setIter->Unlink();
      initResult->Unlink();
      initIndex->Unlink();
      brStart->Unlink();
      jtRepeat->Unlink();
      setValueNode->Unlink();
      if (setSlotNode) setSlotNode->Unlink();
      collectStmt->Unlink();  // bodyExpr was already detached by BCSetARef::Resolve()
      incrIndex->Unlink();
      popIV0->Unlink();
      popIV1->Unlink();
      iterNext->Unlink();
      jtStart->Unlink();
      iterDone->Unlink();
      brRepeat->Unlink();
      skipCleanup->Unlink();
      for (Node *jt : breakTargets) jt->Unlink();
      if (setResult2) setResult2->Unlink();
      popR0->Unlink();
      popR1->Unlink();
      jtCleanup->Unlink();
      getResult->Unlink();
      clearResult->Unlink();
      clearIter->Unlink();

      dec.useLocalAs(iter, Decompiler::Local::Use::iter);
      dec.useLocalAs(indexLocal, Decompiler::Local::Use::iter);
      dec.useLocalAs(resultLocal, Decompiler::Local::Use::iter);
      if (slot != -1) dec.useLocalAs(slot, Decompiler::Local::Use::iter);
      dec.useLocalAs(value, Decompiler::Local::Use::iter);

      auto *foreachNode = dec.MakeNode<CFForEachSlotValueCollect>(
        dec, anchor->pc(), slot, value, deeply, obj, bodyExpr);
      anchor->ReplaceWith(foreachNode);
      dec.numASTChanges++;
      return foreachNode->next;
    });
}

bool registerForeachCollect = [] { Register(BuildForeachCollectPattern()); return true; }();

#pragma mark - try ... onException ... do ... end

/**
 \brief `try body [onException sym do exBody]+ end`.
 ```
 NewHandler                    -- anchor, b() == number of handlers (numEx)
 <body>
 PopHandlers
 Branch brDone
 [ExceptionHandler:  <exBody>  Branch exDone]  * (numEx - 1)  -- "middle" handlers
 ExceptionHandler:   <exBody>                                  -- last handler, falls through
 JumpTarget * (numEx - 1)                                      -- exDone targets, clustered
 PopHandlers
 JumpTarget jtDone:
 ```
 Ported from the hand-written BCNewHandler::Resolve() (its whole ControlFlow
 branch used to live here, not a separate ResolveXxx()). Registered as two
 separate specs (built by one parameterized BuildTryPattern(isProvider)
 function, mirroring the if/then/else specs' approach) rather than one spec
 with an internal statement-vs-expr flag threaded through every handler:
 `body`'s own shape (IsStatement() vs IsExpr()) is mutually exclusive and
 must hold *uniformly* for every exBody in the whole construct, so it's a
 structural fork, not a per-node runtime check -- same reasoning as the
 if/then/else split.

 Unlike every other idiom ported so far, this one performs **no jump-pair
 validation at all** -- not for brDone/jtDone, not for any exDone/JumpTarget
 pair. The original never checked those relationships either; it trusts
 structural shape (the right sequence of right node types, the right count
 from `b()`) alone. Preserved exactly -- adding verification the original
 never had would be a behavior change, not a port, even though it would
 arguably make this matcher as rigorous as the others. Worth reconsidering
 as a genuine improvement later, but out of scope here.

 The other half of the reason this port is short: unlike every idiom above,
 the actual node extraction/unlinking isn't done here at all --
 `CFTry`'s own constructor (unchanged, ASTControlFlowHelper.cc) walks from
 `anchor` to the captured `jtDone` and does that itself. This spec's whole
 job is validating the shape and locating `jtDone`; that's also why almost
 none of the captured slots below are ever read back in the callback --
 capturing them still exercises the same match-or-reject logic the original
 relied on, even though only `jtDone` (and the anchor itself) end up used.

 First real use of Repeat(): once for the `numEx - 1` middle handlers, once
 for the trailing cluster of exDone JumpTargets. The explicit
 `Guard(b() >= 1)` replicates a real (if probably unreachable from any
 actual compiler) edge case in the original: its `if (i != b_) break;`
 after a `for (i=1; i<b_; i++)` loop rejects `b()==0` specifically because
 the loop counter never advances past its initial value of 1 -- Repeat()'s
 count is `b()-1`, which is negative for `b()==0` and simply iterates zero
 times either way, so without this guard a zero-handler NewHandler
 wouldn't be rejected at this step (it should still fail the mandatory
 "last handler" Required() right after, since no ExceptionHandler node
 would legitimately be sitting there -- but this guard makes the intent
 explicit rather than relying on that indirectly).
 */
Spec BuildTryPattern(bool isProvider) {
  enum {
    kBody, kBodyPop, kBrDone,
    kHandlers, kLastHandler, kLastExBody,
    kTrailingTargets, kExPop, kJtDone,
  };

  Builder spec(Tag::NewHandler, kFwd);
  spec.Guard([](Node *a) { return a->b() >= 1; });
  // `body`/each handler's exBody is captured via StatementsThenExpr()/
  // Statements() (ASTPattern.h), not a single-node Required() -- a try body
  // or handler body is routinely more than one statement long, a shape a
  // single-node capture can't see now that compressAST() (which used to
  // physically pre-merge such runs into one CodeBlock before this pass ran)
  // is gone. The statement-shaped variant (isProvider=false) still requires
  // at least one statement (NonEmpty) -- a try body is never truly empty --
  // while the expr-shaped variant's OptionalStatementsThenExpr() lets a
  // handler body be entirely absent (straight through to its Branch), same
  // as the original's Optional() did for the single-node case.
  if (isProvider) {
    spec.StatementsThenExpr(kBody);
  } else {
    spec.Statements(kBody);
    spec.NonEmpty(kBody);
  }
  spec
    .Required(Tag::PopHandlers, kBodyPop, /*mustBeResolved=*/false)
    .Required(Tag::Branch, kBrDone, /*mustBeResolved=*/false);

  Builder handlerSub;
  handlerSub.Required(Tag::ExceptionHandler, 0, /*mustBeResolved=*/false);
  if (isProvider) {
    handlerSub.OptionalStatementsThenExpr(1);
  } else {
    handlerSub.Statements(1);
  }
  handlerSub.Required(Tag::Branch, 2, /*mustBeResolved=*/false);

  spec.Repeat(kHandlers, [](Node *a) { return a->b() - 1; }, handlerSub);
  spec.Required(Tag::ExceptionHandler, kLastHandler, /*mustBeResolved=*/false);
  if (isProvider) {
    spec.OptionalStatementsThenExpr(kLastExBody);
  } else {
    spec.Statements(kLastExBody);
  }
  spec
    .Repeat(kTrailingTargets, [](Node *a) { return a->b() - 1; },
            Builder().Required(Tag::JumpTarget, 0, /*mustBeResolved=*/false))
    .Required(Tag::PopHandlers, kExPop, /*mustBeResolved=*/false)
    .Required(Tag::JumpTarget, kJtDone, /*mustBeResolved=*/false)
    .Name(isProvider ? "try-expr" : "try-statement")
    .Priority(10);

  return spec.Build([isProvider](Decompiler &dec, Node *anchor, Match &m) -> Node* {
      auto *jtDone = m.as<JumpTarget>(kJtDone);
      auto *exNode = dec.MakeNode<CFTry>(
        dec, anchor->pc(), isProvider ? kProvidesOne : kProvidesNone, anchor, jtDone);
      anchor->ReplaceWith(exNode);
      dec.numASTChanges++;
      return exNode->next;
    });
}

bool registerTryStatement = [] { Register(BuildTryPattern(/*isProvider=*/false)); return true; }();
bool registerTryExpr = [] { Register(BuildTryPattern(/*isProvider=*/true)); return true; }();

} // namespace
