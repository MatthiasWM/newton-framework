
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
 <cond>                -- anchor's Input()
 BranchIfTrue jt1       -- anchor
 ```
 Ported from the hand-written BCBranchIfTrue::ResolveWhileDo() (removed from
 ASTControlFlow.cc); walks backward from the anchor, mirroring `loop`.
 */
Spec BuildWhilePattern() {
  enum { kJt2, kBody, kJt1, kBranch2 };
  return Builder(Tag::BranchIfTrue, kBwd)
    .Guard([](Node *a) { return a->b() <= a->pc(); })  // jump must be backward
    .Required(Tag::JumpTarget, kJt2, /*mustBeResolved=*/false)
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

      Node *cond = static_cast<BCBranchIfTrue*>(anchor)->Input();
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
 */
Spec BuildIfThenPattern() {
  enum { kBody, kJt1 };
  return Builder(Tag::BranchIfFalse, kFwd)
    .Guard([](Node *a) { return a->pc() <= a->b(); })  // jump must be forward
    .Statements(kBody)
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
 <body>                 -- non-empty statement run
 Branch jt2
 JumpTarget jt1:
 <elseBody>             -- non-empty statement run
 JumpTarget jt2:
 ```
 See BuildIfThenPattern() above for why this and the bare if-then never
 both match the same bytecode.
 */
