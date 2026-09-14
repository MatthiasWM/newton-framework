
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

} // namespace
