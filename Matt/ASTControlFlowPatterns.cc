
/*
 File:    Matt/ASTControlFlowPatterns.cc

 Matt's decompiler Abstract Syntax Tree.
 Control-flow idioms expressed as declarative pattern::Spec registrations,
 built with the combinator engine in Matt/ASTPattern.h.

 This file is where NEW idioms get added: a registration here needs no edit
 to any Bytecode-derived node class (they only need a tag() override, added
 once in ASTControlFlow.h/ASTDataFlow.h/ASTControlFlowHelper.h). See
 Matt/ASTPattern.h and the design note at AST.cc:10ff for the rationale.

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
enum LoopSlot { kBody, kJt };

Spec BuildLoopPattern() {
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

} // namespace
