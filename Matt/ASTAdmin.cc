
/*
 File:    MattsDecompiler.cc

 Decompile a NewtonScript function.

 Written by:  Matt, 2025.
 */

#include "Matt/ASTAdmin.h"
#include "Matt/ASTDataFlow.h"
#include "Matt/ASTControlFlow.h"
#include "Matt/Decompiler.h"
#include "Matt/ObjectPrinter.h"

extern bool IsPathExpr(RefArg inObj);

using namespace ast;

#pragma mark - FirstNode


#pragma mark - LastNode


#pragma mark - Bytecode

void Bytecode::PrintPathExpr(Node *inNode) {
  BCPush *pushLiteral { nullptr };
  if ( (pushLiteral  = dynamic_cast<BCPush*>(inNode)) ) {
    Ref lit = dec.GetLiteral(pushLiteral->b());
    if (IsInt(lit)) {
      dec.p.PrintInteger(lit);
    } else if (::IsSymbol(lit)) {
      dec.p.PrintTag(lit);
    } else if (IsPathExpr(lit)) {
      dec.p.PrintPathExpr(lit);
    } else {
      assert(0);
    }
  } else {
    dec.p.Printf("("); inNode->Print(0); dec.p.Printf(")");
  }
}

#pragma mark - Consume1

void Consume1::PrintChildren(bool deep)
{
  if (in_) in_->PrintNode(deep);
}

Node *Consume1::Resolve(Pass pass)
{
  if ((pass != Pass::DataFlow) || Resolved()) return next;

  if (prev->IsExpr()) {
    in_ = prev;
    prev->Unlink();
    dec.numASTChanges++;
    return next;
  } else {
    return next;
  }
}


#pragma mark - Consume2

void Consume2::PrintChildren(bool deep)
{
  if (in1_) in1_->PrintNode(deep);
  if (in1_) in2_->PrintNode(deep);
}

Node *Consume2::Resolve(Pass pass)
{
  if ((pass != Pass::DataFlow) || Resolved()) return next;

  if ((prev->IsExpr()) && (prev->prev->IsExpr())) {
    in2_ = prev; prev->Unlink();
    in1_ = prev; prev->Unlink();
    dec.numASTChanges++;
    return this;
  }
  return next;
}


#pragma mark - ConsumeN

void ConsumeN::PrintChildren(bool deep)
{
  for (auto &in: ins_)
    if (in) in->PrintNode(deep);
}

Node *ConsumeN::Resolve(Pass pass)
{
  if ((pass != Pass::DataFlow) || Resolved()) return next;

  Node *nd = this;
  for (int i=0; i<numIns_; i++) {
    nd = nd->prev;
    if (!nd->IsExpr()) { nd = nullptr; break; }
  }
  if (nd == nullptr) return next;
  for (int i=0; i<numIns_; i++) {
    ins_.push_back(nd);
    nd = nd->next;
    nd->prev->Unlink();
  }
  dec.numASTChanges++;
  return this;
}

void ConsumeN::PrintResolvedCall(int nArgs) {
  // Called by BCCall, BCSend, and BCResend
  BCPush *pushLiteral = dynamic_cast<BCPush*>(ins_[numIns_-1]);
  if (pushLiteral) {
    dec.printLiteralAsTag(pushLiteral->b());
  } else {
    assert(0);
  }
  dec.p.Printf("(");          // Print a list of all arguments
  dec.p.StartList(",");
  for (int i=0; i<nArgs; i++) {
    dec.p.Item();
    ins_[i]->Print();
    dec.p.ItemDone();
  }
  dec.p.Trailer(); dec.p.Printf(")");
  dec.p.EndList();
}
