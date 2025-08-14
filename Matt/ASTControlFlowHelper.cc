
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

#pragma mark - CodeBlock

/**
 \class CodeBlock
 \brief A node that holds a block of statements, possibly followed by an expression.
 This is the base for control flow nodes.
 The Newton documentation would call the body of this node "compound expression".
*/

/**
 \brief Constructor called by derived classes.
 \param[in] d back link to the decompiler
 \param[in] pc original position in bytecode
 \param[in] inProvides sets the value that will be returned by Provides()
 */
CodeBlock::CodeBlock(Decompiler &d, int pc, int inProvides)
: Node(d, pc),
  provides_(inProvides)
{ }

/**
 \brief Remove nodes from the AST root an add them as dependencies to this node.
 \param[in] nd start with this node
 \param[in] numNodes number of nodes to move
 \param[in] append them to this list
 */
void CodeBlock::moveToBody(Node *nd, int numNodes, std::vector<Node*> &body)
{
  for (int i = 0; i < numNodes; ++i) {
    Node *nx = nd->next;
    nd->Unlink();
    add(nd);
    nd = nx;
  }
}

void CodeBlock::add(Node *nd)
{
  CodeBlock *cb = dynamic_cast<CodeBlock*>(nd);
  if (cb) {
    for (auto &n: cb->body_) {
      add(n);
    }
  } else {
    body_.push_back(nd);
  }
}

/**
 \brief Print the body nodes of a code block.
 */
void CodeBlock::PrintChildren(bool deep) {
  dec.p.Tag(); dec.p.Print("##### ---> Body");
  for (auto &nd: body_) if (nd) nd->PrintNode(deep);
  dec.p.Tag(); dec.p.Print("##### <--- Body");
}

/**
 \brief Print the code block.
 Add 'begin' and 'end' if needed.
 */
void CodeBlock::Print(uint32_t flags)
{
  if (flags & kPrintSuppressList)
    flags |= kPrintSuppressBeginEnd;
  if (body_.size() > 1) {
    if ((flags & kPrintSuppressBeginEnd) == 0)
      dec.p.Print("begin");
    if ((flags & kPrintSuppressList) == 0)
      dec.p.DeepList(";");
    for (auto &nd: body_) {
      dec.p.Item();
      nd->Print();
      dec.p.ItemDone();
    }
    if ((flags & kPrintSuppressBeginEnd) == 0) {
      dec.p.Trailer();
      dec.p.Print("end");
    }
    if ((flags & kPrintSuppressList) == 0)
      dec.p.EndList();
  } else if (body_.size() == 1) {
    body_[0]->Print();
  }
  // TODO: Do not print a trailing "find-and-set-var a, find-var a"
  // TODO: Also, do not count a trailing "find-and-set-var a, find-var a"
}

/**
 \brief Print a typical code block body.
 */
void CodeBlock::PrintBody(const std::string &prolog,
                             const std::string &separator,
                             const std::string &epilog,
                             std::vector<Node*> &body)
{
  dec.p.Print(prolog);
  dec.p.DeepList(separator);
  for (auto &nd: body) {
    dec.p.Item();
    nd->Print();
    dec.p.ItemDone();
  }
  if (!epilog.empty()) {
    dec.p.Trailer();
    dec.p.Print(epilog);
  }
  dec.p.EndList();

}

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
  body_->PrintOnNewLine();
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
  body_->PrintOnNewLine();
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
  body_->PrintOnNewLine(kPrintSuppressBeginEnd);
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
      && !cond_->IsMultiStatement() && !body_->IsMultiStatement()) {
    int ppp = dec.precedence;
    bool parentheses = (dec.precedence > 1);
    dec.precedence = 1;
    if (parentheses) dec.p.Printf("(");
    cond_->Print();
    dec.p.Print(" and ");
    body_->Print();
    if (parentheses) dec.p.Printf(")");
    dec.precedence = ppp;
  } else {
    bool forceBeginEnd = true;
    dec.p.Print("if ");
    int pp = dec.precedence; dec.precedence = 0;
    cond_->Print();
    dec.p.Print(" then ");

    if (body_->IsMultiStatement()) {
      body_->Print();
      if (elseBody_)
        dec.p.Print(" ");
    } else {
      if (forceBeginEnd) dec.p.Print("begin");
      dec.p.DeepList(";");
      dec.p.FreshLine();
      body_->Print();
      dec.p.EndList();
      if (elseBody_) {
        dec.p.FreshLine();
        if (forceBeginEnd) dec.p.Print("end ");
      } else {
        if (forceBeginEnd) { dec.p.FreshLine(); dec.p.Print("end"); }
      }
    }
    if (elseBody_) {
      dec.p.Print("else ");
      if (dynamic_cast<CFIfThen*>(elseBody_)) {
        // We have an "else if" statement. If we don;t indent it, the source is more readable.
        elseBody_->Print();
      } else if (elseBody_->IsMultiStatement()) {
        // The elseBody_ will print begin end
        elseBody_->Print();
      } else {
        // Only one statement, print "begin end" if requested
        if (forceBeginEnd) dec.p.Print("begin");
        elseBody_->PrintOnNewLine();
        if (forceBeginEnd) { dec.p.FreshLine(); dec.p.Print("end"); }
      }
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
  int ppp = dec.precedence;
  bool parentheses = (dec.precedence > 1);
  dec.precedence = 1;
  if (parentheses) dec.p.Printf("(");
  left_->Print();
  dec.p.Print(" or ");
  right_->Print();
  if (parentheses) dec.p.Printf(")");
  dec.precedence = ppp;
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
  body_->PrintOnNewLine();
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
  body_->PrintOnNewLine();
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
    body_->PrintOnNewLine();
  } else {
    dec.p.DeepList(";");
    dec.p.Item();
    dec.p.Print("begin end");
    dec.p.EndList();
  }
}

#pragma mark - CFTry

CFTry::CFTry(Decompiler &d, int pc, int provides, Node *first, Node *last)
: Node(d, pc), provides_(provides)
{
  int numEx = first->b(); // First is the BCNewHandler
  Node *it = first->next;
  body_ = it; it = it->next; 
  it = it->next; it = it->next; // Skip BCPopHandlers and BCBranch
  // Handle the 'onException...do...' pattern
  for (int i=0; i<numEx; i++) {
    ExceptionHandler *h = dynamic_cast<ExceptionHandler*>(it); it = it->next;
    if ((provides == kProvidesNone) && it->IsStatement()) {
      h->Body(it); it = it->next;
    } else if ((provides == kProvidesOne) && it->IsExpr()) {
      h->Body(it); it = it->next;
    } else {
      h->Body(NewNil());
    }
    exList_.push_back(h);
    it = it->next; // Skip the unconditional branch. On the last ex it's the jump target.
  }
  // That's it. Unlink all nodes.
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
  body_->PrintOnNewLine(kPrintSuppressBeginEnd);
  for (auto &nd: exList_) {
    dec.p.Item();
    nd->Print();
    dec.p.ItemDone();
  }
}
