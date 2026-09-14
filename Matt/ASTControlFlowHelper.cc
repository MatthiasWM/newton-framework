
/*
 File:    Matt/ASTControlFlowHelper.h

 Matt's decompiler Abstract Syntax Tree.
 Control Flow nodes that are created for script output.

 Written by:  Matt, 2025.
 */

#include "Matt/ASTControlFlowHelper.h"
#include "Matt/ASTControlFlow.h"
#include "Matt/ASTDataFlow.h"
#include "Matt/Decompiler.h"
#include "Matt/ObjectPrinter.h"

using namespace ast;

#pragma mark - JumpTarget

/**
 \brief Print jump targets that have not been resolved and are still in the AST.
 */
void JumpTarget::Print(uint32_t flags)
{
  PrintNode(true);
}

void JumpTarget::PrintNode(bool deep)
{
  Node::PrintNode(deep);
  dec.p.Printf(" from %d", origin_);
}


namespace {
/**
 \brief Print a ControlBlock body that may be a chain of N>=1 nodes (as
 produced by pattern::Builder::Statements() + Node::UnlinkChain()), not just
 a single node or a CodeBlock*.
 For a single-node chain this reproduces Node::PrintOnNewLine(flags) exactly
 (the node prints itself, wrapped in a bare ';'-list unless it happens to be
 a multi-statement CodeBlock, matching every body_ this could hold before the
 pattern engine existed). A real chain of 2+ nodes -- not yet reachable while
 Decompiler::compressAST() still pre-merges consecutive statements, but
 produced once a matcher is ported without it -- prints each statement
 itself, honoring kPrintSuppressBeginEnd/kPrintSuppressList exactly as
 CodeBlock::Print() does (e.g. CFRepeat passes kPrintSuppressBeginEnd since
 `repeat`/`until` are their own delimiters, not `begin`/`end`).
 */
void PrintBodyChain(Decompiler &dec, Node *head, uint32_t flags = 0) {
  if (!head) return;
  if (head->next == nullptr) {
    head->PrintOnNewLine(flags);
    return;
  }
  if (flags & kPrintSuppressList) flags |= kPrintSuppressBeginEnd;
  if ((flags & kPrintSuppressBeginEnd) == 0) dec.p.Print("begin");
  if ((flags & kPrintSuppressList) == 0) dec.p.DeepList(";");
  for (Node *it = head; it; it = it->next) {
    dec.p.Item();
    it->Print();
    dec.p.ItemDone();
  }
  if ((flags & kPrintSuppressBeginEnd) == 0) {
    dec.p.Trailer();
    dec.p.Print("end");
  }
  if ((flags & kPrintSuppressList) == 0) dec.p.EndList();
}

/**
 \brief Like Node::IsMultiStatement(), but also true for a raw chain of N>=2
 nodes produced by pattern::Builder::Statements() + Node::UnlinkChain() --
 which Node::IsMultiStatement() (false by default, true only on CodeBlock)
 has no way to see, since it only ever looks at the single node it's called
 on, never its `next` chain.
 */
bool IsChainMultiStatement(Node *n) {
  return n && (n->next != nullptr || n->IsMultiStatement());
}
} // namespace

#pragma mark - ControlBlock

ControlBlock::ControlBlock(Decompiler &d, int pc, int inProvides)
: Node(d, pc),
provides_(inProvides)
{ }

void ControlBlock::PrintChildren(bool deep) {
  dec.p.Tag(); dec.p.Print("##### ---> Body");
  body_->PrintNode(deep);
  dec.p.Tag(); dec.p.Print("##### <--- Body");
}

#pragma mark - CFLoop

/**
 \class CFLoop
 \brief Holds the code block of a 'loop' instruction.
 This node is created by resolving another node pattern.
 It is alway marked as resolved.
 */

/**
 \brief Create a new node for a 'loop' instruction.
 The node returns a single value and is marked Resolved.
 */
