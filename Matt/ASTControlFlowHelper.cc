
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
  body_->Print();
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
  body_->Print();
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
  dec.p.DeepList(";");
  body_->Print(kPrintSuppressList);
  dec.p.Trailer(); dec.p.Printf("until "); cond_->Print();
  dec.p.EndList();
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

void CFIfThen::Print(uint32_t flags) {
  if (!Resolved()) return PrintNode(false);
#if 0
  bool needBeginEnd = ((body_.size() > 1) || (elseBody_.size() > 1));
  // >> if (condition) the begin
  dec.p.Print("if ");
  int pp = dec.precedence; dec.precedence = 0;
  cond_->Print();
  dec.precedence = pp;
  dec.p.Print(" then");
  if (needBeginEnd) dec.p.Printf(" begin");
  // >>   if-Branch
  dec.p.DeepList(";");
  for (auto &nd: body_) {
    dec.p.Item(); nd->Print(); dec.p.ItemDone();
  }
  if (elseBody_.empty()) {
    if (needBeginEnd) { dec.p.Trailer(); dec.p.Printf("end"); }
    dec.p.EndList();
  }
  if (!elseBody_.empty()) {
    // >> end else if
    dec.p.Trailer();
    if (needBeginEnd) dec.p.Printf("end else begin"); else dec.p.Printf("else");
    dec.p.EndList();
    // >>   else-Branch
    dec.p.DeepList(";");
    for (auto &nd: elseBody_) {
      dec.p.Item(); nd->Print(); dec.p.ItemDone();
    }
    if (needBeginEnd) { dec.p.Trailer(); dec.p.Printf("end"); }
    dec.p.EndList();
  }
#else
  dec.p.Print("if ");
  int pp = dec.precedence; dec.precedence = 0;
  cond_->Print();
  dec.p.Print(" then ");
  body_->Print();
  if (elseBody_) {
    dec.p.Print(" else ");
    elseBody_->Print();
  }
  dec.precedence = pp;
#endif
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
  body_->Print();
}

#pragma mark - CFForEachSlotDo

CFForEachSlotValueDo::CFForEachSlotValueDo(Decompiler &d, int pc, int slot, int value, bool deeply, Node *obj, Node *body)
: ControlBlock(d, pc, kProvidesOne),
object_(obj), slot_(slot), value_(value), deeply_(deeply)
{
  body_ = body;
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
  CodeBlock *cb = dynamic_cast<CodeBlock*>(body_);
  if (cb && (cb->size() > 1)) {
    body_->Print();
  } else {
    dec.p.DeepList(";");
    body_->Print();
    dec.p.EndList();
  }
}

#pragma mark - CFTry

CFTry::CFTry(Decompiler &d, int pc, Node *first, Node *last)
: Node(d, pc)
{
  int numEx = first->b(); // First is the BCNewHandler
  Node *it = first->next;
  body_ = it; it = it->next; 
  it = it->next; it = it->next; // Skip BCPopHandlers and BCBranch
  // Handle the 'onException...do...' pattern
  for (int i=0; i<numEx; i++) {
    ExceptionHandler *h = dynamic_cast<ExceptionHandler*>(it); it = it->next;
    h->Body(it); it = it->next;
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
  dec.p.DeepList("");
  dec.p.OffsetIndent(-1);
  dec.p.Item();
  dec.p.Print("try ");
  dec.p.DeepList(";");
  body_->Print(kPrintSuppressList);
  dec.p.EndList();
  dec.p.ItemDone();
  for (auto &nd: exList_) {
    dec.p.Item();
    nd->Print();
    dec.p.ItemDone();
  }
  dec.p.EndList();
}