Spec BuildIfThenElsePattern() {
  enum { kBody, kBi2, kJt1, kElseBody, kJt2 };
  return Builder(Tag::BranchIfFalse, kFwd)
    .Guard([](Node *a) { return a->pc() <= a->b(); })  // jump must be forward
    .Statements(kBody)
    .NonEmpty(kBody)
    .Required(Tag::Branch, kBi2, /*mustBeResolved=*/false)
    .Required(Tag::JumpTarget, kJt1, /*mustBeResolved=*/false)
    .JumpPair(Builder::kAnchor, kJt1)
    .Statements(kElseBody)
    .NonEmpty(kElseBody)
    .Required(Tag::JumpTarget, kJt2, /*mustBeResolved=*/false)
    .JumpPair(kBi2, kJt2)
    .Name("if-then-else")
    .Priority(5)
    .Build([](Decompiler &dec, Node *anchor, Match &m) -> Node* {
      Node *bi2 = m.node(kBi2);
      auto *jt1 = m.as<JumpTarget>(kJt1);
      auto *jt2 = m.as<JumpTarget>(kJt2);
      const std::vector<Node*> &bodyRun = m.run(kBody);
      const std::vector<Node*> &elseRun = m.run(kElseBody);
      Node *body = bodyRun.front()->UnlinkChain(bodyRun.back());
      Node *elseBody = elseRun.front()->UnlinkChain(elseRun.back());
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
 <body: expr>
 Branch jt2
 JumpTarget jt1:
 <elseBody: expr>
 JumpTarget jt2:
 ```
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
    .Required(Tag::Any, kBody, [](Node *n) { return n->IsExpr(); }, /*mustBeResolved=*/true)
    .Required(Tag::Branch, kBi2, /*mustBeResolved=*/false)
    .Required(Tag::JumpTarget, kJt1, /*mustBeResolved=*/false)
    .JumpPair(Builder::kAnchor, kJt1)
    .Required(Tag::Any, kElseBody, [](Node *n) { return n->IsExpr(); }, /*mustBeResolved=*/true)
    .Required(Tag::JumpTarget, kJt2, /*mustBeResolved=*/false)
    .JumpPair(kBi2, kJt2)
    .Name("if-then-else-expr")
    .Priority(5)
    .Build([](Decompiler &dec, Node *anchor, Match &m) -> Node* {
      Node *body = m.node(kBody);
      Node *elseBody = m.node(kElseBody);
      Node *bi2 = m.node(kBi2);
      auto *jt1 = m.as<JumpTarget>(kJt1);
      auto *jt2 = m.as<JumpTarget>(kJt2);
      body->Unlink();
      elseBody->Unlink();
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
 [[preamble]], SetVar iter, SetVar limit, SetVar incr    -- "start" (see note)
 GetVar iter                                              -- getIter
 Branch brTest                                            -- jump to the test first
 JumpTarget jtAgain:
 <body>                                                    -- optional, one node
 IncrVar incr                                              -- incIter
 JumpTarget jtTest:
 GetVar limit                                              -- getLimit
 BranchLoop jtAgain                                         -- anchor: tests and loops back
 ```
 Ported from the hand-written BCBranchLoop::Resolve() (its whole body used
 to live here, not a separate ResolveXxx() -- BCBranchLoop had no other use
 for Resolve()). This is the first idiom where the "no CodeBlock needed"
 promise of Statements() does NOT hold yet: `iter`/`limit`/`incr` are set by
 three consecutive BCSetVar *statements*, so as long as
 Decompiler::compressAST() keeps pre-merging any run of 2+ statements before
 the ControlFlow pass runs (it still does -- see Matt/CLAUDE.md), those
 three SetVars are already fused into one opaque CodeBlock ("start") by the
 time this pattern is tried, indistinguishable on the flat list from
 whatever unrelated statements preceded them. There is no way to require
 them individually via Statements()/Required() today; the CodeBlock and its
 tail-indexing has to stay, exactly as fragile as in the original, until
 Stage 8 (compressAST() removal) makes iter/limit/incr individually
 addressable list nodes again -- at which point this spec should become
 `Statements(preamble) + Required(SetVar,iter) + Required(SetVar,limit) +
 Required(SetVar,incr)` and the whole CodeBlock-reaching callback below goes
 away.
 */
Spec BuildForLoopPattern() {
  enum { kGetLimit, kJtTest, kIncIter, kBody, kJtAgain, kBrTest, kGetIter, kStart };
  return Builder(Tag::BranchLoop, kBwd)
    .Required(Tag::GetVar, kGetLimit, /*mustBeResolved=*/true)
    .Required(Tag::JumpTarget, kJtTest, /*mustBeResolved=*/false)
    .Required(Tag::IncrVar, kIncIter, /*mustBeResolved=*/false)
    .Optional(Tag::Any, kBody, [](Node *n) { return n->IsStatement(); }, /*mustBeResolved=*/false)
    .Required(Tag::JumpTarget, kJtAgain, /*mustBeResolved=*/false)
    .Required(Tag::Branch, kBrTest, /*mustBeResolved=*/false)
    .Required(Tag::GetVar, kGetIter, /*mustBeResolved=*/false)
    .Required(Tag::Any, kStart, [](Node *n) {
        auto *cb = dynamic_cast<CodeBlock*>(n);
        return cb && cb->size() >= 4;
      }, /*mustBeResolved=*/true)
    .JumpPair(Builder::kAnchor, kJtAgain)
    .JumpPair(kBrTest, kJtTest)
    .Name("for-to-by-do")
    .Priority(10)
    .Build([](Decompiler &dec, Node *anchor, Match &m) -> Node* {
      auto *getLimit = m.as<BCGetVar>(kGetLimit);
      Node *jtTest = m.node(kJtTest);
      Node *incIter = m.node(kIncIter);
      Node *body = m.has(kBody) ? m.node(kBody) : nullptr;
      Node *jtAgain = m.node(kJtAgain);
      Node *brTest = m.node(kBrTest);
      auto *getIter = m.as<BCGetVar>(kGetIter);
      auto *start = m.as<CodeBlock>(kStart);

      // Reach into the CodeBlock's tail for iter/limit/incr -- see the class
      // comment above for why this is still necessary today.
      int nInstr = start->size();
      auto *getIncr  = dynamic_cast<BCGetVar*>(start->at(nInstr - 1));
      auto *setIncr  = dynamic_cast<BCSetVar*>(start->at(nInstr - 2));
      auto *setLimit = dynamic_cast<BCSetVar*>(start->at(nInstr - 3));
      auto *setIter  = dynamic_cast<BCSetVar*>(start->at(nInstr - 4));
      if (!getIncr || !getIncr->Resolved()) return nullptr;
      if (!setIncr || !setIncr->Resolved()) return nullptr;
      if (!setLimit || !setLimit->Resolved()) return nullptr;
      if (!setIter || !setIter->Resolved()) return nullptr;

      // Check that all three loop locals agree between their SetVar (inside
      // the CodeBlock tail) and GetVar (found separately, on the flat list).
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

      start->pop_back();
      start->pop_back();
      start->pop_back();
      start->pop_back();
      start->UnlinkIfEmpty();

      // Mark the locals with an alternative use, so they are not declared.
      dec.useLocalAs(incr, Decompiler::Local::Use::iter);
      dec.useLocalAs(limit, Decompiler::Local::Use::iter);
      dec.useLocalAs(iter, Decompiler::Local::Use::iter);

      if (body) body->Unlink(); else body = anchor->NewNil();
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
 node, unlinking it from the list (or, if it's a CodeBlock, only its
 trailing expression) -- shared by both foreach idioms' construction
 callbacks.
 */
Node *ExtractForeachObject(Node *setObject) {
  auto *objBlock = dynamic_cast<CodeBlock*>(setObject);
  if (objBlock) {
    Node *obj = objBlock->back();
    objBlock->pop_back();
    objBlock->UnlinkIfEmpty();
    return obj;
  }
  setObject->Unlink();
  return setObject;
}

#pragma mark - foreach [slot,] value [deeply] in object do body end

/**
 \brief `foreach [slot,] value [deeply] in object do body end`.
 ```
 [[preamble]], <object>       -- setObject (see note on Custom() step below)
 PushConst deeply
 NewIter                       -- anchor
 SetVar iter
 Branch brStart                -- jump to the "done?" test first
 JumpTarget jtRepeat:
 [SetVar value := iterator[4]]  -- setValueNode, required
 [SetVar slot  := iterator[0]]  -- setSlotNode, optional
 <body>                         -- whatever of the above CodeBlock is left over
 IterNext iterator
 JumpTarget jtStart:
 IterDone iterator
 BranchIfFalse brRepeat
 PushConst nil                  -- pushNil ("clear iterator" is the node right after it)
 ```
 Ported from the hand-written BCNewIter::ResolveForeachSlotValueDo() (removed
 from ASTControlFlow.cc). Two irregularities this idiom has that no earlier
 port did, both handled with Builder::Custom() rather than forced into the
 named combinators:

 1. Unlike every other anchor ported so far, BCNewIter never runs its own
    Consume2 DataFlow resolution (BCNewIter::Resolved() is hardcoded
    false), so "object"/"deeply" are never wired into Input()/in1_/in2_ --
    this matcher has always had to walk `prev`/`prev->prev` directly to
    find them, and still does, in the first Custom() step below. Since that
    step inspects the anchor's *backward* neighbors while every other step
    in this (forward-anchored) spec walks *forward*, it deliberately never
    touches the shared Cursor.

 2. The value/slot extraction is inherently branchy: if compressAST() has
    already merged the per-iteration setup (SetVar value; [SetVar slot;])
    together with whatever body code follows into one CodeBlock, they must
    be read from that CodeBlock's front instead of the flat list, and *no*
    cursor advance is needed (the preceding Optional() capture already
    consumed the CodeBlock); if not, they're read directly off the flat
    list and the cursor must advance past whichever of them were found.
    This is exactly the same "still CodeBlock-dependent until Stage 8"
    situation as `for`'s iter/limit/incr -- see the note there and in
    Matt/CLAUDE.md -- except here a CodeBlock isn't even guaranteed to
    exist (unlike `for`, where 3 consecutive SetVars always force one).
 */
Spec BuildForeachDoPattern() {
  enum {
    kDeeplyConst, kSetObject,
    kSetIter, kBrStart, kJtRepeat, kBodyBlock,
    kSetValueNode, kSetSlotNode,
    kIterNext, kJtStart, kIterDone, kBrRepeat, kPushNil,
  };
  return Builder(Tag::NewIter, kFwd)
    // -- Walk backward from the anchor for "object"/"deeply" (see note 1).
    .Custom(ForeachObjectDeeplyStep(kDeeplyConst, kSetObject))
    // -- Everything else walks forward from the anchor as usual.
    .Required(Tag::SetVar, kSetIter, /*mustBeResolved=*/false)
    .Required(Tag::Branch, kBrStart, /*mustBeResolved=*/false)
    .Required(Tag::JumpTarget, kJtRepeat, /*mustBeResolved=*/false)
    .Optional(Tag::Any, kBodyBlock, [](Node *n) { return dynamic_cast<CodeBlock*>(n) != nullptr; },
              /*mustBeResolved=*/true)
    // -- Extract setValue/[setSlot] from the CodeBlock's front, or straight
    //    off the flat list if there's no CodeBlock (see note 2).
    .Custom([](Cursor &c, Match &m) -> bool {
      auto *cb = m.has(kBodyBlock) ? static_cast<CodeBlock*>(m.node(kBodyBlock)) : nullptr;
      BCSetVar *setValueNode = nullptr;
      BCSetVar *setSlotNode = nullptr;
      if (cb) {
        setValueNode = dynamic_cast<BCSetVar*>(cb->at(0));
        setSlotNode = dynamic_cast<BCSetVar*>(cb->at(1));
      } else {
        Node *it = c.peek();
        setValueNode = dynamic_cast<BCSetVar*>(it);
        setSlotNode = it ? dynamic_cast<BCSetVar*>(it->next) : nullptr;
      }

      // "value" is required: it must read the iterator's value slot (4).
      if (!IsIterSlotSetter(setValueNode, 4)) return false;
      // "slot" is optional: it must read the iterator's tag slot (0).
      if (!IsIterSlotSetter(setSlotNode, 0)) setSlotNode = nullptr;

      if (!cb) {
        // No pre-merged CodeBlock: advance past whichever of
        // setValueNode/setSlotNode were consumed directly off the list.
        c.advance();
        if (setSlotNode) c.advance();
      }

      m.SetNode(kSetValueNode, setValueNode);
      if (setSlotNode) m.SetNode(kSetSlotNode, setSlotNode); else m.SetAbsent(kSetSlotNode);
      return true;
    })
    .Required(Tag::IterNext, kIterNext, /*mustBeResolved=*/false)
    .Required(Tag::JumpTarget, kJtStart, /*mustBeResolved=*/false)
    .Required(Tag::IterDone, kIterDone, /*mustBeResolved=*/false)
    .Required(Tag::BranchIfFalse, kBrRepeat, /*mustBeResolved=*/false)
    .Required(Tag::PushConst, kPushNil, [](Node *n) { return n->b() == NILREF; }, /*mustBeResolved=*/false)
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
      auto *body = m.has(kBodyBlock) ? static_cast<CodeBlock*>(m.node(kBodyBlock)) : nullptr;
      auto *setValueNode = m.as<BCSetVar>(kSetValueNode);
      auto *setSlotNode = m.has(kSetSlotNode) ? m.as<BCSetVar>(kSetSlotNode) : nullptr;
      auto *iterNext = m.node(kIterNext);
      auto *jtStart = m.as<JumpTarget>(kJtStart);
      Node *iterDone = m.node(kIterDone);
      Node *brRepeat = m.node(kBrRepeat);
      Node *pushNil = m.node(kPushNil);

      int value = setValueNode->b();
      int slot = setSlotNode ? setSlotNode->b() : -1;
      int iter = setIter->b();
      bool deeply = (deeplyConst->b() == TRUEREF);

      // Eval and unlink the jump targets of any break instructions inside
      // the loop, then remove the trailing "clear iterator" cleanup node.
      Node *afterPushNil = pushNil->next;
      Node::HandleBreakTargets(jtRepeat, afterPushNil, false);
      afterPushNil->Unlink();

      // If setObject is a CodeBlock, only use the last expression.
      Node *obj = ExtractForeachObject(setObject);

      deeplyConst->Unlink();
      setIter->Unlink();
      brStart->Unlink();
      jtRepeat->Unlink();
      if (body) {
        body->pop_front();               // unlink 'setValue'
        if (slot != -1) body->pop_front();  // unlink 'setSlot'
        body->Unlink();
      } else {
        if (setValueNode) setValueNode->Unlink();
        if (setSlotNode) setSlotNode->Unlink();
      }
      iterNext->Unlink();
      jtStart->Unlink();
      iterDone->Unlink();
      brRepeat->Unlink();
      pushNil->Unlink();

      if (slot != -1) dec.useLocalAs(slot, Decompiler::Local::Use::iter);
      dec.useLocalAs(value, Decompiler::Local::Use::iter);
      dec.useLocalAs(iter, Decompiler::Local::Use::iter);

      auto *foreachNode = dec.MakeNode<CFForEachSlotValueDo>(
        dec, anchor->pc(), slot, value, deeply, obj, body);
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
 [[preamble]], <object>, PushConst deeply, NewIter        -- anchor (as foreach...do)
 SetVar iter
 CodeBlock initBlock: [ SetVar result := Array(iter[5], nil);  SetVar index := 0 ]
 Branch brStart
 JumpTarget jtRepeat:
 CodeBlock setupBlock: [ SetVar value := iter[1];  [SetVar slot := iter[0];]
                          SetARef(result, index, <body>); Pop ]
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
 CodeBlock prepareForGC: [ SetVar result := nil;  SetVar iter := nil ]
 ```
 Ported from the never-finished BCNewIter::ResolveForeachSlotValueCollect()
 (removed from ASTControlFlow.cc, along with its FIXME'd, commented-out,
 non-compiling construction code) -- re-derived from scratch against real
 compiled output (`-debug bc`/`-decompile` on hand-written `collect` test
 scripts) rather than trusting that abandoned sketch's own structure; see
 Matt/CLAUDE.md for the reasoning. Shares BuildForeachDoPattern's backward
 object/deeply capture (ForeachObjectDeeplyStep) and its
 value/slot-extraction validation (IsIterSlotSetter), but is otherwise a
 genuinely different shape: `initBlock` (pre-sizing the result array) and
 `setupBlock`'s trailing `SetARef` collector have no equivalent in `do`, and
 unlike `do`'s optional single-node body, `collect`'s per-iteration setup
 is *always* >= 2 statements (value-set + the collect assignment), so it's
 *always* a CodeBlock by the time compressAST() is done -- no dual
 CodeBlock-or-raw-list fallback needed here, simplifying that part of the
 port relative to `do`.

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
 never resolves into the single clean expression Required(kBodyExpr) needs,
 so this whole spec fails to match -- exactly like today, before this port
 existed at all. No silent misdecompile; the function just stays flagged as
 unresolved, same as now.
 */
Spec BuildForeachCollectPattern() {
  enum {
    kDeeplyConst, kSetObject,
    kSetIter, kInitBlock, kBrStart, kJtRepeat,
    kSetupBlock, kBodyExpr,
    kIncrIndex, kPopIV0, kPopIV1,
    kIterNext, kJtStart, kIterDone, kBrRepeat, kSkipCleanup,
    kBreakTargets, kSetResult2, kPopR0, kPopR1,
    kJtCleanup, kGetResult, kPrepareForGC,
  };
  return Builder(Tag::NewIter, kFwd)
    // -- Walk backward from the anchor for "object"/"deeply" (shared with
    //    foreach...do; see its class comment, note 1).
    .Custom(ForeachObjectDeeplyStep(kDeeplyConst, kSetObject))
    // -- Everything else walks forward from the anchor as usual.
    .Required(Tag::SetVar, kSetIter, /*mustBeResolved=*/false)
    .Required(Tag::Any, kInitBlock, [](Node *n) {
        auto *cb = dynamic_cast<CodeBlock*>(n);
        return cb && cb->size() == 2
            && dynamic_cast<BCSetVar*>(cb->at(0))
            && dynamic_cast<BCSetVar*>(cb->at(1));
      }, /*mustBeResolved=*/true)
    .Required(Tag::Branch, kBrStart, /*mustBeResolved=*/false)
    .Required(Tag::JumpTarget, kJtRepeat, /*mustBeResolved=*/false)
    .Required(Tag::Any, kSetupBlock, [](Node *n) { return dynamic_cast<CodeBlock*>(n) != nullptr; },
              /*mustBeResolved=*/true)
    // -- Validate value/[slot] at the block's front, extract the collect
    //    expression from its trailing SetARef(result, index, <body>); Pop.
    .Custom([](Cursor&, Match &m) -> bool {
      auto *cb = static_cast<CodeBlock*>(m.node(kSetupBlock));
      int n = cb->size();
      if (n != 2 && n != 3) return false;
      if (!IsIterSlotSetter(dynamic_cast<BCSetVar*>(cb->at(0)), 4)) return false;
      if (n == 3 && !IsIterSlotSetter(dynamic_cast<BCSetVar*>(cb->at(1)), 0)) return false;

      auto *collectStmt = dynamic_cast<BCPop*>(cb->at(n - 1));
      if (!collectStmt) return false;
      auto *setARef = dynamic_cast<BCSetARef*>(collectStmt->Input());
      if (!setARef) return false;
      Node *bodyExpr = setARef->Element();
      if (!bodyExpr) return false;

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
    .Required(Tag::Any, kPrepareForGC, [](Node *n) {
        auto *cb = dynamic_cast<CodeBlock*>(n);
        return cb && cb->size() == 2;
      }, /*mustBeResolved=*/true)
    .JumpPair(kBrStart, kJtStart)
    .JumpPair(kBrRepeat, kJtRepeat)
    .JumpPair(kSkipCleanup, kJtCleanup)
    .Name("foreach-collect")
    .Priority(10)
    .Build([](Decompiler &dec, Node *anchor, Match &m) -> Node* {
      auto *deeplyConst = m.as<BCPushConst>(kDeeplyConst);
      Node *setObject = m.node(kSetObject);
      auto *setIter = m.as<BCSetVar>(kSetIter);
      auto *initBlock = m.as<CodeBlock>(kInitBlock);
      Node *brStart = m.node(kBrStart);
      auto *jtRepeat = m.as<JumpTarget>(kJtRepeat);
      auto *setupBlock = m.as<CodeBlock>(kSetupBlock);
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
      auto *prepareForGC = m.as<CodeBlock>(kPrepareForGC);

      // Local indices, read from the already-validated init/setup blocks.
      int resultLocal = static_cast<BCSetVar*>(initBlock->at(0))->b();
      int indexLocal = static_cast<BCSetVar*>(initBlock->at(1))->b();
      int n = setupBlock->size();
      int value = static_cast<BCSetVar*>(setupBlock->at(0))->b();
      int slot = (n == 3) ? static_cast<BCSetVar*>(setupBlock->at(1))->b() : -1;
      int iter = setIter->b();
      bool deeply = (deeplyConst->b() == TRUEREF);

      // If setObject is a CodeBlock, only use the last expression.
      Node *obj = ExtractForeachObject(setObject);

      deeplyConst->Unlink();
      setIter->Unlink();
      initBlock->Unlink();
      brStart->Unlink();
      jtRepeat->Unlink();
      setupBlock->Unlink();  // bodyExpr was already detached by BCSetARef::Resolve()
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
      prepareForGC->Unlink();

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
  Cond bodyCond = isProvider
    ? Cond([](Node *n) { return n->IsExpr(); })
    : Cond([](Node *n) { return n->IsStatement(); });

  return Builder(Tag::NewHandler, kFwd)
    .Guard([](Node *a) { return a->b() >= 1; })
    .Required(Tag::Any, kBody, bodyCond, /*mustBeResolved=*/true)
    .Required(Tag::PopHandlers, kBodyPop, /*mustBeResolved=*/false)
    .Required(Tag::Branch, kBrDone, /*mustBeResolved=*/false)
    .Repeat(kHandlers, [](Node *a) { return a->b() - 1; },
            Builder()
              .Required(Tag::ExceptionHandler, 0, /*mustBeResolved=*/false)
              .Optional(Tag::Any, 1, bodyCond, /*mustBeResolved=*/true)
              .Required(Tag::Branch, 2, /*mustBeResolved=*/false))
    .Required(Tag::ExceptionHandler, kLastHandler, /*mustBeResolved=*/false)
    .Optional(Tag::Any, kLastExBody, bodyCond, /*mustBeResolved=*/true)
    .Repeat(kTrailingTargets, [](Node *a) { return a->b() - 1; },
            Builder().Required(Tag::JumpTarget, 0, /*mustBeResolved=*/false))
    .Required(Tag::PopHandlers, kExPop, /*mustBeResolved=*/false)
    .Required(Tag::JumpTarget, kJtDone, /*mustBeResolved=*/false)
    .Name(isProvider ? "try-expr" : "try-statement")
    .Priority(10)
    .Build([isProvider](Decompiler &dec, Node *anchor, Match &m) -> Node* {
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