CFLoop::CFLoop(Decompiler &d, int pc, int prov, Node *body)
: ControlBlock(d, pc, prov)
{
  body_ = body;
}

/**
 \brief Print the source code for 'loop'.
 */
void CFLoop::Print(uint32_t flags) {
  if (!Resolved()) return PrintNode(false);
  dec.p.Print("loop ");
  PrintBodyChain(dec, body_);
};

#pragma mark - CFWhile

CFWhile::CFWhile(Decompiler &d, int pc, int prov, Node *condition, Node *body)
: ControlBlock(d, pc, prov), cond_(condition)
{
  body_ = body;
}

void CFWhile::PrintChildren(bool deep) {
  dec.p.Tag(); dec.p.Print("##### ---> while");
  if (cond_) cond_->PrintNode(deep);
  dec.p.Tag(); dec.p.Print("##### <--> do");
  if (body_) body_->PrintNode(deep);
  dec.p.Tag(); dec.p.Print("##### <--- Body");
}

void CFWhile::Print(uint32_t flags) {
  if (!Resolved()) return PrintNode(false);
  dec.p.Printf("while "); cond_->Print();
  dec.p.Printf(" do ");
  PrintBodyChain(dec, body_);
};

#pragma mark - CFRepeat

CFRepeat::CFRepeat(Decompiler &d, int pc, int prov, Node *condition, Node *body)
: ControlBlock(d, pc, prov), cond_(condition)
{
  body_ = body;
}

void CFRepeat::PrintChildren(bool deep) {
  dec.p.Tag(); dec.p.Print("##### ---> Repeat");
  if (body_) body_->PrintNode(deep);
  dec.p.Tag(); dec.p.Print("##### <--> Until");
  if (cond_) cond_->PrintNode(deep);
  dec.p.Tag(); dec.p.Print("##### <--- Condition");
}

void CFRepeat::Print(uint32_t flags) {
  if (!Resolved()) return PrintNode(false);

  dec.p.Printf("repeat");
  PrintBodyChain(dec, body_, kPrintSuppressBeginEnd);
  dec.p.FreshLine(); dec.p.Printf("until "); cond_->Print();
};

#pragma mark - CFIfThen

CFIfThen::CFIfThen(Decompiler &d, int pc, Node *condition, bool returnsAValue)
: ControlBlock(d, pc, returnsAValue ? kProvidesOne : kProvidesNone), cond_(condition)
{ }

void CFIfThen::PrintChildren(bool deep) {
  dec.p.Tag(); dec.p.Print("##### ---> If Condition");
  if (cond_) cond_->PrintNode(deep);
  dec.p.Tag(); dec.p.Print("##### <--> If Body ");
  if (body_) body_->PrintNode(deep);
  dec.p.Tag(); dec.p.Print("##### <--> Else Body ");
  if (elseBody_) elseBody_->PrintNode(deep);
  dec.p.Tag(); dec.p.Print("##### <--- If Done");
}

