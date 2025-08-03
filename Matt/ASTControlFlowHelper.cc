
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

#pragma mark - AST_JumpTarget

/**
 \brief Print jump targets that have not been resolved and are still in the AST.
 */
void AST_JumpTarget::Print(uint32_t flags)
{
  PrintNode(true);
}

void AST_JumpTarget::PrintNode(bool deep)
{
  ASTNode::PrintNode(deep);
  dec.p.Printf(" from %d", origin_);
}

#pragma mark - AST_CodeBlock

/**
 \class AST_CodeBlock
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
AST_CodeBlock::AST_CodeBlock(Decompiler &d, int pc, int inProvides)
: ASTNode(d, pc),
  provides_(inProvides)
{ }

/**
 \brief Remove nodes from the AST root an add them as dependencies to this node.
 \param[in] nd start with this node
 \param[in] numNodes number of nodes to move
 \param[in] append them to this list
 */
void AST_CodeBlock::moveToBody(ASTNode *nd, int numNodes, std::vector<ASTNode*> &body)
{
  for (int i = 0; i < numNodes; ++i) {
    ASTNode *nx = nd->next;
    nd->Unlink();
    add(nd);
    nd = nx;
  }
}

void AST_CodeBlock::add(ASTNode *nd)
{
  AST_CodeBlock *cb = dynamic_cast<AST_CodeBlock*>(nd);
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
void AST_CodeBlock::PrintChildren(bool deep) {
  dec.p.Tag(); dec.p.Print("##### ---> Body");
  for (auto &nd: body_) if (nd) nd->PrintNode(deep);
  dec.p.Tag(); dec.p.Print("##### <--- Body");
}

/**
 \brief Print the code block.
 Add 'begin' and 'end' if needed.
 */
void AST_CodeBlock::Print(uint32_t flags)
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
void AST_CodeBlock::PrintBody(const std::string &prolog,
                             const std::string &separator,
                             const std::string &epilog,
                             std::vector<ASTNode*> &body)
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

#pragma mark - AST_ControlBlock

AST_ControlBlock::AST_ControlBlock(Decompiler &d, int pc, int inProvides)
: ASTNode(d, pc),
provides_(inProvides)
{ }

void AST_ControlBlock::PrintChildren(bool deep) {
  dec.p.Tag(); dec.p.Print("##### ---> Body");
  body_->PrintNode(deep);
  dec.p.Tag(); dec.p.Print("##### <--- Body");
}

#pragma mark - AST_CF_Loop

/**
 \class AST_CF_Loop
 \brief Holds the code block of a 'loop' instruction.
 This node is created by resolving another node pattern.
 It is alway marked as resolved.
 */

/**
 \brief Create a new node for a 'loop' instruction.
 The node returns a single value and is marked Resolved.
 */
AST_CF_Loop::AST_CF_Loop(Decompiler &d, int pc)
: AST_ControlBlock(d, pc, kProvidesOne)
{ }

/**
 \brief Print the source code for 'loop'.
 */
void AST_CF_Loop::Print(uint32_t flags) {
  if (!Resolved()) return PrintNode(false);
  dec.p.Print("loop ");
  body_->Print();
};

#pragma mark - AST_CF_While

AST_CF_While::AST_CF_While(Decompiler &d, int pc, ASTNode *condition)
: AST_ControlBlock(d, pc, kProvidesNone), cond_(condition)
{ }

void AST_CF_While::PrintChildren(bool deep) {
  dec.p.Tag(); dec.p.Print("##### ---> while");
  if (cond_) cond_->PrintNode(deep);
  dec.p.Tag(); dec.p.Print("##### <--> do");
  if (body_) body_->PrintNode(deep);
  dec.p.Tag(); dec.p.Print("##### <--- Body");
}

void AST_CF_While::Print(uint32_t flags) {
  if (!Resolved()) return PrintNode(false);
  dec.p.Printf("while "); cond_->Print();
  dec.p.Printf(" do ");
  body_->Print();
};

#pragma mark - AST_CF_Repeat

AST_CF_Repeat::AST_CF_Repeat(Decompiler &d, int pc, ASTNode *condition)
: AST_ControlBlock(d, pc, kProvidesNone), cond_(condition)
{ }

void AST_CF_Repeat::PrintChildren(bool deep) {
  dec.p.Tag(); dec.p.Print("##### ---> Repeat");
  if (body_) body_->PrintNode(deep);
  dec.p.Tag(); dec.p.Print("##### <--> Until");
  if (cond_) cond_->PrintNode(deep);
  dec.p.Tag(); dec.p.Print("##### <--- Condition");
}

void AST_CF_Repeat::Print(uint32_t flags) {
  if (!Resolved()) return PrintNode(false);

  dec.p.Printf("repeat");
  dec.p.DeepList(";");
  body_->Print(kPrintSuppressList);
  dec.p.Trailer(); dec.p.Printf("until "); cond_->Print();
  dec.p.EndList();
};

#pragma mark - AST_CF_IfThen

AST_CF_IfThen::AST_CF_IfThen(Decompiler &d, int pc, ASTNode *condition, bool returnsAValue)
: AST_ControlBlock(d, pc, returnsAValue ? kProvidesOne : kProvidesNone), cond_(condition)
{ }

void AST_CF_IfThen::PrintChildren(bool deep) {
  dec.p.Tag(); dec.p.Print("##### ---> If Condition");
  if (cond_) cond_->PrintNode(deep);
  dec.p.Tag(); dec.p.Print("##### <--> If Body ");
  if (body_) body_->PrintNode(deep);
  dec.p.Tag(); dec.p.Print("##### <--> Else Body ");
  if (elseBody_) elseBody_->PrintNode(deep);
  dec.p.Tag(); dec.p.Print("##### <--- If Done");
}

void AST_CF_IfThen::Print(uint32_t flags) {
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

#pragma mark - AST_CF_Break

/**
 \brief This node writes out a 'break' instruction.
 */
AST_CF_Break::AST_CF_Break(Decompiler &d, int origin, int target, ASTNode *input)
: ASTNode(d, origin, 0, target), in_(input)
{ }

/**
 \brief Print the 'break' instruction.
 'Break' takes an expression, but if that is 'nil', it's not written out in the source code.
 */
void AST_CF_Break::Print(uint32_t flags) {
  dec.p.Printf("break");
  if (!in_->IsNIL()) {
    dec.p.Printf(" ");
    in_->Print();
  }
}

#pragma mark - AST_CF_ForLoop

AST_CF_ForLoop::AST_CF_ForLoop(Decompiler &d, int pc, ASTNode *iter, ASTNode *limit, ASTNode *incr)
: AST_CodeBlock(d, pc, kProvidesOne),
  iter_(iter), limit_(limit), incr_(incr)
{ }

void AST_CF_ForLoop::PrintChildren(bool deep)
{
  dec.p.Tag(); dec.p.Print("##### ---> For start");
  if (iter_) iter_->PrintNode(deep);
  dec.p.Tag(); dec.p.Print("##### <--> to ");
  if (limit_) limit_->PrintNode(deep);
  dec.p.Tag(); dec.p.Print("##### <--> by ");
  if (incr_) incr_->PrintNode(deep);
  dec.p.Tag(); dec.p.Print("##### <--> For Body ");
  for (auto &nd: body_) if (nd) nd->PrintNode(deep);
  dec.p.Tag(); dec.p.Print("##### <--- For Done");
}

void AST_CF_ForLoop::Print(uint32_t flags)
{
  bool printBy = true;
  AST_BC_PushConst *incrValNode = dynamic_cast<AST_BC_PushConst*>(incr_);
  if (incrValNode && (incrValNode->b() == MAKEINT(1))) printBy = false;

  dec.p.Print("for ");
  iter_->Print();
  dec.p.Print(" to ");
  limit_->Print();
  if (printBy) {
    dec.p.Print(" by ");
    incr_->Print();
  }
  if (body_.size() <= 1)
    PrintBody(" do", ";", "", body_);
  else
    PrintBody(" do begin", ";", "end", body_);
}

#pragma mark - AST_CF_ForEachSlotDo

AST_CF_ForEachSlotValueDo::AST_CF_ForEachSlotValueDo(Decompiler &d, int pc, ASTNode *obj, int slot, int value, bool deeply)
: AST_CodeBlock(d, pc, kProvidesNone),
object_(obj), slot_(slot), value_(value), deeply_(deeply)
{ }

void AST_CF_ForEachSlotValueDo::PrintChildren(bool deep)
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
  for (auto &nd: body_) if (nd) nd->PrintNode(deep);
  dec.p.Tag(); dec.p.Print("##### <--- Foreach Done");
}

void AST_CF_ForEachSlotValueDo::Print(uint32_t flags)
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
  if (body_.size() <= 1)
    PrintBody(" do", ";", "", body_);
  else
    PrintBody(" do begin", ";", "end", body_);
}



