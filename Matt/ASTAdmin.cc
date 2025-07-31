
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

#pragma mark -

void ASTFirstNode::Print() {
  if (dec.output == Print::script) {
  } else {
    dec.p.Item();
    printHeader();
    dec.p.Printf("ASTFirstNode ###");
  }
}

#pragma mark -

void ASTLastNode::Print() {
  if (dec.output == Print::script) {
  } else {
    dec.p.Item();
    printHeader();
    dec.p.Printf("ASTLastNode ###\n");
  }
}

#pragma mark -

void ASTBytecodeNode::Print() {
  dec.p.Item();
  printHeader();
  dec.p.Printf("%3d: ERROR: ASTBytecodeNode a=%d, b=%d ###", pc_, a_, b_);
}

void ASTBytecodeNode::PrintPathExpr(ASTNode *inNode) {
  AST_Push *pushLiteral { nullptr };
  if ( (pushLiteral  = dynamic_cast<AST_Push*>(inNode)) ) {
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
    dec.p.Printf("("); inNode->Print(); dec.p.Printf(")");
  }
}

#pragma mark -

auto AST_Consume1::ResolveDataFlow() -> std::tuple<bool, ASTNode*> {
  if (!Resolved() && prev->IsExpr()) {
    in_ = prev;
    prev->Unlink();
    return { true, next };
  } else {
    return { false, next };
  }
}

void AST_Consume1::PrintChildren() {
  if (in_) {
    dec.p.DeepList();
    in_->Print();
    dec.p.EndList();
  }
}

void AST_Consume1::Print() {
  if (dec.output == Print::deep) PrintChildren();
  dec.p.Item();
  printHeader();
  dec.p.Printf("%3d: ERROR: AST_Consume1 ###", pc_);
}

#pragma mark -

auto AST_Consume2::ResolveDataFlow() -> std::tuple<bool, ASTNode*> {
  if (Resolved()) return { false, next }; // nothing more to do
  if ((prev->IsExpr()) && (prev->prev->IsExpr())) {
    in2_ = prev; prev->Unlink();
    in1_ = prev; prev->Unlink();
    return { true, this };
  }
  return { false, next };
}

void AST_Consume2::PrintChildren() {
  dec.p.DeepList();
  if (in1_) in1_->Print();
  if (in2_) in2_->Print();
  dec.p.EndList();
}

void AST_Consume2::Print() {
  if (dec.output == Print::deep) PrintChildren();
  dec.p.Item();
  printHeader();
  dec.p.Printf("%3d: ERROR: AST_Consume2 ###", pc_);
}

#pragma mark -

auto AST_ConsumeN::ResolveDataFlow() -> std::tuple<bool, ASTNode*> {
  if (Resolved()) return { false, next }; // nothing more to do
  ASTNode *nd = this;
  for (int i=0; i<numIns_; i++) {
    nd = nd->prev;
    if (!nd->IsExpr()) { nd = nullptr; break; }
  }
  if (nd == nullptr) return { false, next };
  for (int i=0; i<numIns_; i++) {
    ins_.push_back(nd);
    nd = nd->next;
    nd->prev->Unlink();
  }
  return { true, this };
}

void AST_ConsumeN::PrintChildren() {
  dec.p.DeepList();
  for (auto &nd: ins_) nd->Print();
  dec.p.EndList();
}

void AST_ConsumeN::Print() {
  if (dec.output == Print::deep) PrintChildren();
  dec.p.Item();
  printHeader();
  dec.p.Printf("%3d: ERROR: AST_ConsumeN ###", pc_);
}

void AST_ConsumeN::PrintResolvedCall(int nArgs) {
  // Called by AST_Call, AST_Send, and AST_Resend
  AST_Push *pushLiteral = dynamic_cast<AST_Push*>(ins_[numIns_-1]);
  if (pushLiteral) {
    dec.printLiteralAsTag(pushLiteral->b());
  } else {
    assert(0);
  }
  dec.p.Printf("(");          // Print a list of all arguments
  dec.p.StartList(",");
  for (int i=0; i<nArgs; i++) {
    dec.p.Item(); ins_[i]->Print(); dec.p.ItemDone();
  }
  dec.p.Trailer(); dec.p.Printf(")");
  dec.p.EndList();
}