void CFIfThen::Print(uint32_t flags)
{
  if ((provides() == kProvidesOne) && elseBody_ && elseBody_->IsNIL()
      && !cond_->IsMultiStatement() && !IsChainMultiStatement(body_)) {
    Precedence pp = dec.precedence;
    bool parentheses = (dec.precedence > kPrecedenceAndOr);
    dec.precedence = kPrecedenceAndOr;
    {
      if (parentheses) dec.p.Printf("(");
      cond_->Print();
      dec.p.Print(" and ");
      dec.precedence++;
      body_->Print();
      dec.precedence--;
      if (parentheses) dec.p.Printf(")");
    }
    dec.precedence = pp;
  } else {
    Precedence pp = dec.precedence;
    bool parentheses = (dec.precedence > kPrecedenceAssign);
    dec.precedence = kPrecedenceAssign;
    {
      if (parentheses) dec.p.Printf("(");
      bool forceBeginEnd = true;
      dec.p.Print("if ");
      cond_->Print();
      dec.p.Print(" then ");

      // body_ is never a self-printing "IsMultiStatement()" object anymore
      // (that was CodeBlock's job before Stage 8 removed it) -- it's either
      // a single node or a raw chain from Statements()/UnlinkChain(), and
      // this loop already walks either correctly, so there's no longer a
      // separate single-vs-multi branch to take here.
      if (forceBeginEnd) dec.p.Print("begin");
      dec.p.DeepList(";");
      dec.p.FreshLine();
      for (Node *it = body_; it; it = it->next) {
        dec.p.Item();
        it->Print();
        dec.p.ItemDone();
      }
      dec.p.EndList();
      if (elseBody_) {
        dec.p.FreshLine();
        if (forceBeginEnd) dec.p.Print("end ");
      } else {
        if (forceBeginEnd) { dec.p.FreshLine(); dec.p.Print("end"); }
      }
      if (elseBody_) {
        dec.p.Print("else ");
        if (dynamic_cast<CFIfThen*>(elseBody_)) {
          // We have an "else if" statement. If we don;t indent it, the source is more readable.
          elseBody_->Print();
        } else {
          // Same reasoning as body_ above: elseBody_ is never a self-
          // printing IsMultiStatement() object anymore, just this loop.
          if (forceBeginEnd) dec.p.Print("begin");
          dec.p.DeepList(";");
          for (Node *it = elseBody_; it; it = it->next) {
            dec.p.Item();
            it->Print();
            dec.p.ItemDone();
          }
          dec.p.EndList();
          if (forceBeginEnd) { dec.p.FreshLine(); dec.p.Print("end"); }
        }
      }
      if (parentheses) dec.p.Printf(")");
    }
    dec.precedence = pp;
  }
}

#pragma mark - CFOr

CFOr::CFOr(Decompiler &d, int pc, Node *left, Node *right)
: ControlBlock(d, pc, kProvidesOne), left_(left), right_(right)
{ }

void CFOr::PrintChildren(bool deep) {
  if (left_) left_->PrintNode(deep);
  if (right_) right_->PrintNode(deep);
}

void CFOr::Print(uint32_t flags)
{
  Precedence pp = dec.precedence;
  bool parentheses = (dec.precedence > kPrecedenceAndOr);
  dec.precedence = kPrecedenceAndOr;
  {
    if (parentheses) dec.p.Printf("(");
    left_->Print();
    dec.p.Print(" or ");
    dec.precedence++;
    right_->Print();
    dec.precedence--;
    if (parentheses) dec.p.Printf(")");
  }
  dec.precedence = pp;
}

#pragma mark - CFBreak

/**
 \brief This node writes out a 'break' instruction.
 */
CFBreak::CFBreak(Decompiler &d, int origin, int target, Node *input)
: Node(d, origin, 0, target), in_(input)
{ }

/**
 \brief Print the 'break' instruction.
 'Break' takes an expression, but if that is 'nil', it's not written out in the source code.
 */
void CFBreak::Print(uint32_t flags) {
  dec.p.Printf("break");
  if (!in_->IsNIL()) {
    dec.p.Printf(" ");
    in_->Print();
  }
}

#pragma mark - CFForLoop

CFForLoop::CFForLoop(Decompiler &d, int pc, int prov, Node *iter, Node *limit, Node *incr, Node *body)
: ControlBlock(d, pc, prov),
  iter_(iter), limit_(limit), incr_(incr)
{
  body_ = body;
}

void CFForLoop::PrintChildren(bool deep)
{
  dec.p.Tag(); dec.p.Print("##### ---> For start");
  if (iter_) iter_->PrintNode(deep);
  dec.p.Tag(); dec.p.Print("##### <--> to ");
  if (limit_) limit_->PrintNode(deep);
  dec.p.Tag(); dec.p.Print("##### <--> by ");
  if (incr_) incr_->PrintNode(deep);
  dec.p.Tag(); dec.p.Print("##### <--> For Body ");
  if (body_) body_->PrintNode(deep);
  dec.p.Tag(); dec.p.Print("##### <--- For Done");
}

