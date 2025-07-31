
/*
 File:    Matt/ASTDataFlow.cc

 Matt's decompiler Abstract Syntax Tree.
 Data Flow nodes.

 Written by:  Matt, 2025.
 */

#include "Matt/ASTDataFlow.h"
#include "Matt/ASTControlFlow.h"
#include "Matt/Decompiler.h"
#include "Matt/ObjectPrinter.h"

#include "Frames/Frames.h"

#pragma mark - AST_Push

bool AST_Push::IsSymbol()
{
  return ::IsSymbol(dec.GetLiteral(b_));
}

void AST_Push::Print() {
  if (!Resolved()) return PrintNode(false);
  dec.printLiteral(b_);
}

#pragma mark - AST_PushConst

bool AST_PushConst::IsNIL()
{
  return (b_ == NILREF);
}

void AST_PushConst::Print() {
  if (!Resolved()) return PrintNode(false);
  dec.p.PrintConstant(b_);
}

#pragma mark - AST_PushSelf

void AST_PushSelf::Print() {
  if (!Resolved()) return PrintNode(false);
  dec.p.Printf("self");
}

#pragma mark - AST_FindVar

void AST_FindVar::Print() {
  if (!Resolved()) return PrintNode(false);
  dec.printLiteralAsTag(b_);
}

#pragma mark - AST_GetVar

void AST_GetVar::Print() {
  if (!Resolved()) return PrintNode(false);
  dec.printLocal(b_);
}

#pragma mark - AST_Pop

ASTNode *AST_Pop::Resolve(Pass pass)
{
  if ((pass != Pass::DataFlow) || Resolved()) return next;

  AST_Branch *branch = prev ? dynamic_cast<AST_Branch*>(prev) : nullptr;
  if (branch && (branch->b() > pc_)) {
    // If the sequence is 'branch; pop;', the pop can never be reached
    // because there is no jump target between them.
    // Lucky for us, break operations are by definition expressions, so
    // the pop is needed, which makes this a reliable way to find a
    // break instruction.
    // The AST_Break will take care of the jump target when resolved.
    AST_Break *breakNode = new AST_Break(dec, branch->pc(), 0, branch->b());
    delete branch->Unlink();
    this->ReplaceWith(breakNode);
    dec.numASTChanges++;
    return breakNode;
  }
  return AST_Consume1::Resolve(pass);
}

void AST_Pop::Print() {
  if (!Resolved()) return PrintNode(false);
  in_->Print();
// ??  dec.p.Item();
}

#pragma mark - AST_Dup

void AST_Dup::Print() {
  if (!Resolved()) return PrintNode(false);
}

#pragma mark - AST_SetVar

void AST_SetVar::Print() {
  if (!Resolved()) return PrintNode(false);
  dec.p.Item();
  dec.printLocal(b_);
  dec.p.Printf(" := ");
  in_->Print();
}

#pragma mark - AST_FindAndSetVar

void AST_FindAndSetVar::Print() {
  if (!Resolved()) return PrintNode(false);
  dec.printLiteralAsTag(b_);
  dec.p.Printf(" := ");
  in_->Print();
}

#pragma mark - AST_Not

void AST_Not::Print() {
  if (!Resolved()) return PrintNode(false);
  int ppp = dec.precedence;
  bool parentheses = (dec.precedence > 2);
  dec.precedence = 2;
  dec.p.Item();
  if (parentheses) dec.p.Printf("(");
  dec.p.Printf("not ");
  in_->Print();
  if (parentheses) dec.p.Printf(")");
  dec.precedence = ppp;
}

#pragma mark - AST_Length

void AST_Length::Print() {
  if (!Resolved()) return PrintNode(false);
  dec.p.Item();
  dec.p.Printf("length(");
  in_->Print();
  dec.p.Printf(")");
}

#pragma mark - AST_Clone

void AST_Clone::Print() {
  if (!Resolved()) return PrintNode(false);
  dec.p.Item();
  dec.p.Printf("clone(");
  in_->Print();
  dec.p.Printf(")");
}

#pragma mark - AST_Stringer

void AST_Stringer::Print() {
  if (!Resolved()) return PrintNode(false);
}
//  TODO: The input is an Array with at least two elements
//  a '1 && 2' is handled as a '1 & " " & 2', generating an array with three values
//  So the in_ node is AST_MakeArray which consumes the inputs and the 'array symbol
//  void printSource() {
//    if (in_) {
//      AST_MakeArray *array = dynamic_cast<AST_MakeArray*>(in_);
//      if (array) {
//        dec.p.Printf(array->in[0] " & " array->in[1] ... )
//      }
//    }
//  }

#pragma mark - AST_ClassOf

void AST_ClassOf::Print() {
  if (!Resolved()) return PrintNode(false);
  dec.p.Item();
  dec.p.Printf("ClassOf(");
  in_->Print();
  dec.p.Printf(")");
}

#pragma mark - AST_BitNot

void AST_BitNot::Print() {
  if (!Resolved()) return PrintNode(false);
  dec.p.Item();
  dec.p.Printf("bNot(");
  in_->Print();
  dec.p.Printf(")");
}

#pragma mark - AST_BinaryExpression

ASTNode *AST_BinaryExpression::Resolve(Pass pass)
{
  if ((pass != Pass::DataFlow) || Resolved()) return next;

  if ((prev->IsExpr()) && (prev->prev->IsExpr())) {
    in2_ = prev->Unlink();
    in1_ = prev->Unlink();
    dec.numASTChanges++;
    return this;
  } else {
    return next;
  }
}

