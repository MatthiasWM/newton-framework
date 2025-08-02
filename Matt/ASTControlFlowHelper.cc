
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

#pragma mark - ASTJumpTarget

/**
 \brief Print jump targets that have not been resolved and are still in the AST.
 */
void ASTJumpTarget::Print()
{
  PrintNode(true);
}

void ASTJumpTarget::PrintNode(bool deep)
{
  ASTNode::PrintNode(deep);
  dec.p.Printf(" from %d", origin_);
}

#pragma mark - ASTCodeBlock

/**
 \class ASTCodeBlock
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
ASTCodeBlock::ASTCodeBlock(Decompiler &d, int pc, int inProvides)
: ASTNode(d, pc),
  provides_(inProvides)
{ }

/**
 \brief Remove nodes from the AST root an add them as dependencies to this node.
 \param[in] nd start with this node
 \param[in] numNodes number of nodes to move
 \param[in] append them to this list
 */
void ASTCodeBlock::moveToBody(ASTNode *nd, int numNodes, std::vector<ASTNode*> &body)
{
  for (int i = 0; i < numNodes; ++i) {
    ASTNode *nx = nd->next;
    nd->Unlink();
    body.push_back(nd);
    nd = nx;
  }
}

/**
 \brief Print the body nodes of a code block.
 */
void ASTCodeBlock::PrintChildren(bool deep) {
  dec.p.Tag(); dec.p.Print("##### ---> Body");
  for (auto &nd: body_) if (nd) nd->PrintNode(deep);
  dec.p.Tag(); dec.p.Print("##### <--- Body");
}

/**
 \brief Print a typical code block body.
 */
void ASTCodeBlock::PrintBody(const std::string &prolog,
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

#pragma mark - ASTLoop

/**
 \class ASTLoop
 \brief Holds the code block of a 'loop' instruction.
 This node is created by resolving another node pattern.
 It is alway marked as resolved.
 */

/**
 \brief Create a new node for a 'loop' instruction.
 The node returns a single value and is marked Resolved.
 */
ASTLoop::ASTLoop(Decompiler &d, int pc)
: ASTCodeBlock(d, pc, kProvidesOne)
{ }

/**
 \brief Print the source code for 'loop'.
 */
void ASTLoop::Print() {
  if (!Resolved()) return PrintNode(false);
  if (body_.size() > 1) {
    PrintBody("loop begin", ";", "end", body_);
  } else if (body_.size() == 1) {
    PrintBody("loop", ";", "", body_);
  } else {
    dec.p.Print("loop nil"); // special case, loops forever
  }
};

#pragma mark - ASTWhileDo

ASTWhileDo::ASTWhileDo(Decompiler &d, int pc, ASTNode *condition)
: ASTCodeBlock(d, pc, kProvidesNone), cond_(condition)
{ }

void ASTWhileDo::PrintChildren(bool deep) {
  dec.p.Tag(); dec.p.Print("##### ---> while");
  if (cond_) cond_->PrintNode(deep);
  dec.p.Tag(); dec.p.Print("##### <--> do");
  for (auto &nd: body_) if (nd) nd->PrintNode(deep);
  dec.p.Tag(); dec.p.Print("##### <--- Body");
}

void ASTWhileDo::Print() {
  if (!Resolved()) return PrintNode(false);
  dec.p.Printf("while "); cond_->Print();
  if (body_.size() > 1) {
    PrintBody(" do begin", ";", "end", body_);
  } else if (body_.size() == 1) {
    PrintBody(" do", ";", "", body_);
  } else {
    dec.p.Printf(" do nil"); // special case, loops forever
  }
};

#pragma mark - ASTRepeatUntil

ASTRepeatUntil::ASTRepeatUntil(Decompiler &d, int pc, ASTNode *condition)
: ASTCodeBlock(d, pc, kProvidesNone), cond_(condition)
{ }

void ASTRepeatUntil::PrintChildren(bool deep) {
  dec.p.Tag(); dec.p.Print("##### ---> Repeat");
  for (auto &nd: body_) if (nd) nd->PrintNode(deep);
  dec.p.Tag(); dec.p.Print("##### <--> Until");
  if (cond_) cond_->PrintNode(deep);
  dec.p.Tag(); dec.p.Print("##### <--- Condition");
}

void ASTRepeatUntil::Print() {
  if (!Resolved()) return PrintNode(false);

  dec.p.Printf("repeat");
  dec.p.DeepList(";");
  for (auto &nd: body_) {
    dec.p.Item();
    nd->Print();
    dec.p.ItemDone();
  }
  dec.p.Trailer(); dec.p.Printf("until "); cond_->Print();
  dec.p.EndList();
};

#pragma mark - ASTIfThenElseNode

ASTIfThenElseNode::ASTIfThenElseNode(Decompiler &d, int pc, ASTNode *condition, bool returnsAValue)
: ASTCodeBlock(d, pc, returnsAValue ? kProvidesOne : kProvidesNone), cond_(condition)
{ }

void ASTIfThenElseNode::PrintChildren(bool deep) {
  dec.p.Tag(); dec.p.Print("##### ---> If Condition");
  if (cond_) cond_->PrintNode(deep);
  dec.p.Tag(); dec.p.Print("##### <--> If Body ");
  for (auto &nd: body_) if (nd) nd->PrintNode(deep);
  dec.p.Tag(); dec.p.Print("##### <--> Else Body ");
  for (auto &nd: elseBody_) if (nd) nd->PrintNode(deep);
  dec.p.Tag(); dec.p.Print("##### <--- If Done");
}

void ASTIfThenElseNode::Print() {
  if (!Resolved()) return PrintNode(false);
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
}

#pragma mark - AST_Break

/**
 \brief This node writes out a 'break' instruction.
 */
AST_Break::AST_Break(Decompiler &d, int origin, int target, ASTNode *input)
: ASTNode(d, origin, 0, target), in_(input)
{ }

/**
 \brief Print the 'break' instruction.
 'Break' takes an expression, but if that is 'nil', it's not written out in the source code.
 */
void AST_Break::Print() {
  dec.p.Printf("break");
  if (!in_->IsNIL()) {
    dec.p.Printf(" ");
    in_->Print();
  }
}