void CFForLoop::Print(uint32_t flags)
{
  bool printBy = true;
  BCPushConst *incrValNode = dynamic_cast<BCPushConst*>(incr_);
  if (incrValNode && (incrValNode->b() == MAKEINT(1))) printBy = false;

  dec.p.Print("for ");
  iter_->Print();
  dec.p.Print(" to ");
  limit_->Print();
  if (printBy) {
    dec.p.Print(" by ");
    incr_->Print();
  }
  dec.p.Print(" do ");
  PrintBodyChain(dec, body_);
}

#pragma mark - CFForEachSlotDo

CFForEachSlotValueDo::CFForEachSlotValueDo(Decompiler &d, int pc, int slot, int value, bool deeply, Node *obj, Node *body)
: ControlBlock(d, pc, kProvidesOne),
object_(obj), slot_(slot), value_(value), deeply_(deeply)
{
  if (body)
    body_ = body;
  else
    body_ = NewNil();
}

void CFForEachSlotValueDo::PrintChildren(bool deep)
{
  dec.p.Tag();
  dec.p.Print("##### ---> Foreach ");
  if (slot_ != -1) {
    dec.printLocal(slot_);
    dec.p.Print(", ");
  }
  dec.printLocal(value_);
  if (deeply_) dec.p.Print(" deeply");
  dec.p.Print(" in");
  if (object_) object_->PrintNode(deep);
  dec.p.Tag(); dec.p.Print("##### <--> do ");
  if (body_) body_->PrintNode(deep);
  dec.p.Tag(); dec.p.Print("##### <--- Foreach Done");
}

void CFForEachSlotValueDo::Print(uint32_t flags)
{
  dec.p.Print("foreach ");
  if (slot_ != -1) {
    dec.printLocal(slot_);
    dec.p.Print(", ");
  }
  dec.printLocal(value_);
  if (deeply_) dec.p.Print(" deeply");
  dec.p.Print(" in ");
  object_->Print();
  dec.p.Print(" do ");
  PrintBodyChain(dec, body_);
}

#pragma mark - CFForEachSlotValueCollect

CFForEachSlotValueCollect::CFForEachSlotValueCollect(
    Decompiler &d, int pc, int slot, int value, bool deeply, Node *obj, Node *body)
: ControlBlock(d, pc, kProvidesOne),
object_(obj), slot_(slot), value_(value), deeply_(deeply)
{
  body_ = body ? body : NewNil();
}

void CFForEachSlotValueCollect::PrintChildren(bool deep)
{
  dec.p.Tag();
  dec.p.Print("##### ---> Foreach ");
  if (slot_ != -1) {
    dec.printLocal(slot_);
    dec.p.Print(", ");
  }
  dec.printLocal(value_);
  if (deeply_) dec.p.Print(" deeply");
  dec.p.Print(" in");
  if (object_) object_->PrintNode(deep);
  dec.p.Tag(); dec.p.Print("##### <--> collect ");
  if (body_) body_->PrintNode(deep);
  dec.p.Tag(); dec.p.Print("##### <--- Foreach Done");
}

void CFForEachSlotValueCollect::Print(uint32_t flags)
{
  dec.p.Print("foreach ");
  if (slot_ != -1) {
    dec.printLocal(slot_);
    dec.p.Print(", ");
  }
  dec.printLocal(value_);
  if (deeply_) dec.p.Print(" deeply");
  dec.p.Print(" in ");
  object_->Print();
  dec.p.Print(" collect ");
  body_->Print();
}

#pragma mark - ExceptionHandler

void ExceptionHandler::PrintChildren(bool deep)
{
  if (body_) body_->PrintNode(deep);
}