#pragma mark - AST_BinaryFunction

void AST_BinaryFunction::Print() {
  if (!Resolved()) return PrintNode(false);
  int ppp = dec.precedence;
  dec.precedence = 0;
  dec.p.Item();
  dec.p.Printf("%s(", func_);
  in1_->Print();
  dec.p.Printf(", ");
  in2_->Print();
  dec.p.Printf(")");
  dec.precedence = ppp;
}

#pragma mark - AST_BinaryOperator

void AST_BinaryOperator::Print() {
  if (!Resolved()) return PrintNode(false);
  int ppp = dec.precedence;
  bool parentheses = (dec.precedence > precedence_);
  dec.precedence = precedence_;
  if (parentheses) dec.p.Print("(");
  in1_->Print(); dec.p.Printf(" %s ", op_); in2_->Print();
  if (parentheses) dec.p.Print(")");
  dec.precedence = ppp;
}

#pragma mark - AST_NewArray

void AST_NewArray::Print() {
  if (!Resolved()) return PrintNode(false);
}

#pragma mark - AST_GetPath

void AST_GetPath::Print() {
  if (!Resolved()) return PrintNode(false);
  in1_->Print();
  dec.p.Printf(".");
  PrintPathExpr(in2_);
}

#pragma mark - AST_ARef

void AST_ARef::Print() {
  if (!Resolved()) return PrintNode(false);
  dec.p.Item();
  in1_->Print();
  dec.p.Printf("["); in2_->Print(); dec.p.Printf("]");
}

#pragma mark - AST_SetClass

void AST_SetClass::Print() {
  if (!Resolved()) return PrintNode(false);
  dec.p.Item();
  dec.p.Printf("SetClass(");
  in1_->Print(); dec.p.Printf(", ");
  in2_->Print(); dec.p.Printf(")");
}

#pragma mark - AST_AddArraySlot

void AST_AddArraySlot::Print() {
  if (!Resolved()) return PrintNode(false);
  dec.p.Item();
  dec.p.Printf("AddArraySlot(");
  in1_->Print(); dec.p.Printf(", ");
  in2_->Print(); dec.p.Printf(")");
}

#pragma mark - AST_HasPath

void AST_HasPath::Print() {
  if (!Resolved()) return PrintNode(false);
}

#pragma mark - AST_SetPath

int AST_SetPath::provides() {
  if (Resolved())
    return (b_ == 0) ? kProvidesNone : 1;
  else
    return kProvidesUnknown;
}

ASTNode *AST_SetPath::Resolve(Pass pass)
{
  if ((pass != Pass::DataFlow) || Resolved()) return next;

  if (   (prev->IsExpr())
      && (prev->prev->IsExpr())
      && (prev->prev->prev->IsExpr())) {
    value_ = prev->Unlink();
    path_ = prev->Unlink();
    object_ = prev->Unlink();
    dec.numASTChanges++;
    return next;
  }
  return next;
}

void AST_SetPath::PrintChildren(bool deep) {
  if (object_) object_->PrintNode(deep);
  if (path_) path_->PrintNode(deep);
  if (value_) value_->PrintNode(deep);
}

void AST_SetPath::Print() {
  if (!Resolved()) return PrintNode(false);
  object_->Print();
  dec.p.Print(".");
  PrintPathExpr(path_);
  dec.p.Print(" := ");
  value_->Print();
}

#pragma mark - AST_SetARef

ASTNode *AST_SetARef::Resolve(Pass pass)
{
  if ((pass != Pass::DataFlow) || Resolved()) return next;

  if (   (prev->IsExpr())
      && (prev->prev->IsExpr())
      && (prev->prev->IsExpr())) {
    element_ = prev; prev->Unlink();
    index_ = prev; prev->Unlink();
    object_ = prev; prev->Unlink();
    dec.numASTChanges++;
    return this;
  }
  return next;
}

void AST_SetARef::PrintChildren(bool deep) {
  if (object_) object_->PrintNode(deep);
  if (index_) index_->PrintNode(deep);
  if (element_) element_->PrintNode(deep);
}

void AST_SetARef::Print() {
  if (!Resolved()) return PrintNode(false);
}


#pragma mark - AST_MakeFrame

void AST_MakeFrame::Print() {
  if (!Resolved()) return PrintNode(false);
  AST_Push *mapNode { nullptr };
  if ( (mapNode = dynamic_cast<AST_Push*>(ins_[numIns_-1])) ) {
    dec.p.Print("{");
    dec.p.StartList(","); // TODO: is there a way to figure out if we need a deep list?
    Ref map = dec.GetLiteral(mapNode->b());
    int n = ComputeMapSize(map);
    for (int i = 0; i < n; i++) {
      dec.p.Item();
      Ref tag = GetTag(map, i);
      dec.p.PrintTag(tag);
      dec.p.Print(": ");
      if (i >= numIns_-1)
        dec.p.Printf("nil");
      else
        ins_[i]->Print();
      dec.p.ItemDone();
    }
    dec.p.Trailer(); dec.p.Print("}");
    dec.p.EndList();
  } else {
    assert(0);
  }
}

#pragma mark - AST_MakeArray

void AST_MakeArray::Print() {
  if (!Resolved()) return PrintNode(false);
  dec.p.Print("[");
  dec.p.StartList(","); // TODO: is there a way to figure out if we need a deep list?
  for (int i = 0; i < numIns_-1; i++) {
    dec.p.Item();
    ins_[i]->Print();
    dec.p.ItemDone();
  }
  dec.p.Trailer(); dec.p.Print("]");
  dec.p.EndList();
}