void ExceptionHandler::Print(uint32_t flags)
{
  dec.p.Print("onException ");
  dec.printLiteralAsTag(excp_);
  dec.p.Print(" do ");
  if (body_) {
    PrintBodyChain(dec, body_);
  } else {
    dec.p.DeepList(";");
    dec.p.Item();
    dec.p.Print("begin end");
    dec.p.EndList();
  }
}

namespace {
/**
 \brief Consume the (possibly multi-node, possibly entirely absent) run of
 body nodes starting at `it`: 0+ IsStatement() nodes, then -- only if
 `isProvider` -- exactly one required IsExpr() node. Mirrors ASTPattern.h's
 Statements()/StatementsThenExpr() combinators, which the
 ASTControlFlowPatterns.cc spec already used to validate this exact shape
 before CFTry was even constructed -- CFTry's own constructor re-walks the
 raw (still fully linked) node list independently of that spec, so it needs
 the same boundary logic here rather than the single-node IsStatement()/
 IsExpr() peek this used to be (safe only while compressAST() pre-merged
 multi-statement bodies into one CodeBlock node).
 Leaves `it` pointing at the first node past the run (unchanged if the run
 is empty). Returns the run's head (nullptr if empty) and, via `outTail`,
 its last node -- callers detach the run from the root list with
 `head->UnlinkChain(*outTail)`, which (unlike plain Unlink()) preserves the
 run's own internal next-chain so it can still be walked/printed as a
 multi-statement body afterward.
 */
Node *ConsumeOptionalBody(Node *&it, bool isProvider, Node **outTail) {
  Node *head = it;
  Node *tail = nullptr;
  while (it && it->IsStatement()) { tail = it; it = it->next; }
  if (isProvider && it && it->IsExpr()) { tail = it; it = it->next; }
  if (!tail) { it = head; return nullptr; }
  *outTail = tail;
  return head;
}
} // namespace

#pragma mark - CFTry

CFTry::CFTry(Decompiler &d, int pc, int provides, Node *first, Node *last)
: Node(d, pc), provides_(provides)
{
  int numEx = first->b(); // First is the BCNewHandler
  bool isProvider = (provides == kProvidesOne);
  Node *it = first->next;

  Node *bodyTail = nullptr;
  body_ = ConsumeOptionalBody(it, isProvider, &bodyTail);
  if (body_) body_->UnlinkChain(bodyTail);
  it = it->next; it = it->next; // Skip BCPopHandlers and BCBranch
  // Handle the 'onException...do...' pattern
  for (int i=0; i<numEx; i++) {
    ExceptionHandler *h = dynamic_cast<ExceptionHandler*>(it); it = it->next;
    Node *hBodyTail = nullptr;
    Node *hBody = ConsumeOptionalBody(it, isProvider, &hBodyTail);
    if (hBody) {
      hBody->UnlinkChain(hBodyTail);
      h->Body(hBody);
    } else {
      h->Body(NewNil());
    }
    exList_.push_back(h);
    it = it->next; // Skip the unconditional branch. On the last ex it's the jump target.
  }
  // That's it. Unlink all remaining (single, scaffolding) nodes -- body_
  // and every handler's body were already detached above via UnlinkChain(),
  // which is required for a multi-node body/handler-body: plain Unlink()
  // (used here for the single scaffolding nodes -- PopHandlers, Branch,
  // ExceptionHandler, trailing JumpTargets -- that have no chain of their
  // own to preserve) nulls out the unlinked node's own prev/next, which
  // would otherwise silently truncate a multi-statement body/handler-body
  // chain to just its head node.
  while (first->next && (first->next != last)) first->next->Unlink();
  last->Unlink();
}

void CFTry::PrintChildren(bool deep)
{
  body_->Print();
  for (auto &nd: exList_) {
    nd->PrintNode(deep);
  }
}

void CFTry::Print(uint32_t flags)
{
  dec.p.Print("try");
  PrintBodyChain(dec, body_, kPrintSuppressBeginEnd);
  for (auto &nd: exList_) {
    dec.p.Item();
    nd->Print();
    dec.p.ItemDone();
  }
}
